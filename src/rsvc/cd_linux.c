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

#include <rsvc/cd.h>

#include <Block.h>
#include <unistd.h>

void rsvc_cd_create(char* path, void(^done)(rsvc_cd_t, rsvc_error_t)) {
    rsvc_errorf(^(rsvc_error_t error){
        done(NULL, error);
    }, __FILE__, __LINE__, "not implemented");
}

void rsvc_cd_destroy(rsvc_cd_t cd) {
}

const char* rsvc_cd_mcn(rsvc_cd_t cd) {
    return NULL;
}

size_t rsvc_cd_nsessions(rsvc_cd_t cd) {
    return 0;
}

rsvc_cd_session_t rsvc_cd_session(rsvc_cd_t cd, size_t n) {
    return NULL;
}

bool rsvc_cd_each_session(rsvc_cd_t cd, void (^block)(rsvc_cd_session_t, rsvc_stop_t stop)) {
    return true;
}

size_t rsvc_cd_ntracks(rsvc_cd_t cd) {
    return 0;
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
