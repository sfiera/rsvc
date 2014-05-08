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

#include "options.h"

#include <stdlib.h>
#include <string.h>

bool rsvc_options(size_t argc, char* const* argv, rsvc_option_callbacks_t* callbacks,
                  rsvc_done_t fail) {
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
            if (eq) {
                // --option=value
                *eq = '\0';
                __block bool used_value = false;
                if (!callbacks->long_option(opt, ^bool (char** value, rsvc_done_t fail){
                    used_value = true;
                    *value = eq + 1;
                    return true;
                }, fail)) {
                    return false;
                } else if (!used_value) {
                    rsvc_errorf(fail, __FILE__, __LINE__, "option --%s: no argument permitted", opt);
                    return false;
                }
            } else {
                // --option; --option value
                __block bool used_value = false;
                if (!callbacks->long_option(opt, ^bool (char** value, rsvc_done_t fail){
                    if (!used_value) {
                        if (++i == argc) {
                            rsvc_errorf(fail, __FILE__, __LINE__,
                                        "option --%s: argument required", opt);
                            return false;
                        }
                    }
                    *value = argv[i];
                    used_value = true;
                    return true;
                }, fail)) {
                    return false;
                };
            }
            free(opt);
        } else if ((arg[0] == '-') && (arg[1] != '\0')) {
            // -abco; -abcovalue; -abco value
            __block char* val = NULL;
            for (char* ch = arg + 1; *ch && !val; ++ch) {
                if (!callbacks->short_option(*ch, ^bool (char** value, rsvc_done_t fail){
                    if (!val) {
                        val = ch + 1;
                        if (!*val) {
                            if (++i == argc) {
                                rsvc_errorf(fail, __FILE__, __LINE__,
                                            "option -%c: argument required", *ch);
                                return false;
                            }
                            val = argv[i];
                        }
                    }
                    *value = val;
                    return true;
                }, fail)) {
                    return false;
                };
            }
        } else {
            // argument
            if (!callbacks->argument(arg, fail)) {
                return false;
            }
        }
    }

    // Catch arguments after --.
    for ( ; i < argc; ++i) {
        if (!callbacks->argument(argv[i], fail)) {
            return false;
        }
    }
    return true;
}

bool rsvc_long_option(
        struct rsvc_long_option_name table[],
        bool (^short_option)(int32_t opt, rsvc_option_value_t get_value, rsvc_done_t fail),
        const char* opt, rsvc_option_value_t get_value, rsvc_done_t fail) {
    for ( ; table->long_name; ++table) {
        if (strcmp(opt, table->long_name) == 0) {
            return short_option(table->short_name, get_value, fail);
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "illegal option --%s", opt);
    return false;
}

bool rsvc_illegal_short_option(int32_t opt, rsvc_done_t fail) {
    rsvc_errorf(fail, __FILE__, __LINE__, "illegal option -%c", opt);
    return false;
}

bool rsvc_illegal_long_option(const char* opt, rsvc_done_t fail) {
    rsvc_errorf(fail, __FILE__, __LINE__, "illegal option --%s", opt);
    return false;
}

bool rsvc_string_option(char** string, rsvc_option_value_t get_value, rsvc_done_t fail) {
    if (*string) {
        free(*string);
    }
    char* value;
    if (!get_value(&value, fail)) {
        return false;
    }
    *string = strdup(value);
    return true;
}

bool rsvc_integer_option(int* integer, rsvc_option_value_t get_value, rsvc_done_t fail) {
    char* value;
    if (!get_value(&value, fail)) {
        return false;
    }
    char* end;
    int result = strtol(value, &end, 10);
    if (*end) {
        rsvc_errorf(fail, __FILE__, __LINE__, "an integer is required");
        return false;
    }
    *integer = result;
    return true;
}

bool rsvc_boolean_option(bool* boolean) {
    *boolean = true;
    return true;
}
