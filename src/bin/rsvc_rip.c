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

#include <rsvc/cd.h>
#include <rsvc/disc.h>
#include <rsvc/musicbrainz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../rsvc/group.h"
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

static void rip_all(rsvc_cd_t cd, rip_options_t options, rsvc_done_t done);
static void rip_track(size_t n, size_t ntracks, rsvc_group_t group, rip_options_t options,
                      rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_progress_t progress);
static void get_tags(rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     void (^done)(rsvc_error_t error, rsvc_tags_t tags));
static void set_tags(int fd, char* path, rsvc_tags_t source, rsvc_done_t done);

void rsvc_command_rip(rip_options_t options, rsvc_done_t done) {
    if (!options->disk) {
        rsvc_usage();
        return;
    }
    if (!validate_encode_options(&options->encode, done)) {
        return;
    }

    rsvc_cd_create(options->disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        rip_all(cd, options, ^(rsvc_error_t error){
            rsvc_cd_destroy(cd);
            if (error) {
                done(error);
            } else if (options->eject) {
                rsvc_disc_eject(options->disk, done);
            } else {
                done(NULL);
            }
        });
    });
}

static void rip_all(rsvc_cd_t cd, rip_options_t options, rsvc_done_t done) {
    printf("Ripping…\n");
    rsvc_cd_session_t session = rsvc_cd_session(cd, 0);
    const size_t ntracks = rsvc_cd_session_ntracks(session);
    rsvc_progress_t progress = rsvc_progress_create();

    rsvc_group_t group = rsvc_group_create(^(rsvc_error_t error){
        rsvc_progress_destroy(progress);
        if (!error) {
            printf("all rips done.\n");
        }
        done(error);
    });

    rip_track(0, ntracks, group, options, cd, session, progress);
}

static void rip_track(size_t n, size_t ntracks, rsvc_group_t group, rip_options_t options,
                      rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_progress_t progress) {
    if (n == ntracks) {
        rsvc_group_ready(group);
        return;
    }

    // Skip data tracks.
    rsvc_cd_track_t track = rsvc_cd_session_track(session, n);
    size_t track_number = rsvc_cd_track_number(track);
    if (rsvc_cd_track_type(track) == RSVC_CD_TRACK_DATA) {
        printf("skipping track %zu/%zu\n", track_number, ntracks);
        rip_track(n + 1, ntracks, group, options, cd, session, progress);
        return;
    }

    rsvc_done_t rip_done = rsvc_group_add(group);
    rip_done = ^(rsvc_error_t error){
        if (error) {
            rsvc_group_ready(group);
            rip_done(error);
        } else {
            rip_done(error);
            rip_track(n + 1, ntracks, group, options, cd, session, progress);
        }
    };

    get_tags(cd, session, track, ^(rsvc_error_t error, rsvc_tags_t tags){
        if (error) {
            rip_done(error);
            return;
        }

        rsvc_tags_strf(tags, options->path_format, options->encode.format->extension,
                       ^(rsvc_error_t error, char* path){
            if (error) {
                rip_done(error);
                rsvc_tags_destroy(tags);
                return;
            }

            __block bool makedirs_succeeded;
            rsvc_dirname(path, ^(const char* parent){
                makedirs_succeeded = rsvc_makedirs(parent, 0755, rip_done);
            });
            int file;
            if (!makedirs_succeeded
                || !rsvc_open(path, O_RDWR | O_CREAT | O_EXCL, 0644, &file, rip_done)) {
                rsvc_tags_destroy(tags);
                return;
            }

            rsvc_group_t rip_group = rsvc_group_create(rip_done);
            rsvc_done_t decode_done = rsvc_group_add(rip_group);
            rsvc_done_t encode_done = rsvc_group_add(rip_group);
            rsvc_group_ready(rip_group);

            int read_pipe, write_pipe;
            if (!rsvc_pipe(&read_pipe, &write_pipe, rip_done)) {
                close(file);
                return;
            }

            // Rip the current track.  If that fails, bail.  If it succeeds,
            // start ripping the next track.
            rsvc_cd_track_rip(track, write_pipe, &rsvc_sigint, ^(rsvc_error_t error){
                close(write_pipe);
                decode_done(error);
            });

            // Encode the current track.
            rsvc_progress_node_t node = rsvc_progress_start(progress, path);
            size_t nsamples = rsvc_cd_track_nsamples(track);
            void (^progress)(double) = ^(double fraction){
                rsvc_progress_update(node, fraction);
            };

            struct rsvc_encode_options encode_options = {
                .bitrate                = options->encode.bitrate,
                .samples_per_channel    = nsamples,
                .progress               = progress,
            };
            char* path_copy = strdup(path);
            encode_done = ^(rsvc_error_t error){
                close(file);
                close(read_pipe);
                rsvc_tags_destroy(tags);
                rsvc_progress_done(node);
                free(path_copy);
                encode_done(error);
            };
            options->encode.format->encode(read_pipe, file, &encode_options, ^(rsvc_error_t error){
                if (error) {
                    encode_done(error);
                } else {
                    set_tags(file, path_copy, tags, encode_done);
                }
            });
        });
    });
}

static void get_tags(rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     void (^done)(rsvc_error_t error, rsvc_tags_t tags)) {
    const char* discid      = rsvc_cd_session_discid(session);
    size_t track_number     = rsvc_cd_track_number(track);
    const size_t ntracks    = rsvc_cd_session_ntracks(session);
    const char* mcn         = rsvc_cd_mcn(cd);

    rsvc_tags_t tags = rsvc_tags_new();
    rsvc_done_t wrapped_done = ^(rsvc_error_t error){
        if (error) {
            rsvc_tags_destroy(tags);
            done(error, NULL);
        } else {
            done(NULL, tags);
        }
    };

    if (!rsvc_tags_addf(tags, wrapped_done, RSVC_ENCODER, "ripservice %s", RSVC_VERSION)
            || !rsvc_tags_add(tags, wrapped_done, RSVC_MUSICBRAINZ_DISCID, discid)
            || !rsvc_tags_addf(tags, wrapped_done, RSVC_TRACKNUMBER, "%zu", track_number)
            || !rsvc_tags_addf(tags, wrapped_done, RSVC_TRACKTOTAL, "%zu", ntracks)
            || (*mcn && !rsvc_tags_add(tags, wrapped_done, RSVC_MCN, mcn))) {
        return;
    }

    rsvc_apply_musicbrainz_tags(tags, ^(rsvc_error_t error){
        // MusicBrainz tagging could fail for a number of reasons:
        // wasn't reachable; couldn't find album.  None of those is
        // reason to stop ripping, so ignore and proceed.
        (void)error;
        wrapped_done(NULL);
    });
}

static void set_tags(int fd, char* path, rsvc_tags_t source, rsvc_done_t done) {
    rsvc_format_t format;
    if (!rsvc_format_detect(path, fd, RSVC_FORMAT_OPEN_TAGS, &format, done)) {
        return;
    }
    format->open_tags(path, RSVC_TAG_RDWR, ^(rsvc_tags_t tags, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        rsvc_done_t original_done = done;
        rsvc_done_t done = ^(rsvc_error_t error){
            rsvc_tags_destroy(tags);
            original_done(error);
        };
        if (!rsvc_tags_copy(tags, source, done)) {
            return;
        }

        rsvc_tags_save(tags, done);
    });
}
