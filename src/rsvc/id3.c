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

#include <rsvc/id3.h>

#include <Block.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "common.h"
#include "encoding.h"
#include "list.h"

typedef struct id3_frame_type* id3_frame_type_t;
typedef struct id3_frame_spec* id3_frame_spec_t;
typedef struct id3_frame_node* id3_frame_node_t;
typedef struct id3_frame_list* id3_frame_list_t;

typedef bool (*id3_read_t)(id3_frame_list_t frames, id3_frame_spec_t spec,
                           uint8_t* data, size_t size, rsvc_done_t fail);
typedef void (*id3_yield_t)(id3_frame_node_t node,
                            void (^block)(const char* name, const char* value));

static bool id3_text_read(id3_frame_list_t frames, id3_frame_spec_t spec,
                          uint8_t* data, size_t size, rsvc_done_t fail);
static void id3_text_yield(id3_frame_node_t node,
                           void (^block)(const char* name, const char* value));
static void id3_sequence_yield(id3_frame_node_t node,
                               void (^block)(const char* name, const char* value));

struct id3_frame_type {
    id3_read_t          read;
    id3_yield_t         yield;
};

static struct id3_frame_type id3_text = {
    .read   = id3_text_read,
    .yield  = id3_text_yield,
};

static struct id3_frame_type id3_sequence = {
    .read   = id3_text_read,
    .yield  = id3_sequence_yield,
};

static struct id3_frame_spec {
    const char*         vorbis_name;
    const char*         id3_name;
    id3_frame_type_t    id3_type;
} id3_frame_specs[] = {
    // 4.2.1. Identification frames.
    {"GROUPING",        "TIT1",    &id3_text},
    {"TITLE",           "TIT2",    &id3_text},
    {"SUBTITLE",        "TIT3",    &id3_text},
    {"ALBUM",           "TALB",    &id3_text},
    {"TRACKNUMBER",     "TRCK",    &id3_sequence},
    {"DISCNUMBER",      "TPOS",    &id3_sequence},
    {"ISRC",            "TSRC",    &id3_text},

    // 4.2.2. Involved person frames.
    {"ARTIST",          "TPE1",    &id3_text},
    {"ALBUMARTIST",     "TPE2",    &id3_text},
    {"COMPOSER",        "TCOM",    &id3_text},
    {"ENCODER",         "TENC",    &id3_text},

    // 4.2.3. Derived and subjective properties frames.
    {"BPM",             "TBPM",    &id3_text},
    {"GENRE",           "TCON",    &id3_text},

    // 4.2.4. Rights and license frames.
    {"COPYRIGHT",       "TCOP",    &id3_text},

    // 4.2.5. Other text frames.
    {"DATE",            "TDRC",    &id3_text},
    {"ENCODER",         "TSSE",    &id3_text},
    {"ALBUMSORT",       "TSOA",    &id3_text},
    {"ARTISTSORT",      "TSOP",    &id3_text},
    {"TITLESORT",       "TSOT",    &id3_text},
};

#define ID3_FRAME_SPECS_SIZE (sizeof(id3_frame_specs) / sizeof(id3_frame_specs[0]))

struct id3_frame_node {
    id3_frame_node_t        prev, next;
    id3_frame_spec_t        spec;
    uint8_t                 data[0];
};

struct id3_frame_list {
    id3_frame_node_t        head, tail;
};

typedef struct rsvc_id3_tags* rsvc_id3_tags_t;
struct rsvc_id3_tags {
    struct rsvc_tags        super;

    char*                   path;
    int                     fd;

    // ID3 header data:
    int                     version[2];  // e.g. {4, 1} for ID3v2.4.1
    size_t                  size;

    // ID3 tag content:
    struct id3_frame_list   frames;
};

static bool rsvc_id3_tags_remove(rsvc_tags_t tags, const char* name,
                                 rsvc_done_t fail) {
    rsvc_errorf(fail, __FILE__, __LINE__, "removing id3 tags not supported");
    return true;
}

static bool rsvc_id3_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                              rsvc_done_t fail) {
    rsvc_errorf(fail, __FILE__, __LINE__, "adding id3 tags not supported");
    return false;
}

static bool rsvc_id3_tags_each(
        rsvc_tags_t tags,
        void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    __block bool loop = true;
    for (id3_frame_node_t curr = self->frames.head; curr && loop; curr = curr->next) {
        curr->spec->id3_type->yield(curr, ^(const char* name, const char* value){
            block(name, value, ^{
                loop = false;
            });
        });
    }
    return loop;
}

static void rsvc_id3_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        done(NULL);
    });
}

static void rsvc_id3_tags_clear(rsvc_id3_tags_t tags) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    close(self->fd);
    free(self->path);
    RSVC_LIST_CLEAR(&self->frames, ^(id3_frame_node_t node){});
}

static void rsvc_id3_tags_destroy(rsvc_tags_t tags) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    rsvc_id3_tags_clear(self);
    free(self);
}

static struct rsvc_tags_methods id3_vptr = {
    .remove     = rsvc_id3_tags_remove,
    .add        = rsvc_id3_tags_add,
    .each       = rsvc_id3_tags_each,
    .save       = rsvc_id3_tags_save,
    .destroy    = rsvc_id3_tags_destroy,
};

static bool read_id3_header(rsvc_id3_tags_t tags, rsvc_done_t fail) {
    rsvc_logf(2, "reading ID3 header");
    uint8_t data[10];
    ssize_t size = read(tags->fd, data, 10);
    if (size < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    } else if (size < 10) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of file");
        return false;
    }

    if (memcmp(data, "ID3", 3) != 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "no ID3 tag found");
        return false;
    }

    tags->version[0] = data[3];
    tags->version[1] = data[4];
    if (tags->version[0] != 4) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported ID3 version 2.%d.%d",
                    tags->version[0], tags->version[1]);
        return false;
    }

    uint8_t flags = data[5];
    if (flags & 0x80) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsynchronized ID3 tags not supported");
        return false;
    } else if (flags & 0x40) {
        rsvc_errorf(fail, __FILE__, __LINE__, "extended ID3 header not supported");
        return false;
    } else if (flags & 0x20) {
        rsvc_errorf(fail, __FILE__, __LINE__, "experimental ID3 tags not supported");
        return false;
    } else if (flags & 0x10) {
        rsvc_errorf(fail, __FILE__, __LINE__, "ID3 footer not supported");
        return false;
    } else if (flags & 0x0f) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unrecognized ID3 flags");
        return false;
    }

    tags->size = 0;
    for (int i = 6; i < 10; ++i) {
        if (data[i] & 0x80) {
            rsvc_errorf(fail, __FILE__, __LINE__, "high bit set in ID3 size");
            return false;
        }
        tags->size <<= 7;
        tags->size |= data[i];
    }
    return true;
}

static bool get_id3_frame_spec(const uint8_t* data, id3_frame_spec_t* spec, rsvc_done_t fail) {
    for (size_t i = 0; i < ID3_FRAME_SPECS_SIZE; ++i) {
        if (memcmp(id3_frame_specs[i].id3_name, data, 4) == 0) {
            *spec = &id3_frame_specs[i];
            return true;
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "%.4s: invalid ID3 frame type", (const char*)data);
    return false;
}

static void id3_text_yield(id3_frame_node_t node,
                           void (^block)(const char* name, const char* value)) {
    block(node->spec->vorbis_name, (const char*)&node->data);
}

static void id3_sequence_yield(id3_frame_node_t node,
                               void (^block)(const char* name, const char* value)) {
    const char* text = (const char*)&node->data;
    const char* slash = text + strcspn(text, "/");
    if (*slash) {
        bool sequence_is_track = (strcmp(node->spec->vorbis_name, "TRACKNUMBER") == 0);
        const char* seq_number = node->spec->vorbis_name;
        const char* seq_total = sequence_is_track ? "TRACKTOTAL" : "DISCTOTAL";

        char* seq_number_value = strndup(text, slash - text);
        block(seq_number, seq_number_value);
        block(seq_total, slash + 1);
        free(seq_number_value);
    } else {
        block(node->spec->vorbis_name, text);
    }
}

static bool id3_text_read(id3_frame_list_t frames, id3_frame_spec_t spec,
                          uint8_t* data, size_t size, rsvc_done_t fail) {
    if (size < 2) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of frame");
        return false;
    }

    uint8_t encoding = *(data++); --size;
    rsvc_decode_f decode;
    switch (encoding) {
        case 0x00: decode = rsvc_decode_latin1; break;
        case 0x01: decode = rsvc_decode_utf16bom; break;
        case 0x02: decode = rsvc_decode_utf16be; break;
        case 0x03: decode = rsvc_decode_utf8; break;
        default: {
            rsvc_errorf(fail, __FILE__, __LINE__, "unknown encoding");
            return false;
        }
    }

    __block bool result = false;
    decode(data, size, ^(const char* data, size_t size, rsvc_error_t error){
        if (error) {
            fail(error);
            return;
        }
        const char* zero = memchr(data, '\0', size);
        if (!zero) {
            rsvc_errorf(fail, __FILE__, __LINE__, "missing null terminator");
        } else if (zero != (data + size - 1)) {
            rsvc_errorf(fail, __FILE__, __LINE__, "more than one tag value");
        } else {
            rsvc_logf(3, "read text %s", data);
            id3_frame_node_t node = malloc(sizeof(struct id3_frame_node) + size);
            node->spec = spec;
            memcpy(node->data, data, size);
            RSVC_LIST_PUSH(frames, node);
            result = true;
        }
    });
    return result;
}

static bool read_id3_frame(rsvc_id3_tags_t tags, uint8_t** data, size_t* size, rsvc_done_t fail) {
    rsvc_logf(3, "reading ID3 %.4s frame", *data);
    id3_frame_spec_t spec;
    if (!get_id3_frame_spec(*data, &spec, fail)) {
        return false;
    }
    fail = ^(rsvc_error_t error){
        rsvc_errorf(fail, error->file, error->lineno, "%s: %s", spec->id3_name, error->message);
    };
    size_t frame_size = 0;
    for (int i = 4; i < 8; ++i) {
        if ((*data)[i] & 0x80) {
            rsvc_errorf(fail, __FILE__, __LINE__, "high bit set in ID3 frame size");
            return false;
        }
        frame_size <<= 7;
        frame_size |= (*data)[i];
    }
    if (((*data)[8] != 0) || ((*data)[9] != 0)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "ID3 frame flags not supported");
        return false;
    }

    *data += 10;
    *size -= 10;
    if (frame_size > *size) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%lu: invalid ID3 frame size", frame_size);
        return false;
    }

    spec->id3_type->read(&tags->frames, spec, *data, frame_size, fail);

    *data += frame_size;
    *size -= frame_size;
    return true;
}

static bool read_id3_frames(rsvc_id3_tags_t tags, rsvc_done_t fail) {
    rsvc_logf(2, "reading ID3 frames");
    uint8_t* orig_data = malloc(tags->size);
    uint8_t* data = orig_data;
    fail = ^(rsvc_error_t error){
        free(orig_data);
        fail(error);
    };
    ssize_t size = read(tags->fd, data, tags->size);
    if (size < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    } else if (size < tags->size) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of file");
        return false;
    }

    size_t unsigned_size = size;
    rsvc_logf(4, "%lu bytes to read", unsigned_size);
    while (unsigned_size > 0) {
        if ((unsigned_size < 10) || (*data == '\0')) {
            break;
        } else if (!read_id3_frame(tags, &data, &unsigned_size, fail)) {
            return false;
        }
        rsvc_logf(4, "%lu bytes remain", unsigned_size);
    }
    while (unsigned_size > 0) {
        if (*data != '\0') {
            rsvc_errorf(fail, __FILE__, __LINE__, "junk data after last ID3 tag");
            return false;
        }
        ++data;
        --unsigned_size;
    }

    free(orig_data);
    return true;
}

void rsvc_id3_read_tags(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t)) {
    __block struct rsvc_id3_tags tags = {
        .super = {
            .vptr = &id3_vptr,
        },
        .path = strdup(path),
    };
    void (^done_copy)(rsvc_tags_t, rsvc_error_t) = done;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        rsvc_logf(1, "reading ID3 file");
        void (^done)(rsvc_tags_t, rsvc_error_t) = done_copy;
        done = ^(rsvc_tags_t result_tags, rsvc_error_t error){
            if (error) {
                rsvc_id3_tags_clear(&tags);
            }
            done(result_tags, error);
        };
        rsvc_done_t fail = ^(rsvc_error_t error){
            done(NULL, error);
        };
        if (!rsvc_open(tags.path, O_RDWR, 0644, &tags.fd, fail)
                || !read_id3_header(&tags, fail)
                || !read_id3_frames(&tags, fail)) {
            return;
        }

        rsvc_id3_tags_t copy = memdup(&tags, sizeof(tags));
        done(&copy->super, NULL);
    });
}

void rsvc_id3_format_register() {
    rsvc_container_format_register("id3", 4, "ID3\004", rsvc_id3_read_tags);
}
