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

#ifndef RSVC_COMMON_H_
#define RSVC_COMMON_H_

#include <stdbool.h>
#include <stdlib.h>

#define RSVC_VERSION "0.0.0"

typedef void (^rsvc_stop_t)();

typedef struct rsvc_error* rsvc_error_t;
struct rsvc_error {
    const char* message;
    const char* file;
    int lineno;
};
void                    rsvc_errorf(void (^callback)(rsvc_error_t),
                                    const char* file, int lineno, const char* format, ...);
void                    rsvc_strerrorf(void (^callback)(rsvc_error_t),
                                       const char* file, int lineno, const char* format, ...);

typedef struct option_callbacks {
    bool (^short_option)(char opt, char* (^)());
    bool (^long_option)(char* opt, char* (^)());
    bool (^argument)(char* arg);
    void (^usage)(const char* message, ...);
} option_callbacks_t;

void                    rsvc_options(size_t argc, char* const* argv,
                                     option_callbacks_t* callbacks);

#endif  // RSVC_COMMON_H_
