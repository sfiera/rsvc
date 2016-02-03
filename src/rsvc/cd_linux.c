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

#include <discid/discid.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "unix.h"

struct rsvc_cd {
    dispatch_queue_t   queue;

    FILE*              file;
    char*              path;
    struct cdrom_mcn   mcn;

    size_t             nsessions;
    rsvc_cd_session_t  sessions;
    size_t             ntracks;
    rsvc_cd_track_t    tracks;
};

struct rsvc_cd_session {
    size_t           number;
    rsvc_cd_track_t  track_begin;
    rsvc_cd_track_t  track_end;
    size_t           lead_out;
    DiscId*          discid;
};

struct rsvc_cd_track {
    size_t                   number;
    rsvc_cd_t                cd;
    rsvc_cd_session_t        session;
    enum rsvc_cd_track_type  type;
    size_t                   sector_begin;
    size_t                   sector_end;
    int                      nchannels;
};

bool build_tracks(  rsvc_cd_t cd, rsvc_cd_track_t* track, size_t begin, size_t end,
                    rsvc_done_t fail) {
    for (size_t i = begin; i != end; ++i) {
        struct cdrom_tocentry tocentry = {
            .cdte_track = i,
            .cdte_format = CDROM_LBA,
        };
        if (ioctl(fileno(cd->file), CDROMREADTOCENTRY, &tocentry) != 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return false;
        }
        if (i > begin) {
            (*track)++->sector_end = tocentry.cdte_addr.lba;
        }
        bool data = tocentry.cdte_ctrl & CDROM_DATA_TRACK;
        bool quad = tocentry.cdte_ctrl & 0x8;
        struct rsvc_cd_track build_track = {
            .number        = i,
            .cd            = cd,
            .session       = &cd->sessions[0],
            .type          = data ? RSVC_CD_TRACK_DATA : RSVC_CD_TRACK_AUDIO,
            .sector_begin  = tocentry.cdte_addr.lba,
            .nchannels     = quad ? 4 : 2,
        };
        **track = build_track;
    }
    return true;
}

bool build_sessions(rsvc_cd_t cd, size_t begin, size_t end, rsvc_done_t fail) {
    struct rsvc_cd_session build_session = {
        .number         = 1,
        .track_begin    = &cd->tracks[0],
        .track_end      = &cd->tracks[end - begin],
        .lead_out       = 0,
        .discid         = discid_new(),
    };
    cd->sessions[0] = build_session;
    (void)fail;
    return true;
}

static bool read_leadout(rsvc_cd_t cd, rsvc_cd_track_t track, rsvc_done_t fail) {
    struct cdrom_tocentry tocentry = {
        .cdte_track = CDROM_LEADOUT,
        .cdte_format = CDROM_LBA,
    };
    if (ioctl(fileno(cd->file), CDROMREADTOCENTRY, &tocentry) != 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    track->sector_end = tocentry.cdte_addr.lba;
    return true;
}

bool calculate_musicbrainz_discid(rsvc_cd_t cd, rsvc_done_t fail) {
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
            rsvc_errorf(fail, __FILE__, __LINE__, "%s", discid_get_error_msg(session->discid));
            return false;
        }
    }
    return true;
}

bool rsvc_cd_create(char* path, rsvc_cd_t* cd, rsvc_done_t fail) {
    fail = ^(rsvc_error_t error){
        rsvc_prefix_error(path, error, fail);
    };

    bool ok = false;
    FILE* file;
    if (rsvc_opendev(path, O_RDONLY | O_NONBLOCK, 0, &file, fail)) {
        struct cdrom_tochdr tochdr = {};
        if (ioctl(fileno(file), CDROMREADTOCHDR, &tochdr) != 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        } else {
            size_t begin = tochdr.cdth_trk0;
            size_t end = tochdr.cdth_trk1 + 1;

            struct rsvc_cd build_cd = {
                .file       = file,
                .queue      = dispatch_queue_create("net.sfiera.ripservice.cd", NULL),
                .path       = strdup(path),
                .nsessions  = 1,
                .sessions   = calloc(1, sizeof(struct rsvc_cd_session)),

                .ntracks    = end - begin,
                .tracks     = calloc(end - begin, sizeof(struct rsvc_cd_track)),
            };
            *cd = memdup(&build_cd, sizeof(build_cd));

            // Read the CD's MCN, if it has one.
            struct cdrom_mcn mcn = {};
            if (ioctl(fileno(file), CDROM_GET_MCN, &mcn) == 0) {
                (*cd)->mcn = mcn;
            }

            rsvc_cd_track_t track = &(*cd)->tracks[0];
            if (build_tracks(*cd, &track, begin, end, fail) &&
                read_leadout(*cd, track, fail) &&
                build_sessions(*cd, begin, end, fail)) {
                if (calculate_musicbrainz_discid(*cd, fail)) {
                    ok = true;
                }
                if (!ok) {
                    discid_free((*cd)->sessions[0].discid);
                }
            }
            if (!ok) {
                free((*cd)->sessions);
                free((*cd)->tracks);
                free((*cd)->path);
                dispatch_release((*cd)->queue);
                free(*cd);
            }
        }
        if (!ok) {
            fclose(file);
        }
    }
    if (!ok) {
        *cd = NULL;
    }
    return ok;
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
    fclose(cd->file);
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

size_t rsvc_cd_ntracks(rsvc_cd_t cd) {
    return cd->ntracks;
}

rsvc_cd_track_t rsvc_cd_track(rsvc_cd_t cd, size_t n) {
    return cd->tracks + n;
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

size_t rsvc_cd_track_number(rsvc_cd_track_t track) {
    return track->number;
}

enum rsvc_cd_track_type rsvc_cd_track_type(rsvc_cd_track_t track) {
    return track->type;
}

size_t rsvc_cd_track_nchannels(rsvc_cd_track_t track) {
    return track->nchannels;
}

size_t rsvc_cd_track_nsectors(rsvc_cd_track_t track) {
    return track->sector_end - track->sector_begin;
}

size_t rsvc_cd_track_nsamples(rsvc_cd_track_t track) {
    return rsvc_cd_track_nsectors(track) * CD_FRAMESIZE_RAW / 4;
}

void rsvc_cd_track_isrc(rsvc_cd_track_t track, void (^done)(const char* isrc)) {
    (void)track;  // TODO(sfiera): ISRC on Linux.
    done(NULL);
}

static bool read_frame(rsvc_cd_t cd, FILE* out, size_t frame, rsvc_done_t fail) {
    unsigned char buffer[CD_FRAMESIZE_RAW] = {};
    struct cdrom_read_audio read_audio = {
        .addr_format = CDROM_LBA,
        .addr = {
            .lba = frame,
        },
        .nframes = 1,
        .buf = buffer,
    };

    bool eof = false;
    if (ioctl(fileno(cd->file), CDROMREADAUDIO, &read_audio) != 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", cd->path);
        return false;
    } else if (!rsvc_write("pipe", out, buffer, CD_FRAMESIZE_RAW, fail)) {
        return false;
    } else if (eof) {
        // TODO(sfiera): 0 is not an error, but we must break early.
        rsvc_errorf(fail, __FILE__, __LINE__, "pipe closed");
        return false;
    } else {
        return true;
    }
}

static void read_range(rsvc_cd_t cd, FILE* out, size_t begin, size_t end, bool* cancelled,
                       rsvc_done_t done) {
    if (begin == end) {
        done(NULL);
        return;
    } else if (*cancelled) {
        rsvc_errorf(done, __FILE__, __LINE__, "cancelled");
        return;
    } else if (read_frame(cd, out, begin, done)) {
        dispatch_async(cd->queue, ^{
            read_range(cd, out, begin + 1, end, cancelled, done);
        });
    }
}

void rsvc_cd_track_rip(rsvc_cd_track_t track, FILE* file, rsvc_cancel_t cancel, rsvc_done_t done) {
    rsvc_cd_t cd = track->cd;
    __block bool stopped = false;
    if (cancel) {
        rsvc_cancel_handle_t cancel_handle = rsvc_cancel_add(cancel, ^{
            dispatch_async(cd->queue, ^{
                stopped = true;
            });
        });
        done = ^(rsvc_error_t error){
            rsvc_cancel_remove(cancel, cancel_handle);
            done(error);
        };
    }

    dispatch_async(cd->queue, ^{
        read_range(cd, file, track->sector_begin, track->sector_end, &stopped, done);
    });
}
