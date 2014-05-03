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

#define _POSIX_C_SOURCE 200809L

#include "rsvc.h"

#include <rsvc/tag.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>

#include "../rsvc/group.h"
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

static bool validate_convert_options(convert_options_t options, rsvc_done_t fail);
static void convert_read(convert_options_t options, int write_fd, rsvc_done_t done,
                         void (^start)(bool ok, size_t samples_per_channel));
static void convert_write(convert_options_t options, size_t samples_per_channel, int read_fd,
                          rsvc_done_t done);
static void decode_file(const char* path, int read_fd, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done);
static void encode_file(struct encode_options* opts, int read_fd, int write_fd, const char* path,
                        size_t samples_per_channel, rsvc_done_t done);
static bool change_extension(const char* path, const char* extension, char* new_path,
                             rsvc_done_t fail);
static void copy_tags(const char* read_path, int read_fd, const char* write_path, int write_fd,
                      rsvc_done_t done);

void rsvc_command_convert(convert_options_t options, rsvc_done_t done) {
    int read_pipe, write_pipe;
    if (!(validate_convert_options(options, done)
          && rsvc_pipe(&read_pipe, &write_pipe, done))) {
        return;
    }

    rsvc_group_t group = rsvc_group_create(done);
    convert_read(options, write_pipe, rsvc_group_add(group),
                 ^(bool ok, size_t samples_per_channel){
        if (ok) {
            convert_write(options, samples_per_channel, read_pipe, rsvc_group_add(group));
        } else {
            close(read_pipe);
        }
    });
    rsvc_group_ready(group);
}

static bool validate_convert_options(convert_options_t options, rsvc_done_t fail) {
    if (!validate_encode_options(&options->encode, fail)) {
        return false;
    } else if (!options->input) {
        rsvc_usage(fail);
        return false;
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
    decode_file(options->input, read_fd, write_fd, metadata, ^(rsvc_error_t error){
        close(read_fd);
        if (!got_metadata) {
            start(false, -1);
        }
        rsvc_prefix_error(options->input, error, done);
    });
}

static void convert_write(convert_options_t options, size_t samples_per_channel, int read_fd,
                          rsvc_done_t done) {
    done = ^(rsvc_error_t error){
        close(read_fd);
        done(error);
    };

    int write_fd;
    char* new_path;
    char* tmp_path;
    if (options->output) {
        new_path = strdup(options->output);
        char tmp_path_storage[MAXPATHLEN];
        if (!rsvc_temp(new_path, 0644, tmp_path_storage, &write_fd, done)) {
            return;
        }
        tmp_path = strdup(tmp_path_storage);
    } else {
        char new_path_storage[MAXPATHLEN], tmp_path_storage[MAXPATHLEN];
        if (!(change_extension(options->input, options->encode.format->extension, new_path_storage, done) &&
              rsvc_temp(new_path_storage, 0644, tmp_path_storage, &write_fd, done))) {
            return;
        }
        new_path = strdup(new_path_storage);
        tmp_path = strdup(tmp_path_storage);
    }

    done = ^(rsvc_error_t error){
        free(new_path);
        free(tmp_path);
        close(write_fd);
        rsvc_prefix_error(new_path, error, done);
    };

    encode_file(&options->encode, read_fd, write_fd, new_path, samples_per_channel,
                ^(rsvc_error_t error){
        if (error) {
            done(error);
        } else {
            copy_tags(options->input, read_fd, tmp_path, write_fd, ^(rsvc_error_t error){
                if (error) {
                    done(error);
                } else if (rsvc_mv(tmp_path, new_path, done)) {
                    done(NULL);
                }
            });
        }
    });
}

static void decode_file(const char* path, int read_fd, int write_fd,
                        rsvc_decode_metadata_f start, rsvc_done_t done) {
    rsvc_format_t format;
    if (!rsvc_format_detect(path, read_fd, RSVC_FORMAT_DECODE, &format, done)) {
        return;
    }
    format->decode(read_fd, write_fd, start, done);
}

static void encode_file(struct encode_options* opts, int read_fd, int write_fd, const char* path,
                        size_t samples_per_channel, rsvc_done_t done) {
    rsvc_progress_t progress = rsvc_progress_create();
    rsvc_progress_node_t node = rsvc_progress_start(progress, path);
    struct rsvc_encode_options encode_options = {
        .bitrate                = opts->bitrate,
        .samples_per_channel    = samples_per_channel,
        .progress = ^(double fraction){
            rsvc_progress_update(node, fraction);
        },
    };
    opts->format->encode(read_fd, write_fd, &encode_options, done);
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
    memcpy(p, path, end - path);
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
