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

typedef enum {
    ARTIST          = 'a',
    ALBUM           = 'A',
    ALBUM_ARTIST    = 'b',
    TITLE           = 't',
    GENRE           = 'g',
    DATE            = 'y',
    TRACK           = 'k',
    TRACK_TOTAL     = 'K',
    DISC            = 'd',
    DISC_TOTAL      = 'D',
} format_code_t;

static const char* get_tag_name(int opt) {
    switch (opt) {
      case ARTIST:          return RSVC_ARTIST;
      case ALBUM:           return RSVC_ALBUM;
      case ALBUM_ARTIST:    return RSVC_ALBUMARTIST;
      case TITLE:           return RSVC_TITLE;
      case GENRE:           return RSVC_GENRE;
      case DATE:            return RSVC_DATE;
      case TRACK:           return RSVC_TRACKNUMBER;
      case TRACK_TOTAL:     return RSVC_TRACKTOTAL;
      case DISC:            return RSVC_DISCNUMBER;
      case DISC_TOTAL:      return RSVC_DISCTOTAL;
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
    while (*format) {
        size_t literal_size = strcspn(format, "%/");
        if (literal_size) {
            block(0, format, literal_size);
        }
        format += literal_size;

        switch (*format) {
          case '\0':
            break;
          case '/':
            literal_size = strspn(format, "/");
            block(0, format, literal_size);
            format += literal_size;
            break;
          case '%':
            ++format;
            if (*format == '%') {
                block(0, format, 1);
                ++format;
            } else if (get_tag_name(*format)) {
                // Check that it's a valid option, then add it.
                block(*format, NULL, 0);
                ++format;
            } else if ((' ' <= *format) && (*format < 0x80)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid format code %%%c", *format);
                return false;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid format code %%\\%o", *format);
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
            if ((type == TRACK) && (ctx.type == DISC)) {
                prefix = "-";
            } else if (ctx.type) {
                prefix = " ";
            }

            size_t precision = 0;
            if (type == TRACK) {
                precision = max_precision(tags, RSVC_TRACKTOTAL, 2);
            } else if (type == DISC) {
                precision = max_precision(tags, RSVC_DISCTOTAL, 1);
            } else if ((type == ALBUM_ARTIST) && !any_tags(tags, RSVC_ALBUMARTIST)) {
                type = ARTIST;
            }

            __block size_t count = 0;
            rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
                if (strcmp(name, get_tag_name(type)) == 0) {
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
        rsvc_tags_add(dst, fail, name, value);
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
