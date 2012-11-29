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

#include "common.h"
#include "list.h"

typedef struct rsvc_group_cancel_node* rsvc_group_cancel_node_t;
struct rsvc_group_cancel_node {
    void (^cancel)();
    rsvc_group_cancel_node_t prev, next;
};

typedef struct rsvc_group_cancel_list* rsvc_group_cancel_list_t;
struct rsvc_group_cancel_list {
    rsvc_group_cancel_node_t head, tail;
};

struct rsvc_group {
    dispatch_queue_t queue;
    size_t pending;
    rsvc_error_t final_error;
    rsvc_done_t done;
    struct rsvc_group_cancel_list on_cancel_list;
};

rsvc_group_t rsvc_group_create(dispatch_queue_t queue, rsvc_done_t done) {
    struct rsvc_group initializer = {queue, 1};
    rsvc_group_t group = memdup(&initializer, sizeof(initializer));
    rsvc_done_t group_done = ^(rsvc_error_t error){
        done(group->final_error);
        rsvc_error_destroy(group->final_error);
        Block_release(group->done);
    };
    group->done = Block_copy(group_done);
    return group;
}

static void finalize(rsvc_group_t group) {
    RSVC_LIST_CLEAR(&group->on_cancel_list, ^(rsvc_group_cancel_node_t node){
        Block_release(node->cancel);
    });
    group->done(group->final_error);
    Block_release(group->done);
    rsvc_error_destroy(group->final_error);
    free(group);
}

static void adopt_error(rsvc_group_t group, rsvc_error_t error) {
    if (group->final_error) {
        rsvc_error_destroy(error);
    } else {
        group->final_error = error;
        RSVC_LIST_CLEAR(&group->on_cancel_list, ^(rsvc_group_cancel_node_t node){
            node->cancel();
            Block_release(node->cancel);
        });
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

void rsvc_group_on_cancel(rsvc_group_t group, rsvc_stop_t stop) {
    dispatch_async(group->queue, ^{
        if (group->final_error) {
            stop();
        } else {
            struct rsvc_group_cancel_node node = {Block_copy(stop)};
            RSVC_LIST_PUSH(&group->on_cancel_list, memdup(&node, sizeof(node)));
        }
    });
}

void rsvc_group_cancel(rsvc_group_t group, rsvc_error_t error) {
    error = rsvc_error_clone(error);
    dispatch_async(group->queue, ^{
        adopt_error(group, error);
    });
}
