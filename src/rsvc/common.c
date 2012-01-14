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

#include <rsvc/common.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

void rsvc_strerror(void (^callback)(rsvc_error_t), const char* file, int lineno) {
    size_t buflen = strerror_r(errno, NULL, 0);
    char* msg = malloc(buflen);
    strerror_r(errno, msg, buflen);
    struct rsvc_error error = {msg, file, lineno};
    callback(&error);
    free(msg);
}

void rsvc_const_error(void (^callback)(rsvc_error_t),
                      const char* file, int lineno, const char* message) {
    struct rsvc_error error = {message, file, lineno};
    callback(&error);
}

void rsvc_options(size_t argc, char* const* argv, option_callbacks_t* callbacks) {
    __block char* message = NULL;
    void (^options_failure)() = ^{
        callbacks->usage(message);
        free(message);
        exit(EX_USAGE);
    };
    __block size_t i;
    for (i = 1; i < argc; ++i) {
        char* arg = argv[i];
        if (arg[0] == '-') {
            if (arg[1] == '-') {
                if (arg[2] == '\0') {
                    break;
                }
                char* opt = strdup(arg + 2);
                char* eq = strchr(opt, '=');
                if (eq) {
                    *eq = '\0';
                    char* value = eq + 1;
                    __block bool used_value = false;
                    if (!callbacks->long_option(opt, ^{
                        used_value = true;
                        return value;
                    })) {
                        asprintf(&message, "illegal option: --%s", opt);
                        options_failure();
                    };
                    if (!used_value) {
                        asprintf(&message, "option --%s: no argument permitted", opt);
                        options_failure();
                    }
                } else {
                    if (!callbacks->long_option(opt, ^{
                        if (++i == argc) {
                            asprintf(&message, "option --%s: argument required", opt);
                            options_failure();
                        }
                        return argv[i];
                    })) {
                        asprintf(&message, "illegal option: --%s", opt);
                        options_failure();
                    };
                }
                free(opt);
            } else {
                __block bool used_value = false;
                for (char* ch = arg + 1; *ch && !used_value; ++ch) {
                    if (!callbacks->short_option(*ch, ^{
                        used_value = true;
                        char* value = ch + 1;
                        if (*value) {
                            return value;
                        } else {
                            if (++i == argc) {
                                asprintf(&message, "option -%c: argument required", *ch);
                                options_failure();
                            }
                            return argv[i];
                        }
                    })) {
                        asprintf(&message, "illegal option: -%c", *ch);
                        options_failure();
                    };
                }
            }
        } else {
            if (!callbacks->argument(arg)) {
                asprintf(&message, "too many arguments");
                options_failure();
            }
        }
    }

    // Catch arguments after --.
    for ( ; i < argc; ++i) {
        if (!callbacks->argument(argv[i])) {
            asprintf(&message, "too many arguments");
            options_failure();
        }
    }
}
