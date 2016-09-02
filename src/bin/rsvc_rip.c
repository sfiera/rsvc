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
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include <rsvc/audio.h>
#include <rsvc/cd.h>
#include <rsvc/disc.h>
#include <rsvc/format.h>
#include <rsvc/musicbrainz.h>
#include "../rsvc/group.h"
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

static struct rip_options {
    char* disk;
    struct encode_options encode;
    bool eject;
    char* path_format;
} opts;

static void rip_all(rsvc_cd_t cd, rsvc_done_t done);
static void rip_track(size_t n, size_t ntracks, rsvc_group_t group,
                      rsvc_cd_t cd, rsvc_cd_session_t session);
static void get_tags(rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     void (^done)(rsvc_error_t error, rsvc_tags_t tags));
static void set_tags(FILE* file, char* path, rsvc_tags_t source, rsvc_done_t done);

struct rsvc_command rsvc_rip = {
    .name = "rip",

    .usage = ^{
        errf(
                "usage: %s rip [OPTIONS] [DEVICE]\n"
                "\n"
                "Options:\n"
                "  -b, --bitrate RATE      bitrate in SI format (default: 192k)\n"
                "  -e, --eject             eject CD after ripping\n"
                "  -f, --format FMT        output format (default: flac or vorbis)\n"
                "  -h, --help              show this help page\n"
                "  -p, --path PATH         format string for output (default %%k)\n"
                "\n"
                "Formats:\n",
                rsvc_progname);
        rsvc_formats_foreach(fmt) {
            if (fmt->encode) {
                errf("  %s\n", fmt->name);
            }
        }
    },

    .run = ^(rsvc_done_t done){
        if (opts.disk == NULL) {
            rsvc_default_disk(^(rsvc_error_t error, char* disk){
                if (error) {
                    done(error);
                } else {
                    opts.disk = strdup(disk);
                    rsvc_rip.run(done);
                }
            });
            return;
        }
        if (!opts.path_format) {
            opts.path_format = "%k";
        }

        if (!validate_encode_options(&opts.encode, done)) {
            return;
        }

        rsvc_cd_t cd;
        if (rsvc_cd_create(opts.disk, &cd, done)) {
            rip_all(cd, ^(rsvc_error_t error){
                rsvc_cd_destroy(cd);
                if (error) {
                    done(error);
                } else if (opts.eject) {
                    if (rsvc_disc_eject(opts.disk, done)) {
                        done(NULL);
                    }
                } else {
                    done(NULL);
                }
            });
        }
    },

    .short_option = ^bool (int32_t opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        switch (opt) {
          case 'b': return bitrate_option(&opts.encode, get_value, fail);
          case 'f': return format_option(&opts.encode, get_value, fail);
          case 'p': return path_option(&opts.path_format, get_value, fail);
          case 'e': return rsvc_boolean_option(&opts.eject);
          default:  return rsvc_illegal_short_option(opt, fail);
        }
    },

    .long_option = ^bool (char* opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        return rsvc_long_option((struct rsvc_long_option_name[]){
            {"bitrate",  'b'},
            {"eject",    'e'},
            {"format",   'f'},
            {"path",     'p'},
            {NULL}
        }, callbacks.short_option, opt, get_value, fail);
    },

    .argument = ^bool (char* arg, rsvc_done_t fail) {
        if (!opts.disk) {
            opts.disk = arg;
            return true;
        } else {
            rsvc_errorf(fail, __FILE__, __LINE__, "too many arguments");
            return false;
        }
    },
};

static void rip_all(rsvc_cd_t cd, rsvc_done_t done) {
    outf("Ripping…\n");
    rsvc_cd_session_t session = rsvc_cd_session(cd, 0);
    const size_t ntracks = rsvc_cd_session_ntracks(session);

    rsvc_group_t group = rsvc_group_create(^(rsvc_error_t error){
        if (!error) {
            outf("all rips done.\n");
        }
        done(error);
    });

    rip_track(0, ntracks, group, cd, session);
}

static void rip_track(size_t n, size_t ntracks, rsvc_group_t group,
                      rsvc_cd_t cd, rsvc_cd_session_t session) {
    if (n == ntracks) {
        rsvc_group_ready(group);
        return;
    }

    // Skip data tracks.
    rsvc_cd_track_t track = rsvc_cd_session_track(session, n);
    size_t track_number = rsvc_cd_track_number(track);
    if (rsvc_cd_track_type(track) == RSVC_CD_TRACK_DATA) {
        outf("skipping track %zu/%zu\n", track_number, ntracks);
        rip_track(n + 1, ntracks, group, cd, session);
        return;
    }

    rsvc_done_t rip_done = rsvc_group_add(group);
    rip_done = ^(rsvc_error_t error){
        if (error) {
            rsvc_group_ready(group);
            rip_done(error);
        } else {
            rip_done(error);
            rip_track(n + 1, ntracks, group, cd, session);
        }
    };

    get_tags(cd, session, track, ^(rsvc_error_t error, rsvc_tags_t tags){
        if (error) {
            rip_done(error);
            return;
        }

        char* path;
        if (!rsvc_tags_strf(tags, opts.path_format, opts.encode.format->extension,
                            &path, rip_done)) {
            rsvc_tags_destroy(tags);
            return;
        }

        char parent[MAXPATHLEN];
        rsvc_dirname(path, parent);
        FILE* file;
        if (!(rsvc_makedirs(parent, 0755, rip_done)
              && rsvc_open(path, O_RDWR | O_CREAT | O_EXCL, 0644, &file, rip_done))) {
            rsvc_tags_destroy(tags);
            return;
        }

        rsvc_group_t rip_group = rsvc_group_create(rip_done);
        rsvc_done_t decode_done = rsvc_group_add(rip_group);
        rsvc_done_t encode_done = rsvc_group_add(rip_group);
        rsvc_group_ready(rip_group);

        FILE* read_pipe;
        FILE* write_pipe;
        if (!rsvc_pipe(&read_pipe, &write_pipe, rip_done)) {
            fclose(file);
            free(path);
            return;
        }

        // Rip the current track.  If that fails, bail.  If it succeeds,
        // start ripping the next track.
        rsvc_cd_track_rip(track, write_pipe, &rsvc_sigint, ^(rsvc_error_t error){
            fclose(write_pipe);
            decode_done(error);
        });

        // Encode the current track.
        rsvc_progress_t progress = rsvc_progress_start(path);
        size_t nsamples = rsvc_cd_track_nsamples(track);

        encode_done = ^(rsvc_error_t error){
            fclose(file);
            fclose(read_pipe);
            rsvc_tags_destroy(tags);
            if (error) {
                rsvc_progress_done(progress, "fail");
            } else {
                rsvc_progress_done(progress, "done");
            }
            free(path);
            encode_done(error);
        };
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            struct rsvc_encode_options encode_options = {
                .bitrate = opts.encode.bitrate,
                .info = {
                    .sample_rate          = 44100,
                    .channels             = 2,
                    .samples_per_channel  = nsamples,
                    .bits_per_sample      = 16,
                    .block_align          = 4,
                },
                .progress = ^(double fraction){
                    rsvc_progress_update(progress, fraction);
                },
            };

            if (!opts.encode.format->encode(read_pipe, file, &encode_options, encode_done)) {
                return;
            }
            set_tags(file, path, tags, encode_done);
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

    if (!(rsvc_tags_addf(tags, wrapped_done, RSVC_ENCODER, "ripservice %s", RSVC_VERSION)
          && rsvc_tags_add(tags, wrapped_done, RSVC_MUSICBRAINZ_DISCID, discid)
          && rsvc_tags_addf(tags, wrapped_done, RSVC_TRACKNUMBER, "%zu", track_number)
          && rsvc_tags_addf(tags, wrapped_done, RSVC_TRACKTOTAL, "%zu", ntracks)
          && (*mcn ? rsvc_tags_add(tags, wrapped_done, RSVC_MCN, mcn) : true))) {
        return;
    }

    (void)rsvc_apply_musicbrainz_tags(tags, ^(rsvc_error_t error){
        // MusicBrainz tagging could fail for a number of reasons:
        // wasn't reachable; couldn't find album.  None of those is
        // reason to stop ripping, so ignore and proceed.
        (void)error;
    });
    wrapped_done(NULL);
}

static void set_tags(FILE* file, char* path, rsvc_tags_t source, rsvc_done_t done) {
    rsvc_format_t format;
    rsvc_tags_t tags;
    if (!rsvc_format_detect(path, file, &format, done)) {
        return;
    }
    if (!format->open_tags) {
        rsvc_errorf(done, __FILE__, __LINE__, "%s: can't tag %s file", path, format->name);
        return;
    }
    if (!format->open_tags(path, RSVC_TAG_RDWR, &tags, done)) {
        return;
    }
    done = ^(rsvc_error_t error){
        rsvc_tags_destroy(tags);
        done(error);
    };
    if (!(rsvc_tags_copy(tags, source, done)
          && rsvc_tags_save(tags, done))) {
        return;
    }
    done(NULL);
}
