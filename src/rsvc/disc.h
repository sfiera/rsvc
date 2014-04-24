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

#ifndef SRC_RSVC_DISC_H_
#define SRC_RSVC_DISC_H_

#include <rsvc/disc.h>

struct rsvc_watch_context {
    dispatch_queue_t queue;
    bool enable;
    rsvc_disc_watch_callbacks_t callbacks;

    struct disc_node* head;
    struct disc_node* tail;
};

struct disc_node {
    char* name;
    rsvc_disc_type_t type;
    struct disc_node* prev;
    struct disc_node* next;
};

void rsvc_send_disc(struct rsvc_watch_context* watch, const char* name, rsvc_disc_type_t type);

#endif  // SRC_RSVC_DISC_H_
