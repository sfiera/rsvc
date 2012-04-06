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

#include "progress.h"

#include <dispatch/dispatch.h>
#include <stdio.h>
#include <string.h>

static void* memdup(void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}

struct rsvc_progress_node {
    char* name;
    int percent;
    rsvc_progress_t parent;
    rsvc_progress_node_t prev;
    rsvc_progress_node_t next;
};

struct rsvc_progress {
    rsvc_progress_node_t head;
    rsvc_progress_node_t tail;
};

rsvc_progress_t rsvc_progress_create() {
    struct rsvc_progress p = {NULL, NULL};
    return memdup(&p, sizeof(p));
}

void rsvc_progress_destroy(rsvc_progress_t progress) {
    free(progress);
}

// main queue only.
static void progress_hide(rsvc_progress_t progress) {
    for (rsvc_progress_node_t curr = progress->head; curr; curr = curr->next) {
        printf("\033[1A\033[2K");
    }
}

// main queue only.
static void progress_show(rsvc_progress_t progress) {
    for (rsvc_progress_node_t curr = progress->head; curr; curr = curr->next) {
        printf("%4d%%   %s\n", curr->percent, curr->name);
    }
}

rsvc_progress_node_t rsvc_progress_start(rsvc_progress_t progress, const char* name) {
    rsvc_progress_node_t node = malloc(sizeof(struct rsvc_progress_node));
    node->name = strdup(name);
    node->percent = 0;
    node->parent = progress;
    node->prev = node->next = NULL;

    dispatch_async(dispatch_get_main_queue(), ^{
        progress_hide(progress);
        if (progress->tail) {
            progress->tail->next = node;
        } else {
            progress->head = node;
        }
        node->prev = progress->tail;
        progress->tail = node;
        progress_show(progress);
    });
    return node;
}

void rsvc_progress_update(rsvc_progress_node_t node, double fraction) {
    int percent = fraction * 100;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (node->percent == percent) {
            return;
        }
        progress_hide(node->parent);
        node->percent = percent;
        progress_show(node->parent);
    });
}

void rsvc_progress_done(rsvc_progress_node_t node) {
    dispatch_async(dispatch_get_main_queue(), ^{
        progress_hide(node->parent);
        printf(" done   %s\n", node->name);
        free(node->name);
        if (node->prev) {
            node->prev->next = node->next;
        } else {
            node->parent->head = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        } else {
            node->parent->tail = node->prev;
        }
        free(node);
        progress_show(node->parent);
    });
}
