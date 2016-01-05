//
// This file is part of Rip Service.
//
// Copyright (C) 2014 Chris Pickel <sfiera@sfzmail.com>
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

#include "disc.h"

#include <string.h>

#include "list.h"
#include "common.h"

const char* rsvc_disc_type_name[] = {
    [RSVC_DISC_TYPE_NONE]   = "none",
    [RSVC_DISC_TYPE_CD]     = "cd",
    [RSVC_DISC_TYPE_DVD]    = "dvd",
    [RSVC_DISC_TYPE_BD]     = "bd",
    [RSVC_DISC_TYPE_HDDVD]  = "hddvd",
};

void rsvc_send_disc(struct rsvc_watch_context* watch, const char* name, enum rsvc_disc_type type) {
    if (type == RSVC_DISC_TYPE_NONE) {
        for (struct disc_node* node = watch->head; node; node = node->next) {
            if (strcmp(node->name, name) == 0) {
                watch->callbacks.disappeared(node->type, node->name);
                RSVC_LIST_ERASE(watch, node);
                break;
            }
        }
    } else {
        for (struct disc_node* node = watch->head; node; node = node->next) {
            if (strcmp(node->name, name) == 0) {
                if (node->type != type) {
                    watch->callbacks.disappeared(node->type, node->name);
                    node->type = type;
                    watch->callbacks.appeared(node->type, node->name);
                }
                return;
            }
        }
        struct disc_node node = {
            .name = strdup(name),
            .type = type,
        };
        RSVC_LIST_PUSH(watch, memdup(&node, sizeof(node)));
        watch->callbacks.appeared(node.type, node.name);
    }
}
