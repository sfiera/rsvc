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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

void rsvc_errorf(void (^callback)(rsvc_error_t),
                 const char* file, int lineno, const char* format, ...) {
    char* message;
    va_list vl;
    va_start(vl, format);
    vasprintf(&message, format, vl);
    va_end(vl);
    struct rsvc_error error = {message, file, lineno};
    callback(&error);
    free(message);
}

void rsvc_strerrorf(void (^callback)(rsvc_error_t),
                    const char* file, int lineno, const char* format, ...) {
    size_t buflen = strerror_r(errno, NULL, 0);
    char* strerror = malloc(buflen);
    strerror_r(errno, strerror, buflen);
    if (format) {
        char* message;
        va_list vl;
        va_start(vl, format);
        vasprintf(&message, format, vl);
        va_end(vl);
        rsvc_errorf(callback, file, lineno, "%s: %s", message, strerror);
        free(message);
    } else {
        rsvc_errorf(callback, file, lineno, "%s", strerror);
    }
    free(strerror);
}

bool rsvc_open(const char* path, int oflag, mode_t mode, int* fd,
               void (^fail)(rsvc_error_t error)) {
    *fd = open(path, oflag, mode);
    if (*fd < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    return true;
}

void rsvc_options(size_t argc, char* const* argv, rsvc_option_callbacks_t* callbacks) {
    __block size_t i = 1;
    for ( ; i < argc; ++i) {
        char* arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            // --
            ++i;
            break;
        } else if (strncmp(arg, "--", 2) == 0) {
            char* opt = strdup(arg + 2);
            char* eq = strchr(opt, '=');
            __block char* value = NULL;
            if (eq) {
                // --option=value
                *eq = '\0';
                value = eq + 1;
                if (!callbacks->long_option(opt, ^{
                    if (!value) {
                        value = eq + 1;
                    }
                    return value;
                })) {
                    callbacks->usage("illegal option: --%s", opt);
                }
                if (!value) {
                    callbacks->usage("option --%s: no argument permitted", opt);
                }
            } else {
                // --option; --option value
                if (!callbacks->long_option(opt, ^{
                    if (!value) {
                        if (++i == argc) {
                            callbacks->usage("option --%s: argument required", opt);
                        }
                        value = argv[i];
                    }
                    return value;
                })) {
                    callbacks->usage("illegal option: --%s", opt);
                };
            }
            free(opt);
        } else if ((arg[0] == '-') && (arg[1] != '\0')) {
            // -abco; -abcovalue; -abco value
            __block char* value = NULL;
            for (char* ch = arg + 1; *ch && !value; ++ch) {
                if (!callbacks->short_option(*ch, ^{
                    if (!value) {
                        value = ch + 1;
                        if (!*value) {
                            if (++i == argc) {
                                callbacks->usage("option -%c: argument required", *ch);
                            }
                            value = argv[i];
                        }
                    }
                    return value;
                })) {
                    callbacks->usage("illegal option: -%c", *ch);
                };
            }
        } else {
            // argument
            if (!callbacks->argument(arg)) {
                callbacks->usage("too many arguments");
            }
        }
    }

    // Catch arguments after --.
    for ( ; i < argc; ++i) {
        if (!callbacks->argument(argv[i])) {
            callbacks->usage("too many arguments");
        }
    }
}
