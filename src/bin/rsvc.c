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
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#include <rsvc/core-audio.h>
#include <rsvc/disc.h>
#include <rsvc/flac.h>
#include <rsvc/id3.h>
#include <rsvc/lame.h>
#include <rsvc/mad.h>
#include <rsvc/mp4.h>
#include <rsvc/vorbis.h>

const char*                 rsvc_progname;
rsvc_option_callbacks_t     callbacks;
rsvc_command_t              command = NULL;
int                         rsvc_exit = EX_OK;
int                         rsvc_jobs = -1;

static int rsvc_jobs_default();
static bool bitrate_option(struct encode_options* encode, rsvc_option_value_t get_value,
                           rsvc_done_t fail);
static bool format_option(struct encode_options* encode, rsvc_option_value_t get_value,
                          rsvc_done_t fail);

static bool read_si_number(const char* in, int64_t* out);

static void rsvc_main(int argc, char* const* argv) {
    rsvc_jobs = rsvc_jobs_default();
#ifdef __APPLE__
    rsvc_core_audio_format_register();
#endif
    rsvc_flac_format_register();
    rsvc_id3_format_register();
    rsvc_lame_format_register();
    rsvc_mad_format_register();
    rsvc_mp4_format_register();
    rsvc_vorbis_format_register();

    rsvc_progname = strdup(basename(argv[0]));

    __block char* print_disk = NULL;
    __block struct rsvc_command print = {
        .name = "print",
        .argument = ^bool (char* arg, rsvc_done_t fail) {
            if (!print_disk) {
                print_disk = arg;
                return true;
            }
            return false;
        },
        .usage = ^{
            fprintf(stderr, "usage: %s print [DEVICE]\n", rsvc_progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_print(print_disk, done);
        },
    };

    __block struct rsvc_command ls = {
        .name = "ls",
        .usage = ^{
            fprintf(stderr, "usage: %s ls\n", rsvc_progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_ls(done);
        },
    };

    __block struct rsvc_command watch = {
        .name = "watch",
        .usage = ^{
            fprintf(stderr, "usage: %s watch\n", rsvc_progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_watch(done);
        },
    };

    __block char* eject_disk = NULL;
    __block struct rsvc_command eject = {
        .name = "eject",
        .argument = ^bool (char* arg, rsvc_done_t fail) {
            if (!eject_disk) {
                eject_disk = arg;
                return true;
            }
            return false;
        },
        .usage = ^{
            fprintf(stderr, "usage: %s eject [DEVICE]\n", rsvc_progname);
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_eject(eject_disk, done);
        },
    };

    __block struct rip_options rip_options = {
        .path_format  = strdup("%k"),
    };
    __block struct rsvc_command rip = {
        .name = "rip",
        .short_option = ^bool (int32_t opt, rsvc_option_value_t get_value, rsvc_done_t fail){
            switch (opt) {
              case 'b': return bitrate_option(&rip_options.encode, get_value, fail);
              case 'f': return format_option(&rip_options.encode, get_value, fail);
              case 'p': return rsvc_string_option(&rip_options.disk, get_value, fail);
              case 'e': return rsvc_boolean_option(&rip_options.eject);
              default:  return rsvc_illegal_short_option(opt, fail);
            }
        },
        .long_option = ^bool (char* opt, rsvc_option_value_t get_value, rsvc_done_t fail){
            return rsvc_long_option((rsvc_long_option_names){
                {"bitrate",  'b'},
                {"eject",    'e'},
                {"format",   'f'},
                {"path",     'p'},
                {NULL}
            }, callbacks.short_option, opt, get_value, fail);
        },
        .argument = ^bool (char* arg, rsvc_done_t fail) {
            if (!rip_options.disk) {
                rip_options.disk = arg;
                return true;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "too many arguments");
                return false;
            }
        },
        .usage = ^{
            fprintf(stderr,
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
            rsvc_formats_each(^(rsvc_format_t format, rsvc_stop_t stop){
                if (format->encode) {
                    fprintf(stderr, "  %s\n", format->name);
                }
            });
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_rip(&rip_options, done);
        },
    };

    __block struct convert_options convert_options = {
    };
    __block struct rsvc_command convert = {
        .name = "convert",
        .short_option = ^bool (int32_t opt, rsvc_option_value_t get_value, rsvc_done_t fail){
            switch (opt) {
              case 'b': return bitrate_option(&convert_options.encode, get_value, fail);
              case 'f': return format_option(&convert_options.encode, get_value, fail);
              case 'r': return rsvc_boolean_option(&convert_options.recursive);
              case 'u': return rsvc_boolean_option(&convert_options.update);
              default:  return rsvc_illegal_short_option(opt, fail);
            }
        },
        .long_option = ^bool (char* opt, rsvc_option_value_t get_value, rsvc_done_t fail){
            return rsvc_long_option((rsvc_long_option_names){
                {"bitrate",     'b'},
                {"format",      'f'},
                {"recursive",   'r'},
                {"update",      'u'},
                {NULL}
            }, callbacks.short_option, opt, get_value, fail);
        },
        .argument = ^bool (char* arg, rsvc_done_t fail) {
            if (!convert_options.input) {
                convert_options.input = arg;
            } else if (!convert_options.output) {
                convert_options.output = arg;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "too many arguments");
                return false;
            }
            return true;
        },
        .usage = ^{
            fprintf(stderr,
                    "usage: %s convert [OPTIONS] IN [OUT]\n"
                    "\n"
                    "Options:\n"
                    "  -b, --bitrate RATE      bitrate in SI format (default: 192k)\n"
                    "  -f, --format FMT        output format (default: flac or vorbis)\n"
                    "  -r, --recursive         convert folder recursively\n"
                    "  -u, --update            skip files that are newer than the source\n"
                    "\n"
                    "Formats:\n",
                    rsvc_progname);
            rsvc_formats_each(^(rsvc_format_t format, rsvc_stop_t stop){
                if (format->encode && format->decode) {
                    fprintf(stderr, "  %s (in, out)\n", format->name);
                } else if (format->encode) {
                    fprintf(stderr, "  %s (out)\n", format->name);
                } else if (format->decode) {
                    fprintf(stderr, "  %s (in)\n", format->name);
                }
            });
        },
        .run = ^(rsvc_done_t done){
            rsvc_command_convert(&convert_options, done);
        },
    };

    callbacks.short_option = ^bool (int32_t opt, rsvc_option_value_t get_value, rsvc_done_t fail){
        switch (opt) {
          case 'h':
            rsvc_usage(^(rsvc_error_t ignore){});
            exit(0);
          case 'j':
            return rsvc_integer_option(&rsvc_jobs, get_value, fail);
          case 'v':
            ++rsvc_verbosity;
            return true;
          case 'V':
            fprintf(stderr, "rsvc %s\n", RSVC_VERSION);
            exit(0);
          default:
            if (command && command->short_option) {
                return command->short_option(opt, get_value, fail);
            }
            break;
        }
        return rsvc_illegal_short_option(opt, fail);
    };

    callbacks.long_option = ^bool (char* opt, rsvc_option_value_t get_value, rsvc_done_t fail){
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
    };

    callbacks.argument = ^bool (char* arg, rsvc_done_t fail){
        if (command == NULL) {
            if (strcmp(arg, "print") == 0) {
                command = &print;
            } else if (strcmp(arg, "ls") == 0) {
                command = &ls;
            } else if (strcmp(arg, "watch") == 0) {
                command = &watch;
            } else if (strcmp(arg, "eject") == 0) {
                command = &eject;
            } else if (strcmp(arg, "rip") == 0) {
                command = &rip;
            } else if (strcmp(arg, "convert") == 0) {
                command = &convert;
            } else {
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
    };

    rsvc_done_t done = ^(rsvc_error_t error){
        if (error) {
            if (command) {
                fprintf(stderr, "%s %s: %s (%s:%d)\n",
                        rsvc_progname, command->name, error->message, error->file, error->lineno);
            } else {
                fprintf(stderr, "%s: %s (%s:%d)\n",
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
        fprintf(stderr,
                "usage: %s COMMAND [OPTIONS]\n"
                "\n"
                "Commands:\n"
                "  print [DEVICE]        print CD contents\n"
                "  ls                    list CDs\n"
                "  watch                 watch for CDs\n"
                "  eject [DEVICE]        eject CD\n"
                "  rip [DEVICE]          rip tracks to files\n"
                "  convert IN [OUT]      convert files\n"
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
    rsvc_disc_watch_callbacks_t callbacks;
    callbacks.appeared = ^(rsvc_disc_type_t type, const char* path){
        ++ndisks;
        if (!disk) {
            disk = strdup(path);
        }
    };
    callbacks.disappeared = ^(rsvc_disc_type_t type, const char* path){};
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

static bool bitrate_option(struct encode_options* encode, rsvc_option_value_t get_value,
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

static bool format_option(struct encode_options* encode, rsvc_option_value_t get_value,
                          rsvc_done_t fail) {
    char* value;
    if (!get_value(&value, fail)) {
        return false;
    }
    encode->format = rsvc_format_named(value, RSVC_FORMAT_ENCODE);
    if (!encode->format) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid format: %s", value);
        return false;
    }
    return true;
}

bool validate_encode_options(struct encode_options* encode, rsvc_done_t fail) {
    if (!encode->format) {
        if (encode->bitrate) {
            encode->format = rsvc_format_named("vorbis", RSVC_FORMAT_ENCODE);
        } else {
            encode->format = rsvc_format_named("flac", RSVC_FORMAT_ENCODE);
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
