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

#define _BSD_SOURCE

#include "progress.h"

#include <dispatch/dispatch.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "list.h"

struct rsvc_progress {
    char* name;
    int percent;
    rsvc_progress_t prev, next;
};

static struct {
    rsvc_progress_t head;
    rsvc_progress_t tail;
} progress;

// main queue only.
static void progress_hide() {
    for (rsvc_progress_t curr = progress.head; curr; curr = curr->next) {
        printf("\033[1A\033[2K");
    }
}

// main queue only.
static void progress_show() {
    for (rsvc_progress_t curr = progress.head; curr; curr = curr->next) {
        printf("%4d%%   %s\n", curr->percent, curr->name);
    }
}

rsvc_progress_t rsvc_progress_start(const char* name) {
    struct rsvc_progress item_data = {
        .name = strdup(name),
        .percent = 0,
    };
    rsvc_progress_t item = memdup(&item_data, sizeof(item_data));

    dispatch_async(dispatch_get_main_queue(), ^{
        progress_hide();
        RSVC_LIST_PUSH(&progress, item);
        progress_show();
    });
    return item;
}

void rsvc_progress_update(rsvc_progress_t item, double fraction) {
    int percent = fraction * 100;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (item->percent == percent) {
            return;
        }
        progress_hide();
        item->percent = percent;
        progress_show();
    });
}

void rsvc_progress_done(rsvc_progress_t item) {
    dispatch_async(dispatch_get_main_queue(), ^{
        progress_hide();
        if (item->percent >= 100) {
            printf(" done   %s\n", item->name);
        } else {
            printf("%4d%%   %s\n", item->percent, item->name);
        }
        free(item->name);
        RSVC_LIST_ERASE(&progress, item);
        progress_show();
    });
}
