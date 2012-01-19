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

#include <rsvc/comment.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Comments structure: a singly-linked list of (name, value) pairs.
// There are certainly more efficient ways that this could be
// represented, but we expect to be handling on the order of a CD's
// worth of distinct tags at any given time, so there's no point in any
// more complexity.
struct rsvc_comments {
    struct rsvc_comment* head;
};

struct rsvc_comment {
    char* name;
    char* value;
    struct rsvc_comment* next;
};

static void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}

rsvc_comments_t rsvc_comments_create() {
    struct rsvc_comments comments = {NULL};
    return (rsvc_comments_t)memdup(&comments, sizeof(comments));
}

static void rsvc_comment_copy(struct rsvc_comment** dst, struct rsvc_comment* src) {
    if (src) {
        struct rsvc_comment copy = {
            .name = src->name,
            .value = src->value,
            .next = NULL,
        };
        rsvc_comment_copy(&copy.next, src->next);
        *dst = (struct rsvc_comment*)memdup(&copy, sizeof(copy));
    }
}

rsvc_comments_t rsvc_comments_copy(rsvc_comments_t comments) {
    struct rsvc_comments copy = {NULL};
    rsvc_comment_copy(&copy.head, comments->head);
    return (rsvc_comments_t)memdup(&copy, sizeof(copy));
}

static void rsvc_comment_destroy(struct rsvc_comment* comment) {
    if (comment) {
        rsvc_comment_destroy(comment->next);
        free(comment->name);
        free(comment->value);
    }
}

void rsvc_comments_destroy(rsvc_comments_t comments) {
    rsvc_comment_destroy(comments->head);
    free(comments);
}

void rsvc_comments_clear(rsvc_comments_t comments, const char* name) {
    for (struct rsvc_comment** curr = &comments->head; *curr; curr = &(*curr)->next) {
        while (strcmp((*curr)->name, name) == 0) {
            struct rsvc_comment* old = *curr;
            *curr = (*curr)->next;
            old->next = NULL;
            rsvc_comment_destroy(old);
        }
    }
}

void rsvc_comments_add(rsvc_comments_t comments, const char* name, const char* value) {
    struct rsvc_comment comment = {
        .name = strdup(name),
        .value = strdup(value),
        .next = NULL,
    };
    struct rsvc_comment** curr;
    for (curr = &comments->head; *curr; curr = &(*curr)->next) { }
    *curr = (struct rsvc_comment*)memdup(&comment, sizeof(comment));
}

void rsvc_comments_add_int(rsvc_comments_t comments, const char* name, int value) {
    char buffer[64];
    sprintf(buffer, "%d", value);
    rsvc_comments_add(comments, name, buffer);
}

void rsvc_comments_set(rsvc_comments_t comments, const char* name, const char* value) {
    rsvc_comments_clear(comments, name);
    rsvc_comments_add(comments, name, value);
}

size_t rsvc_comments_size(rsvc_comments_t comments) {
    size_t ncomments = 0;
    for (struct rsvc_comment* curr = comments->head; curr; curr = curr->next) {
        ++ncomments;
    }
    return ncomments;
}

bool rsvc_comments_get(rsvc_comments_t comments,
                       const char* names[], const char* values[], size_t* ncomments) {
    size_t size = *ncomments;
    *ncomments = 0;
    for (struct rsvc_comment* curr = comments->head; curr; curr = curr->next) {
        if (*ncomments == size) {
            return false;
        }
        names[*ncomments] = curr->name;
        values[*ncomments] = curr->value;
        ++*ncomments;
    }
    return true;
}

size_t rsvc_comments_count(rsvc_comments_t comments, const char* name) {
    size_t nvalues = 0;
    for (struct rsvc_comment* curr = comments->head; curr; curr = curr->next) {
        if (strcmp(curr->name, name) == 0) {
            ++nvalues;
        }
    }
    return nvalues;
}

bool rsvc_comments_find(rsvc_comments_t comments,
                        const char* name, const char* values[], size_t* nvalues) {
    size_t size = *nvalues;
    *nvalues = 0;
    for (struct rsvc_comment* curr = comments->head; curr; curr = curr->next) {
        if (strcmp(curr->name, name) == 0) {
            if (*nvalues == size) {
                return false;
            }
            values[*nvalues] = curr->value;
            ++*nvalues;
        }
    }
    return true;
}

void rsvc_comments_each(rsvc_comments_t comments,
                        void (^block)(const char*, const char*, rsvc_stop_t)) {
    for (__block struct rsvc_comment* curr = comments->head; curr; curr = curr->next) {
        block(curr->name, curr->value, ^{
            curr = NULL;
        });
    }
}
