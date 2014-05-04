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

#include "group.h"

#include <Block.h>
#include <stdio.h>

#include "common.h"
#include "list.h"

struct rsvc_group {
    dispatch_queue_t queue;
    size_t pending;
    rsvc_error_t final_error;
    rsvc_done_t done;
};

rsvc_group_t rsvc_group_create(rsvc_done_t done) {
    dispatch_queue_t queue = dispatch_queue_create("org.rsvc.group", NULL);
    struct rsvc_group initializer = {
        .queue = queue,
        .pending = 1,
        .done = Block_copy(done),
    };
    return memdup(&initializer, sizeof(initializer));
}

static void finalize(rsvc_group_t group) {
    group->done(group->final_error);
    Block_release(group->done);
    rsvc_error_destroy(group->final_error);
    dispatch_release(group->queue);
    free(group);
}

static void adopt_error(rsvc_group_t group, rsvc_error_t error) {
    if (group->final_error) {
        rsvc_error_destroy(error);
    } else {
        group->final_error = error;
    }
}

rsvc_done_t rsvc_group_add(rsvc_group_t group) {
    dispatch_sync(group->queue, ^{
        ++group->pending;
    });
    __block rsvc_done_t done = ^(rsvc_error_t error){
        error = rsvc_error_clone(error);
        dispatch_async(group->queue, ^{
            if (error) {
                adopt_error(group, error);
            }
            if (--group->pending == 0) {
                finalize(group);
            }
            Block_release(done);
        });
    };
    done = Block_copy(done);
    return done;
}

void rsvc_group_ready(rsvc_group_t group) {
    dispatch_async(group->queue, ^{
        if (--group->pending == 0) {
            finalize(group);
        }
    });
}
