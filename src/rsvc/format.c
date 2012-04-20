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

#include <rsvc/format.h>

#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "list.h"

typedef struct rsvc_container_format_node* rsvc_container_format_node_t;
struct rsvc_container_format_node {
    struct rsvc_container_format format;
    rsvc_container_format_node_t prev, next;
};

struct {
    rsvc_container_format_node_t head, tail;
} formats;

void rsvc_container_format_register(const char* name, size_t magic_size, const char* magic,
                                    rsvc_read_tags_f read_tags) {
    struct rsvc_container_format_node node = {
        .format = {
            .name = strdup(name),
            .magic_size = magic_size,
            .magic = strdup(magic),
            .read_tags = read_tags,
        },
    };
    RSVC_LIST_PUSH(&formats, memdup(&node, sizeof(node)));
}

rsvc_container_format_t rsvc_container_format_named(const char* name) {
    for (rsvc_container_format_node_t curr = formats.head; curr; curr = curr->next) {
        rsvc_container_format_t format = &curr->format;
        if (strcmp(format->name, name) == 0) {
            return format;
        }
    }
    return NULL;
}

bool rsvc_container_format_detect(int fd, rsvc_container_format_t* format, rsvc_done_t fail) {
    // Don't open directories.
    struct stat st;
    if (fstat(fd, &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    if (st.st_mode & S_IFDIR) {
        rsvc_errorf(fail, __FILE__, __LINE__, "is a directory");
        return false;
    }

    for (rsvc_container_format_node_t curr = formats.head; curr; curr = curr->next) {
        uint8_t* data = malloc(curr->format.magic_size);
        ssize_t size = pread(fd, data, curr->format.magic_size, 0);
        if (size < 0) {
            free(data);
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return false;
        } else if (size < curr->format.magic_size) {
            goto next_container_type;
        }
        for (size_t i = 0; i < curr->format.magic_size; ++i) {
            if ((curr->format.magic[i] != '?') && (curr->format.magic[i] != data[i])) {
                goto next_container_type;
            }
        }
        *format = &curr->format;
        free(data);
        return true;
next_container_type:
        free(data);
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
}
