//
// This file is part of Rip Service.
//
// Copyright (C) 2016 Chris Pickel <sfiera@sfzmail.com>
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

#include <sys/mman.h>
#include <string.h>
#include <rsvc/audio.h>
#include <rsvc/format.h>

#include "strlist.h"
#include "../rsvc/common.h"
#include "../rsvc/list.h"
#include "../rsvc/unix.h"

static struct info_options {
    struct string_list  paths;
} options;

static void push_string(struct string_list* list, const char* value) {
    struct string_list_node tmp = {
        .value = strdup(value),
    };
    RSVC_LIST_PUSH(list, memdup(&tmp, sizeof(tmp)));
}

static bool print_audio_info(const char* path, int fd, rsvc_format_t format, int width, rsvc_done_t fail) {
    struct rsvc_audio_info info;
    if (!format->audio_info(fd, &info, fail)) {
        return false;
    }

    outf("%s: ", path);
    for (int i = strlen(path); i < width; ++i) {
        outf(" ");
    }
    int64_t ms = 1000 * (double)info.samples_per_channel / info.sample_rate;
    int64_t s = ms / 1000;  ms %= 1000;
    int64_t m = s / 60;     s %= 60;
    int64_t h = m / 60;     m %= 60;
    if (h) {
        outf("%lld:%02lld:%02lld.%03lld", h, m, s, ms);
    } else {
        outf("%lld:%02lld.%03lld", m, s, ms);
    }
    if (info.channels == 1) {
        outf(" mono");
    } else if (info.channels == 2) {
        outf(" stereo");
    } else {
        outf(" %zu-channel", info.channels);
    }
    outf(" %s audio (%zu-bit,", format->name, info.bits_per_sample);

    int64_t khz = info.sample_rate / 1000;
    int64_t hz = info.sample_rate % 1000;
    if (hz % 10) {
        outf(" %lld.%03lld kHz", khz, hz);
    } else if (hz % 100) {
        outf(" %lld.%02lld kHz", khz, hz / 10);
    } else if (hz % 1000) {
        outf(" %lld.%01lld kHz", khz, hz / 100);
    } else {
        outf(" %lld kHz", khz);
    }
    outf(")\n");
    return true;
}

static bool print_image_info(const char* path, int fd, rsvc_format_t format, int width, rsvc_done_t fail) {
    bool ok = false;
    uint8_t* data;
    size_t size;
    if (rsvc_mmap(path, fd, &data, &size, fail)) {
        struct rsvc_image_info info;
        if (format->image_info(path, data, size, &info, fail)) {
            ok = true;
            outf("%s: ", path);
            for (int i = strlen(path); i < width; ++i) {
                outf(" ");
            }
            outf("%zu×%zu %s image (%zu-bit", info.width, info.height, format->name, info.depth);
            if (info.palette_size != 0) {
                outf(", %zu colors)\n", info.palette_size);
            }
            outf(")\n");
        }
        munmap(data, size);
    }
    return ok;
}

static bool print_info(const char* path, int fd, int width, rsvc_done_t fail) {
    rsvc_format_t format;
    if (!rsvc_format_detect(path, fd, &format, fail)) {
        return false;
    }

    if (format->audio_info) {
        return print_audio_info(path, fd, format, width, fail);
    } else if (format->image_info) {
        return print_image_info(path, fd, format, width, fail);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "no info available");
        return false;
    }
}

struct rsvc_command rsvc_info = {
    .name = "info",

    .usage = ^{
        errf("usage: %s info FILE...\n", rsvc_progname);
    },

    .run = ^(rsvc_done_t done){
        if (!options.paths.head) {
            rsvc_errorf(done, __FILE__, __LINE__, "no input files");
            return;
        }

        int width = 0;
        for (string_list_node_t curr = options.paths.head; curr; curr = curr->next) {
            if (strlen(curr->value) > width) {
                width = strlen(curr->value);
            }
        }

        for (string_list_node_t curr = options.paths.head; curr; curr = curr->next) {
            int fd;
            if (!rsvc_open(curr->value, O_RDONLY, 0644, &fd, done)) {
                return;
            }
            rsvc_done_t file_fail = ^(rsvc_error_t error){
                close(fd);
                rsvc_prefix_error(curr->value, error, done);
            };
            if (!print_info(curr->value, fd, width, file_fail)) {
                return;
            }
            close(fd);
        }
        done(NULL);
    },

    .argument = ^bool (char* arg, rsvc_done_t fail) {
        (void)fail;
        push_string(&options.paths, arg);
        return true;
    },
};
