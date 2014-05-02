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

#include <stdio.h>
#include <stdlib.h>

#include <rsvc/cd.h>

static void print_track(rsvc_cd_session_t session, size_t n);
static void print_session(rsvc_cd_t cd, size_t n);

void rsvc_command_print(char* disk, rsvc_done_t done) {
    if (disk == NULL) {
        rsvc_usage();
        done(NULL);
        return;
    }
    rsvc_cd_create(disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        const char* mcn = rsvc_cd_mcn(cd);
        if (*mcn) {
            printf("MCN: %s\n", mcn);
        }
        print_session(cd, 0);

        rsvc_cd_destroy(cd);
        done(NULL);
    });
}

static void print_session(rsvc_cd_t cd, size_t n) {
    if (n == rsvc_cd_nsessions(cd)) {
        return;
    }
    rsvc_cd_session_t session = rsvc_cd_session(cd, n);
    printf("- Session: %zu\n", rsvc_cd_session_number(session));
    print_track(session, 0);
    print_session(cd, n + 1);
}

static void print_track(rsvc_cd_session_t session, size_t n) {
    if (n == rsvc_cd_session_ntracks(session)) {
        return;
    }
    rsvc_cd_track_t track = rsvc_cd_session_track(session, n);
    printf("  - Track: %zu\n", rsvc_cd_track_number(track));
    size_t sectors = rsvc_cd_track_nsectors(track);
    switch (rsvc_cd_track_type(track)) {
        case RSVC_CD_TRACK_DATA: {
            printf("    Type: data\n");
            printf("    Sectors: %zu\n", sectors);
        }
        break;
        case RSVC_CD_TRACK_AUDIO: {
            printf("    Type: audio\n");
            printf("    Channels: %zu\n", rsvc_cd_track_nchannels(track));
            printf("    Duration: %zu:%02zu.%03zu\n",
                   sectors / (75 * 60),
                   (sectors / 75) % 60,
                   ((sectors % 75) * 1000) / 75);
        }
        break;
    }
    print_track(session, n + 1);
}
