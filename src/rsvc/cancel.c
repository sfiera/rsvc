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

#include <rsvc/cancel.h>

#include <Block.h>
#include <assert.h>
#include <dispatch/dispatch.h>
#include <signal.h>

#include "common.h"
#include "list.h"

struct rsvc_cancel {
    dispatch_queue_t queue;
    bool cancelled;
    rsvc_cancel_handle_t head, tail;
};

struct rsvc_cancel_handle {
    rsvc_stop_t fn;
    rsvc_cancel_handle_t prev, next;
};

struct rsvc_cancel rsvc_sigint;

static dispatch_once_t sigint_once;
static dispatch_source_t sigint_source = NULL;

static void sigint_init(void* ignored) {
    rsvc_sigint.queue = dispatch_queue_create("net.sfiera.ripservice.cancel", NULL);
}

static void enable_sigint() {
    if (sigint_source || rsvc_sigint.cancelled) {
        return;
    }
    sigint_source = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(sigint_source, ^{
        rsvc_cancel(&rsvc_sigint);
    });
    dispatch_resume(sigint_source);
    signal(SIGINT, SIG_IGN);
    siginterrupt(SIGINT, 1);
}

static void disable_sigint() {
    if (!sigint_source) {
        return;
    }
    signal(SIGINT, SIG_DFL);
    dispatch_source_cancel(sigint_source);
    dispatch_release(sigint_source);
    sigint_source = NULL;
}

rsvc_cancel_t rsvc_cancel_new() {
    struct rsvc_cancel build_cancel = {
        .queue = dispatch_queue_create("net.sfiera.ripservice.cancel", NULL),
    };
    return memdup(&build_cancel, sizeof(build_cancel));
}

void rsvc_cancel_destroy(rsvc_cancel_t cancel) {
    dispatch_release(cancel->queue);
    assert(cancel->head == NULL);
    assert(cancel->tail == NULL);
    free(cancel);
}

rsvc_cancel_handle_t rsvc_cancel_add(rsvc_cancel_t cancel, rsvc_stop_t fn) {
    dispatch_once_f(&sigint_once, NULL, sigint_init);
    __block rsvc_cancel_handle_t handle;
    dispatch_sync(cancel->queue, ^{
        struct rsvc_cancel_handle build_handle = {
            .fn = Block_copy(fn),
        };
        handle = memdup(&build_handle, sizeof(build_handle));
        RSVC_LIST_PUSH(cancel, handle);
        if (cancel->cancelled) {
            fn();
        }
        if (cancel == &rsvc_sigint) {
            enable_sigint();
        }
    });
    return handle;
}

void rsvc_cancel(rsvc_cancel_t cancel) {
    dispatch_once_f(&sigint_once, NULL, sigint_init);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        dispatch_sync(cancel->queue, ^{
            if (cancel->cancelled) {
                return;
            }
            cancel->cancelled = true;
            for (rsvc_cancel_handle_t h = cancel->head; h; h = h->next) {
                h->fn();
            }
        });
    });
}

void rsvc_cancel_remove(rsvc_cancel_t cancel, rsvc_cancel_handle_t handle) {
    dispatch_once_f(&sigint_once, NULL, sigint_init);
    dispatch_sync(cancel->queue, ^{
        Block_release(handle->fn);
        RSVC_LIST_ERASE(cancel, handle);
        if ((cancel->head == NULL) && (cancel == &rsvc_sigint)) {
            disable_sigint();
        }
    });
}
