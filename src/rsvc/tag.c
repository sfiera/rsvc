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

#include <rsvc/tag.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tags structure: a singly-linked list of (name, value) pairs.  There
// are certainly more efficient ways that this could be represented, but
// we expect to be handling on the order of a CD's worth of distinct
// tags at any given time, so there's no point in any more complexity.
struct rsvc_tags {
    struct rsvc_tag* head;
};

struct rsvc_tag {
    char* name;
    char* value;
    struct rsvc_tag* next;
};

static void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}

rsvc_tags_t rsvc_tags_create() {
    struct rsvc_tags tags = {NULL};
    return (rsvc_tags_t)memdup(&tags, sizeof(tags));
}

static void rsvc_tag_copy(struct rsvc_tag** dst, struct rsvc_tag* src) {
    if (src) {
        struct rsvc_tag copy = {
            .name = src->name,
            .value = src->value,
            .next = NULL,
        };
        rsvc_tag_copy(&copy.next, src->next);
        *dst = (struct rsvc_tag*)memdup(&copy, sizeof(copy));
    }
}

rsvc_tags_t rsvc_tags_copy(rsvc_tags_t tags) {
    struct rsvc_tags copy = {NULL};
    rsvc_tag_copy(&copy.head, tags->head);
    return (rsvc_tags_t)memdup(&copy, sizeof(copy));
}

static void rsvc_tag_destroy(struct rsvc_tag* tag) {
    if (tag) {
        rsvc_tag_destroy(tag->next);
        free(tag->name);
        free(tag->value);
    }
}

void rsvc_tags_destroy(rsvc_tags_t tags) {
    rsvc_tag_destroy(tags->head);
    free(tags);
}

void rsvc_tags_clear(rsvc_tags_t tags, const char* name) {
    for (struct rsvc_tag** curr = &tags->head; *curr; curr = &(*curr)->next) {
        while (strcmp((*curr)->name, name) == 0) {
            struct rsvc_tag* old = *curr;
            *curr = (*curr)->next;
            old->next = NULL;
            rsvc_tag_destroy(old);
        }
    }
}

static bool tag_name_is_valid(const char* name) {
    return name[strspn(name, "ABCDEFGHIJ" "KLMNOPQRST" "UVWXYZ" "_")] == '\0';
}

static bool tag_add_allocated(rsvc_tags_t tags, const char* name, char* value) {
    struct rsvc_tag tag = {
        .name = strdup(name),
        .value = value,
        .next = NULL,
    };
    struct rsvc_tag** curr;
    for (curr = &tags->head; *curr; curr = &(*curr)->next) { }
    *curr = (struct rsvc_tag*)memdup(&tag, sizeof(tag));
    return true;
}

bool rsvc_tags_add(rsvc_tags_t tags, const char* name, const char* value) {
    return tag_name_is_valid(name)
        && tag_add_allocated(tags, name, strdup(value));
}

bool rsvc_tags_addf(rsvc_tags_t tags, const char* name, const char* format, ...) {
    if (!tag_name_is_valid(name)) {
        return false;
    }
    char* value;
    va_list vl;
    va_start(vl, format);
    vasprintf(&value, format, vl);
    va_end(vl);
    return tag_add_allocated(tags, name, value);
}

size_t rsvc_tags_size(rsvc_tags_t tags) {
    size_t ntags = 0;
    for (struct rsvc_tag* curr = tags->head; curr; curr = curr->next) {
        ++ntags;
    }
    return ntags;
}

bool rsvc_tags_get(rsvc_tags_t tags,
                   const char* names[], const char* values[], size_t* ntags) {
    size_t size = *ntags;
    *ntags = 0;
    for (struct rsvc_tag* curr = tags->head; curr; curr = curr->next) {
        if (*ntags == size) {
            return false;
        }
        names[*ntags] = curr->name;
        values[*ntags] = curr->value;
        ++*ntags;
    }
    return true;
}

size_t rsvc_tags_count(rsvc_tags_t tags, const char* name) {
    size_t nvalues = 0;
    for (struct rsvc_tag* curr = tags->head; curr; curr = curr->next) {
        if (strcmp(curr->name, name) == 0) {
            ++nvalues;
        }
    }
    return nvalues;
}

bool rsvc_tags_find(rsvc_tags_t tags,
                    const char* name, const char* values[], size_t* nvalues) {
    size_t size = *nvalues;
    *nvalues = 0;
    for (struct rsvc_tag* curr = tags->head; curr; curr = curr->next) {
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

bool rsvc_tags_each(rsvc_tags_t tags,
                    void (^block)(const char*, const char*, rsvc_stop_t)) {
    __block bool loop = true;
    for (struct rsvc_tag* curr = tags->head; curr && loop; curr = curr->next) {
        block(curr->name, curr->value, ^{
            loop = false;
        });
    }
    return loop;
}
