//
// This file is part of Rip Service.
//
// Copyright (C) 2013 Chris Pickel <sfiera@sfzmail.com>
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

#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <rsvc/format.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "list.h"

static size_t formats_capacity = 0;

void rsvc_format_register(rsvc_format_t format) {
    if ((rsvc_nformats + 1) >= formats_capacity) {
        if (!formats_capacity) {
            formats_capacity = 16;
        }
        rsvc_formats = realloc(rsvc_formats, sizeof(rsvc_format_t*) * (formats_capacity * 2));
    }
    rsvc_formats[rsvc_nformats++] = format;
    rsvc_formats[rsvc_nformats] = NULL;
}

rsvc_format_t rsvc_format_named(const char* name) {
    for (rsvc_format_t* fmt = rsvc_formats; *fmt; ++fmt) {
        if (strcmp((*fmt)->name, name) == 0) {
            return *fmt;
        }
    }
    return NULL;
}

rsvc_format_t rsvc_format_with_mime(const char* mime) {
    for (rsvc_format_t* fmt = rsvc_formats; *fmt; ++fmt) {
        if (strcmp((*fmt)->mime, mime) == 0) {
            return *fmt;
        }
    }
    return NULL;
}

static bool check_magic(rsvc_format_t format, int fd,
                        bool* matches, rsvc_format_t* out, rsvc_done_t fail) {
    if (!format->magic_size) {
        return true;
    }
    uint8_t* data = malloc(format->magic_size);

    ssize_t size = pread(fd, data, format->magic_size, 0);
    if (size < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        free(data);
        return false;
    } else if (size >= format->magic_size) {
        for (size_t i = 0; i < 4; ++i) {
            const char* magic = format->magic[i];
            if (!magic) {
                break;
            }
            for (size_t j = 0; j < format->magic_size; ++j) {
                if ((magic[j] != '?') && (magic[j] != data[j])) {
                    goto next_format;
                }
            }
            *matches = true;
            *out = format;
            break;
next_format:
            continue;
        }
    }

    free(data);
    return true;
}

static const char* get_extension(const char* path) {
    char* last_slash = strrchr(path, '/');
    char* last_dot = strrchr(path, '.');
    if (last_dot == NULL) {
        return path + strlen(path);
    } else if ((last_slash == NULL) || (last_slash < last_dot)) {
        return last_dot + 1;
    } else {
        return path + strlen(path);
    }
}

static bool check_extension(rsvc_format_t format, const char* path,
                            bool* matches, rsvc_format_t* out, rsvc_done_t fail) {
    (void)fail;  // Never fails (but could fail to match).
    if (!format->extension) {
        return true;
    }
    if (strcmp(get_extension(path), format->extension) == 0) {
        *out = format;
        *matches = true;
    }
    return true;
}

bool rsvc_format_detect(const char* path, int fd,
                        rsvc_format_t* format, rsvc_done_t fail) {
    // Don't open directories.
    struct stat st;
    if (fstat(fd, &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "is a directory");
        return false;
    }

    for (rsvc_format_t* fmt = rsvc_formats; *fmt; ++fmt) {
        bool matches = false;
        if (!(check_magic(*fmt, fd, &matches, format, fail) &&
              check_extension(*fmt, path, &matches, format, fail))) {
            return false;
        } else if (matches) {
            return true;
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
}

const char* rsvc_format_group_name(enum rsvc_format_group format_group) {
    static const char names[3][6] = {"audio", "video", "image"};
    return names[format_group];
}
