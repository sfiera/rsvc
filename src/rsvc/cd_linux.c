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

#include <rsvc/cd.h>

#include "common.h"
#include <Block.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct rsvc_cd {
    dispatch_queue_t queue;

    int fd;
    char* path;
    struct cdrom_mcn mcn;

    size_t nsessions;
    size_t ntracks;
};

void rsvc_cd_create(char* path, void(^done)(rsvc_cd_t, rsvc_error_t)) {
    int fd;
    if ((fd = open(path, O_RDONLY | O_NONBLOCK, 0, NULL)) < 0) {
        rsvc_strerrorf(^(rsvc_error_t error){
            done(NULL, error);
        }, __FILE__, __LINE__, "%s", path);
        return;
    }

    struct rsvc_cd build_cd = {
        .queue      = dispatch_queue_create("net.sfiera.ripservice.cd", NULL),
        .fd         = fd,
        .path       = strdup(path),
    };
    rsvc_cd_t cd = memdup(&build_cd, sizeof(build_cd));

    // Ensure that the done callback does not run in cd->queue.
    done = ^(rsvc_cd_t cd, rsvc_error_t error){
        rsvc_error_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), error,
                         ^(rsvc_error_t error){
            done(cd, error);
        });
    };

    dispatch_async(cd->queue, ^{
        // Read the CD's MCN, if it has one.
        struct cdrom_mcn mcn = {};
        if (ioctl(fd, CDROM_GET_MCN, &mcn) == 0) {
            memcpy(&cd->mcn, &mcn, sizeof(mcn));
        }

        // Calculate the discid for MusicBrainz.
        /*
        for (size_t i = 0; i < cd->nsessions; ++i) {
            rsvc_cd_session_t session = &cd->sessions[i];
            int offsets[101] = {};
            int* offset = offsets;
            *(offset++) = (session->track_end - 1)->sector_end + 150;
            for (rsvc_cd_track_t track = session->track_begin;
                 track != session->track_end; ++track) {
                *(offset++) = track->sector_begin + 150;
            }
            size_t ntracks = session->track_end - session->track_begin;
            if (!discid_put(session->discid, 1, ntracks, offsets)) {
                rsvc_errorf(^(rsvc_error_t error){
                    done(NULL, error);
                }, __FILE__, __LINE__, "%s: discid failure", cd->path);
                goto create_cleanup;
            }
        }
        */

        done(cd, NULL);
    });
}

void rsvc_cd_destroy(rsvc_cd_t cd) {
}

const char* rsvc_cd_mcn(rsvc_cd_t cd) {
    return (const char*)cd->mcn.medium_catalog_number;
}

size_t rsvc_cd_nsessions(rsvc_cd_t cd) {
    return cd->nsessions;
}

rsvc_cd_session_t rsvc_cd_session(rsvc_cd_t cd, size_t n) {
    return NULL;
}

bool rsvc_cd_each_session(rsvc_cd_t cd, void (^block)(rsvc_cd_session_t, rsvc_stop_t stop)) {
    return true;
}

size_t rsvc_cd_ntracks(rsvc_cd_t cd) {
    return cd->ntracks;
}

rsvc_cd_track_t rsvc_cd_track(rsvc_cd_t cd, size_t n) {
    return NULL;
}

bool rsvc_cd_each_track(rsvc_cd_t cd, void (^block)(rsvc_cd_track_t, rsvc_stop_t stop)) {
    return true;
}

size_t rsvc_cd_session_number(rsvc_cd_session_t session) {
    return 0;
}

const char* rsvc_cd_session_discid(rsvc_cd_session_t session) {
    return NULL;
}

size_t rsvc_cd_session_ntracks(rsvc_cd_session_t session) {
    return 0;
}

rsvc_cd_track_t rsvc_cd_session_track(rsvc_cd_session_t session, size_t n) {
    return NULL;
}

bool rsvc_cd_session_each_track(rsvc_cd_session_t session,
                                void (^block)(rsvc_cd_track_t, rsvc_stop_t stop)) {
    return true;
}

size_t rsvc_cd_track_number(rsvc_cd_track_t track) {
    return 0;
}

rsvc_cd_track_type_t rsvc_cd_track_type(rsvc_cd_track_t track) {
    return 0;
}

size_t rsvc_cd_track_nchannels(rsvc_cd_track_t track) {
    return 0;
}

size_t rsvc_cd_track_nsectors(rsvc_cd_track_t track) {
    return 0;
}

size_t rsvc_cd_track_nsamples(rsvc_cd_track_t track) {
    return 0;
}

void rsvc_cd_track_isrc(rsvc_cd_track_t track, void (^done)(const char* isrc)) {
    done(NULL);
}

rsvc_stop_t rsvc_cd_track_rip(rsvc_cd_track_t track, int fd, rsvc_done_t done) {
    rsvc_errorf(done, __FILE__, __LINE__, "unimplemented");
    return NULL;
}
