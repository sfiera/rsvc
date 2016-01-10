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

static bool check_has_audio_info(rsvc_format_t format, rsvc_done_t fail) {
    if (!format->audio_info) {
        rsvc_errorf(fail, __FILE__, __LINE__, "can't get audio info for %s file", format->name);
        return false;
    }
    return true;
}

static bool print_info(const char* value, int fd, rsvc_done_t fail) {
    rsvc_format_t format;
    struct rsvc_audio_info info;
    if (!(rsvc_format_detect(value, fd, &format, fail)
          && check_has_audio_info(format, fail)
          && format->audio_info(fd, &info, fail))) {
        return false;
    }

    int64_t ms = 1000 * (double)info.samples_per_channel / info.sample_rate;
    int64_t s = ms / 1000;  ms %= 1000;
    int64_t m = s / 60;     s %= 60;
    int64_t h = m / 60;     m %= 60;
    if (h) {
        outf("  - length:      %lld:%02lld:%02lld.%03lld\n", h, m, s, ms);
    } else {
        outf("  - length:      %lld:%02lld.%03lld\n", m, s, ms);
    }
    outf("  - sample rate: %zu\n", info.sample_rate);
    outf("  - channels:    %zu\n", info.channels);
    outf("  - bits:        %zu\n", info.bits_per_sample);
    return true;
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
        for (string_list_node_t curr = options.paths.head; curr; curr = curr->next) {
            int fd;
            if (!rsvc_open(curr->value, O_RDONLY, 0644, &fd, done)) {
                return;
            }
            rsvc_done_t file_fail = ^(rsvc_error_t error){
                close(fd);
                rsvc_prefix_error(curr->value, error, done);
            };
            outf("%s:\n", curr->value);
            if (!print_info(curr->value, fd, file_fail)) {
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
