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

#include <Block.h>
#include <discid/discid.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "unix.h"

struct rsvc_cd {
    dispatch_queue_t queue;

    int fd;
    char* path;
    struct cdrom_mcn mcn;

    size_t nsessions;
    rsvc_cd_session_t sessions;
    size_t ntracks;
    rsvc_cd_track_t tracks;
};

struct rsvc_cd_session {
    size_t number;
    rsvc_cd_track_t track_begin;
    rsvc_cd_track_t track_end;
    size_t lead_out;
    DiscId* discid;
};

struct rsvc_cd_track {
    size_t number;
    rsvc_cd_t cd;
    rsvc_cd_session_t session;
    rsvc_cd_track_type_t type;
    size_t sector_begin;
    size_t sector_end;
    int nchannels;
};

void rsvc_cd_create(char* path, void(^done)(rsvc_cd_t, rsvc_error_t)) {
    int fd;
    if (!rsvc_opendev(path, O_RDONLY | O_NONBLOCK, 0, &fd, ^(rsvc_error_t error){
        done(NULL, error);
    })) {
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
            if (error) {
                rsvc_cd_destroy(cd);
                done(NULL, error);
            } else {
                done(cd, NULL);
            }
        });
    };
    rsvc_done_t fail = ^(rsvc_error_t error){
        done(NULL, error);
    };

    dispatch_async(cd->queue, ^{
        // Read the CD's MCN, if it has one.
        struct cdrom_mcn mcn = {};
        if (ioctl(fd, CDROM_GET_MCN, &mcn) == 0) {
            memcpy(&cd->mcn, &mcn, sizeof(mcn));
        }

        struct cdrom_tochdr tochdr = {};
        if (ioctl(fd, CDROMREADTOCHDR, &tochdr) != 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
            return;
        }

        size_t begin = tochdr.cdth_trk0;
        size_t end = tochdr.cdth_trk1 + 1;

        cd->nsessions = 1;
        cd->sessions = calloc(cd->nsessions, sizeof(struct rsvc_cd_session));

        cd->ntracks = end - begin;
        cd->tracks = calloc(cd->ntracks, sizeof(struct rsvc_cd_track));
        rsvc_cd_track_t track = &cd->tracks[0];
        for (size_t i = begin; i != end; ++i) {
            struct cdrom_tocentry tocentry = {
                .cdte_track = i,
                .cdte_format = CDROM_LBA,
            };
            if (ioctl(fd, CDROMREADTOCENTRY, &tocentry) != 0) {
                rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
                return;
            }
            if (i > begin) {
                (track++)->sector_end = tocentry.cdte_addr.lba;
            }
            bool data = tocentry.cdte_ctrl & CDROM_DATA_TRACK;
            bool quad = tocentry.cdte_ctrl & 0x8;
            struct rsvc_cd_track build_track = {
                .number = i,
                .cd = cd,
                .session = &cd->sessions[0],
                .type = data ? RSVC_CD_TRACK_DATA : RSVC_CD_TRACK_AUDIO,
                .sector_begin = tocentry.cdte_addr.lba,
                .nchannels = quad ? 4 : 2,
            };
            memcpy(track, &build_track, sizeof(build_track));
        }

        /* read leadout */ {
            struct cdrom_tocentry tocentry = {
                .cdte_track = CDROM_LEADOUT,
                .cdte_format = CDROM_LBA,
            };
            if (ioctl(fd, CDROMREADTOCENTRY, &tocentry) != 0) {
                rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
                return;
            }
            track->sector_end = tocentry.cdte_addr.lba;
        }

        struct rsvc_cd_session build_session = {
            .number         = 1,
            .track_begin    = &cd->tracks[0],
            .track_end      = &cd->tracks[end - begin],
            .lead_out       = 0,
            .discid         = discid_new(),
        };
        memcpy(&cd->sessions[0], &build_session, sizeof(build_session));

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
    if (cd->sessions) {
        for (size_t i = 0; i < cd->nsessions; ++i) {
            if (cd->sessions[i].discid) {
                discid_free(cd->sessions[i].discid);
            }
        }
        free(cd->sessions);
    }
    if (cd->tracks) {
        free(cd->tracks);
    }
    free(cd->path);
    close(cd->fd);
    dispatch_release(cd->queue);
    free(cd);
}

const char* rsvc_cd_mcn(rsvc_cd_t cd) {
    return (const char*)cd->mcn.medium_catalog_number;
}

size_t rsvc_cd_nsessions(rsvc_cd_t cd) {
    return cd->nsessions;
}

rsvc_cd_session_t rsvc_cd_session(rsvc_cd_t cd, size_t n) {
    return cd->sessions + n;
}

bool rsvc_cd_each_session(rsvc_cd_t cd, void (^block)(rsvc_cd_session_t, rsvc_stop_t stop)) {
    __block bool loop = true;
    for (size_t i = 0; loop && (i < cd->nsessions); ++i) {
        block(&cd->sessions[i], ^{
            loop = false;
        });
    }
    return loop;
}

size_t rsvc_cd_ntracks(rsvc_cd_t cd) {
    return cd->ntracks;
}

rsvc_cd_track_t rsvc_cd_track(rsvc_cd_t cd, size_t n) {
    return cd->tracks + n;
}

bool rsvc_cd_each_track(rsvc_cd_t cd, void (^block)(rsvc_cd_track_t, rsvc_stop_t stop)) {
    __block bool loop = true;
    for (size_t i = 0; loop && (i < cd->ntracks); ++i) {
        block(&cd->tracks[i], ^{
            loop = false;
        });
    }
    return loop;
}

size_t rsvc_cd_session_number(rsvc_cd_session_t session) {
    return session->number;
}

const char* rsvc_cd_session_discid(rsvc_cd_session_t session) {
    return discid_get_id(session->discid);
}

size_t rsvc_cd_session_ntracks(rsvc_cd_session_t session) {
    return session->track_end - session->track_begin;
}

rsvc_cd_track_t rsvc_cd_session_track(rsvc_cd_session_t session, size_t n) {
    return session->track_begin + n;
}

bool rsvc_cd_session_each_track(rsvc_cd_session_t session,
                                void (^block)(rsvc_cd_track_t, rsvc_stop_t stop)) {
    __block bool loop = true;
    for (rsvc_cd_track_t track = session->track_begin;
         loop && (track != session->track_end); ++track) {
        block(track, ^{
            loop = false;
        });
    }
    return loop;
}

size_t rsvc_cd_track_number(rsvc_cd_track_t track) {
    return track->number;
}

rsvc_cd_track_type_t rsvc_cd_track_type(rsvc_cd_track_t track) {
    return track->type;
}

size_t rsvc_cd_track_nchannels(rsvc_cd_track_t track) {
    return track->nchannels;
}

size_t rsvc_cd_track_nsectors(rsvc_cd_track_t track) {
    return track->sector_end - track->sector_begin;
}

size_t rsvc_cd_track_nsamples(rsvc_cd_track_t track) {
    return rsvc_cd_track_nsectors(track) * 2536 / 4;
}

void rsvc_cd_track_isrc(rsvc_cd_track_t track, void (^done)(const char* isrc)) {
    done(NULL);
}

rsvc_stop_t rsvc_cd_track_rip(rsvc_cd_track_t track, int fd, rsvc_done_t done) {
    rsvc_errorf(done, __FILE__, __LINE__, "unimplemented");
    return NULL;
}
