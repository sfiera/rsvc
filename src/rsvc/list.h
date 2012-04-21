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

#define RSVC_LIST_PUSH(LIST, NODE) \
    ((^(__typeof__(LIST) list, __typeof__(NODE) node){ \
        if (list->head) { \
            list->tail->next = node; \
            list->tail->next->prev = list->tail; \
            list->tail = list->tail->next; \
        } else { \
            list->head = list->tail = node; \
        } \
    })(LIST, NODE))

#define RSVC_LIST_ERASE(LIST, NODE) \
    ((^(__typeof__(LIST) list, __typeof__(NODE) node){ \
        if (node->prev) { \
            node->prev->next = node->next; \
        } else { \
            list->head = node->next; \
        } \
        if (node->next) { \
            node->next->prev = node->prev; \
        } else { \
            list->tail = node->prev; \
        } \
        free(node); \
    })(LIST, NODE))

#define RSVC_LIST_CLEAR(LIST, BLOCK) \
    ((^(__typeof__(LIST) list, __typeof__(BLOCK) block){ \
        while (list->head) { \
            block(list->head); \
            RSVC_LIST_ERASE(list, list->head); \
        } \
    })(LIST, BLOCK))

#endif  // RSVC_LIST_H_
