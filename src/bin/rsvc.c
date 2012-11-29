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
#include <rsvc/musicbrainz.h>
#include <rsvc/tag.h>
#include <rsvc/vorbis.h>

#include "../rsvc/common.h"
#include "../rsvc/list.h"
#include "../rsvc/options.h"
#include "../rsvc/progress.h"
#include "../rsvc/unix.h"

typedef struct command* command_t;
struct command {
    const char* name;

    bool (^short_option)(char opt, char* (^value)());
    bool (^long_option)(char* opt, char* (^value)());
    bool (^argument)(char* arg);
    void (^usage)();

    void (^run)(rsvc_done_t done);
};

static void rsvc_command_print(char* disk,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done);

static void rsvc_command_ls(void (^usage)(const char* message, ...),
                            rsvc_done_t done);

static void rsvc_command_watch(void (^usage)(const char* message, ...),
                               rsvc_done_t done);

static void rsvc_command_rip(char* disk, rsvc_encode_format_t format,
                             bool has_bitrate, int64_t bitrate,
                             const char* format_path,
                             void (^usage)(const char* message, ...),
                             rsvc_done_t done);

static void rip_all(rsvc_cd_t cd,
                    rsvc_encode_format_t format, int64_t bitrate, const char* format_path,
                    rsvc_done_t done);
static void get_tags(rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     void (^done)(rsvc_error_t error, rsvc_tags_t tags));
static void set_tags(int fd, char* path, rsvc_tags_t source, rsvc_done_t done);
static bool read_si_number(const char* in, int64_t* out);

static void rsvc_main(int argc, char* const* argv) {
    rsvc_core_audio_format_register();
    rsvc_flac_format_register();
    rsvc_mp4_format_register();
    rsvc_vorbis_format_register();

    char* const progname = strdup(basename(argv[0]));

    __block rsvc_option_callbacks_t callbacks;
    __block command_t command = NULL;

    __block char* print_disk = NULL;
    __block struct command print = {
        .name = "print",
        .argument = ^bool (char* arg) {
            if (!print_disk) {
                print_disk = arg;
                return true;
            }
            return false;
        },
        .usage = ^{
            fprintf(stderr, "usage: %s print DEVICE\n", progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_print(print_disk, callbacks.usage, done);
        },
    };

    __block struct command ls = {
        .name = "ls",
        .usage = ^{
            fprintf(stderr, "usage: %s ls\n", progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_ls(callbacks.usage, done);
        },
    };

    __block struct command watch = {
        .name = "watch",
        .usage = ^{
            fprintf(stderr, "usage: %s watch\n", progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_watch(callbacks.usage, done);
        },
    };

    __block char* rip_disk                  = NULL;
    __block rsvc_encode_format_t format     = NULL;
    __block bool has_bitrate                = false;
    __block int64_t bitrate;
    __block char* format_path               = strdup("%k");
    __block struct command rip = {
        .name = "rip",
        .short_option = ^bool (char opt, char* (^value)()){
            switch (opt) {
              case 'f':
                if (format_path) {
                    free(format_path);
                }
                format_path = strdup(value());
                return true;
            }
            return false;
        },
        .long_option = ^bool (char* opt, char* (^value)()){
            if (strcmp(opt, "format-path") == 0) {
                return callbacks.short_option('f', value);
            }
            return false;
        },
        .argument = ^bool (char* arg) {
            if (!rip_disk) {
                rip_disk = arg;
                return true;
            } else if (!format) {
                format = rsvc_encode_format_named(arg);
                if (!format) {
                    callbacks.usage("invalid format: %s", arg);
                }
                return true;
            } else if (!has_bitrate) {
                if (!read_si_number(arg, &bitrate)) {
                    callbacks.usage("invalid bitrate: %s", arg);
                }
                has_bitrate = true;
                return true;
            }
            return false;
        },
        .usage = ^{
            fprintf(stderr,
                    "usage: %s rip [OPTIONS] DEVICE FORMAT [BITRATE]\n"
                    "\n"
                    "Options:\n"
                    "  -f, --format-path PATH  format string for output (default %%k)\n"
                    "\n"
                    "Formats:\n",
                    progname);
            rsvc_encode_formats_each(^(rsvc_encode_format_t format, rsvc_stop_t stop){
                fprintf(stderr, "  %s\n", format->name);
            });
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_rip(rip_disk, format, has_bitrate, bitrate, format_path,
                             callbacks.usage, done);
        },
    };

    callbacks.short_option = ^bool (char opt, char* (^value)()){
        switch (opt) {
          case 'V':
            fprintf(stderr, "rsvc %s\n", RSVC_VERSION);
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
        exit(EX_USAGE);
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

static void rsvc_command_print(char* disk,
                               void (^usage)(const char* message, ...),
                               rsvc_done_t done) {
    if (disk == NULL) {
        usage(NULL);
    }
    rsvc_cd_create(disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        const char* mcn = rsvc_cd_mcn(cd);
        if (*mcn) {
            printf("MCN: %s\n", mcn);
        }
        rsvc_cd_each_session(cd, ^(rsvc_cd_session_t session, rsvc_stop_t stop){
            printf("- Session: %zu\n", rsvc_cd_session_number(session));
            rsvc_cd_session_each_track(session, ^(rsvc_cd_track_t track, rsvc_stop_t stop){
                printf("  - Track: %zu\n", rsvc_cd_track_number(track));
                size_t sectors = rsvc_cd_track_nsectors(track);
                switch (rsvc_cd_track_type(track)) {
                    case RSVC_CD_TRACK_DATA: {
                        printf("    Type: data\n");
                        printf("    Sectors: %zu\n", sectors);
                    }
                    break;
                    case RSVC_CD_TRACK_AUDIO: {
                        printf("    Type: audio\n");
                        printf("    Channels: %zu\n", rsvc_cd_track_nchannels(track));
                        printf("    Duration: %zu:%02zu.%03zu\n",
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

static void rsvc_command_ls(void (^usage)(const char* message, ...),
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

static void rsvc_command_watch(void (^usage)(const char* message, ...),
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

static void rsvc_command_rip(char* disk, rsvc_encode_format_t format,
                             bool has_bitrate, int64_t bitrate,
                             const char* format_path,
                             void (^usage)(const char* message, ...),
                             rsvc_done_t done) {
    if (!disk) {
        usage(NULL);
    } else if (!format) {
        usage("must choose format");
    } else if (has_bitrate && format->lossless) {
        usage("bitrate provided for lossless format %s", format->name);
    } else if (!has_bitrate && !format->lossless) {
        usage("bitrate not provided for lossy format %s", format->name);
    }
    rsvc_cd_create(disk, ^(rsvc_cd_t cd, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        rip_all(cd, format, bitrate, format_path, ^(rsvc_error_t error){
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

typedef struct rsvc_group_cancel_node* rsvc_group_cancel_node_t;
struct rsvc_group_cancel_node {
    void (^cancel)();
    rsvc_group_cancel_node_t prev, next;
};

typedef struct rsvc_group_cancel_list* rsvc_group_cancel_list_t;
struct rsvc_group_cancel_list {
    rsvc_group_cancel_node_t head, tail;
};

typedef struct rsvc_group* rsvc_group_t;
struct rsvc_group {
    dispatch_queue_t queue;
    size_t pending;
    rsvc_error_t final_error;
    rsvc_done_t done;
    struct rsvc_group_cancel_list on_cancel_list;
};

rsvc_group_t rsvc_group_create(dispatch_queue_t queue, rsvc_done_t done) {
    struct rsvc_group initializer = {queue, 1};
    rsvc_group_t group = memdup(&initializer, sizeof(initializer));
    rsvc_done_t group_done = ^(rsvc_error_t error){
        done(group->final_error);
        rsvc_error_destroy(group->final_error);
        Block_release(group->done);
    };
    group->done = Block_copy(group_done);
    return group;
}

static void finalize(rsvc_group_t group) {
    RSVC_LIST_CLEAR(&group->on_cancel_list, ^(rsvc_group_cancel_node_t node){
        Block_release(node->cancel);
    });
    group->done(group->final_error);
    Block_release(group->done);
    rsvc_error_destroy(group->final_error);
    free(group);
}

static void adopt_error(rsvc_group_t group, rsvc_error_t error) {
    if (group->final_error) {
        rsvc_error_destroy(error);
    } else {
        group->final_error = error;
        RSVC_LIST_CLEAR(&group->on_cancel_list, ^(rsvc_group_cancel_node_t node){
            node->cancel();
            Block_release(node->cancel);
        });
    }
}

rsvc_done_t rsvc_group_add(rsvc_group_t group) {
    dispatch_sync(group->queue, ^{
        ++group->pending;
    });
    __block rsvc_done_t done = ^(rsvc_error_t error){
        error = rsvc_error_clone(error);
        dispatch_async(group->queue, ^{
            if (error) {
                adopt_error(group, error);
            }
            if (--group->pending == 0) {
                finalize(group);
            }
            Block_release(done);
        });
    };
    done = Block_copy(done);
    return done;
}

void rsvc_group_ready(rsvc_group_t group) {
    dispatch_async(group->queue, ^{
        if (--group->pending == 0) {
            finalize(group);
        }
    });
}

void rsvc_group_on_cancel(rsvc_group_t group, rsvc_stop_t stop) {
    dispatch_async(group->queue, ^{
        if (group->final_error) {
            stop();
        } else {
            struct rsvc_group_cancel_node node = {Block_copy(stop)};
            RSVC_LIST_PUSH(&group->on_cancel_list, memdup(&node, sizeof(node)));
        }
    });
}

void rsvc_group_cancel(rsvc_group_t group, rsvc_error_t error) {
    error = rsvc_error_clone(error);
    dispatch_async(group->queue, ^{
        adopt_error(group, error);
    });
}

static void rip_track(size_t n, size_t ntracks, rsvc_group_t group,
                      rsvc_encode_format_t format, int64_t bitrate, const char* format_path,
                      rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_progress_t progress) {
    if (n == ntracks) {
        rsvc_group_ready(group);
        return;
    }

    rsvc_done_t rip_done = rsvc_group_add(group);
    rsvc_done_t encode_done = rsvc_group_add(group);
    rsvc_done_t cancel_rip = ^(rsvc_error_t error){
        if (error) {
            rip_done(error);
            encode_done(error);
            rsvc_group_ready(group);
        }
    };

    rsvc_cd_track_t track = rsvc_cd_session_track(session, n);
    size_t track_number = rsvc_cd_track_number(track);

    // Skip data tracks.
    if (rsvc_cd_track_type(track) == RSVC_CD_TRACK_DATA) {
        printf("skipping track %zu/%zu\n", track_number, ntracks);
        rip_track(n + 1, ntracks, group, format, bitrate, format_path, cd, session, progress);
        return;
    }

    // Open up a pipe to run between the ripper and encoder, and a
    // file to receive the encoded content.  If either of these
    // fails, then stop encoding without proceeding on to the next
    // track.
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        rsvc_strerrorf(cancel_rip, __FILE__, __LINE__, "pipe");
        return;
    }
    int write_pipe = pipe_fd[1];
    int read_pipe = pipe_fd[0];

    get_tags(cd, session, track, ^(rsvc_error_t error, rsvc_tags_t tags){
        if (error) {
            cancel_rip(error);
            return;
        }

        rsvc_tags_strf(tags, format_path, format->extension,
                       ^(rsvc_error_t error, char* path){
            if (error) {
                rsvc_tags_destroy(tags);
                cancel_rip(error);
                return;
            }

            __block bool makedirs_succeeded;
            rsvc_dirname(path, ^(const char* parent){
                makedirs_succeeded = rsvc_makedirs(parent, 0755, cancel_rip);
            });
            int file;
            if (!makedirs_succeeded
                || !rsvc_open(path, O_RDWR | O_CREAT | O_EXCL, 0644, &file, cancel_rip)) {
                rsvc_tags_destroy(tags);
                return;
            }

            // Rip the current track.  If that fails, bail.  If it succeeds,
            // start ripping the next track.
            rsvc_stop_t stop_rip = rsvc_cd_track_rip(track, write_pipe, ^(rsvc_error_t error){
                close(write_pipe);
                rip_done(error);
                if (error) {
                    rip_track(ntracks, ntracks, group, format, bitrate, format_path, cd, session, progress);
                } else {
                    rip_track(n + 1, ntracks, group, format, bitrate, format_path, cd, session, progress);
                }
            });
            rsvc_group_on_cancel(group, stop_rip);

            // Encode the current track.  If that fails, bail.  If it
            // succeeds, decrement the number of pending operations.
            rsvc_progress_node_t node = rsvc_progress_start(progress, path);
            size_t nsamples = rsvc_cd_track_nsamples(track);
            void (^progress)(double) = ^(double fraction){
                rsvc_progress_update(node, fraction);
            };

            struct rsvc_encode_options encode_options = {
                .bitrate                = bitrate,
                .samples_per_channel    = nsamples,
                .progress               = progress,
            };
            char* path_copy = strdup(path);
            format->encode(read_pipe, file, &encode_options, ^(rsvc_error_t error){
                close(read_pipe);
                if (error) {
                    free(path_copy);
                    rsvc_tags_destroy(tags);
                    encode_done(error);
                    return;
                }
                set_tags(file, path_copy, tags, ^(rsvc_error_t error){
                    free(path_copy);
                    rsvc_tags_destroy(tags);
                    close(file);
                    rsvc_progress_done(node);
                    encode_done(error);
                });
            });
        });
    });
}

static void rip_all(rsvc_cd_t cd,
                    rsvc_encode_format_t format, int64_t bitrate, const char* format_path,
                    rsvc_done_t done) {
    printf("Ripping…\n");
    rsvc_cd_session_t session = rsvc_cd_session(cd, 0);
    const size_t ntracks = rsvc_cd_session_ntracks(session);
    rsvc_progress_t progress = rsvc_progress_create();

    rsvc_group_t group = rsvc_group_create(dispatch_get_main_queue(), ^(rsvc_error_t error){
        rsvc_progress_destroy(progress);
        if (!error) {
            printf("all rips done.\n");
        }
        done(error);
    });
    set_sigint_callback(^{
        rsvc_group_cancel(group, NULL);
    });

    rip_track(0, ntracks, group, format, bitrate, format_path, cd, session, progress);
}

static void get_tags(rsvc_cd_t cd, rsvc_cd_session_t session, rsvc_cd_track_t track,
                     void (^done)(rsvc_error_t error, rsvc_tags_t tags)) {
    const char* discid      = rsvc_cd_session_discid(session);
    size_t track_number     = rsvc_cd_track_number(track);
    const size_t ntracks    = rsvc_cd_session_ntracks(session);
    const char* mcn         = rsvc_cd_mcn(cd);
    const char* isrc        = rsvc_cd_track_isrc(track);

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
            || (*mcn && !rsvc_tags_add(tags, wrapped_done, RSVC_MCN, mcn))
            || (*isrc && !rsvc_tags_add(tags, wrapped_done, RSVC_ISRC, isrc))) {
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
    rsvc_tag_format_t tag_format;
    if (!rsvc_tag_format_detect(fd, &tag_format, done)) {
        return;
    }
    tag_format->open_tags(path, RSVC_TAG_RDWR, ^(rsvc_tags_t tags, rsvc_error_t error){
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
