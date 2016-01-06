//
// This file is part of Rip Service.
//
// Copyright (C) 2014 Chris Pickel <sfiera@sfzmail.com>
//
// Rip Service is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or (at
// your option) any later version.
//
// Rip Service is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Rip Service; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#define _BSD_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L

#include "rsvc.h"

#include <fts.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <rsvc/audio.h>
#include <rsvc/format.h>
#include <rsvc/tag.h>
#include "../rsvc/group.h"
#include "../rsvc/list.h"
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

struct file_pair {
    char*  input;
    int    input_fd;
    char*  output;
    int    output_fd;
};

static struct convert_options {
    struct file_pair f;

    bool recursive;
    bool update;
    bool delete_;
    bool skip_unknown;
    bool makedirs;
    struct encode_options encode;
} convert_options;

static void convert(struct convert_options* options, rsvc_done_t done);
static void convert_recursive(struct convert_options* options, rsvc_done_t done);
static bool validate_convert_options(rsvc_done_t fail);
static void convert_read(struct convert_options* options, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, rsvc_audio_meta_t meta));
static void convert_write(struct convert_options* options, rsvc_audio_meta_t meta,
                          int read_fd, const char* tmp_path, rsvc_done_t done);
static void decode_file(struct convert_options* options, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done);
static void encode_file(struct convert_options* options, int read_fd, const char* path,
                        rsvc_audio_meta_t meta, rsvc_done_t done);
static bool change_extension(const char* path, const char* extension, char* new_path,
                             rsvc_done_t fail);
static void copy_tags(struct convert_options* options, const char* tmp_path, rsvc_done_t done);

struct rsvc_command rsvc_convert = {
    .name = "convert",

    .usage = ^{
        errf(
                "usage: %s convert [OPTIONS] IN [-o OUT]\n"
                "\n"
                "Options:\n"
                "  -b, --bitrate RATE      bitrate in SI format (default: 192k)\n"
                "  -f, --format FMT        output format (default: flac or vorbis)\n"
                "  -r, --recursive         convert folder recursively\n"
                "  -u, --update            skip files that are newer than the source\n"
                "      --delete            delete extraneous files from destination\n"
                "\n"
                "Formats:\n",
                rsvc_progname);
        rsvc_formats_each(^(rsvc_format_t format, rsvc_stop_t stop){
            if (format->encode && format->decode) {
                errf("  %s (in, out)\n", format->name);
            } else if (format->encode) {
                errf("  %s (out)\n", format->name);
            } else if (format->decode) {
                errf("  %s (in)\n", format->name);
            }
        });
    },

    .run = ^(rsvc_done_t done){
        if (!validate_convert_options(done)) {
            return;
        } else if (convert_options.recursive) {
            convert_recursive(&convert_options, done);
        } else {
            convert(&convert_options, done);
        }
    },

    .short_option = ^bool (int32_t opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        switch (opt) {
          case 'b': return bitrate_option(&convert_options.encode, get_value, fail);
          case 'f': return format_option(&convert_options.encode, get_value, fail);
          case 'o': return rsvc_string_option(&convert_options.f.output, get_value, fail);
          case 'r': return rsvc_boolean_option(&convert_options.recursive);
          case 'u': return rsvc_boolean_option(&convert_options.update);
          case -1: return rsvc_boolean_option(&convert_options.delete_);
          default:  return rsvc_illegal_short_option(opt, fail);
        }
    },

    .long_option = ^bool (char* opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        return rsvc_long_option((struct rsvc_long_option_name[]){
            {"bitrate",     'b'},
            {"format",      'f'},
            {"recursive",   'r'},
            {"update",      'u'},
            {"delete",      -1},
            {NULL}
        }, callbacks.short_option, opt, get_value, fail);
    },

    .argument = ^bool (char* arg, rsvc_done_t fail) {
        if (!convert_options.f.input) {
            convert_options.f.input = arg;
        } else {
            rsvc_errorf(fail, __FILE__, __LINE__, "too many arguments");
            return false;
        }
        return true;
    },
};

static void convert(struct convert_options* options, rsvc_done_t done) {
    options = memdup(options, sizeof(*options));
    options->f.input = strdup(options->f.input);
    options->f.output = options->f.output ? strdup(options->f.output) : NULL;
    done = ^(rsvc_error_t error){
        if (options->f.output) {
            free(options->f.output);
        }
        free(options->f.input);
        free(options);
        done(error);
    };

    // Pick the output file name.  If it was explicitly specified, use
    // that; otherwise, generate it by changing the extension on the
    // input file name.
    char path_storage[MAXPATHLEN];
    if (!options->f.output) {
        if (!change_extension(options->f.input, options->encode.format->extension, path_storage,
                              done)) {
            return;
        }
        options->f.output = strdup(path_storage);
    }

    if (!rsvc_open(options->f.input, O_RDONLY, 0644, &options->f.input_fd, done)) {
        return;
    }
    done = ^(rsvc_error_t error){
        close(options->f.input_fd);
        done(error);
    };

    // If the output file exists, stat it and check that it is different
    // from the input file.  It could be the same if the user passed the
    // same argument twice at the command-line (`rsvc convert a.flac
    // a.flac`) or when using an implicit filename with formats that use
    // the same extension (`rsvc convert a.m4a -falac`).
    //
    // Then, if --update was passed, skip if the output is newer.
    struct stat st_input, st_output;
    if ((fstat(options->f.input_fd, &st_input) == 0)
        && (stat(options->f.output, &st_output) == 0)) {
        if ((st_input.st_dev == st_output.st_dev)
            && (st_input.st_ino == st_output.st_ino)) {
            rsvc_errorf(done, __FILE__, __LINE__, "%s and %s are the same file",
                        options->f.input, options->f.output);
            return;
        }
        if (options->update && (st_input.st_mtime < st_output.st_mtime)) {
            outf(" skip   %s\n", options->f.output);
            done(NULL);
            return;
        }
    }

    if (options->makedirs) {
        char parent[MAXPATHLEN];
        rsvc_dirname(options->f.output, parent);
        if (!rsvc_makedirs(parent, 0755, done)) {
            return;
        }
    }

    // Open a temporary file next to the output path.
    if (!rsvc_temp(options->f.output, 0644, path_storage, &options->f.output_fd, done)) {
        return;
    }
    char* tmp_path = strdup(path_storage);
    done = ^(rsvc_error_t error){
        close(options->f.output_fd);
        unlink(tmp_path);
        free(tmp_path);
        done(error);
    };

    int read_pipe, write_pipe;
    if (!rsvc_pipe(&read_pipe, &write_pipe, done)) {
        return;
    }

    rsvc_group_t group = rsvc_group_create(done);
    convert_read(options, write_pipe, rsvc_group_add(group),
                 ^(bool ok, rsvc_audio_meta_t meta){
        if (ok) {
            convert_write(options, meta, read_pipe, tmp_path, rsvc_group_add(group));
        } else {
            close(read_pipe);
        }
    });
    rsvc_group_ready(group);
}

static void build_path(char* out, const char* a, const char* b, const char* c) {
    out[0] = '\0';
    strcat(out, a);
    if (b) {
        strcat(out, "/");
        strcat(out, b);
    }
    if (c) {
        strcat(out, "/");
        strcat(out, c);
    }
}

struct path_list {
    struct path_node {
        char path[MAXPATHLEN];
        struct path_node *prev, *next;
    } *head, *tail;
};

static void convert_recursive(struct convert_options* options, rsvc_done_t done) {
    rsvc_group_t group = rsvc_group_create(done);
    rsvc_done_t walk_done = rsvc_group_add(group);
    dispatch_semaphore_t sema = dispatch_semaphore_create(rsvc_jobs);

    __block struct path_list outputs = {};
    if (options->delete_) {
        if (!rsvc_walk(options->f.output, FTS_NOCHDIR, walk_done,
                       ^bool(unsigned short info, const char* dirname, const char* basename,
                             struct stat* st, rsvc_done_t fail){
            if (info != FTS_F) {
                return true;
            }
            struct path_node node = {};
            strcat(node.path, options->f.output);
            if (dirname) {
                strcat(node.path, "/");
                strcat(node.path, dirname);
            }
            strcat(node.path, "/");
            strcat(node.path, basename);
            RSVC_LIST_PUSH(&outputs, memdup(&node, sizeof(node)));
            return true;
        })) {
            return;
        }
    }

    if (rsvc_walk(options->f.input, FTS_NOCHDIR, walk_done,
                  ^bool(unsigned short info, const char* dirname, const char* basename,
                        struct stat* st, rsvc_done_t fail){
        if (info == FTS_DP) {
            if (!options->delete_) {
                return true;
            }
            char dir[MAXPATHLEN];
            strcpy(dir, options->f.output);
            if (dirname) {
                strcat(dir, "/");
                strcat(dir, dirname);
            }
            if (basename) {
                strcat(dir, "/");
                strcat(dir, basename);
            }
            rsvc_logf(1, "cleaning %s", dir);
            for (struct path_node* node = outputs.head; node; node = node->next) {
                if (strstr(node->path, dir) == node->path) {
                    char* slash = strrchr(node->path, '/');
                    if (slash && (slash == node->path + strlen(dir))) {
                        if (!rsvc_rm(node->path, fail)) {
                            return false;
                        }
                    }
                }
            }
            return true;
        } else if (info != FTS_F) {
            return true;
        }

        char input[MAXPATHLEN], output[MAXPATHLEN];
        build_path(input, options->f.input, dirname, basename);
        build_path(output, options->f.output, dirname, basename);
        if (!change_extension(output, options->encode.format->extension, output, fail)) {
            return false;
        }

        if (options->delete_) {
            for (struct path_node* node = outputs.head; node; node = node->next) {
                if (strcmp(node->path, output) == 0) {
                    RSVC_LIST_ERASE(&outputs, node);
                    break;
                }
            }
        }

        struct convert_options inner_options = {
            .f = {
                .input = input,
                .output = output,
            },
            .recursive = false,
            .update = options->update,
            .skip_unknown = true,
            .makedirs = true,
            .encode = options->encode,
        };
        rsvc_logf(2, "- %s", inner_options.f.input);
        rsvc_logf(2, "+ %s", inner_options.f.output);

        dispatch_retain(sema);
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        rsvc_done_t convert_done = rsvc_group_add(group);
        convert(&inner_options, ^(rsvc_error_t error){
            convert_done(error);
            dispatch_semaphore_signal(sema);
            dispatch_release(sema);
        });
        return true;
    })) {
        walk_done(NULL);
    }
    dispatch_release(sema);
    rsvc_group_ready(group);
}

static bool validate_convert_options(rsvc_done_t fail) {
    if (!validate_encode_options(&convert_options.encode, fail)) {
        return false;
    } else if (!convert_options.f.input) {
        rsvc_usage(fail);
        return false;
    } else if (!convert_options.f.output) {
        if (convert_options.recursive) {
            rsvc_errorf(fail, __FILE__, __LINE__, "-r requires output path");
            return false;
        }
    }

    return true;
}

static void convert_read(struct convert_options* options, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, rsvc_audio_meta_t meta)) {
    __block bool got_metadata = false;
    done = ^(rsvc_error_t error){
        close(write_fd);
        if (!got_metadata) {
            start(false, NULL);
        }
        rsvc_prefix_error(options->f.input, error, done);
    };

    rsvc_decode_metadata_f metadata = ^(rsvc_audio_meta_t meta){
        got_metadata = true;
        start(true, meta);
    };
    decode_file(options, write_fd, metadata, done);
}

static void convert_write(struct convert_options* options, rsvc_audio_meta_t meta,
                          int read_fd, const char* tmp_path, rsvc_done_t done) {
    done = ^(rsvc_error_t error){
        close(read_fd);
        done(error);
    };

    // Start the encoder.  When it's done, copy over tags, and then move
    // the temporary file to the final location.
    encode_file(options, read_fd, options->f.output, meta,
                ^(rsvc_error_t error){
        if (error) {
            done(error);
        } else {
            copy_tags(options, tmp_path, ^(rsvc_error_t error){
                if (error) {
                    done(error);
                } else if (rsvc_mv(tmp_path, options->f.output, done)) {
                    done(NULL);
                }
            });
        }
    });
}

static void decode_file(struct convert_options* options, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done) {
    rsvc_format_t format;
    rsvc_done_t cant_decode = ^(rsvc_error_t error){
        if (options->skip_unknown) {
            outf(" skip   %s\n", options->f.output);
            done(NULL);
        } else {
            done(error);
        }
    };
    if (!rsvc_format_detect(options->f.input, options->f.input_fd, &format, cant_decode)) {
        return;
    } else if (!format->decode) {
        rsvc_errorf(cant_decode, __FILE__, __LINE__,
                    "%s: can't decode %s file", options->f.input, format->name);
        return;
    }
    format->decode(options->f.input_fd, write_fd, start, done);
}

static void encode_file(struct convert_options* options, int read_fd, const char* path,
                        rsvc_audio_meta_t meta, rsvc_done_t done) {
    rsvc_progress_t node = rsvc_progress_start(path);
    struct rsvc_encode_options encode_options = {
        .bitrate                  = options->encode.bitrate,
        .meta = {
            .sample_rate          = meta->sample_rate,
            .channels             = meta->channels,
            .samples_per_channel  = meta->samples_per_channel,
        },
        .progress = ^(double fraction){
            rsvc_progress_update(node, fraction);
        },
    };
    options->encode.format->encode(read_fd, options->f.output_fd, &encode_options,
                                   ^(rsvc_error_t error){
        rsvc_progress_done(node);
        done(error);
    });
}

static bool change_extension(const char* path, const char* extension, char* new_path,
                             rsvc_done_t fail) {
    const char* dot = strrchr(path, '.');
    const char* end;
    if (!dot || strchr(dot, '/')) {
        end = path + strlen(path);
    } else {
        end = dot;
    }

    if (((end - path) + strlen(extension) + 2) >= MAXPATHLEN) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: File name too long", path);
        return false;
    }

    char* p = new_path;
    if (new_path != path) {
        memcpy(p, path, end - path);
    }
    p += (end - path);
    *(p++) = '.';
    strcpy(p, extension);
    return true;
}

static void copy_tags(struct convert_options* options, const char* tmp_path, rsvc_done_t done) {
    // If we don't have tag support for both the input and output
    // formats (e.g. conversion to/from WAV), then silently do nothing.
    rsvc_format_t read_fmt, write_fmt;
    rsvc_done_t ignore = ^(rsvc_error_t error){ /* do nothing */ };
    if (!(rsvc_format_detect(options->f.input, options->f.input_fd, &read_fmt, ignore)
          && read_fmt->open_tags
          && rsvc_format_detect(tmp_path, options->f.output_fd, &write_fmt, ignore)
          && write_fmt->open_tags)) {
        done(NULL);
        return;
    }

    rsvc_tags_t read_tags;
    if (!read_fmt->open_tags(options->f.input, RSVC_TAG_RDONLY, &read_tags, done)) {
        return;
    }

    done = ^(rsvc_error_t error){
        rsvc_tags_destroy(read_tags);
        done(error);
    };

    rsvc_tags_t write_tags;
    if (!write_fmt->open_tags(tmp_path, RSVC_TAG_RDWR, &write_tags, done)) {
        return;
    }

    done = ^(rsvc_error_t error){
        rsvc_tags_destroy(write_tags);
        done(error);
    };
    rsvc_logf(1, "copying %zu images from %s", rsvc_tags_image_size(read_tags), options->f.input);
    rsvc_tags_image_each(read_tags, ^(rsvc_format_t format, const uint8_t* data, size_t size,
                                      rsvc_stop_t stop){
        rsvc_tags_image_add(write_tags, format, data, size, ^(rsvc_error_t error){});
    });
    rsvc_tags_each(read_tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        rsvc_tags_add(write_tags, ^(rsvc_error_t error){}, name, value);
    });
    if (!rsvc_tags_save(write_tags, done)) {
        return;
    }
    done(NULL);
}
