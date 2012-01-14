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

#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

struct rsvc_cd {
    dispatch_queue_t queue;

    int fd;
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
};

struct rsvc_cd_track {
    size_t number;
    rsvc_cd_t cd;
    rsvc_cd_session_t session;
    rsvc_cd_track_type_t type;
    size_t sector_begin;
    size_t sector_end;
    int nchannels;
    bool pre_emphasis;
    bool copy_permitted;
    CDISRC isrc;
};

static inline bool is_normal_track(const CDTOCDescriptor* desc) {
    return (desc->point < 100) && (desc->adr == 1);
}

void rsvc_cd_create(char* path, void(^done)(rsvc_cd_t, rsvc_error_t)) {
    int fd;
    if ((fd = opendev(path, O_RDONLY | O_NONBLOCK, 0, NULL)) < 0) {
        rsvc_strerror(^(rsvc_error_t error){
            done(NULL, error);
        }, __FILE__, __LINE__);
        return;
    }

    rsvc_cd_t cd    = malloc(sizeof(struct rsvc_cd));
    cd->queue       = dispatch_queue_create("net.sfiera.ripservice.cd", NULL);
    cd->fd          = fd;
    cd->mcn[0]      = '\0';
    cd->ntracks     = 0;

    dispatch_async(cd->queue, ^{
        // Read the CD's MCN, if it has one.
        dk_cd_read_mcn_t cd_read_mcn;
        memset(&cd_read_mcn, 0, sizeof(dk_cd_read_mcn_t));
        if (ioctl(fd, DKIOCCDREADMCN, &cd_read_mcn) >= 0) {
            strcpy(cd->mcn, cd_read_mcn.mcn);
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
        if (ioctl(fd, DKIOCCDREADTOC, &cd_read_toc) < 0) {
            rsvc_strerror(^(rsvc_error_t error){
                done(NULL, error);
            }, __FILE__, __LINE__);
            return;
        }
        CDTOC* toc = (CDTOC*)buffer;

        // Set up the sessions.
        cd->nsessions       = (toc->sessionLast - toc->sessionFirst) + 1;
        cd->sessions        = calloc(cd->nsessions, sizeof(struct rsvc_cd_session));
        for (size_t i = 0; i < cd->nsessions; ++i) {
            memset(&cd->sessions[i], 0, sizeof(struct rsvc_cd_session));
            cd->sessions[i].number = toc->sessionFirst + i;
        }

        // Count the number of normal tracks.
        size_t track_map[512];
        memset(track_map, 0, sizeof(size_t[512]));
        size_t ndesc = CDTOCGetDescriptorCount(toc);
        for (size_t i = 0; i < ndesc; ++i) {
            CDTOCDescriptor* desc = &toc->descriptors[i];
            if (is_normal_track(desc)) {
                track_map[desc->point] = cd->ntracks++;
            } else {
                track_map[desc->point] = -1;
            }
        }

        // Load up the tracks.
        cd->tracks = calloc(cd->ntracks, sizeof(struct rsvc_cd_track));
        rsvc_cd_track_t track = &cd->tracks[0];
        for (size_t i = 0; i < ndesc; ++i) {
            CDTOCDescriptor* desc = &toc->descriptors[i];
            if (is_normal_track(desc)) {
                memset(track, 0, sizeof(struct rsvc_cd_track));
                track->cd               = cd;
                track->session          = cd->sessions + desc->session - cd->sessions[0].number;
                track->number           = desc->point;
                track->sector_begin     = CDConvertMSFToLBA(desc->p);

                track->pre_emphasis     = (desc->control & 0x01) == 0x01;
                track->copy_permitted   = (desc->control & 0x02) == 0x02;
                track->type             = (desc->control & 0x04)
                                        ? RSVC_CD_TRACK_DATA
                                        : RSVC_CD_TRACK_AUDIO;
                track->nchannels        = (desc->control & 0x08) ? 4 : 2;
                track->isrc[0]          = '\0';

                dk_cd_read_isrc_t cd_read_isrc;
                memset(&cd_read_isrc, 0, sizeof(dk_cd_read_isrc_t));
                cd_read_isrc.track = track->number;
                if (ioctl(fd, DKIOCCDREADISRC, &cd_read_isrc) >= 0) {
                    strcpy(track->isrc, cd_read_isrc.isrc);
                }

                ++track;
            } else if (desc->adr == 1) {
                rsvc_cd_session_t session = cd->sessions + desc->session - cd->sessions[0].number;
                switch (desc->point) {
                  case 0xa0:
                    session->track_begin = cd->tracks + track_map[desc->p.minute];
                    break;
                  case 0xa1:
                    session->track_end = cd->tracks + track_map[desc->p.minute] + 1;
                    break;
                  case 0xa2:
                    session->lead_out = CDConvertMSFToLBA(desc->p);
                    break;
                }
            }
        }

        // Set session_end on all tracks.
        for (size_t i = 0; i < cd->ntracks - 1; ++i) {
            cd->tracks[i].sector_end = cd->tracks[i + 1].sector_begin;
        }
        for (size_t i = 0; i < cd->nsessions; ++i) {
            (cd->sessions[i].track_end - 1)->sector_end = cd->sessions[i].lead_out;
        }

        rsvc_strerror(^(rsvc_error_t error){
            done(cd, NULL);
        }, __FILE__, __LINE__);
    });
}

void rsvc_cd_destroy(rsvc_cd_t cd) {
    free(cd->tracks);
    free(cd->sessions);
    close(cd->fd);
    dispatch_release(cd->queue);
    free(cd);
}

const char* rsvc_cd_mcn(rsvc_cd_t cd) {
    return cd->mcn;
}

void rsvc_cd_each_session(rsvc_cd_t cd, void (^block)(rsvc_cd_session_t, rsvc_stop_t stop)) {
    for (__block size_t i = 0; i < cd->nsessions; ++i) {
        block(&cd->sessions[i], ^{
            i = cd->nsessions;
        });
    }
}

size_t rsvc_cd_ntracks(rsvc_cd_t cd) {
    return cd->ntracks;
}

rsvc_cd_track_t rsvc_cd_track(rsvc_cd_t cd, size_t n) {
    return cd->tracks + n;
}

void rsvc_cd_each_track(rsvc_cd_t cd, void (^block)(rsvc_cd_track_t, rsvc_stop_t stop)) {
    for (__block size_t i = 0; i < cd->ntracks; ++i) {
        block(&cd->tracks[i], ^{
            i = cd->ntracks;
        });
    }
}

size_t rsvc_cd_session_number(rsvc_cd_session_t session) {
    return session->number;
}

size_t rsvc_cd_session_lead_out(rsvc_cd_session_t session) {
    return session->lead_out;
}

size_t rsvc_cd_session_ntracks(rsvc_cd_session_t session) {
    return session->track_end - session->track_begin;
}

void rsvc_cd_session_each_track(rsvc_cd_session_t session,
                                void (^block)(rsvc_cd_track_t, rsvc_stop_t stop)) {
    __block rsvc_cd_track_t track;
    for (track = session->track_begin; track != session->track_end; ++track) {
        block(track, ^{
            track = session->track_end;
        });
    }
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
    return rsvc_cd_track_nsectors(track) * kCDSectorSizeCDDA / 4;
}

const char* rsvc_cd_track_isrc(rsvc_cd_track_t track) {
    return track->isrc;
}

void rsvc_cd_track_rip(rsvc_cd_track_t track, int fd_out, void (^done)(rsvc_error_t)) {
    dispatch_async(track->cd->queue, ^{
        uint8_t buffer[kCDSectorSizeCDDA];
        dk_cd_read_t cd_read;

        uint16_t speed = kCDSpeedMax;
        ioctl(track->cd->fd, DKIOCCDSETSPEED, &speed);

        size_t offset = track->sector_begin * kCDSectorSizeCDDA;
        for (size_t sector = track->sector_begin; sector < track->sector_end; ++sector) {
            memset(&cd_read, 0, sizeof(dk_cd_read_t));
            memset(buffer, 0, kCDSectorSizeCDDA);

            cd_read.offset          = offset;
            cd_read.sectorArea      = kCDSectorAreaUser;
            cd_read.sectorType      = kCDSectorTypeCDDA;
            cd_read.buffer          = buffer;
            cd_read.bufferLength    = sizeof(buffer);

            if (ioctl(track->cd->fd, DKIOCCDREAD, &cd_read) < 0) {
                rsvc_strerror(done, __FILE__, __LINE__);
                return;
            }

            size_t written = 0;
            size_t remaining = cd_read.bufferLength;
            while (remaining > 0) {
                int result = write(fd_out, buffer + written, remaining);
                // TODO(sfiera): 0 is not an error, but we must break early.
                if (result < 0) {
                    rsvc_strerror(done, __FILE__, __LINE__);
                    return;
                } else if (result == 0) {
                    rsvc_const_error(done, __FILE__, __LINE__, "pipe closed");
                    return;
                } else {
                    written += result;
                    remaining -= result;
                }
            }
            offset += cd_read.bufferLength;
        }
        done(NULL);
    });
}
