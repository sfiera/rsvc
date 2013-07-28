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
    rsvc_vasprintf(&value, format, vl);
    va_end(vl);
    bool result = tags->vptr->add(tags, name, value, fail);
    free(value);
    return result;
}

bool rsvc_tags_each(rsvc_tags_t tags,
                    void (^block)(const char*, const char*, rsvc_stop_t)) {
    return tags->vptr->each(tags, block);
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
    rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        if ((strcmp(name, tag_name) == 0) && (strlen(value) > result)) {
            result = strlen(value);
        }
    });
    return result;
}

static bool any_tags(rsvc_tags_t tags, const char* tag_name) {
    return !rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        if (strcmp(name, tag_name) == 0) {
            stop();
        }
    });
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
            } else if ((' ' <= *ptr) && (*ptr < 0x80)) {
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

            __block size_t count = 0;
            rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
                if (strcmp(name, rsvc_tag_code_get(type)) == 0) {
                    if (!*value) {
                        return;
                    }
                    if (count++ == 0) {
                        clipped_cat(&dst, &dst_size, prefix, strlen(prefix), size_needed);
                    } else {
                        clipped_cat(&dst, &dst_size, separator, strlen(separator), size_needed);
                    }
                    if (precision && is_canonical_int(value)) {
                        for (size_t i = strlen(value); i < precision; ++i) {
                            clipped_cat(&dst, &dst_size, "0", 1, size_needed);
                        }
                    }
                    char* escaped = strdup(value);
                    escape_for_path(escaped);
                    clipped_cat(&dst, &dst_size, escaped, strlen(escaped), size_needed);
                    free(escaped);
                }
            });
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

void rsvc_tags_strf(rsvc_tags_t tags, const char* format, const char* extension,
                    void (^done)(rsvc_error_t error, char* path)) {
    rsvc_done_t fail = ^(rsvc_error_t error){
        done(error, NULL);
    };

    size_t size;
    if (!snpathf(NULL, 0, &size, format, tags, extension, fail)) {
        return;
    }
    char* new_path = malloc(size + 1);
    if (!snpathf(new_path, size + 1, NULL, format, tags, extension, fail)) {
        return;
    }
    done(NULL, new_path);
    free(new_path);
}

bool rsvc_tags_validate_strf(const char* format, rsvc_done_t fail) {
    return parse_format(format, fail, ^(int type, const char* data, size_t size){ });
}

bool rsvc_tags_copy(rsvc_tags_t dst, rsvc_tags_t src, rsvc_done_t fail) {
    if (!rsvc_tags_clear(dst, fail)) {
        return false;
    }
    return rsvc_tags_each(src, ^(const char* name, const char* value, rsvc_stop_t stop){
        if (!rsvc_tags_add(dst, fail, name, value)) {
            stop();
        }
    });
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
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    if (name) {
        rsvc_detached_tags_node_t curr = self->list.head;
        while (curr) {
            rsvc_detached_tags_node_t next = curr->next;
            if (strcmp(curr->name, name) == 0) {
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
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    struct rsvc_detached_tags_node node = {
        .name   = strdup(name),
        .value  = strdup(value),
    };
    RSVC_LIST_PUSH(&self->list, memdup(&node, sizeof(node)));
    return true;
}

static bool rsvc_detached_tags_each(
        rsvc_tags_t tags,
        void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    __block bool loop = true;
    for (rsvc_detached_tags_node_t curr = self->list.head; curr && loop; curr = curr->next) {
        block(curr->name, curr->value, ^{
            loop = false;
        });
    }
    return loop;
}

static void rsvc_detached_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        done(NULL);
    });
}

static void rsvc_detached_tags_destroy(rsvc_tags_t tags) {
    rsvc_detached_tags_t self = DOWN_CAST(struct rsvc_detached_tags, tags);
    rsvc_detached_tags_remove(tags, NULL, ^(rsvc_error_t error){
        (void)error;  // Nothing we can do.
    });
    free(self);
}

static struct rsvc_tags_methods detached_vptr = {
    .remove     = rsvc_detached_tags_remove,
    .add        = rsvc_detached_tags_add,
    .each       = rsvc_detached_tags_each,
    .save       = rsvc_detached_tags_save,
    .destroy    = rsvc_detached_tags_destroy,
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

typedef struct rsvc_tag_format_node* rsvc_tag_format_node_t;
struct rsvc_tag_format_node {
    struct rsvc_tag_format format;
    rsvc_tag_format_node_t prev, next;
};

static struct {
    rsvc_tag_format_node_t head, tail;
} formats;

void rsvc_tag_format_register(const char* name, size_t magic_size, const char* magic,
                              const char* extension, rsvc_open_tags_f open_tags) {
    struct rsvc_tag_format_node node = {
        .format = {
            .name = strdup(name),
            .magic_size = magic_size,
            .magic = strdup(magic),
            .extension = strdup(extension),
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

static bool check_magic(rsvc_tag_format_t format, int fd,
                        bool* matches, rsvc_tag_format_t* out, rsvc_done_t fail) {
    if (!format->magic_size) {
        return true;
    }
    uint8_t* data = malloc(format->magic_size);
    ssize_t size = pread(fd, data, format->magic_size, 0);
    if (size < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        goto failure;
    } else if (size < format->magic_size) {
        goto success;
    }
    for (size_t i = 0; i < format->magic_size; ++i) {
        if ((format->magic[i] != '?') && (format->magic[i] != data[i])) {
            goto success;
        }
    }
    *matches = true;
    *out = format;
success:
    free(data);
    return true;
failure:
    free(data);
    return false;
}

static const char* get_extension(const char* path) {
    char* last_slash = strrchr(path, '/');
    char* last_dot = strrchr(path, '.');
    if (last_dot == NULL) {
        return path + strlen(path);
    } else if ((last_slash == NULL) || (last_slash < last_dot)) {
        return last_dot + 1;
    } else {
        return path + strlen(path);
    }
}

static bool check_extension(rsvc_tag_format_t format, const char* path,
                            bool* matches, rsvc_tag_format_t* out, rsvc_done_t fail) {
    if (!format->extension) {
        return true;
    }
    if (strcmp(get_extension(path), format->extension) == 0) {
        *out = format;
        *matches = true;
    }
    return true;
}

bool rsvc_tag_format_detect(const char* path, int fd,
                            rsvc_tag_format_t* format, rsvc_done_t fail) {
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
        bool matches = false;
        if (!(check_magic(&curr->format, fd, &matches, format, fail) &&
              check_extension(&curr->format, path, &matches, format, fail))) {
            return false;
        } else if (matches) {
            return true;
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
}
