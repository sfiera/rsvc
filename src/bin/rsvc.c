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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <rsvc/cd.h>
#include <rsvc/core-audio.h>
#include <rsvc/disc.h>
#include <rsvc/flac.h>
#include <rsvc/mp4.h>
#include <rsvc/tag.h>
#include <rsvc/vorbis.h>

#include "../rsvc/options.h"
#include "../rsvc/posix.h"
#include "../rsvc/progress.h"

typedef struct command* command_t;
struct command {
    const char* name;

    bool (^short_option)(char opt, char* (^value)());
    bool (^long_option)(char* opt, char* (^value)());
    bool (^argument)(char* arg);
    void (^usage)();

    void (^run)(rsvc_done_t done);
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

struct rip_options {
    char*                   disk;

    rsvc_encode_format_t    format;
    bool                    has_bitrate;
    int64_t                 bitrate;
};
typedef struct rip_options* rip_options_t;
static void rsvc_command_rip(rip_options_t options,
                             void (^usage)(const char* message, ...),
                             rsvc_done_t done);

static void rip_all(rsvc_cd_t cd, rip_options_t options, rsvc_done_t done);
static void set_tags(int fd, char* path,
                     rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     rsvc_done_t done);
static bool read_si_number(const char* in, int64_t* out);

static void rsvc_main(int argc, char* const* argv) {
    rsvc_core_audio_format_register();
    rsvc_flac_format_register();
    rsvc_mp4_format_register();
    rsvc_vorbis_format_register();

    char* const progname = strdup(basename(argv[0]));

    __block rsvc_option_callbacks_t callbacks;
    __block command_t command = NULL;
    __block struct print_options print_options = {};
    __block struct ls_options ls_options = {};
    __block struct watch_options watch_options = {};
    __block struct rip_options rip_options = {};

    __block struct command print = {
        .name = "print",
        .argument = ^bool (char* arg) {
            if (!print_options.disk) {
                print_options.disk = arg;
                return true;
            }
            return false;
        },
        .usage = ^{
            fprintf(stderr, "usage: %s print DEVICE\n", progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_print(&print_options, callbacks.usage, done);
        },
    };

    __block struct command ls = {
        .name = "ls",
        .usage = ^{
            fprintf(stderr, "usage: %s ls\n", progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_ls(&ls_options, callbacks.usage, done);
        },
    };

    __block struct command watch = {
        .name = "watch",
        .usage = ^{
            fprintf(stderr, "usage: %s watch\n", progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_watch(&watch_options, callbacks.usage, done);
        },
    };

    __block struct command rip = {
        .name = "rip",
        .short_option = ^bool (char opt, char* (^value)()){
            switch (opt) {
              case 'b':
                if (!read_si_number(value(), &rip_options.bitrate)) {
                    callbacks.usage("invalid bitrate: %s", value());
                }
                rip_options.has_bitrate = true;
                return true;
              case 'f':
                rip_options.format = rsvc_encode_format_named(value());
                if (!rip_options.format) {
                    callbacks.usage("invalid format: %s", value());
                }
                return true;
              default:
                return false;
            }
        },
        .long_option = ^bool (char* opt, char* (^value)()){
            if (strcmp(opt, "bitrate") == 0) {
                return callbacks.short_option('b', value);
            } else if (strcmp(opt, "format") == 0) {
                return callbacks.short_option('f', value);
            }
            return false;
        },
        .argument = ^bool (char* arg) {
            if (!rip_options.disk) {
                rip_options.disk = arg;
                return true;
            }
            return false;
        },
        .usage = ^{
            fprintf(stderr,
                    "usage: %s rip -f FORMAT [OPTIONS] DEVICE\n"
                    "\n"
                    "Options:\n"
                    "  -f, --format FORMAT   choose format\n"
                    "  -b, --bitrate KBPS    set bitrate for lossy codecs\n"
                    "\n"
                    "Formats:\n",
                    progname);
            rsvc_encode_formats_each(^(rsvc_encode_format_t format, rsvc_stop_t stop){
                fprintf(stderr, "  %s\n", format->name);
            });
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_rip(&rip_options, callbacks.usage, done);
        },
    };

    callbacks.short_option = ^bool (char opt, char* (^value)()){
        switch (opt) {
          case 'V':
            fprintf(stderr, "ripservice %s\n", RSVC_VERSION);
            exit(0);
          default:
            if (command && command->short_option) {
                return command->short_option(opt, value);
            }
            return false;
        }
    };

    callbacks.long_option = ^bool (char* opt, char* (^value)()){
        if (strcmp(opt, "version") == 0) {
            return callbacks.short_option('V', value);
        } else if (command && command->long_option) {
            return command->long_option(opt, value);
        }
        return false;
    };

    callbacks.argument = ^bool (char* arg){
        if (command == NULL) {
            if (strcmp(arg, "print") == 0) {
                command = &print;
            } else if (strcmp(arg, "ls") == 0) {
                command = &ls;
            } else if (strcmp(arg, "watch") == 0) {
                command = &watch;
            } else if (strcmp(arg, "rip") == 0) {
                command = &rip;
            } else {
                callbacks.usage("illegal command: %s", arg);
            }
            return true;
        } else if (command->argument) {
            return command->argument(arg);
        } else {
            return false;
        }
    };

    callbacks.usage = ^(const char* message, ...){
        if (message) {
            if (command) {
                fprintf(stderr, "%s %s: ", progname, command->name);
            } else {
                fprintf(stderr, "%s: ", progname);
            }
            va_list vl;
            va_start(vl, message);
            vfprintf(stderr, message, vl);
            va_end(vl);
            fprintf(stderr, "\n");
        }
        if (command) {
            command->usage();
        } else {
            fprintf(stderr,
                    "usage: %s COMMAND [OPTIONS]\n"
                    "\n"
                    "Commands:\n"
                    "  print DEVICE          print CD contents\n"
                    "  ls                    list CDs\n"
                    "  watch                 watch for CDs\n"
                    "  rip DEVICE            rip tracks to files\n"
                    "\n"
                    "Options:\n"
                    "  -V, --version         show version and exit\n",
                    progname);
        }
    };

    rsvc_options(argc, argv, &callbacks);

    rsvc_done_t done = ^(rsvc_error_t error){
        if (error) {
            if (command) {
                fprintf(stderr, "%s %s: %s (%s:%d)\n",
                        progname, command->name, error->message, error->file, error->lineno);
            } else {
                fprintf(stderr, "%s: %s (%s:%d)\n",
                        progname, error->message, error->file, error->lineno);
            }
            exit(1);
        } else {
            exit(EX_OK);
        }
    };

    if (command) {
        command->run(done);
    } else {
        callbacks.usage(NULL);
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
    __block rsvc_stop_t stop;

    rsvc_disc_watch_callbacks_t callbacks;
    callbacks.appeared = ^(rsvc_disc_type_t type, const char* path){
        printf("%s\t%s\n", path, rsvc_disc_type_name[type]);
    };
    callbacks.disappeared = ^(rsvc_disc_type_t type, const char* path){};
    callbacks.initialized = ^{
        stop();
        done(NULL);
    };

    stop = rsvc_disc_watch(callbacks);
}

static void rsvc_command_watch(watch_options_t options,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done) {
    __block bool show = false;

    rsvc_disc_watch_callbacks_t callbacks;
    callbacks.appeared = ^(rsvc_disc_type_t type, const char* path){
        if (show) {
            printf("+\t%s\t%s\n", path, rsvc_disc_type_name[type]);
        }
    };
    callbacks.disappeared = ^(rsvc_disc_type_t type, const char* path){
        printf("-\t%s\t%s\n", path, rsvc_disc_type_name[type]);
    };
    callbacks.initialized = ^{
        show = true;
    };

    rsvc_disc_watch(callbacks);
}

static void rsvc_command_rip(rip_options_t options,
                             void (^usage)(const char* message, ...),
                             rsvc_done_t done) {
    if (!options->disk) {
        usage(NULL);
    } else if (!options->format) {
        usage("must choose format");
    } else if (options->has_bitrate && options->format->lossless) {
        usage("bitrate provided for lossless format %s", options->format->name);
    } else if (!options->has_bitrate && !options->format->lossless) {
        usage("bitrate not provided for lossy format %s", options->format->name);
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

static dispatch_source_t    sigint_source       = NULL;
static bool                 sigint_received     = false;
static rsvc_stop_t          sigint_callback     = NULL;

static void set_sigint_callback(rsvc_stop_t stop) {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (sigint_received) {
            stop();
        } else {
            if (sigint_callback) {
                Block_release(sigint_callback);
            } else if (stop) {
                if (!sigint_source) {
                    sigint_source = dispatch_source_create(
                        DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
                    dispatch_source_set_event_handler(sigint_source, ^{
                        sigint_received = true;
                        if (sigint_callback) {
                            sigint_callback();
                        }
                        Block_release(sigint_callback);
                        sigint_callback = NULL;
                    });
                }
                dispatch_resume(sigint_source);
                signal(SIGINT, SIG_IGN);
            }

            if (stop) {
                sigint_callback = Block_copy(stop);
            } else {
                sigint_callback = NULL;
                signal(SIGINT, SIG_DFL);
                dispatch_source_cancel(sigint_source);
            }
        }
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
    // appropriate cleanup when it reaches 0.  We also track the error
    // that should be returned when done.
    __block size_t pending = 1;
    __block rsvc_error_t final_error = NULL;
    void (^increment_pending)() = ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            ++pending;
        });
    };
    rsvc_done_t decrement_pending = ^(rsvc_error_t error){
        error = rsvc_error_clone(error);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (error) {
                if (final_error) {
                    rsvc_error_destroy(error);
                } else {
                    final_error = error;
                }
            }
            if (--pending == 0) {
                set_sigint_callback(NULL);
                if (!final_error) {
                    printf("all rips done.\n");
                }
                done(final_error);
                rsvc_error_destroy(final_error);
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
            decrement_pending(NULL);
            return;
        }
        rsvc_cd_track_t track = rsvc_cd_session_track(session, n);
        size_t track_number = rsvc_cd_track_number(track);

        // Skip data tracks.
        if (rsvc_cd_track_type(track) == RSVC_CD_TRACK_DATA) {
            printf("skipping track %ld/%ld\n", track_number, ntracks);
            rip_track(n + 1);
            return;
        }

        // Open up a pipe to run between the ripper and encoder, and a
        // file to receive the encoded content.  If either of these
        // fails, then stop encoding without proceeding on to the next
        // track.
        int pipe_fd[2];
        if (pipe(pipe_fd) < 0) {
            rsvc_strerrorf(decrement_pending, __FILE__, __LINE__, "pipe");
            decrement_pending(NULL);
            return;
        }
        int write_pipe = pipe_fd[1];
        int read_pipe = pipe_fd[0];

        int file;
        char* path = malloc(256);
        sprintf(path, "%02ld.%s", track_number, options->format->extension);
        if (!rsvc_open(path, O_RDWR | O_CREAT | O_TRUNC, 0644, &file, decrement_pending)) {
            decrement_pending(NULL);
            return;
        }

        // Rip the current track.  If that fails, bail.  If it succeeds,
        // start ripping the next track.
        increment_pending();
        rsvc_stop_t stop_rip = rsvc_cd_track_rip(track, write_pipe, ^(rsvc_error_t error){
            close(write_pipe);
            decrement_pending(error);
            if (error) {
                decrement_pending(NULL);
            } else {
                rip_track(n + 1);
            }
        });
        set_sigint_callback(stop_rip);
        Block_release(stop_rip);

        // Encode the current track.  If that fails, bail.  If it
        // succeeds, decrement the number of pending operations.
        rsvc_progress_node_t node = rsvc_progress_start(progress, path);
        size_t nsamples = rsvc_cd_track_nsamples(track);
        void (^progress)(double) = ^(double fraction){
            rsvc_progress_update(node, fraction);
        };

        increment_pending();
        struct rsvc_encode_options encode_options = {
            .bitrate                = options->bitrate,
            .samples_per_channel    = nsamples,
            .progress               = progress,
        };
        options->format->encode(read_pipe, file, &encode_options, ^(rsvc_error_t error){
            close(read_pipe);
            if (error) {
                decrement_pending(error);
                free(path);
                return;
            }
            set_tags(file, path, cd, session, track, ^(rsvc_error_t error){
                close(file);
                free(path);
                rsvc_progress_done(node);
                decrement_pending(error);
            });
        });
    };
    rip_track = Block_copy(rip_track);
    rip_track(0);
}

static void set_tags(int fd, char* path,
                     rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     rsvc_done_t done) {
    const char* discid      = rsvc_cd_session_discid(session);
    size_t track_number     = rsvc_cd_track_number(track);
    const size_t ntracks    = rsvc_cd_session_ntracks(session);
    const char* mcn         = rsvc_cd_mcn(cd);
    const char* isrc        = rsvc_cd_track_isrc(track);

    rsvc_tag_format_t format;
    if (!rsvc_tag_format_detect(fd, &format, done)) {
        return;
    }
    format->open_tags(path, RSVC_TAG_RDWR, ^(rsvc_tags_t tags, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        if (!rsvc_tags_addf(tags, done, RSVC_ENCODER, "ripservice %s", RSVC_VERSION)
                || !rsvc_tags_add(tags, done, RSVC_MUSICBRAINZ_DISCID, discid)
                || !rsvc_tags_addf(tags, done, RSVC_TRACKNUMBER, "%ld", track_number)
                || !rsvc_tags_addf(tags, done, RSVC_TRACKTOTAL, "%ld", ntracks)
                || (*mcn && !rsvc_tags_add(tags, done, RSVC_MCN, mcn))
                || (*isrc && !rsvc_tags_add(tags, done, RSVC_ISRC, isrc))) {
            rsvc_tags_destroy(tags);
            return;
        }
        rsvc_tags_save(tags, ^(rsvc_error_t error){
            rsvc_tags_destroy(tags);
            done(error);
        });
    });
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
