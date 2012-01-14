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

#ifndef RSVC_CD_H_
#define RSVC_CD_H_

#include <stdlib.h>
#include <rsvc/common.h>

typedef struct rsvc_cd* rsvc_cd_t;
typedef struct rsvc_cd_session* rsvc_cd_session_t;
typedef struct rsvc_cd_track* rsvc_cd_track_t;

typedef enum rsvc_cd_track_type_t {
    RSVC_CD_TRACK_AUDIO,
    RSVC_CD_TRACK_DATA,
} rsvc_cd_track_type_t;

void                    rsvc_cd_create(char* path, void (^done)(rsvc_cd_t, rsvc_error_t));
void                    rsvc_cd_destroy(rsvc_cd_t cd);
const char*             rsvc_cd_mcn(rsvc_cd_t cd);
void                    rsvc_cd_each_session(rsvc_cd_t,
                                             void (^block)(rsvc_cd_session_t, rsvc_stop_t));
size_t                  rsvc_cd_ntracks(rsvc_cd_t cd);
rsvc_cd_track_t         rsvc_cd_track(rsvc_cd_t cd, size_t n);
void                    rsvc_cd_each_track(rsvc_cd_t cd,
                                           void (^block)(rsvc_cd_track_t, rsvc_stop_t));

size_t                  rsvc_cd_session_number(rsvc_cd_session_t session);
size_t                  rsvc_cd_session_lead_out(rsvc_cd_session_t session);
size_t                  rsvc_cd_session_ntracks(rsvc_cd_session_t session);
void                    rsvc_cd_session_each_track(rsvc_cd_session_t session,
                                                   void (^block)(rsvc_cd_track_t, rsvc_stop_t));

size_t                  rsvc_cd_track_number(rsvc_cd_track_t track);
rsvc_cd_track_type_t    rsvc_cd_track_type(rsvc_cd_track_t track);
size_t                  rsvc_cd_track_nchannels(rsvc_cd_track_t track);
size_t                  rsvc_cd_track_nsectors(rsvc_cd_track_t track);
size_t                  rsvc_cd_track_nsamples(rsvc_cd_track_t track);
const char*             rsvc_cd_track_isrc(rsvc_cd_track_t track);
void                    rsvc_cd_track_rip(rsvc_cd_track_t track, int fd_out,
                                          void (^done)(rsvc_error_t));

#endif  // RSVC_CD_H_
