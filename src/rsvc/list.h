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

#ifndef SRC_RSVC_LIST_H_
#define SRC_RSVC_LIST_H_

#define RSVC_LIST_PUSH(list, node_data, node_size) \
    do { \
        if ((list)->head) { \
            (list)->tail->next = memdup((node_data), (node_size)); \
            (list)->tail->next->prev = (list)->tail; \
            (list)->tail = (list)->tail->next; \
        } else { \
            (list)->head = (list)->tail = memdup((node_data), (node_size)); \
        } \
    } while (false);

#define RSVC_LIST_CLEAR(list, block) \
    while ((list)->head) { \
        (block)((list)->head); \
        void* old = (list)->head; \
        (list)->head = (list)->head->next; \
        if ((list)->head) { \
            (list)->head->prev = NULL; \
        } \
        free(old); \
    } \
    (list)->tail = NULL;

#endif  // RSVC_LIST_H_
