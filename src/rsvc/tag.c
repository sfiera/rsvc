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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}

#define DOWN_CAST(TYPE, PTR) \
    ((TYPE*)((void*)PTR - offsetof(TYPE, super)))

struct rsvc_tag_node {
    char* name;
    char* value;
    struct rsvc_tag_node* next;
};
typedef struct rsvc_tag_node* rsvc_tag_node_t;

struct rsvc_free_tags {
    struct rsvc_tags super;
    rsvc_tag_node_t head;
};
typedef struct rsvc_free_tags* rsvc_free_tags_t;

static bool rsvc_free_tags_remove(rsvc_tags_t tags, const char* name,
                                  void (^fail)(rsvc_error_t error)) {
    rsvc_free_tags_t self = DOWN_CAST(struct rsvc_free_tags, tags);
    rsvc_tag_node_t* curr = &self->head;
    while (*curr) {
        if ((name == NULL) || (strcmp(name, (*curr)->name) == 0)) {
            rsvc_tag_node_t old = *curr;
            *curr = old->next;
            free(old->name);
            free(old->value);
            free(old);
        } else {
            curr = &(*curr)->next;
        }
    }
    return true;
}

static bool rsvc_free_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                               void (^fail)(rsvc_error_t error)) {
    rsvc_free_tags_t self = DOWN_CAST(struct rsvc_free_tags, tags);
    rsvc_tag_node_t* curr = &self->head;
    while (*curr) {
        curr = &(*curr)->next;
    }
    struct rsvc_tag_node node = {
        .name   = strdup(name),
        .value  = strdup(value),
        .next   = NULL,
    };
    *curr = memdup(&node, sizeof(node));
    return true;
}

static bool rsvc_free_tags_each(
        rsvc_tags_t tags,
        void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_free_tags_t self = DOWN_CAST(struct rsvc_free_tags, tags);
    __block bool loop = true;
    for (rsvc_tag_node_t curr = self->head; loop && (curr != NULL); curr = curr->next) {
        block(curr->name, curr->value, ^{
            loop = false;
        });
    }
    return loop;
}

static void rsvc_free_tags_save(rsvc_tags_t tags, void (^done)(rsvc_error_t)) {
    // nothing
}

static void rsvc_free_tags_destroy(rsvc_tags_t tags) {
    rsvc_free_tags_t self = DOWN_CAST(struct rsvc_free_tags, tags);
    rsvc_free_tags_remove(tags, NULL, ^(rsvc_error_t error){});
    free(self);
}

static struct rsvc_tags_methods free_vptr = {
    .remove = rsvc_free_tags_remove,
    .add = rsvc_free_tags_add,
    .each = rsvc_free_tags_each,
    .save = rsvc_free_tags_save,
    .destroy = rsvc_free_tags_destroy,
};

rsvc_tags_t rsvc_tags_create() {
    struct rsvc_free_tags tags = {
        .super = {
            .vptr = &free_vptr,
        },
        .head = NULL,
    };
    return memdup(&tags, sizeof(tags));
}

void rsvc_tags_save(rsvc_tags_t tags, void (^done)(rsvc_error_t)) {
    tags->vptr->save(tags, done);
}

void rsvc_tags_destroy(rsvc_tags_t tags) {
    tags->vptr->destroy(tags);
}

bool rsvc_tags_clear(rsvc_tags_t tags, void (^fail)(rsvc_error_t error)) {
    return tags->vptr->remove(tags, NULL, fail);
}

bool rsvc_tags_remove(rsvc_tags_t tags, const char* name, void (^fail)(rsvc_error_t error)) {
    return tags->vptr->remove(tags, name, fail);
}

static bool tag_name_is_valid(const char* name) {
    return name[strspn(name, "ABCDEFGHIJ" "KLMNOPQRST" "UVWXYZ" "_")] == '\0';
}

bool rsvc_tags_add(rsvc_tags_t tags, void (^fail)(rsvc_error_t error),
                   const char* name, const char* value) {
    if (!tag_name_is_valid(name)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid tag name: %s", name);
        return false;
    }
    return tags->vptr->add(tags, name, value, fail);
}

bool rsvc_tags_addf(rsvc_tags_t tags, void (^fail)(rsvc_error_t error),
                    const char* name, const char* format, ...) {
    if (!tag_name_is_valid(name)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid tag name: %s", name);
        return false;
    }
    char* value;
    va_list vl;
    va_start(vl, format);
    vasprintf(&value, format, vl);
    va_end(vl);
    bool result = tags->vptr->add(tags, name, value, fail);
    free(value);
    return result;
}

bool rsvc_tags_each(rsvc_tags_t tags,
                    void (^block)(const char*, const char*, rsvc_stop_t)) {
    return tags->vptr->each(tags, block);
}
