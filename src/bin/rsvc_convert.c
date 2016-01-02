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
#include <rsvc/tag.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../rsvc/group.h"
#include "../rsvc/list.h"
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

static void convert(convert_options_t options, rsvc_done_t done);
static void convert_recursive(convert_options_t options, rsvc_done_t done);
static bool validate_convert_options(convert_options_t options, rsvc_done_t fail);
static void convert_read(convert_options_t options, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, size_t channels, size_t samples_per_channel));
static void convert_write(convert_options_t options, size_t channels, size_t samples_per_channel,
                          int read_fd, const char* tmp_path, rsvc_done_t done);
static void decode_file(convert_options_t options, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done);
static void encode_file(convert_options_t options, int read_fd, const char* path,
                        size_t channels, size_t samples_per_channel, rsvc_done_t done);
static bool change_extension(const char* path, const char* extension, char* new_path,
                             rsvc_done_t fail);
static void copy_tags(convert_options_t options, const char* tmp_path, rsvc_done_t done);

void rsvc_command_convert(convert_options_t options, rsvc_done_t done) {
    if (!validate_convert_options(options, done)) {
        return;
    } else if (options->recursive) {
        convert_recursive(options, done);
    } else {
        convert(options, done);
    }
}

static void convert(convert_options_t options, rsvc_done_t done) {
    options = memdup(options, sizeof(*options));
    options->input = strdup(options->input);
    options->output = options->output ? strdup(options->output) : NULL;
    done = ^(rsvc_error_t error){
        if (options->output) {
            free(options->output);
        }
        free(options->input);
        free(options);
        done(error);
    };

    // Pick the output file name.  If it was explicitly specified, use
    // that; otherwise, generate it by changing the extension on the
    // input file name.
    char path_storage[MAXPATHLEN];
    if (!options->output) {
        if (!change_extension(options->input, options->encode.format->extension, path_storage,
                              done)) {
            return;
        }
        options->output = strdup(path_storage);
    }

    if (!rsvc_open(options->input, O_RDONLY, 0644, &options->input_fd, done)) {
        return;
    }
    done = ^(rsvc_error_t error){
        close(options->input_fd);
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
    if ((fstat(options->input_fd, &st_input) == 0)
        && (stat(options->output, &st_output) == 0)) {
        if ((st_input.st_dev == st_output.st_dev)
            && (st_input.st_ino == st_output.st_ino)) {
            rsvc_errorf(done, __FILE__, __LINE__, "%s and %s are the same file",
                        options->input, options->output);
            return;
        }
        if (options->update && (st_input.st_mtime < st_output.st_mtime)) {
            rsvc_outf(" skip   %s\n", options->output);
            done(NULL);
            return;
        }
    }

    if (options->makedirs) {
        char parent[MAXPATHLEN];
        rsvc_dirname(options->output, parent);
        if (!rsvc_makedirs(parent, 0755, done)) {
            return;
        }
    }

    // Open a temporary file next to the output path.
    if (!rsvc_temp(options->output, 0644, path_storage, &options->output_fd, done)) {
        return;
    }
    char* tmp_path = strdup(path_storage);
    done = ^(rsvc_error_t error){
        close(options->output_fd);
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
                 ^(bool ok, size_t channels, size_t samples_per_channel){
        if (ok) {
            convert_write(options, channels, samples_per_channel, read_pipe, tmp_path,
                          rsvc_group_add(group));
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

static void convert_recursive(convert_options_t options, rsvc_done_t done) {
    rsvc_group_t group = rsvc_group_create(done);
    rsvc_done_t walk_done = rsvc_group_add(group);
    dispatch_semaphore_t sema = dispatch_semaphore_create(rsvc_jobs);

    __block struct path_list outputs = {};
    if (options->delete_) {
        if (!rsvc_walk(options->output, FTS_NOCHDIR, walk_done,
                       ^bool(unsigned short info, const char* dirname, const char* basename,
                             struct stat* st, rsvc_done_t fail){
            if (info != FTS_F) {
                return true;
            }
            struct path_node node = {};
            strcat(node.path, options->output);
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

    if (rsvc_walk(options->input, FTS_NOCHDIR, walk_done,
                  ^bool(unsigned short info, const char* dirname, const char* basename,
                        struct stat* st, rsvc_done_t fail){
        if (info == FTS_DP) {
            if (!options->delete_) {
                return true;
            }
            char dir[MAXPATHLEN];
            strcpy(dir, options->output);
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
        build_path(input, options->input, dirname, basename);
        build_path(output, options->output, dirname, basename);
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
            .input = input,
            .output = output,
            .recursive = false,
            .update = options->update,
            .skip_unknown = true,
            .makedirs = true,
            .encode = options->encode,
        };
        rsvc_logf(2, "- %s", inner_options.input);
        rsvc_logf(2, "+ %s", inner_options.output);

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

static bool validate_convert_options(convert_options_t options, rsvc_done_t fail) {
    if (!validate_encode_options(&options->encode, fail)) {
        return false;
    } else if (!options->input) {
        rsvc_usage(fail);
        return false;
    } else if (!options->output) {
        if (options->recursive) {
            rsvc_errorf(fail, __FILE__, __LINE__, "-r requires output path");
            return false;
        }
    }

    return true;
}

static void convert_read(convert_options_t options, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, size_t channels, size_t samples_per_channel)) {
    __block bool got_metadata = false;
    done = ^(rsvc_error_t error){
        close(write_fd);
        if (!got_metadata) {
            start(false, 0, -1);
        }
        rsvc_prefix_error(options->input, error, done);
    };

    rsvc_decode_metadata_f metadata = ^(int32_t bitrate, size_t channels, size_t samples_per_channel){
        got_metadata = true;
        start(true, channels, samples_per_channel);
    };
    decode_file(options, write_fd, metadata, done);
}

static void convert_write(convert_options_t options, size_t channels, size_t samples_per_channel,
                          int read_fd, const char* tmp_path, rsvc_done_t done) {
    done = ^(rsvc_error_t error){
        close(read_fd);
        done(error);
    };

    // Start the encoder.  When it's done, copy over tags, and then move
    // the temporary file to the final location.
    encode_file(options, read_fd, options->output, channels, samples_per_channel,
                ^(rsvc_error_t error){
        if (error) {
            done(error);
        } else {
            copy_tags(options, tmp_path, ^(rsvc_error_t error){
                if (error) {
                    done(error);
                } else if (rsvc_mv(tmp_path, options->output, done)) {
                    done(NULL);
                }
            });
        }
    });
}

static void decode_file(convert_options_t options, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done) {
    rsvc_format_t format;
    if (!rsvc_format_detect(options->input, options->input_fd, RSVC_FORMAT_DECODE, &format,
                            ^(rsvc_error_t error){
        if (options->skip_unknown) {
            rsvc_outf(" skip   %s\n", options->output);
            done(NULL);
        } else {
            done(error);
        }
    })) {
        return;
    }
    format->decode(options->input_fd, write_fd, start, done);
}

static void encode_file(convert_options_t options, int read_fd, const char* path,
                        size_t channels, size_t samples_per_channel, rsvc_done_t done) {
    rsvc_progress_t node = rsvc_progress_start(path);
    struct rsvc_encode_options encode_options = {
        .bitrate                = options->encode.bitrate,
        .channels               = channels,
        .samples_per_channel    = samples_per_channel,
        .progress = ^(double fraction){
            rsvc_progress_update(node, fraction);
        },
    };
    options->encode.format->encode(read_fd, options->output_fd, &encode_options,
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

static void copy_tags(convert_options_t options, const char* tmp_path, rsvc_done_t done) {
    // If we don't have tag support for both the input and output
    // formats (e.g. conversion to/from WAV), then silently do nothing.
    rsvc_format_t read_fmt, write_fmt;
    rsvc_done_t ignore = ^(rsvc_error_t error){ /* do nothing */ };
    if (!(rsvc_format_detect(options->input, options->input_fd, RSVC_FORMAT_OPEN_TAGS,
                             &read_fmt, ignore)
          && rsvc_format_detect(tmp_path, options->output_fd, RSVC_FORMAT_OPEN_TAGS,
                                &write_fmt, ignore))) {
        done(NULL);
        return;
    }

    rsvc_tags_t read_tags;
    if (!read_fmt->open_tags(options->input, RSVC_TAG_RDONLY, &read_tags, done)) {
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
    rsvc_logf(1, "copying %zu images from %s", rsvc_tags_image_size(read_tags), options->input);
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
