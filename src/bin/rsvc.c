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

#define _POSIX_C_SOURCE 200809L

#include "rsvc.h"

#include <ctype.h>
#include <string.h>
#include <sys/param.h>
#include <sysexits.h>

#include <rsvc/audio.h>
#include <rsvc/disc.h>
#include <rsvc/format.h>
#include <rsvc/image.h>
#include "../rsvc/common.h"
#include "../rsvc/unix.h"

char  rsvc_progname[MAXPATHLEN];
int   rsvc_jobs = -1;

static int             rsvc_exit   = EX_OK;
static rsvc_command_t  command     = NULL;
static rsvc_command_t  commands[]  = {
    &rsvc_ls,
    &rsvc_watch,
    &rsvc_print,
    &rsvc_eject,
    &rsvc_rip,
    &rsvc_convert,
    NULL,
};

static int rsvc_jobs_default();
static bool read_si_number(const char* in, int64_t* out);

struct rsvc_option_callbacks callbacks = {
    .short_option = ^bool (int32_t opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        switch (opt) {
          case 'h':
            rsvc_usage(^(rsvc_error_t ignore){ (void)ignore; });
            exit(0);
          case 'j':
            return rsvc_integer_option(&rsvc_jobs, get_value, fail);
          case 'v':
            ++rsvc_verbosity;
            return true;
          case 'V':
            errf("rsvc %s\n", RSVC_VERSION);
            exit(0);
          default:
            if (command && command->short_option) {
                return command->short_option(opt, get_value, fail);
            }
            break;
        }
        return rsvc_illegal_short_option(opt, fail);
    },

    .long_option = ^bool (char* opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        if (strcmp(opt, "help") == 0) {
            return callbacks.short_option('h', get_value, fail);
        } else if (strcmp(opt, "jobs") == 0) {
            return callbacks.short_option('j', get_value, fail);
        } else if (strcmp(opt, "verbose") == 0) {
            return callbacks.short_option('v', get_value, fail);
        } else if (strcmp(opt, "version") == 0) {
            return callbacks.short_option('V', get_value, fail);
        } else if (command && command->long_option) {
            return command->long_option(opt, get_value, fail);
        } else {
            return rsvc_illegal_long_option(opt, fail);
        }
    },

    .argument = ^bool (char* arg, rsvc_done_t fail){
        if (!command) {
            for (int i = 0; commands[i]; ++i) {
                if (strcmp(arg, commands[i]->name) == 0) {
                    command = commands[i];
                    break;
                }
            }
            if (!command) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: illegal command", arg);
                return false;
            }
            return true;
        } else if (command->argument) {
            return command->argument(arg, fail);
        } else {
            rsvc_errorf(fail, __FILE__, __LINE__, "too many arguments");
            return false;
        }
    },
};

static void rsvc_main(int argc, char* const* argv) {
    rsvc_jobs = rsvc_jobs_default();
    rsvc_audio_formats_register();
    rsvc_image_formats_register();

    rsvc_basename(argv[0], rsvc_progname);

    rsvc_done_t done = ^(rsvc_error_t error){
        if (error) {
            if (command) {
                errf("%s %s: %s (%s:%d)\n",
                        rsvc_progname, command->name, error->message, error->file, error->lineno);
            } else {
                errf("%s: %s (%s:%d)\n",
                        rsvc_progname, error->message, error->file, error->lineno);
            }
            if (rsvc_exit == 0) {
                rsvc_exit = 1;
            }
        }
        exit(rsvc_exit);
    };

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if (!rsvc_options(argc, argv, &callbacks, done)) {
            // pass
        } else if (!command) {
            rsvc_usage(done);
        } else {
            command->run(done);
        }
    });

    dispatch_main();
}

void rsvc_usage(rsvc_done_t done) {
    rsvc_exit = EX_USAGE;
    if (command) {
        command->usage();
    } else {
        errf(
                "usage: %s COMMAND [OPTIONS]\n"
                "\n"
                "Commands:\n"
                "  print [DEVICE]        print CD contents\n"
                "  ls                    list CDs\n"
                "  watch                 watch for CDs\n"
                "  eject [DEVICE]        eject CD\n"
                "  rip [DEVICE]          rip tracks to files\n"
                "  convert IN [-o OUT]   convert files\n"
                "\n"
                "Options:\n"
                "  -h, --help            show this help page\n"
                "  -j, --jobs=N          allow N jobs at once (default: %d)\n"
                "  -v, --verbose         more verbose logging\n"
                "  -V, --version         show version and exit\n",
                rsvc_progname, rsvc_jobs_default());
    }
    done(NULL);
};

static int rsvc_jobs_default() {
    return 4;
}

void rsvc_default_disk(void (^done)(rsvc_error_t error, char* disk)) {
    __block int ndisks = 0;
    __block char* disk = NULL;
    struct rsvc_disc_watch_callbacks callbacks;
    callbacks.appeared = ^(enum rsvc_disc_type type, const char* path){
        (void)type;  // TODO(sfiera): filter discs.
        ++ndisks;
        if (!disk) {
            disk = strdup(path);
        }
    };
    callbacks.disappeared = ^(enum rsvc_disc_type type, const char* path){
        (void)type;
        (void)path;
    };
    callbacks.initialized = ^(rsvc_stop_t stop){
        stop();
        rsvc_done_t fail = ^(rsvc_error_t error){
            done(error, NULL);
        };
        if (ndisks > 1) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%d discs available", ndisks);
        } else if (ndisks == 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "no discs available");
        } else {
            done(NULL, disk);
        }
        if (disk) {
            free(disk);
        }
    };
    rsvc_disc_watch(callbacks);
}

bool bitrate_option(struct encode_options* encode, rsvc_option_value_f get_value,
                           rsvc_done_t fail) {
    char* value;
    if (!get_value(&value, fail)) {
        return false;
    }
    if (!(read_si_number(value, &encode->bitrate)
          && (encode->bitrate > 0))) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid bitrate: %s", value);
        return false;
    }
    return true;
}

bool format_option(struct encode_options* encode, rsvc_option_value_f get_value,
                          rsvc_done_t fail) {
    char* value;
    if (!get_value(&value, fail)) {
        return false;
    }
    encode->format = rsvc_format_named(value);
    if (!encode->format || !encode->format->encode) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid format: %s", value);
        return false;
    }
    return true;
}

bool path_option(char** string, rsvc_option_value_f get_value, rsvc_done_t fail) {
    return rsvc_string_option(string, get_value, fail)
        && rsvc_tags_validate_strf(*string, fail);
}

bool validate_encode_options(struct encode_options* encode, rsvc_done_t fail) {
    if (!encode->format) {
        if (encode->bitrate) {
            encode->format = &rsvc_vorbis;
        } else {
            encode->format = &rsvc_flac;
        }
    }

    if (encode->format->lossless) {
        if (encode->bitrate) {
            rsvc_errorf(fail, __FILE__, __LINE__,
                        "bitrate provided for lossless format %s", encode->format->name);
            return false;
        }
    } else {  // lossy
        if (!encode->bitrate) {
            encode->bitrate = 192e3;
        }
    }

    return true;
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
    signal(SIGPIPE, SIG_IGN);
    rsvc_main(argc, argv);
    return EX_OK;
}
