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

#define RSVC_LIST_PUSH(list, node) \
    do { \
        __typeof__(list) RSVC_LIST_PUSH_list = (list); \
        if (RSVC_LIST_PUSH_list->head) { \
            RSVC_LIST_PUSH_list->tail->next = (node); \
            RSVC_LIST_PUSH_list->tail->next->prev = RSVC_LIST_PUSH_list->tail; \
            RSVC_LIST_PUSH_list->tail = RSVC_LIST_PUSH_list->tail->next; \
        } else { \
            RSVC_LIST_PUSH_list->head = RSVC_LIST_PUSH_list->tail = (node); \
        } \
    } while (false)

#define RSVC_LIST_ERASE(list, node) \
    do { \
        __typeof__(list) RSVC_LIST_ERASE_list = (list); \
        __typeof__(node) RSVC_LIST_ERASE_node = (node); \
        if (RSVC_LIST_ERASE_node->prev) { \
            RSVC_LIST_ERASE_node->prev->next = RSVC_LIST_ERASE_node->next; \
        } else { \
            RSVC_LIST_ERASE_list->head = RSVC_LIST_ERASE_node->next; \
        } \
        if (RSVC_LIST_ERASE_node->next) { \
            RSVC_LIST_ERASE_node->next->prev = RSVC_LIST_ERASE_node->prev; \
        } else { \
            RSVC_LIST_ERASE_list->tail = RSVC_LIST_ERASE_node->prev; \
        } \
        free(RSVC_LIST_ERASE_node); \
    } while (false)

#define RSVC_LIST_CLEAR(list, block) \
    do { \
        __typeof__(list) RSVC_LIST_CLEAR_list = (list); \
        while (RSVC_LIST_CLEAR_list->head) { \
            RSVC_LIST_ERASE(RSVC_LIST_CLEAR_list, RSVC_LIST_CLEAR_list->head); \
        } \
    } while (false)

#endif  // RSVC_LIST_H_
