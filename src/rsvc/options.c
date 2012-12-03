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

#include "options.h"

#include <stdlib.h>
#include <string.h>

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
            if (eq) {
                // --option=value
                *eq = '\0';
                __block bool used_value = false;
                if (!callbacks->long_option(opt, ^(rsvc_done_t fail){
                    used_value = true;
                    return eq + 1;
                })) {
                    callbacks->usage("illegal option: --%s", opt);
                }
                if (!used_value) {
                    callbacks->usage("option --%s: no argument permitted", opt);
                }
            } else {
                // --option; --option value
                __block char* value = NULL;
                if (!callbacks->long_option(opt, ^(rsvc_done_t fail){
                    if (!value) {
                        if (++i == argc) {
                            rsvc_errorf(fail, __FILE__, __LINE__,
                                        "option --%s: argument required", opt);
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
                if (!callbacks->short_option(*ch, ^(rsvc_done_t fail){
                    if (!value) {
                        value = ch + 1;
                        if (!*value) {
                            if (++i == argc) {
                                rsvc_errorf(fail, __FILE__, __LINE__,
                                            "option -%c: argument required", *ch);
                                return (char*)NULL;
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
