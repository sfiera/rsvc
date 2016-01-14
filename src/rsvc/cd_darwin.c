//
// This file is part of Rip Service.
//
// Copyright (C) 2012 Chris Pickel <sfiera@sfzmail.com>
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

#include <rsvc/cd.h>

#include <Block.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <discid/discid.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "unix.h"

struct rsvc_cd {
    dispatch_queue_t queue;

    int fd;
    char* path;
    CDMCN mcn;

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
    enum rsvc_cd_track_type type;
    size_t sector_begin;
    size_t sector_end;
    int nchannels;
    bool pre_emphasis;
    bool copy_permitted;
};

static inline bool is_normal_track(const CDTOCDescriptor* desc) {
    return (desc->point < 100) && (desc->adr == 1);
}

bool rsvc_cd_create(char* path, rsvc_cd_t* cd, rsvc_done_t fail) {
    struct rsvc_cd template = {
        .queue    = dispatch_queue_create("net.sfiera.ripservice.cd", NULL),
        .path     = strdup(path),
    };
    *cd = memdup(&template, sizeof(template));

    bool ok = false;
    if (rsvc_opendev(path, O_RDONLY | O_NONBLOCK, 0000, &(*cd)->fd, fail)) {
        // Read the CD's MCN, if it has one.
        dk_cd_read_mcn_t cd_read_mcn;
        memset(&cd_read_mcn, 0, sizeof(dk_cd_read_mcn_t));
        if (ioctl((*cd)->fd, DKIOCCDREADMCN, &cd_read_mcn) >= 0) {
            strcpy((*cd)->mcn, cd_read_mcn.mcn);
        }

        // Read the CD's table of contents.
        typedef uint8_t toc_buffer_t[2048];
        dk_cd_read_toc_t cd_read_toc;
        toc_buffer_t buffer;
        memset(&cd_read_toc, 0, sizeof(dk_cd_read_toc_t));
        memset(buffer, 0, sizeof(toc_buffer_t));
        cd_read_toc.format          = kCDTOCFormatTOC;
        cd_read_toc.buffer          = buffer;
        cd_read_toc.bufferLength    = sizeof(toc_buffer_t);
        if (ioctl((*cd)->fd, DKIOCCDREADTOC, &cd_read_toc) < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", (*cd)->path);
        } else {
            CDTOC* toc = (CDTOC*)buffer;

            // Set up the sessions.
            (*cd)->nsessions       = (toc->sessionLast - toc->sessionFirst) + 1;
            (*cd)->sessions        = calloc((*cd)->nsessions, sizeof(struct rsvc_cd_session));
            for (size_t i = 0; i < (*cd)->nsessions; ++i) {
                rsvc_cd_session_t session = &(*cd)->sessions[i];
                memset(session, 0, sizeof(struct rsvc_cd_session));
                session->number = toc->sessionFirst + i;
                session->discid = discid_new();
            }

            // Count the number of normal tracks.
            size_t track_map[512];
            memset(track_map, 0, sizeof(size_t[512]));
            size_t ndesc = CDTOCGetDescriptorCount(toc);
            for (size_t i = 0; i < ndesc; ++i) {
                CDTOCDescriptor* desc = &toc->descriptors[i];
                if (is_normal_track(desc)) {
                    track_map[desc->point] = (*cd)->ntracks++;
                } else {
                    track_map[desc->point] = -1;
                }
            }

            // Load up the tracks.
            (*cd)->tracks = calloc((*cd)->ntracks, sizeof(struct rsvc_cd_track));
            rsvc_cd_track_t track = &(*cd)->tracks[0];
            for (size_t i = 0; i < ndesc; ++i) {
                CDTOCDescriptor* desc = &toc->descriptors[i];
                if (is_normal_track(desc)) {
                    memset(track, 0, sizeof(struct rsvc_cd_track));
                    track->cd               = *cd;
                    track->session          = (*cd)->sessions + desc->session - (*cd)->sessions[0].number;
                    track->number           = desc->point;
                    track->sector_begin     = CDConvertMSFToLBA(desc->p);

                    track->pre_emphasis     = (desc->control & 0x01) == 0x01;
                    track->copy_permitted   = (desc->control & 0x02) == 0x02;
                    track->type             = (desc->control & 0x04)
                                            ? RSVC_CD_TRACK_DATA
                                            : RSVC_CD_TRACK_AUDIO;
                    track->nchannels        = (desc->control & 0x08) ? 4 : 2;

                    ++track;
                } else if (desc->adr == 1) {
                    rsvc_cd_session_t session = (*cd)->sessions + desc->session - (*cd)->sessions[0].number;
                    switch (desc->point) {
                      case 0xa0:
                        session->track_begin = (*cd)->tracks + track_map[desc->p.minute];
                        break;
                      case 0xa1:
                        session->track_end = (*cd)->tracks + track_map[desc->p.minute] + 1;
                        break;
                      case 0xa2:
                        session->lead_out = CDConvertMSFToLBA(desc->p);
                        break;
                    }
                }
            }

            // Set session_end on all tracks.
            for (size_t i = 0; i < (*cd)->ntracks - 1; ++i) {
                (*cd)->tracks[i].sector_end = (*cd)->tracks[i + 1].sector_begin;
            }
            for (size_t i = 0; i < (*cd)->nsessions; ++i) {
                ((*cd)->sessions[i].track_end - 1)->sector_end = (*cd)->sessions[i].lead_out;
            }

            // Calculate the discid for MusicBrainz.
            for (size_t i = 0; i < (*cd)->nsessions; ++i) {
                rsvc_cd_session_t session = &(*cd)->sessions[i];
                int offsets[101] = {};
                int* offset = offsets;
                *(offset++) = (session->track_end - 1)->sector_end + 150;
                for (rsvc_cd_track_t track = session->track_begin;
                     track != session->track_end; ++track) {
                    *(offset++) = track->sector_begin + 150;
                }
                size_t ntracks = session->track_end - session->track_begin;
                if (!discid_put(session->discid, 1, ntracks, offsets)) {
                    rsvc_errorf(fail, __FILE__, __LINE__, "%s: discid failure", (*cd)->path);
                } else {
                    ok = true;
                }
            }
            if (!ok) {
                for (size_t i = 0; i < (*cd)->nsessions; ++i) {
                    discid_free((*cd)->sessions[i].discid);
                }
                free((*cd)->tracks);
                free((*cd)->sessions);
            }
        }
        if (!ok) {
            close((*cd)->fd);
        }
    }
    if (!ok) {
        dispatch_release((*cd)->queue);
        free((*cd)->path);
        free(*cd);
    }
    return ok;
}

void rsvc_cd_destroy(rsvc_cd_t cd) {
    for (size_t i = 0; i < cd->nsessions; ++i) {
        discid_free(cd->sessions[i].discid);
    }
    free(cd->tracks);
    free(cd->sessions);
    free(cd->path);
    close(cd->fd);
    dispatch_release(cd->queue);
    free(cd);
}

const char* rsvc_cd_mcn(rsvc_cd_t cd) {
    return cd->mcn;
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
    return rsvc_cd_track_nsectors(track) * kCDSectorSizeCDDA / 4;
}

void rsvc_cd_track_isrc(rsvc_cd_track_t track, void (^done)(const char* isrc)) {
    dispatch_async(track->cd->queue, ^{
        dk_cd_read_isrc_t cd_read_isrc;
        memset(&cd_read_isrc, 0, sizeof(dk_cd_read_isrc_t));
        cd_read_isrc.track = track->number;
        if (ioctl(track->cd->fd, DKIOCCDREADISRC, &cd_read_isrc) >= 0) {
            done(cd_read_isrc.isrc);
        } else {
            done(NULL);
        }
    });
}

static void read_range(
        rsvc_cd_track_t track, int fd, bool* stopped,
        size_t begin, size_t end, rsvc_done_t done) {
    if (*stopped) {
        rsvc_errorf(done, __FILE__, __LINE__, "cancelled");
        return;
    }

    uint8_t buffer[kCDSectorSizeCDDA];
    dk_cd_read_t cd_read;

    memset(&cd_read, 0, sizeof(dk_cd_read_t));
    memset(buffer, 0, kCDSectorSizeCDDA);

    cd_read.offset          = begin;
    cd_read.sectorArea      = kCDSectorAreaUser;
    cd_read.sectorType      = kCDSectorTypeCDDA;
    cd_read.buffer          = buffer;
    cd_read.bufferLength    = sizeof(buffer);
    if ((end - begin) < cd_read.bufferLength) {
        cd_read.bufferLength = (end - begin);
    }

    if (ioctl(track->cd->fd, DKIOCCDREAD, &cd_read) < 0) {
        rsvc_strerrorf(done, __FILE__, __LINE__, "%s", track->cd->path);
        return;
    }

    // TODO(sfiera): swap on big-endian, I guess.
    bool eof = false;
    if (!rsvc_write("pipe", fd, buffer, cd_read.bufferLength, NULL, &eof, done)) {
        return;
    } else if (eof) {
        // TODO(sfiera): EOF is not an error, but we must break early.
        rsvc_errorf(done, __FILE__, __LINE__, "pipe closed");
        return;
    }

    begin += cd_read.bufferLength;
    if (begin < end) {
        dispatch_async(track->cd->queue, ^{
            read_range(track, fd, stopped, begin, end, done);
        });
    } else {
        done(NULL);
    }
}

void rsvc_cd_track_rip(rsvc_cd_track_t track, FILE* file, rsvc_cancel_t cancel, rsvc_done_t done) {
    // Ensure that the done callback does not run in cd->queue.
    done = ^(rsvc_error_t error){
        rsvc_error_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                         error, done);
    };

    __block bool stopped = false;
    if (cancel) {
        rsvc_cancel_handle_t handle = rsvc_cancel_add(cancel, ^{
            dispatch_async(track->cd->queue, ^{
                stopped = true;
            });
        });
        done = ^(rsvc_error_t error){
            rsvc_cancel_remove(cancel, handle);
            done(error);
        };
    }

    dispatch_async(track->cd->queue, ^{
        uint16_t speed = kCDSpeedMax;
        ioctl(track->cd->fd, DKIOCCDSETSPEED, &speed);

        size_t begin = track->sector_begin * kCDSectorSizeCDDA;
        size_t end = track->sector_end * kCDSectorSizeCDDA;
        read_range(track, fileno(file), &stopped, begin, end, done);
    });
}
