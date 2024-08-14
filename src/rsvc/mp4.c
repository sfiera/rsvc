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

#define _POSIX_C_SOURCE 200809L

#include "audio.h"

#include <Block.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <tag_c.h>
#include <unistd.h>

#include "common.h"

typedef struct mp4_tag_type* mp4_tag_type_t;

struct rsvc_mp4_tags {
    struct rsvc_tags    super;
    char*               path;
    TagLib_File*        file;
};
typedef struct rsvc_mp4_tags* rsvc_mp4_tags_t;

static bool rsvc_mp4_tags_remove(rsvc_tags_t tags, const char* name,
                                 rsvc_done_t fail) {
    (void)fail;  // always succeeds
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    if (name) {
        taglib_property_set(self->file, name, NULL);
    } else {
        char** keys = taglib_property_keys(self->file);
        if (!keys) {
            return true;
        }
        for (char** k = keys; *k; ++k) {
            taglib_property_set(self->file, *k, NULL);
        }
        taglib_property_free(keys);
    }
    return true;
}

static bool rsvc_mp4_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                              rsvc_done_t fail) {
    (void)fail;  // always succeeds
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    taglib_property_set_append(self->file, name, value);
    return false;
}

static void mp4_break(rsvc_iter_t super_it);
static bool mp4_next(rsvc_iter_t super_it);

static struct rsvc_iter_methods mp4_iter_vptr = {
    .next    = mp4_next,
    .break_  = mp4_break,
};

typedef struct mp4_tags_iter* mp4_tags_iter_t;
struct mp4_tags_iter {
    struct rsvc_tags_iter  super;

    TagLib_File*           file;
    char**                 keys;
    char**                 values;
    int                    key;
    int                    value;
};

static rsvc_tags_iter_t mp4_begin(rsvc_tags_t tags) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    char** keys = taglib_property_keys(self->file);
    struct mp4_tags_iter iter = {
        .super = {
            .vptr = &mp4_iter_vptr,
        },
        .file       = self->file,
        .keys       = keys,
        .values     = NULL,
        .key        = 0,
        .value      = 0,
    };
    mp4_tags_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static void mp4_break(rsvc_iter_t super_it) {
    mp4_tags_iter_t it = DOWN_CAST(struct mp4_tags_iter, (rsvc_tags_iter_t)super_it);
    if (it->keys) {
        taglib_property_free(it->keys);
    }
    if (it->values) {
        taglib_property_free(it->values);
    }
    free(it);
}

static bool mp4_next(rsvc_iter_t super_it) {
    mp4_tags_iter_t it = DOWN_CAST(struct mp4_tags_iter, (rsvc_tags_iter_t)super_it);

    if (!it->keys || !it->keys[it->key]) {
        mp4_break(super_it);
        return false;
    }
    if (!it->values || !it->values[it->value]) {
        if (it->values) {
            taglib_property_free(it->values);
        }
        it->values = taglib_property_get(it->file, it->keys[it->key]);
        it->value = 0;
    }
    it->super.name = it->keys[it->key];
    it->super.value = it->values[it->value++];
    if (!it->values[it->value]) {
        ++it->key;
    }
    return true;
}

static void mp4_image_break(rsvc_iter_t super_it);
static bool mp4_image_next(rsvc_iter_t super_it);

static struct rsvc_iter_methods mp4_image_iter_vptr = {
    .next    = mp4_image_next,
    .break_  = mp4_image_break,
};

typedef struct mp4_tags_image_iter* mp4_tags_image_iter_t;
struct mp4_tags_image_iter {
    struct rsvc_tags_image_iter  super;

    TagLib_File*           file;
    int                    i;
};

static rsvc_tags_image_iter_t mp4_image_begin(rsvc_tags_t tags) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    struct mp4_tags_image_iter iter = {
        .super = {
            .vptr = &mp4_image_iter_vptr,
        },
        .file   = self->file,
        .i      = -1,
    };
    mp4_tags_image_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static void mp4_image_break(rsvc_iter_t super_it) {
    mp4_tags_image_iter_t it = DOWN_CAST(struct mp4_tags_image_iter, (rsvc_tags_image_iter_t)super_it);
    free(it);
}

static bool mp4_image_next(rsvc_iter_t super_it) {
    mp4_tags_image_iter_t it = DOWN_CAST(struct mp4_tags_image_iter, (rsvc_tags_image_iter_t)super_it);
    (void)it;

    mp4_image_break(super_it);
    return false;
}

static bool rsvc_mp4_tags_image_remove(
        rsvc_tags_t tags, size_t* index, rsvc_done_t fail) {
    if (index) {
        rsvc_errorf(fail, __FILE__, __LINE__, "not implemented");
        return false;
    }
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    (void)self;
    rsvc_errorf(fail, __FILE__, __LINE__, "not implemented");
    return false;
}

static bool rsvc_mp4_tags_image_add(
        rsvc_tags_t tags, rsvc_format_t format, const uint8_t* data, size_t size,
        rsvc_done_t fail) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    (void)self;
    (void)format;
    (void)data;
    (void)size;
    rsvc_errorf(fail, __FILE__, __LINE__, "not implemented");
    return false;
}

static bool rsvc_mp4_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    if (!taglib_file_save(self->file)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "failed to save");
        return false;
    }
    return true;
}

static void rsvc_mp4_tags_destroy(rsvc_tags_t tags) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    taglib_file_free(self->file);
    free(self->path);
    free(self);
}

static struct rsvc_tags_methods mp4_vptr = {
    .remove = rsvc_mp4_tags_remove,
    .add = rsvc_mp4_tags_add,
    .image_remove = rsvc_mp4_tags_image_remove,
    .image_add = rsvc_mp4_tags_image_add,
    .save = rsvc_mp4_tags_save,
    .destroy = rsvc_mp4_tags_destroy,

    .iter_begin  = mp4_begin,
    .image_iter_begin  = mp4_image_begin,
};

bool rsvc_mp4_open_tags(const char* path, int flags, rsvc_tags_t* tags, rsvc_done_t fail) {
    struct rsvc_mp4_tags mp4 = {
        .super = {
            .vptr   = &mp4_vptr,
            .flags  = flags,
        },
        .path = strdup(path),
    };
    mp4.file = taglib_file_new(mp4.path);
    if (!taglib_file_is_valid(mp4.file)) {
        free(mp4.path);
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid file handle");
        return false;
    }

    rsvc_mp4_tags_t copy = memdup(&mp4, sizeof(mp4));
    *tags = &copy->super;
    return true;
}
