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
#include <sys/stat.h>

#include "common.h"
#include "list.h"

static bool tag_name_is_valid(const char* name) {
    return name[strspn(name, "ABCDEFGHIJ" "KLMNOPQRST" "UVWXYZ" "_")] == '\0';
}

static bool check_tag_name_is_valid(const char* name, rsvc_done_t fail) {
    if (!tag_name_is_valid(name)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid tag name: %s", name);
        return false;
    }
    return true;
}

static bool tags_writable(rsvc_tags_t tags) {
    return tags->flags & RSVC_TAG_RDWR;
}

static bool check_tags_writable(rsvc_tags_t tags, rsvc_done_t fail) {
    if (!tags_writable(tags)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "tags opened in read-only mode");
        return false;
    }
    return true;
}

void rsvc_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    if (tags_writable(tags)) {
        tags->vptr->save(tags, done);
    } else {
        done(NULL);
    }
}

void rsvc_tags_destroy(rsvc_tags_t tags) {
    tags->vptr->destroy(tags);
}

bool rsvc_tags_clear(rsvc_tags_t tags, rsvc_done_t fail) {
    if (!check_tags_writable(tags, fail)) {
        return false;
    }
    return tags->vptr->remove(tags, NULL, fail);
}

bool rsvc_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    if (!check_tags_writable(tags, fail) || !check_tag_name_is_valid(name, fail)) {
        return false;
    }
    return tags->vptr->remove(tags, name, fail);
}

bool rsvc_tags_add(rsvc_tags_t tags, rsvc_done_t fail,
                   const char* name, const char* value) {
    if (!check_tags_writable(tags, fail) || !check_tag_name_is_valid(name, fail)) {
        return false;
    }
    return tags->vptr->add(tags, name, value, fail);
}

bool rsvc_tags_addf(rsvc_tags_t tags, rsvc_done_t fail,
                    const char* name, const char* format, ...) {
    if (!check_tags_writable(tags, fail) || !check_tag_name_is_valid(name, fail)) {
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

typedef struct rsvc_tag_format_node* rsvc_tag_format_node_t;
struct rsvc_tag_format_node {
    struct rsvc_tag_format format;
    rsvc_tag_format_node_t prev, next;
};

static struct {
    rsvc_tag_format_node_t head, tail;
} formats;

void rsvc_tag_format_register(const char* name, size_t magic_size, const char* magic,
                                    rsvc_open_tags_f open_tags) {
    struct rsvc_tag_format_node node = {
        .format = {
            .name = strdup(name),
            .magic_size = magic_size,
            .magic = strdup(magic),
            .open_tags = open_tags,
        },
    };
    RSVC_LIST_PUSH(&formats, memdup(&node, sizeof(node)));
}

rsvc_tag_format_t rsvc_tag_format_named(const char* name) {
    for (rsvc_tag_format_node_t curr = formats.head; curr; curr = curr->next) {
        rsvc_tag_format_t format = &curr->format;
        if (strcmp(format->name, name) == 0) {
            return format;
        }
    }
    return NULL;
}

bool rsvc_tag_format_detect(int fd, rsvc_tag_format_t* format, rsvc_done_t fail) {
    // Don't open directories.
    struct stat st;
    if (fstat(fd, &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    if (st.st_mode & S_IFDIR) {
        rsvc_errorf(fail, __FILE__, __LINE__, "is a directory");
        return false;
    }

    for (rsvc_tag_format_node_t curr = formats.head; curr; curr = curr->next) {
        uint8_t* data = malloc(curr->format.magic_size);
        ssize_t size = pread(fd, data, curr->format.magic_size, 0);
        if (size < 0) {
            free(data);
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return false;
        } else if (size < curr->format.magic_size) {
            goto next_tag_type;
        }
        for (size_t i = 0; i < curr->format.magic_size; ++i) {
            if ((curr->format.magic[i] != '?') && (curr->format.magic[i] != data[i])) {
                goto next_tag_type;
            }
        }
        *format = &curr->format;
        free(data);
        return true;
next_tag_type:
        free(data);
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
}
