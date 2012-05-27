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

#include <rsvc/encode.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "list.h"

typedef struct rsvc_encode_format_node* rsvc_encode_format_node_t;
struct rsvc_encode_format_node {
    struct rsvc_encode_format format;
    rsvc_encode_format_node_t prev, next;
};

static struct {
    rsvc_encode_format_node_t head, tail;
} formats;

void rsvc_encode_format_register(const char* name, const char* extension, bool lossless,
                                 rsvc_encode_f encode) {
    struct rsvc_encode_format_node node = {
        .format = {
            .name       = strdup(name),
            .extension  = strdup(extension),
            .lossless   = lossless,
            .encode     = encode,
        },
    };
    RSVC_LIST_PUSH(&formats, memdup(&node, sizeof(node)));
}

rsvc_encode_format_t rsvc_encode_format_named(const char* name) {
    __block rsvc_encode_format_t result = NULL;
    rsvc_encode_formats_each(^(rsvc_encode_format_t format, rsvc_stop_t stop){
        if (strcmp(format->name, name) == 0) {
            result = format;
            stop();
        }
    });
    return result;
}

bool rsvc_encode_formats_each(void (^block)(rsvc_encode_format_t format, rsvc_stop_t stop)) {
    __block bool loop = true;
    for (rsvc_encode_format_node_t curr = formats.head; curr && loop; curr = curr->next) {
        block(&curr->format, ^{
            loop = false;
        });
    }
    return loop;
}
