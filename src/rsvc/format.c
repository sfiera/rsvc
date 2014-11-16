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

typedef struct rsvc_format_node* rsvc_format_node_t;
struct rsvc_format_node {
    struct rsvc_format format;
    rsvc_format_node_t prev, next;
};

static struct {
    rsvc_format_node_t head, tail;
} formats;

static char* maybe_strdup(const char* s) {
    if (s) {
        return strdup(s);
    }
    return NULL;
}

void rsvc_format_register(rsvc_format_t format) {
    struct rsvc_format_node node = {
        .format = {
            .name       = strdup(format->name),
            .mime       = strdup(format->mime),
            .magic      = maybe_strdup(format->magic),
            .magic_size = format->magic_size,
            .extension  = maybe_strdup(format->extension),
            .lossless   = format->lossless,
            .open_tags  = format->open_tags,
            .encode     = format->encode,
            .decode     = format->decode,
        },
    };
    RSVC_LIST_PUSH(&formats, memdup(&node, sizeof(node)));
}

static bool check_flags(int flags, rsvc_format_t format) {
    return ((flags & RSVC_FORMAT_OPEN_TAGS)  ? !!format->open_tags  : true)
        && ((flags & RSVC_FORMAT_ENCODE)     ? !!format->encode     : true)
        && ((flags & RSVC_FORMAT_DECODE)     ? !!format->decode     : true);
}

rsvc_format_t rsvc_format_named(const char* name, int flags) {
    __block rsvc_format_t result = NULL;
    rsvc_formats_each(^(rsvc_format_t format, rsvc_stop_t stop){
        if (!check_flags(flags, format)) {
            return;
        }
        if (strcmp(format->name, name) == 0) {
            result = format;
            stop();
        }
    });
    return result;
}

rsvc_format_t rsvc_format_with_mime(const char* mime, int flags) {
    __block rsvc_format_t result = NULL;
    rsvc_formats_each(^(rsvc_format_t format, rsvc_stop_t stop){
        if (!check_flags(flags, format)) {
            return;
        }
        if (strcmp(format->mime, mime) == 0) {
            result = format;
            stop();
        }
    });
    return result;
}

bool rsvc_formats_each(void (^block)(rsvc_format_t format, rsvc_stop_t stop)) {
    __block bool loop = true;
    for (rsvc_format_node_t curr = formats.head; curr && loop; curr = curr->next) {
        block(&curr->format, ^{
            loop = false;
        });
    }
    return loop;
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
        goto failure;
    } else if (size < format->magic_size) {
        goto success;
    }
    for (size_t i = 0; i < format->magic_size; ++i) {
        if ((format->magic[i] != '?') && (format->magic[i] != data[i])) {
            goto success;
        }
    }
    *matches = true;
    *out = format;
success:
    free(data);
    return true;
failure:
    free(data);
    return false;
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
    if (!format->extension) {
        return true;
    }
    if (strcmp(get_extension(path), format->extension) == 0) {
        *out = format;
        *matches = true;
    }
    return true;
}

bool rsvc_format_detect(const char* path, int fd, int flags,
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

    for (rsvc_format_node_t curr = formats.head; curr; curr = curr->next) {
        if (!check_flags(flags, &curr->format)) {
            continue;
        }
        bool matches = false;
        if (!(check_magic(&curr->format, fd, &matches, format, fail) &&
              check_extension(&curr->format, path, &matches, format, fail))) {
            return false;
        } else if (matches) {
            return true;
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
}
