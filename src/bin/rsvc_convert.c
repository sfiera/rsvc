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
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

static void convert(convert_options_t options, rsvc_progress_t progress, rsvc_done_t done);
static void convert_recursive(convert_options_t options, rsvc_progress_t progress, rsvc_done_t done);
static bool validate_convert_options(convert_options_t options, rsvc_done_t fail);
static void convert_read(convert_options_t options, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, size_t samples_per_channel));
static void convert_write(convert_options_t options, size_t samples_per_channel, int read_fd,
                          rsvc_progress_t progress, rsvc_done_t done);
static void decode_file(convert_options_t options, int read_fd, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done);
static void encode_file(struct encode_options* opts, int read_fd, int write_fd, const char* path,
                        size_t samples_per_channel, rsvc_progress_t progress, rsvc_done_t done);
static bool change_extension(const char* path, const char* extension, char* new_path,
                             rsvc_done_t fail);
static void copy_tags(const char* read_path, int read_fd, const char* write_path, int write_fd,
                      rsvc_done_t done);

void rsvc_command_convert(convert_options_t options, rsvc_done_t done) {
    rsvc_progress_t progress = rsvc_progress_create();
    if (!validate_convert_options(options, done)) {
        return;
    } else if (options->recursive) {
        convert_recursive(options, progress, done);
    } else {
        convert(options, progress, done);
    }
}

static void convert(convert_options_t options, rsvc_progress_t progress, rsvc_done_t done) {
    // Pick the output file name.  If it was explicitly specified, use
    // that; otherwise, generate it by changing the extension on the
    // input file name.
    if (!options->output) {
        char new_path[MAXPATHLEN];
        if (!change_extension(options->input, options->encode.format->extension, new_path, done)) {
            return;
        }
        options->output = new_path;
    }

    // If the output file exists, stat it and check that it is different
    // from the input file.  It could be the same if the user passed the
    // same argument twice at the command-line (`rsvc convert a.flac
    // a.flac`) or when using an implicit filename with formats that use
    // the same extension (`rsvc convert a.m4a -falac`).
    //
    // Then, if --update was passed, skip if the output is newer.
    struct stat st_input, st_output;
    if ((stat(options->input, &st_input) == 0)
        && (stat(options->output, &st_output) == 0)) {
        if ((st_input.st_dev == st_output.st_dev)
            && (st_input.st_ino == st_output.st_ino)) {
            rsvc_errorf(done, __FILE__, __LINE__, "%s and %s are the same file",
                        options->input, options->output);
            return;
        }
        if (options->update && (st_input.st_mtime < st_output.st_mtime)) {
            done(NULL);
            return;
        }
    }

    convert_options_t options_copy = memdup(options, sizeof(*options));
    options_copy->input = strdup(options_copy->input);
    options_copy->output = strdup(options_copy->output);
    done = ^(rsvc_error_t error){
        free(options_copy->output);
        free(options_copy->input);
        free(options_copy);
        done(error);
    };

    int read_pipe, write_pipe;
    if (!rsvc_pipe(&read_pipe, &write_pipe, done)) {
        return;
    }

    rsvc_group_t group = rsvc_group_create(done);
    convert_read(options_copy, write_pipe, rsvc_group_add(group),
                 ^(bool ok, size_t samples_per_channel){
        if (ok) {
            convert_write(options_copy, samples_per_channel, read_pipe, progress, rsvc_group_add(group));
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

static void convert_recursive(convert_options_t options, rsvc_progress_t progress, rsvc_done_t done) {
    rsvc_group_t group = rsvc_group_create(done);
    rsvc_done_t walk_done = rsvc_group_add(group);
    dispatch_semaphore_t sema = dispatch_semaphore_create(4);

    if (rsvc_walk(options->input, FTS_NOCHDIR, walk_done,
                  ^bool(unsigned short info, const char* dirname, const char* basename,
                        struct stat* st, rsvc_done_t fail){
        if (info != FTS_F) {
            return true;
        }

        char input[MAXPATHLEN], output[MAXPATHLEN];
        build_path(input, options->input, dirname, basename);
        build_path(output, options->output, dirname, basename);
        if (!change_extension(output, options->encode.format->extension, output, fail)) {
            return false;
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
        convert(&inner_options, progress, ^(rsvc_error_t error){
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
                         void (^start)(bool ok, size_t samples_per_channel)) {
    done = ^(rsvc_error_t error){
        close(write_fd);
        done(error);
    };

    int read_fd;
    if (!rsvc_open(options->input, O_RDONLY, 0644, &read_fd, done)) {
        return;
    }
    __block bool got_metadata = false;
    rsvc_decode_metadata_f metadata = ^(int32_t bitrate, size_t samples_per_channel){
        got_metadata = true;
        start(true, samples_per_channel);
    };
    decode_file(options, read_fd, write_fd, metadata, ^(rsvc_error_t error){
        close(read_fd);
        if (!got_metadata) {
            start(false, -1);
        }
        rsvc_prefix_error(options->input, error, done);
    });
}

static void convert_write(convert_options_t options, size_t samples_per_channel, int read_fd,
                          rsvc_progress_t progress, rsvc_done_t done) {
    char path[MAXPATHLEN];
    done = ^(rsvc_error_t error){
        close(read_fd);
        done(error);
    };

    if (options->makedirs) {
        __block bool cancel = false;
        rsvc_dirname(options->output, ^(const char* parent){
            if (!rsvc_makedirs(parent, 0755, done)) {
                cancel = true;
            }
        });
        if (cancel) {
            return;
        }
    }

    // Open a temporary file next to the output path.
    int write_fd;
    if (!rsvc_temp(options->output, 0644, path, &write_fd, done)) {
        return;
    }
    char* tmp_path = strdup(path);
    done = ^(rsvc_error_t error){
        close(write_fd);
        free(tmp_path);
        done(error);
    };

    // Start the encoder.  When it's done, copy over tags, and then move
    // the temporary file to the final location.
    encode_file(&options->encode, read_fd, write_fd, options->output, samples_per_channel,
                progress, ^(rsvc_error_t error){
        if (error) {
            done(error);
        } else {
            copy_tags(options->input, read_fd, tmp_path, write_fd, ^(rsvc_error_t error){
                if (error) {
                    done(error);
                } else if (rsvc_mv(tmp_path, options->output, done)) {
                    done(NULL);
                }
            });
        }
    });
}

static void decode_file(convert_options_t options, int read_fd, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done) {
    rsvc_format_t format;
    if (!rsvc_format_detect(options->input, read_fd, RSVC_FORMAT_DECODE, &format, ^(rsvc_error_t error){
        if (options->skip_unknown) {
            done(NULL);
        } else {
            done(error);
        }
    })) {
        return;
    }
    format->decode(read_fd, write_fd, start, done);
}

static void encode_file(struct encode_options* opts, int read_fd, int write_fd, const char* path,
                        size_t samples_per_channel, rsvc_progress_t progress, rsvc_done_t done) {
    rsvc_progress_node_t node = rsvc_progress_start(progress, path);
    struct rsvc_encode_options encode_options = {
        .bitrate                = opts->bitrate,
        .samples_per_channel    = samples_per_channel,
        .progress = ^(double fraction){
            rsvc_progress_update(node, fraction);
        },
    };
    opts->format->encode(read_fd, write_fd, &encode_options, ^(rsvc_error_t error){
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

static void copy_tags(const char* read_path, int read_fd, const char* write_path, int write_fd,
                      rsvc_done_t done) {
    // If we don't have tag support for both the input and output
    // formats (e.g. conversion to/from WAV), then silently do nothing.
    rsvc_format_t read_fmt, write_fmt;
    rsvc_done_t ignore = ^(rsvc_error_t error){ /* do nothing */ };
    if (!(rsvc_format_detect(read_path, read_fd, RSVC_FORMAT_OPEN_TAGS, &read_fmt, ignore)
          && rsvc_format_detect(write_path, write_fd, RSVC_FORMAT_OPEN_TAGS, &write_fmt, ignore))) {
        done(NULL);
        return;
    }

    read_fmt->open_tags(read_path, RSVC_TAG_RDONLY, ^(rsvc_tags_t read_tags, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        rsvc_done_t read_done = ^(rsvc_error_t error){
            rsvc_tags_destroy(read_tags);
            done(error);
        };
        write_fmt->open_tags(write_path, RSVC_TAG_RDWR, ^(rsvc_tags_t write_tags, rsvc_error_t error){
            if (error) {
                read_done(error);
                return;
            }
            rsvc_done_t write_done = ^(rsvc_error_t error){
                rsvc_tags_destroy(write_tags);
                read_done(error);
            };
            if (rsvc_tags_copy(write_tags, read_tags, write_done)) {
                rsvc_tags_save(write_tags, write_done);
            }
        });
    });
}
