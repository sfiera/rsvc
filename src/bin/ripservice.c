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

#include <Block.h>
#include <ctype.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <rsvc/cd.h>
#include <rsvc/core-audio.h>
#include <rsvc/disc.h>
#include <rsvc/tag.h>
#include <rsvc/flac.h>
#include <rsvc/vorbis.h>

#include "../rsvc/progress.h"

typedef enum command {
    COMMAND_NONE = 0,
    COMMAND_PRINT,
    COMMAND_LS,
    COMMAND_WATCH,
    COMMAND_RIP,
} command_t;
const char* progname_suffix[] = {
    "",
    " print",
    " ls",
    " watch",
    " rip",
};

struct print_options {
    char* disk;
};
typedef struct print_options* print_options_t;
static void rsvc_command_print(print_options_t options,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done);

struct ls_options {
};
typedef struct ls_options* ls_options_t;
static void rsvc_command_ls(ls_options_t options,
                            void (^usage)(const char* message, ...),
                            rsvc_done_t done);

struct watch_options {
};
typedef struct watch_options* watch_options_t;
static void rsvc_command_watch(watch_options_t options,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done);

typedef enum rip_format {
    FORMAT_NONE = 0,
    FORMAT_FLAC,
    FORMAT_AAC,
    FORMAT_ALAC,
    FORMAT_VORBIS,
} rip_format_t;
struct rip_options {
    char* disk;

    rip_format_t format;
    bool has_bitrate;
    int64_t bitrate;
};
typedef struct rip_options* rip_options_t;
static void rsvc_command_rip(rip_options_t options,
                             void (^usage)(const char* message, ...),
                             rsvc_done_t done);

static void rip_all(rsvc_cd_t cd, rip_options_t options, rsvc_done_t done);
static bool read_format(const char* in, rip_format_t* out);
static bool read_si_number(const char* in, int64_t* out);

static void rsvc_usage(const char* progname, command_t command) {
    switch (command) {
      case COMMAND_NONE:
        fprintf(stderr,
                "usage: %s%s COMMAND [OPTIONS]\n"
                "\n"
                "Commands:\n"
                "  print DEVICE          print CD contents\n"
                "  ls                    list CDs\n"
                "  watch                 watch for CDs\n"
                "  rip DEVICE            rip tracks to files\n"
                "\n"
                "Options:\n"
                "  -V, --version         show version and exit\n",
                progname, progname_suffix[command]);
        break;

      case COMMAND_PRINT:
        fprintf(stderr, "usage: %s print DEVICE\n", progname);
        break;

      case COMMAND_LS:
        fprintf(stderr, "usage: %s ls\n", progname);
        break;

      case COMMAND_WATCH:
        fprintf(stderr, "usage: %s watch\n", progname);
        break;

      case COMMAND_RIP:
        fprintf(stderr,
                "usage: %s rip -f FORMAT [OPTIONS] DEVICE\n"
                "\n"
                "Options:\n"
                "  -f, --format FORMAT   choose format\n"
                "  -b, --bitrate KBPS    set bitrate for lossy codecs\n"
                "\n"
                "Formats:\n"
                "  flac, vorbis\n",
                progname);
        break;
    }
    exit(EX_USAGE);
}

static void rsvc_main(int argc, char* const* argv) {
    char* const progname = strdup(basename(argv[0]));

    __block rsvc_option_callbacks_t callbacks;
    __block command_t command = COMMAND_NONE;
    __block struct print_options print_options = {};
    __block struct ls_options ls_options = {};
    __block struct watch_options watch_options = {};
    __block struct rip_options rip_options = {};

    callbacks.short_option = ^bool (char opt, char* (^value)()){
        switch (command) {
          case COMMAND_PRINT:
          case COMMAND_LS:
          case COMMAND_WATCH:
            break;

          case COMMAND_RIP:
            switch (opt) {
              case 'b':
                if (!read_si_number(value(), &rip_options.bitrate)) {
                    callbacks.usage("invalid bitrate: %s", value());
                }
                rip_options.has_bitrate = true;
                return true;
              case 'f':
                if (!read_format(value(), &rip_options.format)) {
                    callbacks.usage("invalid format: %s", value());
                }
                return true;
              default:
                break;
            }
            break;

          case COMMAND_NONE:
            break;
        }
        switch (opt) {
          case 'V':
            fprintf(stderr, "ripservice %s\n", RSVC_VERSION);
            exit(0);
        }
        return false;
    };

    callbacks.long_option = ^bool (char* opt, char* (^value)()){
        switch (command) {
          case COMMAND_PRINT:
          case COMMAND_LS:
          case COMMAND_WATCH:
            break;
          case COMMAND_RIP:
            if (strcmp(opt, "bitrate") == 0) {
                return callbacks.short_option('b', value);
            } else if (strcmp(opt, "format") == 0) {
                return callbacks.short_option('f', value);
            }
            break;
          case COMMAND_NONE:
            break;
        }
        if (strcmp(opt, "version") == 0) {
            return callbacks.short_option('V', value);
        }
        return false;
    };

    callbacks.argument = ^bool (char* arg){
        switch (command) {
          case COMMAND_PRINT:
            if (!print_options.disk) {
                print_options.disk = arg;
                return true;
            }
            break;

          case COMMAND_LS:
          case COMMAND_WATCH:
            break;

          case COMMAND_RIP:
            if (!rip_options.disk) {
                rip_options.disk = arg;
                return true;
            }
            break;

          case COMMAND_NONE:
            {
                if (strcmp(arg, "print") == 0) {
                    command = COMMAND_PRINT;
                } else if (strcmp(arg, "ls") == 0) {
                    command = COMMAND_LS;
                } else if (strcmp(arg, "watch") == 0) {
                    command = COMMAND_WATCH;
                } else if (strcmp(arg, "rip") == 0) {
                    command = COMMAND_RIP;
                } else {
                    callbacks.usage("illegal command: %s", arg);
                }
            }
            return true;
        }
        return false;
    };

    callbacks.usage = ^(const char* message, ...){
        if (message) {
            fprintf(stderr, "%s%s: ", progname, progname_suffix[command]);
            va_list vl;
            va_start(vl, message);
            vfprintf(stderr, message, vl);
            va_end(vl);
            fprintf(stderr, "\n");
        }
        rsvc_usage(progname, command);
    };

    rsvc_options(argc, argv, &callbacks);

    rsvc_done_t done = ^(rsvc_error_t error){
        if (error) {
            fprintf(stderr, "%s%s: %s (%s:%d)\n",
                    progname, progname_suffix[command], error->message,
                    error->file, error->lineno);
            exit(1);
        } else {
            exit(EX_OK);
        }
    };

    switch (command) {
      case COMMAND_NONE:
        callbacks.usage(NULL);

      case COMMAND_PRINT:
        rsvc_command_print(&print_options, callbacks.usage, done);
        break;

      case COMMAND_LS:
        rsvc_command_ls(&ls_options, callbacks.usage, done);
        break;

      case COMMAND_WATCH:
        rsvc_command_watch(&watch_options, callbacks.usage, done);
        break;

      case COMMAND_RIP:
        rsvc_command_rip(&rip_options, callbacks.usage, done);
        break;
    }

    dispatch_main();
}

static void rsvc_command_print(print_options_t options,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done) {
    if (options->disk == NULL) {
        usage(NULL);
    }
    rsvc_cd_create(options->disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        const char* mcn = rsvc_cd_mcn(cd);
        if (*mcn) {
            printf("MCN: %s\n", mcn);
        }
        rsvc_cd_each_session(cd, ^(rsvc_cd_session_t session, rsvc_stop_t stop){
            printf("- Session: %ld\n", rsvc_cd_session_number(session));
            rsvc_cd_session_each_track(session, ^(rsvc_cd_track_t track, rsvc_stop_t stop){
                printf("  - Track: %ld\n", rsvc_cd_track_number(track));
                size_t sectors = rsvc_cd_track_nsectors(track);
                switch (rsvc_cd_track_type(track)) {
                    case RSVC_CD_TRACK_DATA: {
                        printf("    Type: data\n");
                        printf("    Sectors: %lu\n", sectors);
                    }
                    break;
                    case RSVC_CD_TRACK_AUDIO: {
                        printf("    Type: audio\n");
                        printf("    Channels: %ld\n", rsvc_cd_track_nchannels(track));
                        printf("    Duration: %lu:%02lu.%03lu\n",
                               sectors / (75 * 60),
                               (sectors / 75) % 60,
                               ((sectors % 75) * 1000) / 75);
                    }
                    break;
                }
                const char* isrc = rsvc_cd_track_isrc(track);
                if (*isrc) {
                    printf("    ISRC: %s\n", isrc);
                }
            });
        });

        rsvc_cd_destroy(cd);
        done(NULL);
    });
}

static void rsvc_command_ls(ls_options_t options,
                            void (^usage)(const char* message, ...),
                            rsvc_done_t done) {
    __block rsvc_stop_t stop = rsvc_disc_watch(
            ^(rsvc_disc_type_t type, const char* path){
                printf("%s\t%s\n", path, rsvc_disc_type_name[type]);
            },
            ^(rsvc_disc_type_t type, const char* path){ },
            ^{
                stop();
                done(NULL);
            });
}

static void rsvc_command_watch(watch_options_t options,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done) {
    __block bool show = false;
    rsvc_disc_watch(
            ^(rsvc_disc_type_t type, const char* path){
                if (show) {
                    printf("+\t%s\t%s\n", path, rsvc_disc_type_name[type]);
                }
            },
            ^(rsvc_disc_type_t type, const char* path){
                printf("-\t%s\t%s\n", path, rsvc_disc_type_name[type]);
            },
            ^{
                show = true;
            });
}

static void rsvc_command_rip(rip_options_t options,
                             void (^usage)(const char* message, ...),
                             rsvc_done_t done) {
    if (!options->disk) {
        usage(NULL);
    } else if (!options->format) {
        usage("must choose format");
    }
    switch (options->format) {
      case FORMAT_NONE:
        abort();
      case FORMAT_FLAC:
      case FORMAT_ALAC:
        if (options->has_bitrate) {
            usage("bitrate provided for lossless format");
        }
        break;
      case FORMAT_AAC:
        if (!options->has_bitrate) {
            usage("must choose bitrate");
        }
      case FORMAT_VORBIS:
        if (!options->has_bitrate) {
            usage("must choose bitrate");
        } else if (options->bitrate < 48000) {
            usage("bitrate too low: %lld", options->bitrate);
        } else if (options->bitrate > 480000) {
            usage("bitrate too high: %lld", options->bitrate);
        }
        break;
    }
    rsvc_cd_create(options->disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        rip_all(cd, options, ^(rsvc_error_t error){
            rsvc_cd_destroy(cd);
            done(error);
        });
    });
}

static void rip_all(rsvc_cd_t cd, rip_options_t options, rsvc_done_t done) {
    printf("Ripping…\n");
    rsvc_cd_session_t session = rsvc_cd_session(cd, 0);
    const size_t ntracks = rsvc_cd_session_ntracks(session);
    rsvc_progress_t progress = rsvc_progress_create();
    __block void (^rip_track)(size_t n);

    // Track the number of pending operations: 1 for each track that we
    // are encoding, plus 1 for ripping the whole disc.  This must only
    // be modified on the main queue; increment_pending() and
    // decrement_pending() guarantee this invariant, and perform
    // appropriate cleanup when it reaches 0.  Can't keep the blocks on
    // the stack.
    __block size_t pending = 1;
    void (^increment_pending)() = ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            ++pending;
        });
    };
    void (^decrement_pending)() = ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            if (--pending == 0) {
                printf("all rips done.\n");
                done(NULL);
                Block_release(rip_track);
                rsvc_progress_destroy(progress);
            }
        });
    };

    // Rip via a recursive block.  When each track has been ripped,
    // recursively calls itself to rip the next track (or to to
    // terminate when `n == ntracks`).  Can't keep the block on the
    // stack, since we exit this function immediately.
    rip_track = ^(size_t n){
        if (n == ntracks) {
            decrement_pending();
            return;
        }
        rsvc_cd_track_t track = rsvc_cd_session_track(session, n);
        size_t track_number = rsvc_cd_track_number(track);
        const char* mcn = rsvc_cd_mcn(cd);
        const char* isrc = rsvc_cd_track_isrc(track);

        // Skip data tracks.
        if (rsvc_cd_track_type(track) == RSVC_CD_TRACK_DATA) {
            printf("skipping track %ld/%ld\n", track_number, ntracks);
            rip_track(n + 1);
            return;
        }

        // Must increment pending before we start ripping.
        increment_pending();

        // Open up a pipe to run between the ripper and encoder, and a
        // file to receive the encoded content.
        int pipe_fd[2];
        if (pipe(pipe_fd) < 0) {
            rsvc_strerrorf(done, __FILE__, __LINE__, "pipe");
            return;
        }
        int write_pipe = pipe_fd[1];
        int read_pipe = pipe_fd[0];
        char filename[256];
        switch (options->format) {
          case FORMAT_NONE:
            abort();
          case FORMAT_FLAC:
            sprintf(filename, "%02ld.flac", track_number);
            break;
          case FORMAT_AAC:
            sprintf(filename, "%02ld.m4a", track_number);
            break;
          case FORMAT_ALAC:
            sprintf(filename, "%02ld.m4a", track_number);
            break;
          case FORMAT_VORBIS:
            sprintf(filename, "%02ld.ogv", track_number);
            break;
        }
        int file;
        if (!rsvc_open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644, &file, done)) {
            return;
        }

        // Rip the current track.  If that fails, bail.  If it succeeds,
        // start ripping the next track.
        rsvc_cd_track_rip(track, write_pipe, ^(rsvc_error_t error){
            if (error) {
                done(error);
                return;
            }
            close(write_pipe);
            rip_track(n + 1);
        });

        // Encode the current track.  If that fails, bail.  If it
        // succeeds, decrement the number of pending operations.
        rsvc_progress_node_t node = rsvc_progress_start(progress, filename);
        size_t nsamples = rsvc_cd_track_nsamples(track);
        rsvc_tags_t tags = rsvc_tags_create();

        rsvc_done_t encode_done = ^(rsvc_error_t error){
            if (error) {
                done(error);
                return;
            }
            close(read_pipe);
            rsvc_progress_done(node);
            rsvc_tags_destroy(tags);
            decrement_pending();
        };
        void (^progress)(double) = ^(double fraction){
            rsvc_progress_update(node, fraction);
        };

        const char* discid = rsvc_cd_session_discid(session);
        if (rsvc_tags_addf(tags, encode_done, RSVC_ENCODER, "ripservice %s", RSVC_VERSION)
                && rsvc_tags_add(tags, encode_done, RSVC_MUSICBRAINZ_DISCID, discid)
                && rsvc_tags_addf(tags, encode_done, RSVC_TRACKNUMBER, "%d", track_number)
                && rsvc_tags_addf(tags, encode_done, RSVC_TRACKTOTAL, "%d", ntracks)
                && (!*mcn || rsvc_tags_add(tags, encode_done, RSVC_MCN, mcn))
                && (!*isrc || rsvc_tags_add(tags, encode_done, RSVC_ISRC, isrc))) {
            switch (options->format) {
              case FORMAT_NONE:
                abort();
              case FORMAT_FLAC:
                rsvc_flac_encode(read_pipe, file, nsamples, tags, progress, encode_done);
                return;
              case FORMAT_AAC:
                rsvc_aac_encode(read_pipe, file, nsamples, tags, options->bitrate,
                                progress, encode_done);
                return;
              case FORMAT_ALAC:
                rsvc_alac_encode(read_pipe, file, nsamples, tags, progress, encode_done);
                return;
              case FORMAT_VORBIS:
                rsvc_vorbis_encode(read_pipe, file, nsamples, tags, options->bitrate,
                                   progress, encode_done);
                return;
            }
        }
    };
    rip_track = Block_copy(rip_track);
    rip_track(0);
}

static bool read_format(const char* in, rip_format_t* out) {
    if (strcmp(in, "flac") == 0) {
        *out = FORMAT_FLAC;
        return true;
    } else if (strcmp(in, "aac") == 0) {
        *out = FORMAT_AAC;
        return true;
    } else if (strcmp(in, "alac") == 0) {
        *out = FORMAT_ALAC;
        return true;
    } else if (strcmp(in, "vorbis") == 0) {
        *out = FORMAT_VORBIS;
        return true;
    }
    return false;
}

static bool multiply_safe(int64_t* value, int64_t by) {
    if ((INT64_MAX / by) < *value) {
        return false;
    }
    *value *= by;
    return true;
}

static bool read_si_number(const char* in, int64_t* out) {
    char* end;
    *out = strtol(in, &end, 10);
    if (end[0] == '\0') {
        return true;
    } else if (end[1] == '\0') {
        switch (tolower(end[0])) {
          case 'k': return multiply_safe(out, 1e3);
          case 'm': return multiply_safe(out, 1e6);
          case 'g': return multiply_safe(out, 1e9);
          case 't': return multiply_safe(out, 1e12);
          case 'p': return multiply_safe(out, 1e15);
          case 'e': return multiply_safe(out, 1e18);
        }
    } else if ((end[2] == '\0') && (end[1] == 'i')) {
        switch (tolower(end[0])) {
          case 'k': return multiply_safe(out, 1LL << 10);
          case 'm': return multiply_safe(out, 1LL << 20);
          case 'g': return multiply_safe(out, 1LL << 30);
          case 't': return multiply_safe(out, 1LL << 40);
          case 'p': return multiply_safe(out, 1LL << 50);
          case 'e': return multiply_safe(out, 1LL << 60);
        }
    }
    return false;
}

int main(int argc, char* const* argv) {
    rsvc_main(argc, argv);
    return EX_OK;
}
