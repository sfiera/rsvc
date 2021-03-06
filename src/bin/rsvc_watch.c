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

#include <stdbool.h>
#include <stdio.h>

#include <rsvc/disc.h>
#include "../rsvc/common.h"

struct rsvc_command rsvc_watch = {
    .name = "watch",

    .usage = ^{
        errf("usage: %s watch\n", rsvc_progname);
    },

    .run = ^(rsvc_done_t done){
        (void)done;  // run forever.
        __block bool show = false;

        struct rsvc_disc_watch_callbacks callbacks;
        callbacks.appeared = ^(enum rsvc_disc_type type, const char* path){
            if (show) {
                outf("+\t%s\t%s\n", path, rsvc_disc_type_name[type]);
            }
        };
        callbacks.disappeared = ^(enum rsvc_disc_type type, const char* path){
            outf("-\t%s\t%s\n", path, rsvc_disc_type_name[type]);
        };
        callbacks.initialized = ^(rsvc_stop_t stop){
            (void)stop;  // run forever.
            show = true;
        };

        rsvc_disc_watch(callbacks);
    },
};
