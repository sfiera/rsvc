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

#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <rsvc/tag.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

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

bool rsvc_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    if (tags_writable(tags)) {
        return tags->vptr->save(tags, fail);
    } else {
        return true;
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
    return check_tags_writable(tags, fail)
        && check_tag_name_is_valid(name, fail)
        && tags->vptr->remove(tags, name, fail);
}

bool rsvc_tags_add(rsvc_tags_t tags, rsvc_done_t fail,
                   const char* name, const char* value) {
    return check_tags_writable(tags, fail)
        && check_tag_name_is_valid(name, fail)
        && tags->vptr->add(tags, name, value, fail);
}

bool rsvc_tags_addf(rsvc_tags_t tags, rsvc_done_t fail,
                    const char* name, const char* format, ...) {
    if (!(check_tags_writable(tags, fail)
          && check_tag_name_is_valid(name, fail))) {
        return false;
    }
    char* value;
    va_list vl;
    va_start(vl, format);
    rsvc_vasprintf(&value, format, vl);
    va_end(vl);
    bool result = tags->vptr->add(tags, name, value, fail);
    free(value);
    return result;
}

bool rsvc_tags_image_clear(rsvc_tags_t tags, rsvc_done_t fail) {
    if (!tags->vptr->image_remove) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported format");
        return false;
    } else if (!check_tags_writable(tags, fail)) {
        return false;
    }
    return tags->vptr->image_remove(tags, NULL, fail);
}

bool rsvc_tags_image_remove(rsvc_tags_t tags, size_t index, rsvc_done_t fail) {
    if (!tags->vptr->image_remove) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported format");
        return false;
    } else if (!check_tags_writable(tags, fail)) {
        return false;
    }
    return tags->vptr->image_remove(tags, &index, fail);
}

bool rsvc_tags_image_add(
        rsvc_tags_t tags, rsvc_format_t format, const uint8_t* data, size_t size,
        rsvc_done_t fail) {
    if (!tags->vptr->image_add) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported format");
        return false;
    } else if (!check_tags_writable(tags, fail)) {
        return false;
    }
    return tags->vptr->image_add(tags, format, data, size, fail);
}

static bool empty_next() { return false; }
static void empty_break() {}

static struct rsvc_iter_methods empty_iter_vptr = {
    .next    = empty_next,
    .break_  = empty_break,
};
static struct rsvc_iter_methods* empty_it_ptr = &empty_iter_vptr;
static void* empty = &empty_it_ptr;

rsvc_tags_iter_t rsvc_tags_begin(rsvc_tags_t tags) {
    return tags->vptr->iter_begin(tags);
}

rsvc_tags_image_iter_t rsvc_tags_image_begin(rsvc_tags_t tags) {
    if (tags->vptr->image_iter_begin) {
        return tags->vptr->image_iter_begin(tags);
    } else {
        return empty;
    }
}

size_t rsvc_tags_image_size(rsvc_tags_t tags) {
    size_t size = 0;
    for (rsvc_tags_image_iter_t it = rsvc_tags_image_begin(tags); rsvc_next(it); ) {
        ++size;
    }
    return size;
}

const char* rsvc_tag_code_get(int code) {
    switch (code) {
#define RSVC_TAG_CASE(NAME) case RSVC_CODE_ ## NAME: return RSVC_ ## NAME
        RSVC_TAG_CASE(ARTIST);
        RSVC_TAG_CASE(ALBUM);
        RSVC_TAG_CASE(ALBUMARTIST);
        RSVC_TAG_CASE(TITLE);
        RSVC_TAG_CASE(GENRE);
        RSVC_TAG_CASE(GROUPING);
        RSVC_TAG_CASE(DATE);
        RSVC_TAG_CASE(TRACKNUMBER);
        RSVC_TAG_CASE(TRACKTOTAL);
        RSVC_TAG_CASE(DISCNUMBER);
        RSVC_TAG_CASE(DISCTOTAL);
        RSVC_TAG_CASE(SHOW);
        RSVC_TAG_CASE(EPISODENUMBER);
        RSVC_TAG_CASE(EPISODETOTAL);
        RSVC_TAG_CASE(SEASONNUMBER);
        RSVC_TAG_CASE(SEASONTOTAL);
#undef RSVC_TAG_CASE
    }
    return NULL;
}

// Return true if `str` matches "0|[1-9][0-9]*".
static bool is_canonical_int(const char* str) {
    if (strcmp(str, "0") == 0) {
        return true;
    } else if (('1' <= *str) && (*str <= '9')) {
        for (++str; *str; ++str) {
            if ((*str < '0') || ('9' < *str)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static size_t max_precision(rsvc_tags_t tags, const char* tag_name, size_t minimum) {
    __block size_t result = minimum;
    for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
        if ((strcasecmp(it->name, tag_name) == 0) && (strlen(it->value) > result)) {
            result = strlen(it->value);
        }
    }
    return result;
}

static bool any_tags(rsvc_tags_t tags, const char* tag_name) {
    for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
        if (strcasecmp(it->name, tag_name) == 0) {
            rsvc_break(it);
            return true;
        }
    }
    return false;
}

typedef struct format_node* format_node_t;
typedef struct format_list* format_list_t;

struct format_node {
    int     type;
    size_t  size;
    char    data[0];
};

struct format_list {
    format_node_t   head;
    format_node_t   tail;
};

static bool parse_format(const char* format,
                         rsvc_done_t fail,
                         void (^block)(int type, const char* data, size_t size)) {
    const char* ptr = format;
    while (*ptr) {
        size_t literal_size = strcspn(ptr, "%/");
        if (literal_size) {
            block(0, ptr, literal_size);
        }
        ptr += literal_size;

        switch (*ptr) {
          case '\0':
            break;
          case '/':
            literal_size = strspn(ptr, "/");
            block(0, ptr, literal_size);
            ptr += literal_size;
            break;
          case '%':
            ++ptr;
            if (*ptr == '%') {
                block(0, ptr, 1);
                ++ptr;
            } else if (rsvc_tag_code_get(*ptr)) {
                // Check that it's a valid option, then add it.
                block(*ptr, NULL, 0);
                ++ptr;
            } else if ((' ' <= *ptr) && ((*ptr + 0) < 0x80)) {
                rsvc_errorf(fail, __FILE__, __LINE__,
                            "%s: invalid format code %%%c", format, *ptr);
                return false;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__,
                            "%s: invalid format code %%\\%o", format, *ptr);
                return false;
            }
        }
    }
    return true;
}

// Append `src` to `*dst`, ensuring we do not overrun `*dst`.
//
// Advance `*dst` and `*dst_size` to point to the remainder of the
// string.
static void clipped_cat(char** dst, size_t* dst_size, const char* src, size_t src_size,
                        size_t* size_needed) {
    rsvc_logf(4, "src=%s, size=%zu", src, src_size);
    if (size_needed) {
        *size_needed += src_size;
    }
    if (*dst_size < src_size) {
        src_size = *dst_size;
    }
    memcpy(*dst, src, src_size);
    *dst += src_size;
    *dst_size -= src_size;
}

static void escape_for_path(char* string) {
    for (char* ch = string; *ch; ++ch) {
        // Allow non-ASCII characters and whitelisted ASCII.
        if (*ch & 0x80) {
            continue;
        } else if (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz"
                          "0123456789"
                          " #%&'()*+,-@[]^_{|}~", *ch)) {
            continue;
        }
        *ch = '_';
    }
}

enum snpath_state {
    SNPATH_INITIAL = 0,
    SNPATH_CONTENT,
    SNPATH_NO_CONTENT,
};

struct snpath_context {
    enum snpath_state   state;
    int                 type;
};

static bool snpathf(char* data, size_t size, size_t* size_needed,
                    const char* format, rsvc_tags_t tags, const char* extension,
                    rsvc_done_t fail) {
    bool add_trailing_nul = size;
    __block char* dst = data;
    __block size_t dst_size = size;
    if (add_trailing_nul) {
        --dst_size;
    }
    if (size_needed) {
        *size_needed = 0;
    }

    __block struct snpath_context ctx = { };
    if (!parse_format(format, fail, ^(int type, const char* src, size_t src_size){
        if (type == 0) {
            if (!src_size) {
                return;
            } else if (*src == '/') {
                if (ctx.state != SNPATH_NO_CONTENT) {
                    clipped_cat(&dst, &dst_size, src, src_size, size_needed);
                }
                ctx.state = SNPATH_NO_CONTENT;
            } else {
                clipped_cat(&dst, &dst_size, src, src_size, size_needed);
                ctx.state = SNPATH_CONTENT;
            }
            ctx.type = type;
        } else {
            const char* separator = ", ";
            const char* prefix = "";
            if (((type == RSVC_CODE_TRACKNUMBER) && (ctx.type == RSVC_CODE_DISCNUMBER))
                || ((type == RSVC_CODE_EPISODENUMBER) && (ctx.type == RSVC_CODE_SEASONNUMBER))) {
                prefix = "-";
            } else if (ctx.type) {
                prefix = " ";
            }

            size_t precision = 0;
            if ((type == RSVC_CODE_TRACKNUMBER) || (type == RSVC_CODE_EPISODENUMBER)) {
                precision = max_precision(tags, RSVC_TRACKTOTAL, 2);
            } else if ((type == RSVC_CODE_DISCNUMBER) || (type == RSVC_CODE_SEASONNUMBER)) {
                precision = max_precision(tags, RSVC_DISCTOTAL, 1);
            } else if ((type == RSVC_CODE_ALBUMARTIST) && !any_tags(tags, RSVC_ALBUMARTIST)) {
                type = RSVC_CODE_ARTIST;
            }

            size_t count = 0;
            for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
                if (strcasecmp(it->name, rsvc_tag_code_get(type)) == 0) {
                    if (!*it->value) {
                        continue;
                    }
                    if (count++ == 0) {
                        clipped_cat(&dst, &dst_size, prefix, strlen(prefix), size_needed);
                    } else {
                        clipped_cat(&dst, &dst_size, separator, strlen(separator), size_needed);
                    }
                    if (precision && is_canonical_int(it->value)) {
                        for (size_t i = strlen(it->value); i < precision; ++i) {
                            clipped_cat(&dst, &dst_size, "0", 1, size_needed);
                        }
                    }
                    char* escaped = strdup(it->value);
                    escape_for_path(escaped);
                    clipped_cat(&dst, &dst_size, escaped, strlen(escaped), size_needed);
                    free(escaped);
                }
            }
            if (count) {
                ctx.state = SNPATH_CONTENT;
                ctx.type = type;
            } else if (ctx.state == SNPATH_INITIAL) {
                ctx.state = SNPATH_NO_CONTENT;
            }
        }
    })) {
        return false;
    };

    if (extension) {
        clipped_cat(&dst, &dst_size, ".", 1, size_needed);
        clipped_cat(&dst, &dst_size, extension, strlen(extension), size_needed);
    }

    if (add_trailing_nul) {
        *dst = '\0';
    }
    return true;
}

bool rsvc_tags_strf(rsvc_tags_t tags, const char* format, const char* extension,
                    char** path, rsvc_done_t fail) {
    size_t size;
    if (!snpathf(NULL, 0, &size, format, tags, extension, fail)) {
        return false;
    }
    char* new_path = malloc(size + 1);
    if (!snpathf(new_path, size + 1, NULL, format, tags, extension, fail)) {
        free(new_path);
        return false;
    }
    *path = new_path;
    return true;
}

bool rsvc_tags_validate_strf(const char* format, rsvc_done_t fail) {
    return parse_format(format, fail, ^(int type, const char* data, size_t size){
        (void)type;
        (void)data;
        (void)size;
    });
}

bool rsvc_tags_copy(rsvc_tags_t dst, rsvc_tags_t src, rsvc_done_t fail) {
    if (!rsvc_tags_clear(dst, fail)) {
        return false;
    }
    for (rsvc_tags_iter_t it = rsvc_tags_begin(src); rsvc_next(it); ) {
        if (!rsvc_tags_add(dst, fail, it->name, it->value)) {
            rsvc_break(it);
            break;
        }
    }
    return true;
}

typedef struct rsvc_detached_tags_node* rsvc_detached_tags_node_t;
struct rsvc_detached_tags_node {
    char*                           name;
    char*                           value;
    rsvc_detached_tags_node_t       prev, next;
};

typedef struct rsvc_detached_tags_list* rsvc_detached_tags_list_t;
struct rsvc_detached_tags_list {
    rsvc_detached_tags_node_t       head, tail;
};

typedef struct rsvc_detached_tags* rsvc_detached_tags_t;
struct rsvc_detached_tags {
    struct rsvc_tags                super;
    struct rsvc_detached_tags_list  list;
};

static bool rsvc_detached_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    (void)fail;  // Always succeeds.
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    if (name) {
        rsvc_detached_tags_node_t curr = self->list.head;
        while (curr) {
            rsvc_detached_tags_node_t next = curr->next;
            if (strcasecmp(curr->name, name) == 0) {
                free(curr->name);
                free(curr->value);
                RSVC_LIST_ERASE(&self->list, curr);
            }
            curr = next;
        }
    } else {
        RSVC_LIST_CLEAR(&self->list, ^(rsvc_detached_tags_node_t node){
            free(node->name);
            free(node->value);
        });
    }
    return true;
}

static bool rsvc_detached_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                                   rsvc_done_t fail) {
    (void)fail;  // Always succeeds.
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    struct rsvc_detached_tags_node node = {
        .name   = strdup(name),
        .value  = strdup(value),
    };
    RSVC_LIST_PUSH(&self->list, memdup(&node, sizeof(node)));
    return true;
}

static void detached_break(void* super_it);
static bool detached_next(void* super_it);

static struct rsvc_iter_methods detached_iter_vptr = {
    .next    = detached_next,
    .break_  = detached_break,
};

typedef struct detached_tags_iter* detached_tags_iter_t;
struct detached_tags_iter {
    struct rsvc_tags_iter      super;
    rsvc_detached_tags_node_t  curr;
};

static rsvc_tags_iter_t detached_begin(rsvc_tags_t tags) {
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    struct detached_tags_iter iter = {
        .super = {
            .vptr = &detached_iter_vptr,
        },
        .curr  = self->list.head,
    };
    detached_tags_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static void detached_break(void* super_it) {
    detached_tags_iter_t it = DOWN_CAST(struct detached_tags_iter, (rsvc_tags_iter_t)super_it);
    free(it);
}

static bool detached_next(void* super_it) {
    detached_tags_iter_t it = DOWN_CAST(struct detached_tags_iter, (rsvc_tags_iter_t)super_it);
    if (it->curr) {
        it->super.name = it->curr->name;
        it->super.value = it->curr->value;
        it->curr = it->curr->next;
        return true;
    } else {
        detached_break(super_it);
        return false;
    }
}

static bool rsvc_detached_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    (void)tags;  // Not actually saving anything.
    (void)fail;
    return true;
}

static void rsvc_detached_tags_destroy(rsvc_tags_t tags) {
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    rsvc_detached_tags_remove(tags, NULL, ^(rsvc_error_t error){
        (void)error;  // Nothing we can do.
    });
    free(self);
}

static struct rsvc_tags_methods detached_vptr = {
    .remove      = rsvc_detached_tags_remove,
    .add         = rsvc_detached_tags_add,
    .save        = rsvc_detached_tags_save,
    .destroy     = rsvc_detached_tags_destroy,

    .iter_begin  = detached_begin,
};

rsvc_tags_t rsvc_tags_new() {
    struct rsvc_detached_tags tags = {
        .super = {
            .vptr   = &detached_vptr,
            .flags  = RSVC_TAG_RDWR,
        },
    };
    rsvc_detached_tags_t copy = memdup(&tags, sizeof(tags));
    return &copy->super;
}
