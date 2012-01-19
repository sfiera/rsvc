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
#include <rsvc/comment.h>
#include <rsvc/flac.h>
#include <rsvc/vorbis.h>

typedef enum command {
    NONE,
    PRINT,
    RIP,
} command_t;

struct print_options {
    char* disk;
};
typedef struct print_options* print_options_t;
static void rsvc_command_print(print_options_t options,
                               void (^usage)(const char* message),
                               void (^check_error)(rsvc_error_t));

struct rip_options {
    char* disk;

    rsvc_comments_t comments;
};
typedef struct rip_options* rip_options_t;
static void rsvc_command_rip(rip_options_t options,
                             void (^usage)(const char* message),
                             void (^check_error)(rsvc_error_t));

static void rip_all(rsvc_cd_t cd, rip_options_t options, void (^done)(rsvc_error_t error));

static void rsvc_usage(const char* progname, command_t command) {
    switch (command) {
      case NONE:
        fprintf(stderr,
                "usage: %s COMMAND [OPTIONS]\n"
                "\n"
                "Commands:\n"
                "  print DEVICE          rip tracks to files\n"
                "  rip DEVICE            print CD contents\n",
                progname);
        break;

      case PRINT:
        fprintf(stderr, "usage: %s print DEVICE\n", progname);
        break;

      case RIP:
        fprintf(stderr,
                "usage: %s rip [OPTIONS] DEVICE\n"
                "\n"
                "Options:\n"
                "  -a, --artist ARTIST   add ARTIST comment\n"
                "  -A, --album ALBUM     add ALBUM comment\n"
                "  -g, --genre GENRE     add GENRE comment\n"
                "  -y, --year DATE       add DATE comment\n"
                "  -d, --discnumber DISC add DISCNUMBER comment\n"
                "  -D, --disctotal TOTAL add DISCTOTAL comment\n",
                progname);
        break;
    }
    exit(EX_USAGE);
}

static void rsvc_main(int argc, char* const* argv) {
    __block command_t command = NONE;
    __block struct print_options print_options = {};
    __block struct rip_options rip_options = {};

    const char* progname = basename(argv[0]);
    void (^usage)(const char* message) = ^(const char* message){
        if (message) {
            fprintf(stderr, "%s: %s\n", progname, message);
        }
        rsvc_usage(progname, command);
    };
    option_callbacks_t callbacks = {
        .short_option = ^bool (char opt, char* (^value)()){
            switch (command) {
              case PRINT:
                break;

              case RIP:
                switch (opt) {
                  case 'A':
                    rsvc_comments_add(rip_options.comments, RSVC_ALBUM, value());
                    return true;
                  case 'a':
                    rsvc_comments_add(rip_options.comments, RSVC_ARTIST, value());
                    return true;
                  case 'd':
                    rsvc_comments_add(rip_options.comments, RSVC_DISCNUMBER, value());
                    return true;
                  case 'D':
                    rsvc_comments_add(rip_options.comments, RSVC_DISCTOTAL, value());
                    return true;
                  case 'g':
                    rsvc_comments_add(rip_options.comments, RSVC_GENRE, value());
                    return true;
                  case 'y':
                    rsvc_comments_add(rip_options.comments, RSVC_DATE, value());
                    return true;
                  default:
                    break;
                }
                break;

              case NONE:
                break;
            }
            return false;
        },

        .long_option = ^bool (char* opt, char* (^value)()){
            switch (command) {
              case PRINT:
                break;
              case RIP:
                if (strcmp(opt, "album") == 0) {
                    rsvc_comments_add(rip_options.comments, RSVC_ALBUM, value());
                    return true;
                } else if (strcmp(opt, "artist") == 0) {
                    rsvc_comments_add(rip_options.comments, RSVC_ARTIST, value());
                    return true;
                } else if (strcmp(opt, "discnumber") == 0) {
                    rsvc_comments_add(rip_options.comments, RSVC_DISCNUMBER, value());
                    return true;
                } else if (strcmp(opt, "disctotal") == 0) {
                    rsvc_comments_add(rip_options.comments, RSVC_DISCTOTAL, value());
                    return true;
                } else if (strcmp(opt, "genre") == 0) {
                    rsvc_comments_add(rip_options.comments, RSVC_GENRE, value());
                    return true;
                } else if (strcmp(opt, "date") == 0) {
                    rsvc_comments_add(rip_options.comments, RSVC_DATE, value());
                    return true;
                }
                break;
              case NONE:
                break;
            }
            return false;
        },

        .argument = ^bool (char* arg){
            switch (command) {
              case PRINT:
                if (!print_options.disk) {
                    print_options.disk = arg;
                    return true;
                }
                break;

              case RIP:
                if (!rip_options.disk) {
                    rip_options.disk = arg;
                    return true;
                }
                break;

              case NONE:
                if (strcmp(arg, "print") == 0) {
                    command = PRINT;
                } else if (strcmp(arg, "rip") == 0) {
                    command = RIP;
                    rip_options.comments = rsvc_comments_create();
                } else {
                    char* message;
                    asprintf(&message, "illegal command: %s\n", arg);
                    usage(message);
                    free(message);
                }
                return true;
            }
            return false;
        },

        .usage = usage,
    };
    rsvc_options(argc, argv, &callbacks);

    void (^check_error)(rsvc_error_t) = ^(rsvc_error_t error){
        if (error) {
            fprintf(stderr, "%s: %s (%s:%d)\n",
                    progname, error->message, error->file, error->lineno);
            exit(1);
        }
    };

    switch (command) {
      case NONE:
        callbacks.usage(NULL);

      case PRINT:
        rsvc_command_print(&print_options, callbacks.usage, check_error);
        break;

      case RIP:
        rsvc_command_rip(&rip_options, callbacks.usage, check_error);
        break;
    }

    dispatch_main();
}

static void rsvc_command_print(print_options_t options,
                               void (^usage)(const char* message),
                               void (^check_error)(rsvc_error_t)) {
    if (options->disk == NULL) {
        usage(NULL);
    }
    rsvc_cd_create(options->disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        check_error(error);
        dispatch_async(dispatch_get_main_queue(), ^{
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
            exit(EX_OK);
        });
    });
}

static void rsvc_command_rip(rip_options_t options,
                             void (^usage)(const char* message),
                             void (^check_error)(rsvc_error_t)) {
    if (!options->disk) {
        usage(NULL);
    }
    rsvc_cd_create(options->disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        check_error(error);
        dispatch_async(dispatch_get_main_queue(), ^{
            rip_all(cd, options, ^(rsvc_error_t error){
                check_error(error);
                rsvc_comments_destroy(options->comments);
                rsvc_cd_destroy(cd);
                exit(EX_OK);
            });
        });
    });
}

static void rip_all(rsvc_cd_t cd, rip_options_t options, void (^done)(rsvc_error_t error)) {
    printf("Ripping…\n");
    const size_t ntracks = rsvc_cd_ntracks(cd);
    __block void (^rip_track)(size_t n);

    // Track the number of pending operations: 1 for each track that we
    // are encoding, plus 1 for ripping the whole disc.  This must only
    // be modified on the main queue; increment_pending() and
    // decrement_pending() guarantee this invariant, and perform
    // appropriate cleanup when it reaches 0.  Can't keep the blocks on
    // the stack.
    __block size_t pending = 1;
    __block void (^increment_pending)() = Block_copy(^{
        dispatch_async(dispatch_get_main_queue(), ^{
            ++pending;
        });
    });
    __block void (^decrement_pending)() = Block_copy(^{
        dispatch_async(dispatch_get_main_queue(), ^{
            if (--pending == 0) {
                printf("all rips done.\n");
                done(NULL);
                Block_release(rip_track);
                Block_release(increment_pending);
                Block_release(decrement_pending);
            }
        });
    });

    // Rip via a recursive block.  When each track has been ripped,
    // recursively calls itself to rip the next track (or to to
    // terminate when `n == ntracks`).  Can't keep the block on the
    // stack, since we exit this function immediately.
    rip_track = Block_copy(^(size_t n) {
        if (n == ntracks) {
            decrement_pending();
            return;
        }
        rsvc_cd_track_t track = rsvc_cd_track(cd, n);
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
            rsvc_strerror(done, __FILE__, __LINE__);
            return;
        }
        int write_pipe = pipe_fd[1];
        int read_pipe = pipe_fd[0];
        char filename[256];
        sprintf(filename, "%02ld.ogv", track_number);
        int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file < 0) {
            rsvc_strerror(done, __FILE__, __LINE__);
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
        size_t nsamples = rsvc_cd_track_nsamples(track);
        rsvc_comments_t comments = rsvc_comments_copy(options->comments);
        rsvc_comments_add(comments, RSVC_ENCODER, "ripservice " RSVC_VERSION);
        rsvc_comments_add(comments, RSVC_MUSICBRAINZ_DISCID, rsvc_cd_discid(cd));
        rsvc_comments_add_int(comments, RSVC_TRACKNUMBER, track_number);
        rsvc_comments_add_int(comments, RSVC_TRACKTOTAL, ntracks);
        if (*mcn) {
            rsvc_comments_add(comments, RSVC_MCN, mcn);
        }
        if (*isrc) {
            rsvc_comments_add(comments, RSVC_ISRC, isrc);
        }
        rsvc_vorbis_encode(read_pipe, file, nsamples, comments, ^(rsvc_error_t error){
            if (error) {
                done(error);
                return;
            }
            close(read_pipe);
            printf("track %ld/%ld done.\n", rsvc_cd_track_number(track), ntracks);
            decrement_pending();
        });
    });
    rip_track(0);
}

int main(int argc, char* const* argv) {
    rsvc_main(argc, argv);
    return EX_OK;
}
