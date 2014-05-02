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
#include <rsvc/flac.h>
#include <rsvc/mp4.h>
#include <rsvc/vorbis.h>

const char*                 rsvc_progname;
rsvc_option_callbacks_t     callbacks;
rsvc_command_t              command = NULL;

static bool bitrate_option(struct encode_options* encode, rsvc_option_value_t get_value,
                           rsvc_done_t fail);
static bool format_option(struct encode_options* encode, rsvc_option_value_t get_value,
                          rsvc_done_t fail);

static bool read_si_number(const char* in, int64_t* out);

static void rsvc_main(int argc, char* const* argv) {
#ifdef __APPLE__
    rsvc_core_audio_format_register();
#endif
    rsvc_flac_format_register();
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
            fprintf(stderr, "usage: %s print DEVICE\n", rsvc_progname);
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
            fprintf(stderr, "usage: %s eject DEVICE\n", rsvc_progname);
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
                    "usage: %s rip [OPTIONS] DEVICE\n"
                    "\n"
                    "Options:\n"
                    "  -b, --bitrate RATE      bitrate in SI format (default: 192k)\n"
                    "  -e, --eject             eject CD after ripping\n"
                    "  -f, --format FMT        output format (default: flac or vorbis)\n"
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
              default:  return rsvc_illegal_short_option(opt, fail);
            }
        },
        .long_option = ^bool (char* opt, rsvc_option_value_t get_value, rsvc_done_t fail){
            return rsvc_long_option((rsvc_long_option_names){
                {"bitrate",  'b'},
                {"format",   'f'},
                {NULL}
            }, callbacks.short_option, opt, get_value, fail);
        },
        .argument = ^bool (char* arg, rsvc_done_t fail) {
            if (!convert_options.file) {
                convert_options.file = arg;
                return true;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "too many arguments");
                return false;
            }
        },
        .usage = ^{
            fprintf(stderr,
                    "usage: %s convert [OPTIONS] DEVICE\n"
                    "\n"
                    "Options:\n"
                    "  -b, --bitrate RATE      bitrate in SI format (default: 192k)\n"
                    "  -f, --format FMT        output format (default: flac or vorbis)\n"
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

    callbacks.short_option = ^bool (int32_t opt, rsvc_option_value_t get_value,
                                    rsvc_done_t fail){
        if (command) {
            if (command->short_option) {
                return command->short_option(opt, get_value, fail);
            } else {
                return rsvc_illegal_short_option(opt, fail);
            }
        } else {
            switch (opt) {
              case 'v':
                ++rsvc_verbosity;
                return true;
              case 'V':
                fprintf(stderr, "rsvc %s\n", RSVC_VERSION);
                exit(0);
              default:
                return rsvc_illegal_short_option(opt, fail);
            }
        }
    };

    callbacks.long_option = ^bool (char* opt, rsvc_option_value_t get_value,
                                   rsvc_done_t fail){
        if (command) {
            if (command && command->long_option) {
                return command->long_option(opt, get_value, fail);
            } else {
                return rsvc_illegal_long_option(opt, fail);
            }
        } else {
            if (strcmp(opt, "verbose") == 0) {
                return callbacks.short_option('v', get_value, fail);
            } else if (strcmp(opt, "version") == 0) {
                return callbacks.short_option('V', get_value, fail);
            } else {
                return rsvc_illegal_long_option(opt, fail);
            }
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
            exit(1);
        } else {
            exit(EX_OK);
        }
    };

    if (!rsvc_options(argc, argv, &callbacks, done)) {
        exit(1);
    }

    if (command) {
        command->run(done);
    } else {
        rsvc_usage();
        exit(1);
    }

    dispatch_main();
}

void rsvc_usage() {
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
                "  eject DEVICE          eject CD\n"
                "  rip DEVICE            rip tracks to files\n"
                "  convert FILE          convert files\n"
                "\n"
                "Options:\n"
                "  -v, --verbose         more verbose logging\n"
                "  -V, --version         show version and exit\n",
                rsvc_progname);
    }
    exit(EX_USAGE);
};

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
