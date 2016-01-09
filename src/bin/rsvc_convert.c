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
#include "strlist.h"

struct file_pair {
    char*  input;
    int    input_fd;
    char*  output;
    int    output_fd;
};

static struct convert_options {
    struct string_list     input;
    struct string_list     output;
    bool                   recursive;
    bool                   update;
    bool                   delete_;
    struct encode_options  encode;
} options;

static void convert(struct file_pair f, rsvc_done_t done);
static void convert_recursive(struct file_pair f, dispatch_semaphore_t sema, rsvc_group_t group);
static bool validate_convert_options(rsvc_done_t fail);
static void convert_read(struct file_pair f, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, rsvc_audio_meta_t meta));
static void convert_write(struct file_pair f, rsvc_audio_meta_t meta,
                          int read_fd, const char* tmp_path, rsvc_done_t done);
static bool change_extension(const char* path, const char* extension, char* new_path,
                             rsvc_done_t fail);
static void copy_tags(struct file_pair f, const char* tmp_path, rsvc_done_t done);
static void push_string(struct string_list* list, const char* value);
static bool push_string_option(struct string_list* list, rsvc_option_value_f get_value,
                               rsvc_done_t fail);

struct rsvc_command rsvc_convert = {
    .name = "convert",

    .usage = ^{
        errf(
                "usage: %s convert [OPTIONS] IN... [-o OUT]...\n"
                "\n"
                "Options:\n"
                "  -o, --output PATH       output path name (default: change ext of source)\n"
                "  -b, --bitrate RATE      bitrate in SI format (default: 192k)\n"
                "  -f, --format FMT        output format (default: flac or vorbis)\n"
                "  -r, --recursive         convert folder recursively\n"
                "  -u, --update            skip files that are newer than the source\n"
                "      --delete            delete extraneous files from destination\n"
                "\n"
                "Formats:\n",
                rsvc_progname);
        rsvc_formats_foreach(fmt) {
            if (fmt->encode && fmt->decode) {
                errf("  %s (in, out)\n", fmt->name);
            } else if (fmt->encode) {
                errf("  %s (out)\n", fmt->name);
            } else if (fmt->decode) {
                errf("  %s (in)\n", fmt->name);
            }
        }
    },

    .run = ^(rsvc_done_t done){
        if (!validate_convert_options(done)) {
            return;
        }

        rsvc_group_t group = rsvc_group_create(done);
        dispatch_semaphore_t sema = dispatch_semaphore_create(rsvc_jobs);
        for (string_list_node_t input = options.input.head, output = options.output.head;
             input && output; input = input->next, output = output->next) {
            struct file_pair files = {
                .input = input->value,
                .output = output->value,
            };

            if (options.recursive) {
                convert_recursive(files, sema, group);
            } else {
                rsvc_done_t done = rsvc_group_add(group);
                dispatch_retain(sema);
                dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
                convert(files, ^(rsvc_error_t error){
                    done(error);
                    dispatch_semaphore_signal(sema);
                    dispatch_release(sema);
                });
            }
        }
        rsvc_group_ready(group);
        dispatch_release(sema);
    },

    .short_option = ^bool (int32_t opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        switch (opt) {
          case 'b': return bitrate_option(&options.encode, get_value, fail);
          case 'f': return format_option(&options.encode, get_value, fail);
          case 'o': return push_string_option(&options.output, get_value, fail);
          case 'r': return rsvc_boolean_option(&options.recursive);
          case 'u': return rsvc_boolean_option(&options.update);
          case -1: return rsvc_boolean_option(&options.delete_);
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
        (void)fail;
        push_string(&options.input, arg);
        return true;
    },
};

static void convert(struct file_pair f, rsvc_done_t done) {
    f.input = strdup(f.input);
    f.output = strdup(f.output);
    done = ^(rsvc_error_t error){
        free(f.input);
        free(f.output);
        done(error);
    };

    if (!rsvc_open(f.input, O_RDONLY, 0644, &f.input_fd, done)) {
        return;
    }
    done = ^(rsvc_error_t error){
        close(f.input_fd);
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
    if ((fstat(f.input_fd, &st_input) == 0)
        && (stat(f.output, &st_output) == 0)) {
        if ((st_input.st_dev == st_output.st_dev)
            && (st_input.st_ino == st_output.st_ino)) {
            rsvc_errorf(done, __FILE__, __LINE__, "%s and %s are the same file",
                        f.input, f.output);
            return;
        }
        if (options.update && (st_input.st_mtime < st_output.st_mtime)) {
            outf(" skip   %s\n", f.output);
            done(NULL);
            return;
        }
    }

    if (options.recursive) {
        char parent[MAXPATHLEN];
        rsvc_dirname(f.output, parent);
        if (!rsvc_makedirs(parent, 0755, done)) {
            return;
        }
    }

    // Open a temporary file next to the output path.
    char path_storage[MAXPATHLEN];
    if (!rsvc_temp(f.output, path_storage, &f.output_fd, done)) {
        return;
    }
    char* tmp_path = strdup(path_storage);
    done = ^(rsvc_error_t error){
        close(f.output_fd);
        unlink(tmp_path);
        free(tmp_path);
        done(error);
    };

    int read_pipe, write_pipe;
    if (!rsvc_pipe(&read_pipe, &write_pipe, done)) {
        return;
    }

    rsvc_group_t group = rsvc_group_create(done);
    convert_read(f, write_pipe, rsvc_group_add(group), ^(bool ok, rsvc_audio_meta_t meta){
        if (ok) {
            convert_write(f, meta, read_pipe, tmp_path, rsvc_group_add(group));
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

static void convert_recursive(struct file_pair f, dispatch_semaphore_t sema, rsvc_group_t group) {
    rsvc_done_t walk_done = rsvc_group_add(group);

    __block struct path_list outputs = {};
    if (options.delete_) {
        if (!rsvc_walk(f.output, FTS_NOCHDIR, walk_done,
                       ^bool(unsigned short info, const char* dirname, const char* basename,
                             struct stat* st, rsvc_done_t fail){
            (void)st;
            (void)fail;
            if (info != FTS_F) {
                return true;
            }
            struct path_node node = {};
            strcat(node.path, f.output);
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

    if (rsvc_walk(f.input, FTS_NOCHDIR, walk_done,
                  ^bool(unsigned short info, const char* dirname, const char* basename,
                        struct stat* st, rsvc_done_t fail){
        (void)st;
        if (info == FTS_DP) {
            if (!options.delete_) {
                return true;
            }
            char dir[MAXPATHLEN];
            strcpy(dir, f.output);
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
        build_path(input, f.input, dirname, basename);
        build_path(output, f.output, dirname, basename);
        if (!change_extension(output, options.encode.format->extension, output, fail)) {
            return false;
        }

        if (options.delete_) {
            for (struct path_node* node = outputs.head; node; node = node->next) {
                if (strcmp(node->path, output) == 0) {
                    RSVC_LIST_ERASE(&outputs, node);
                    break;
                }
            }
        }

        struct file_pair inner_files = {
            .input = input,
            .output = output,
        };
        rsvc_logf(2, "- %s", f.input);
        rsvc_logf(2, "+ %s", f.output);

        dispatch_retain(sema);
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        rsvc_done_t convert_done = rsvc_group_add(group);
        convert(inner_files, ^(rsvc_error_t error){
            convert_done(error);
            dispatch_semaphore_signal(sema);
            dispatch_release(sema);
        });
        return true;
    })) {
        walk_done(NULL);
    }
}

static bool validate_convert_options(rsvc_done_t fail) {
    if (!validate_encode_options(&options.encode, fail)) {
        return false;
    } else if (!options.input.head) {
        rsvc_errorf(fail, __FILE__, __LINE__, "no input files");
        return false;
    } else if (options.recursive && !options.output.head) {
        rsvc_errorf(fail, __FILE__, __LINE__, "-r requires output path");
        return false;
    }

    if (options.output.head) {
        // If output paths were specified, ensure that the same number
        // of input and output paths were given.
        for (string_list_node_t input = options.input.head, output = options.output.head;
             input || output; input = input->next, output = output->next) {
            if (!input) {
                rsvc_errorf(fail, __FILE__, __LINE__, "too many output paths");
            } else if (!output) {
                rsvc_errorf(fail, __FILE__, __LINE__, "not enough output paths");
            }
        }
    } else {
        // If they weren't, change the extensions of the input files.
        char path_storage[MAXPATHLEN];
        for (string_list_node_t curr = options.input.head; curr; curr = curr->next) {
            if (!change_extension(curr->value, options.encode.format->extension, path_storage, fail)) {
                return false;
            }
            push_string(&options.output, path_storage);
        }
    }
    return true;
}

static void convert_read(struct file_pair f, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, rsvc_audio_meta_t meta)) {
    __block bool got_metadata = false;
    done = ^(rsvc_error_t error){
        close(write_fd);
        if (!got_metadata) {
            start(false, NULL);
        }
        rsvc_prefix_error(f.input, error, done);
    };

    rsvc_format_t format;
    rsvc_done_t cant_decode = ^(rsvc_error_t error){
        if (options.recursive) {
            outf(" skip   %s\n", f.output);
            done(NULL);
        } else {
            done(error);
        }
    };
    if (!rsvc_format_detect(f.input, f.input_fd, &format, cant_decode)) {
        return;
    } else if (!format->decode) {
        rsvc_errorf(cant_decode, __FILE__, __LINE__,
                    "%s: can't decode %s file", f.input, format->name);
        return;
    }
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if (!format->decode(f.input_fd, write_fd, ^(rsvc_audio_meta_t meta){
            got_metadata = true;
            start(true, meta);
        }, done)) {
            return;
        }
        done(NULL);
    });
}

static void convert_write(struct file_pair f, rsvc_audio_meta_t meta,
                          int read_fd, const char* tmp_path, rsvc_done_t done) {
    done = ^(rsvc_error_t error){
        close(read_fd);
        done(error);
    };

    rsvc_progress_t node = rsvc_progress_start(f.output);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        struct rsvc_encode_options encode_options = {
            .bitrate                  = options.encode.bitrate,
            .meta = {
                .sample_rate          = meta->sample_rate,
                .channels             = meta->channels,
                .samples_per_channel  = meta->samples_per_channel,
            },
            .progress = ^(double fraction){
                rsvc_progress_update(node, fraction);
            },
        };

        if (!options.encode.format->encode(read_fd, f.output_fd, &encode_options, done)) {
            rsvc_progress_done(node, "fail");
            return;
        }

        rsvc_progress_done(node, "done");
        copy_tags(f, tmp_path, ^(rsvc_error_t error){
            if (error) {
                done(error);
                return;
            } else if (!rsvc_mv(tmp_path, f.output, done)) {
                return;
            }

            done(NULL);
        });
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

static void copy_tags(struct file_pair f, const char* tmp_path, rsvc_done_t done) {
    // If we don't have tag support for both the input and output
    // formats (e.g. conversion to/from WAV), then silently do nothing.
    rsvc_format_t read_fmt, write_fmt;
    rsvc_done_t ignore = ^(rsvc_error_t error){ (void)error; /* do nothing */ };
    if (!(rsvc_format_detect(f.input, f.input_fd, &read_fmt, ignore)
          && read_fmt->open_tags
          && rsvc_format_detect(tmp_path, f.output_fd, &write_fmt, ignore)
          && write_fmt->open_tags)) {
        done(NULL);
        return;
    }

    rsvc_tags_t read_tags;
    if (!read_fmt->open_tags(f.input, RSVC_TAG_RDONLY, &read_tags, done)) {
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
    rsvc_logf(1, "copying %zu images from %s", rsvc_tags_image_size(read_tags), f.input);
    for (rsvc_tags_image_iter_t it = rsvc_tags_image_begin(read_tags); rsvc_next(it); ) {
        rsvc_tags_image_add(write_tags, it->format, it->data, it->size, ^(rsvc_error_t error){
            (void)error;
        });
    }
    for (rsvc_tags_iter_t it = rsvc_tags_begin(read_tags); rsvc_next(it); ) {
        rsvc_tags_add(write_tags, ^(rsvc_error_t error){ (void)error; }, it->name, it->value);
    }
    if (!rsvc_tags_save(write_tags, done)) {
        return;
    }
    done(NULL);
}

static void push_string(struct string_list* list, const char* value) {
    struct string_list_node tmp = {
        .value = strdup(value),
    };
    RSVC_LIST_PUSH(list, memdup(&tmp, sizeof(tmp)));
}

static bool push_string_option(struct string_list* list, rsvc_option_value_f get_value,
                               rsvc_done_t fail) {
    char* value;
    if (!get_value(&value, fail)) {
        return false;
    }
    push_string(list, value);
    return true;
}
