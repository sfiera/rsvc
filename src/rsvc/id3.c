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
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "common.h"
#include "encoding.h"
#include "list.h"
#include "unix.h"

typedef struct id3_frame_type* id3_frame_type_t;
typedef struct id3_frame_spec* id3_frame_spec_t;
typedef struct id3_frame_node* id3_frame_node_t;
typedef struct id3_frame_list* id3_frame_list_t;

typedef bool (*id3_read_t)(id3_frame_list_t frames, id3_frame_spec_t spec,
                           uint8_t* data, size_t size, rsvc_done_t fail);
typedef void (*id3_yield_t)(id3_frame_node_t node,
                            void (^block)(const char* name, const char* value));
typedef bool (*id3_add_t)(id3_frame_list_t frames, id3_frame_spec_t spec,
                          const char* value, rsvc_done_t fail);
typedef bool (*id3_remove_t)(id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail);
typedef size_t (*id3_size_t)(id3_frame_node_t node);
typedef void (*id3_write_t)(id3_frame_node_t node, uint8_t* data);

static bool     id3_text_read(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        uint8_t* data, size_t size, rsvc_done_t fail);
static bool     id3_text_read_2_3(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        uint8_t* data, size_t size, rsvc_done_t fail);
static void     id3_text_yield(
                        id3_frame_node_t node,
                        void (^block)(const char* name, const char* value));
static bool     id3_text_add(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        const char* value, rsvc_done_t fail);
static bool     id3_text_remove(
                        id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail);
static size_t   id3_text_size(id3_frame_node_t node);
static void     id3_text_write(id3_frame_node_t node, uint8_t* data);
static void     id3_sequence_yield(
                        id3_frame_node_t node,
                        void (^block)(const char* name, const char* value));
static bool     id3_sequence_number_add(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        const char* value, rsvc_done_t fail);
static bool     id3_sequence_number_remove(
                        id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail);
static bool     id3_sequence_total_add(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        const char* value, rsvc_done_t fail);
static bool     id3_sequence_total_remove(
                        id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail);
static bool     id3_passthru_read(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        uint8_t* data, size_t size, rsvc_done_t fail);
static void     id3_passthru_yield(
                        id3_frame_node_t node,
                        void (^block)(const char* name, const char* value));
static size_t   id3_passthru_size(id3_frame_node_t node);
static void     id3_passthru_write(id3_frame_node_t node, uint8_t* data);
static bool     id3_discard_read(
                        id3_frame_list_t frames, id3_frame_spec_t spec,
                        uint8_t* data, size_t size, rsvc_done_t fail);

struct id3_frame_type {
    id3_read_t          read;
    id3_yield_t         yield;
    id3_add_t           add;
    id3_remove_t        remove;
    id3_size_t          size;
    id3_write_t         write;
};

static struct id3_frame_type id3_text = {
    .read       = id3_text_read,
    .yield      = id3_text_yield,
    .add        = id3_text_add,
    .remove     = id3_text_remove,
    .size       = id3_text_size,
    .write      = id3_text_write,
};

static struct id3_frame_type id3_sequence_number = {
    .read       = id3_text_read,
    .yield      = id3_sequence_yield,
    .add        = id3_sequence_number_add,
    .remove     = id3_sequence_number_remove,
    .size       = id3_text_size,
    .write      = id3_text_write,
};

static struct id3_frame_type id3_sequence_total = {
    .read       = id3_text_read,
    .yield      = id3_sequence_yield,
    .add        = id3_sequence_total_add,
    .remove     = id3_sequence_total_remove,
    .size       = id3_text_size,
    .write      = id3_text_write,
};

static struct id3_frame_type id3_2_3_text = {
    .read       = id3_text_read_2_3,
};

static struct id3_frame_type id3_passthru = {
    .read       = id3_passthru_read,
    .yield      = id3_passthru_yield,
    .size       = id3_passthru_size,
    .write      = id3_passthru_write,
};

static struct id3_frame_type id3_discard = {
    .read       = id3_discard_read,
};

static struct id3_frame_spec {
    const char*         vorbis_name;
    const char*         id3_name;
    id3_frame_type_t    id3_2_3_type;
    id3_frame_type_t    id3_2_4_type;
} id3_frame_specs[] = {
    // 4.2.1. Identification frames.
    {"GROUPING",            "TIT1",     &id3_2_3_text,  &id3_text},
    {"TITLE",               "TIT2",     &id3_2_3_text,  &id3_text},
    {"SUBTITLE",            "TIT3",     &id3_2_3_text,  &id3_text},
    {"ALBUM",               "TALB",     &id3_2_3_text,  &id3_text},
    {"ORIGINALALBUM",       "TOAL",     &id3_2_3_text,  &id3_text},
    {"TRACKNUMBER",         "TRCK",     &id3_2_3_text,  &id3_sequence_number},
    {"TRACKTOTAL",          "TRCK",     &id3_2_3_text,  &id3_sequence_total},
    {"DISCNUMBER",          "TPOS",     &id3_2_3_text,  &id3_sequence_number},
    {"DISCTOTAL",           "TPOS",     &id3_2_3_text,  &id3_sequence_total},
    {"DISCSUBTITLE",        "TSST",     NULL,           &id3_text},
    {"ISRC",                "TSRC",     &id3_2_3_text,  &id3_text},

    // 4.2.2. Involved person frames.
    {"ARTIST",              "TPE1",     &id3_2_3_text,  &id3_text},
    {"ALBUMARTIST",         "TPE2",     &id3_2_3_text,  &id3_text},
    {"CONDUCTOR",           "TPE3",     &id3_2_3_text,  &id3_text},
    {"REMIXER",             "TPE4",     &id3_2_3_text,  &id3_text},
    {"ORIGINALARTIST",      "TOPE",     &id3_2_3_text,  &id3_text},
    {"LYRICIST",            "TEXT",     &id3_2_3_text,  &id3_text},
    {"ORIGINALLYRICIST",    "TOLY",     &id3_2_3_text,  &id3_text},
    {"COMPOSER",            "TCOM",     &id3_2_3_text,  &id3_text},
    {NULL,                  "TMCL",     NULL,           &id3_passthru},
    {NULL,                  "TIPL",     NULL,           &id3_passthru},
    // {NULL,               "IPLS",     &id3_passthru,  NULL},
    {"ENCODEDBY",           "TENC",     &id3_2_3_text,  &id3_text},

    // 4.2.3. Derived and subjective properties frames.
    {"BPM",                 "TBPM",     &id3_2_3_text,  &id3_text},
    {NULL,                  "TLEN",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TKEY",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TLAN",     &id3_passthru,  &id3_passthru},
    {"GENRE",               "TCON",     &id3_2_3_text,  &id3_text},
    {NULL,                  "TFLT",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TMED",     &id3_passthru,  &id3_passthru},
    {"MOOD",                "TMOO",     NULL,           &id3_text},

    // 4.2.4. Rights and license frames.
    {"COPYRIGHT",           "TCOP",     &id3_2_3_text,  &id3_text},
    {"PRODUCED",            "TPRO",     NULL,           &id3_text},
    {"LABEL",               "TPUB",     &id3_2_3_text,  &id3_text},
    {NULL,                  "TOWN",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TRSN",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TRSO",     &id3_passthru,  &id3_passthru},

    // 4.2.5. Other text frames.
    {NULL,                  "TOFN",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TDLY",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TDEN",     NULL,           &id3_passthru},
    {"ORIGINALDATE",        "TDOR",     NULL,           &id3_text},
    {"DATE",                "TDRC",     NULL,           &id3_text},
    {NULL,                  "TDRL",     NULL,           &id3_passthru},
    {NULL,                  "TDTG",     NULL,           &id3_passthru},
    {"ENCODER",             "TSSE",     &id3_2_3_text,  &id3_text},
    {"ALBUMSORT",           "TSOA",     NULL,           &id3_text},
    {"ARTISTSORT",          "TSOP",     NULL,           &id3_text},
    {"TITLESORT",           "TSOT",     NULL,           &id3_text},

    {"ORIGINALDATE",        "TORY",     &id3_2_3_text,  NULL},
    {"DATE",                "TYER",     &id3_2_3_text,  NULL},
    {NULL,                  "TRDA",     &id3_discard,   NULL},
    {NULL,                  "TSIZ",     &id3_discard,   NULL},
    {"ALBUMSORT",           "XSOA",     &id3_2_3_text,  NULL},
    {"ARTISTSORT",          "XSOP",     &id3_2_3_text,  NULL},
    {"TITLESORT",           "XSOT",     &id3_2_3_text,  NULL},

    // TODO(sfiera): merge these into the TYER frame.
    {"DATE",                "TDAT",     &id3_discard,   NULL},
    {"DATE",                "TIME",     &id3_discard,   NULL},

    // 4.2.6. User-defined text information frame.
    {NULL,                  "TXXX",     &id3_passthru,  &id3_passthru},

    // 4.3.1. URL link frames.
    {NULL,                  "WCOM",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WCOP",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WOAF",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WOAR",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WOAS",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WORS",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WPAY",     &id3_passthru,  &id3_passthru},
    {NULL,                  "WPUB",     &id3_passthru,  &id3_passthru},

    // 4.3.2. User-defined URL link frame.
    {NULL,                  "WXXX",     &id3_passthru,  &id3_passthru},

    // 4.14. Attached picture.
    {NULL,                  "APIC",     &id3_passthru,  &id3_passthru},

    // Various ignored frames.
    {NULL,                  "USLT",     &id3_passthru,  &id3_passthru},
    {NULL,                  "COMM",     &id3_passthru,  &id3_passthru},
    {NULL,                  "USER",     &id3_passthru,  &id3_passthru},
    {NULL,                  "PRIV",     &id3_passthru,  &id3_passthru},
    {NULL,                  "TSIZ",     &id3_passthru,  &id3_passthru},
};

#define ID3_FRAME_SPECS_SIZE (sizeof(id3_frame_specs) / sizeof(id3_frame_specs[0]))

// Gets the id3_frame_spec_t with `id3_name` equal to `data`.
static bool get_id3_frame_spec(
        int id3_version, const uint8_t data[4], id3_frame_spec_t* spec, rsvc_done_t fail) {
    for (size_t i = 0; i < ID3_FRAME_SPECS_SIZE; ++i) {
        id3_frame_spec_t candidate = &id3_frame_specs[i];
        id3_frame_type_t type = NULL;
        switch (id3_version) {
          case 3: type = candidate->id3_2_3_type; break;
          case 4: type = candidate->id3_2_4_type; break;
        }
        if (type && (memcmp(candidate->id3_name, data, 4) == 0)) {
            *spec = candidate;
            return true;
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "%.4s: invalid ID3 frame type", (const char*)data);
    return false;
}

// Gets the id3_frame_spec_t with `vorbis_name` equal to `name`.
static bool get_vorbis_frame_spec(const char* name, id3_frame_spec_t* spec, rsvc_done_t fail) {
    for (size_t i = 0; i < ID3_FRAME_SPECS_SIZE; ++i) {
        const char* spec_name = id3_frame_specs[i].vorbis_name;
        if (spec_name && (strcmp(spec_name, name) == 0)) {
            *spec = &id3_frame_specs[i];
            return true;
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "no such ID3 tag: %s", name);
    return false;
}

// Returns another id3_frame_spec_t with the same `id3_name` as `spec`.
static id3_frame_spec_t get_paired_frame_spec(id3_frame_spec_t spec) {
    for (size_t i = 0; i < ID3_FRAME_SPECS_SIZE; ++i) {
        if (&id3_frame_specs[i] == spec) {
            continue;
        }
        if (strcmp(id3_frame_specs[i].id3_name, spec->id3_name) == 0) {
            return &id3_frame_specs[i];
        }
    }
    return NULL;
}

struct id3_frame_node {
    id3_frame_node_t        prev, next;
    id3_frame_spec_t        spec;
    size_t                  size;
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

static id3_frame_node_t find_by_spec(id3_frame_list_t frames, id3_frame_spec_t spec) {
    for (id3_frame_node_t curr = frames->head; curr; curr = curr->next) {
        if (curr->spec == spec) {
            return curr;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////

bool            id3_write_tags(rsvc_id3_tags_t tags, rsvc_done_t fail);

static bool rsvc_id3_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    id3_frame_spec_t spec;
    if (!get_vorbis_frame_spec(name, &spec, fail)) {
        return false;
    }
    return spec->id3_2_4_type->remove(&self->frames, spec, fail);
}

static bool rsvc_id3_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                              rsvc_done_t fail) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    id3_frame_spec_t spec;
    if (!get_vorbis_frame_spec(name, &spec, fail)) {
        return false;
    }
    return spec->id3_2_4_type->add(&self->frames, spec, value, fail);
}

static bool rsvc_id3_tags_each(
        rsvc_tags_t tags,
        void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    __block bool loop = true;
    for (id3_frame_node_t curr = self->frames.head; curr && loop; curr = curr->next) {
        curr->spec->id3_2_4_type->yield(curr, ^(const char* name, const char* value){
            block(name, value, ^{
                loop = false;
            });
        });
    }
    return loop;
}

static void rsvc_id3_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    rsvc_id3_tags_t self = DOWN_CAST(struct rsvc_id3_tags, tags);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if (!id3_write_tags(self, done)) {
            return;
        }
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

void rsvc_id3_format_register() {
    rsvc_tag_format_register("id3", 3, "ID3", rsvc_id3_open_tags);
}

////////////////////////////////////////////////////////////////////////

static bool read_id3_header(rsvc_id3_tags_t tags, rsvc_done_t fail);
static bool read_id3_frames(rsvc_id3_tags_t tags, rsvc_done_t fail);
static bool read_id3_frame(rsvc_id3_tags_t tags, uint8_t** data, size_t* size, rsvc_done_t fail);

void rsvc_id3_open_tags(const char* path, int flags,
                        void (^done)(rsvc_tags_t, rsvc_error_t)) {
    char* path_copy = strdup(path);
    void (^done_copy)(rsvc_tags_t, rsvc_error_t) = done;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        void (^done)(rsvc_tags_t, rsvc_error_t) = done_copy;
        rsvc_logf(1, "reading ID3 file");
        __block struct rsvc_id3_tags tags = {
            .super = {
                .vptr   = &id3_vptr,
                .flags  = flags,
            },
            .path = path_copy,
        };
        done = ^(rsvc_tags_t result_tags, rsvc_error_t error){
            if (error) {
                rsvc_id3_tags_clear(&tags);
            }
            done(result_tags, error);
        };
        rsvc_done_t fail = ^(rsvc_error_t error){
            done(NULL, error);
        };
        bool rdwr = flags & RSVC_TAG_RDWR;
        if (!rsvc_open(tags.path, rdwr ? O_RDWR : O_RDONLY, 0644, &tags.fd, fail)
                || !read_id3_header(&tags, fail)
                || !read_id3_frames(&tags, fail)) {
            return;
        }

        rsvc_id3_tags_t copy = memdup(&tags, sizeof(tags));
        done(&copy->super, NULL);
    });
}

static bool read_sync_safe_size(const uint8_t* data, size_t* out, rsvc_done_t fail) {
    *out = 0;
    for (int i = 0; i < 4; ++i) {
        if (data[i] & 0x80) {
            rsvc_errorf(fail, __FILE__, __LINE__, "high bit set in ID3 size");
            return false;
        }
        *out <<= 7;
        *out |= data[i];
    }
    return true;
}

static bool read_sync_unsafe_size(const uint8_t* data, size_t* out, rsvc_done_t fail) {
    *out = 0;
    for (int i = 0; i < 4; ++i) {
        *out <<= 8;
        *out |= data[i];
    }
    return true;
}

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

    memcpy(tags->version, data + 3, 2);
    if ((tags->version[0] < 3) || (tags->version[0] > 4)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported ID3 version 2.%d.%d",
                    tags->version[0], tags->version[1]);
        return false;
    }

    uint8_t flags = data[5];
    if (flags) {
        rsvc_errorf(fail, __FILE__, __LINE__, "ID3 flags not supported");
        return false;
    }

    if (!read_sync_safe_size(data + 6, &tags->size, fail)) {
        return false;
    }
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
    rsvc_logf(4, "%zu bytes to read", unsigned_size);
    while (unsigned_size > 0) {
        if ((unsigned_size < 10) || (*data == '\0')) {
            break;
        } else if (!read_id3_frame(tags, &data, &unsigned_size, fail)) {
            return false;
        }
        rsvc_logf(4, "%zu bytes remain", unsigned_size);
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

static bool read_id3_frame(rsvc_id3_tags_t tags, uint8_t** data, size_t* size, rsvc_done_t fail) {
    rsvc_logf(3, "reading ID3 %.4s frame", *data);
    id3_frame_spec_t spec;
    if (!get_id3_frame_spec(tags->version[0], *data, &spec, fail)) {
        return false;
    }
    fail = ^(rsvc_error_t error){
        rsvc_errorf(fail, error->file, error->lineno, "%s: %s", spec->id3_name, error->message);
    };
    size_t frame_size;
    bool result;
    switch (tags->version[0]) {
      case 3: result = read_sync_unsafe_size(*data + 4, &frame_size, fail); break;
      case 4: result = read_sync_safe_size(*data + 4, &frame_size, fail); break;
    }
    if (!result) {
        return false;
    }
    if (((*data)[8] != 0) || ((*data)[9] != 0)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "ID3 frame flags not supported");
        return false;
    }

    *data += 10;
    *size -= 10;
    if (frame_size > *size) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%zu: invalid ID3 frame size", frame_size);
        return false;
    }

    id3_frame_type_t type = NULL;
    switch (tags->version[0]) {
        case 3: type = spec->id3_2_3_type; break;
        case 4: type = spec->id3_2_4_type; break;
    }
    if (!type->read(&tags->frames, spec, *data, frame_size, fail)) {
        return false;
    }

    *data += frame_size;
    *size -= frame_size;
    return true;
}

////////////////////////////////////////////////////////////////////////

static void write_id3_header(rsvc_id3_tags_t tags, uint8_t* data, size_t tags_size);
static void write_id3_tags(rsvc_id3_tags_t tags, uint8_t* data);

bool id3_write_tags(rsvc_id3_tags_t tags, rsvc_done_t fail) {
    rsvc_logf(2, "writing ID3 file");

    // Figure out the number of bytes needed to write the body of the
    // ID3 tag.  If that's less than the size of the ID3 tag that we
    // read in, then reuse that; don't shrink the tag.
    size_t body_size = 0;
    for (id3_frame_node_t curr = tags->frames.head; curr; curr = curr->next) {
        body_size += 10 + curr->spec->id3_2_4_type->size(curr);
    }
    if (body_size < tags->size) {
        body_size = tags->size;
    }

    // Header is always 10 bytes; size of body is as determined above.
    // Allocate space for the body.  Free it automatically on failure;
    // we'll have to free it manually on success.
    uint8_t header[10];
    uint8_t* body = malloc(body_size);
    memset(body, 0, body_size);
    fail = ^(rsvc_error_t error){
        free(body);
        fail(error);
    };

    // Write the header and all tags.
    write_id3_header(tags, header, body_size);
    write_id3_tags(tags, body);

    // Position tags->fd at the end of the ID3 content.  If we write the
    // file twice, we will need to return here.  If we write the file
    // twice at the same time, this function will break.
    //
    // TODO(sfiera): serialize calls to this function through a serial
    // dispatch queue.
    if (lseek(tags->fd, 10 + tags->size, SEEK_SET) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }

    // Open a temporary file to write the modified file content to.
    // First write the ID3 tag, then copy the remainder of the original
    // file.
    //
    // TODO(sfiera): if there's enough space in the original file to
    // write out the new ID3 tags, then reuse that space without
    // creating a new file.
    int fd;
    char tmp_path[PATH_MAX + 5] = {'\0'};
    strcat(tmp_path, tags->path);
    strcat(tmp_path, ".tmp");
    if (!rsvc_open(tmp_path, O_RDWR | O_CREAT | O_TRUNC, 0644, &fd, fail)) {
        return false;
    }
    write(fd, header, 10);
    write(fd, body, body_size);

    rsvc_logf(2, "copying MP3 data from %s to %s", tags->path, tmp_path);
    uint8_t buffer[4096];
    while (true) {
        uint8_t* data = buffer;
        ssize_t size = read(tags->fd, data, 4096);
        if (size < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return false;
        } else if (size == 0) {
            break;
        }
        while (size > 0) {
            ssize_t bytes_written = write(fd, data, size);
            if (bytes_written <= 0) {
                rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
                return false;
            }
            data += bytes_written;
            size -= bytes_written;
        }
    }

    // Move the new file over the original.  Close the original fd and
    // move the one that pointed to the temporary file into its place.
    // We'll use that if we try to write the file again.
    rsvc_logf(2, "renaming %s to %s", tmp_path, tags->path);
    if (rename(tmp_path, tags->path) < 0) {
        close(fd);
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    close(tags->fd);
    tags->fd = fd;

    free(body);
    return true;
}

static void write_sync_safe_size(uint8_t* data, size_t in) {
    for (int i = 0; i < 4; ++i) {
        data[3 - i] = in & 0x7f;
        in >>= 7;
    }
}

static void write_id3_header(rsvc_id3_tags_t tags, uint8_t* data, size_t tags_size) {
    rsvc_logf(2, "writing ID3 header");
    memcpy(data, "ID3\4\0\0", 6);
    write_sync_safe_size(data + 6, tags_size);
}

static void write_id3_tags(rsvc_id3_tags_t tags, uint8_t* data) {
    for (id3_frame_node_t curr = tags->frames.head; curr; curr = curr->next) {
        rsvc_logf(3, "writing ID3 %s frame", curr->spec->id3_name);
        size_t size = curr->spec->id3_2_4_type->size(curr);
        memset(data, 0, 10);
        memcpy(data, curr->spec->id3_name, 4);
        write_sync_safe_size(data + 4, size);
        curr->spec->id3_2_4_type->write(curr, data + 10);
        data += 10 + size;
    }
}

////////////////////////////////////////////////////////////////////////

static void id3_frame_addf(
        id3_frame_list_t frames, id3_frame_spec_t spec, const char* format, ...)
        __attribute__((format(printf, 3, 4)));

static id3_frame_node_t id3_frame_create(
        id3_frame_list_t frames, id3_frame_spec_t spec, size_t size) {
    id3_frame_node_t node = malloc(sizeof(struct id3_frame_node) + size);
    node->spec = spec;
    node->size = size;
    RSVC_LIST_PUSH(frames, node);
    return node;
}

static void id3_frame_addf(
        id3_frame_list_t frames, id3_frame_spec_t spec, const char* format, ...) {
    // Get the size of the formatted string.
    va_list vl1;
    va_start(vl1, format);
    size_t size = vsnprintf(NULL, 0, format, vl1) + 1;
    va_end(vl1);

    // Allocate a node with space for the string and format it.
    id3_frame_node_t node = id3_frame_create(frames, spec, size);
    va_list vl2;
    va_start(vl2, format);
    vsnprintf((char*)node->data, size, format, vl2);
    va_end(vl2);
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
            result = id3_text_add(frames, spec, data, fail);
        }
    });
    return result;
}

static bool id3_text_read_2_3(id3_frame_list_t frames, id3_frame_spec_t spec,
                              uint8_t* data, size_t size, rsvc_done_t fail) {
    if (size < 1) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of frame");
        return false;
    }
    if (!get_vorbis_frame_spec(spec->vorbis_name, &spec, fail)) {
        return false;
    }

    uint8_t encoding = *(data++); --size;
    rsvc_decode_f decode;
    switch (encoding) {
        case 0x00: decode = rsvc_decode_latin1; break;
        case 0x01: decode = rsvc_decode_utf16bom; break;
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
        if (zero && ((zero + 1 - data) != size)) {
            rsvc_errorf(fail, __FILE__, __LINE__, "tag value has embedded NUL character");
        } else {
            char* terminated = strndup(data, size);
            rsvc_logf(3, "read text %s", terminated);
            result = id3_text_add(frames, spec, terminated, fail);
            free(terminated);
        }
    });
    return result;
}

static void id3_text_yield(id3_frame_node_t node,
                           void (^block)(const char* name, const char* value)) {
    block(node->spec->vorbis_name, (const char*)&node->data);
}

static bool id3_text_add(
        id3_frame_list_t frames, id3_frame_spec_t spec,
        const char* value, rsvc_done_t fail) {
    id3_frame_node_t match = find_by_spec(frames, spec);
    if (match) {
        rsvc_errorf(fail, __FILE__, __LINE__, "only one ID3 %s tag permitted", spec->vorbis_name);
        return false;
    }
    id3_frame_addf(frames, spec, "%s", value);
    return true;
}

static bool id3_text_remove(
        id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail) {
    id3_frame_node_t match = find_by_spec(frames, spec);
    if (match) {
        RSVC_LIST_ERASE(frames, match);
    }
    return true;
}

static size_t id3_text_size(id3_frame_node_t node) {
    return 1 + node->size;
}

static void id3_text_write(id3_frame_node_t node, uint8_t* data) {
    *(data++) = 0x03;
    memcpy(data, node->data, node->size);
}

static void sequence_split(const char* text,
                           void (^block)(const char* number, const char* total)) {
    const char* slash = text + strcspn(text, "/");
    if (*slash) {
        if (slash == text) {
            block(NULL, slash + 1);
        } else {
            char* seq_number_value = strndup(text, slash - text);
            block(seq_number_value, slash + 1);
            free(seq_number_value);
        }
    } else if (*text) {
        block(text, NULL);
    }
}

static void id3_sequence_yield(id3_frame_node_t node,
                               void (^block)(const char* name, const char* value)) {
    sequence_split((const char*)&node->data, ^(const char* number, const char* total){
        if (number) {
            block(node->spec->vorbis_name, number);
        }
        if (total) {
            id3_frame_spec_t paired_spec;
            paired_spec = get_paired_frame_spec(node->spec);
            block(paired_spec->vorbis_name, total);
        }
    });
}

static void id3_sequence_add(
        id3_frame_list_t frames, id3_frame_spec_t spec,
        const char* number, const char* total) {
    number = number ? number : "";
    if (total) {
        id3_frame_addf(frames, spec, "%s/%s", number, total);
    } else {
        id3_frame_addf(frames, spec, "%s", number);
    }
}

static bool check_seq(id3_frame_spec_t spec, const char* value, rsvc_done_t fail) {
    if (value && (!*value || strchr(value, '/'))) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid ID3 %s %s", spec->vorbis_name, value);
        return false;
    }
    return true;
}

static bool id3_sequence_both_add(
        id3_frame_list_t frames,
        id3_frame_spec_t number_spec, const char* number_value,
        id3_frame_spec_t total_spec, const char* total_value,
        rsvc_done_t fail) {
    if (!number_spec) number_spec = get_paired_frame_spec(total_spec);
    if (!total_spec) total_spec = get_paired_frame_spec(number_spec);
    if (!check_seq(number_spec, number_value, fail) || !check_seq(total_spec, total_value, fail)) {
        return false;
    }
    id3_frame_node_t match = find_by_spec(frames, number_spec);
    if (!match) {
        rsvc_logf(3, "no %s frame; adding (%s, %s)",
                  number_spec->id3_name, number_value, total_value);
        id3_sequence_add(frames, number_spec, number_value, total_value);
        return true;
    }
    __block bool result = false;
    sequence_split((const char*)&match->data, ^(const char* number, const char* total){
        if (number_value && number) {
            rsvc_errorf(fail, __FILE__, __LINE__,
                        "only one ID3 %s tag permitted", number_spec->vorbis_name);
        } else if (total_value && total) {
            rsvc_errorf(fail, __FILE__, __LINE__,
                        "only one ID3 %s tag permitted", total_spec->vorbis_name);
        } else {
            if (!total) total = total_value;
            if (!number) number = number_value;
            rsvc_logf(3, "replacing %s frame with (%s, %s)", number_spec->id3_name, number, total);
            id3_sequence_add(frames, number_spec, number, total);
            RSVC_LIST_ERASE(frames, match);
            result = true;
        }
    });
    return result;
}

static bool id3_sequence_both_remove(
        id3_frame_list_t frames,
        id3_frame_spec_t number_spec, bool remove_number,
        id3_frame_spec_t total_spec, bool remove_total,
        rsvc_done_t fail) {
    if (!number_spec) number_spec = get_paired_frame_spec(total_spec);
    if (!total_spec) total_spec = get_paired_frame_spec(number_spec);
    id3_frame_node_t match = find_by_spec(frames, number_spec);
    if (match) {
        sequence_split((const char*)&match->data, ^(const char* number, const char* total){
            if (remove_number) number = NULL;
            if (remove_total) total = NULL;
            if (number || total) {
                id3_sequence_add(frames, number_spec, number, total);
            }
            RSVC_LIST_ERASE(frames, match);
        });
    }  // else nothing to remove
    return true;
}

static bool id3_sequence_number_add(
        id3_frame_list_t frames, id3_frame_spec_t spec, const char* value, rsvc_done_t fail) {
    return id3_sequence_both_add(frames, spec, value, NULL, NULL, fail);
}

static bool id3_sequence_number_remove(
        id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail) {
    return id3_sequence_both_remove(frames, spec, true, NULL, false, fail);
}

static bool id3_sequence_total_add(
        id3_frame_list_t frames, id3_frame_spec_t spec, const char* value, rsvc_done_t fail) {
    return id3_sequence_both_add(frames, NULL, NULL, spec, value, fail);
}

static bool id3_sequence_total_remove(
        id3_frame_list_t frames, id3_frame_spec_t spec, rsvc_done_t fail) {
    return id3_sequence_both_remove(frames, NULL, false, spec, true, fail);
}

static bool id3_passthru_read(id3_frame_list_t frames, id3_frame_spec_t spec,
                         uint8_t* data, size_t size, rsvc_done_t fail) {
    id3_frame_node_t node = id3_frame_create(frames, spec, size);
    memcpy(node->data, data, size);
    return true;
}

static void id3_passthru_yield(id3_frame_node_t node,
                          void (^block)(const char* name, const char* value)) {
}

static size_t id3_passthru_size(id3_frame_node_t node) {
    return node->size;
}

static void id3_passthru_write(id3_frame_node_t node, uint8_t* data) {
    memcpy(data, node->data, node->size);
}

static bool id3_discard_read(id3_frame_list_t frames, id3_frame_spec_t spec,
                             uint8_t* data, size_t size, rsvc_done_t fail) {
    // do nothing
    return true;
}
