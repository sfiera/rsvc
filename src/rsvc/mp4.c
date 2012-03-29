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

#include <rsvc/mp4.h>

#include <Block.h>
#include <mp4v2/mp4v2.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

static void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}

struct mp4_tag_type {
};
typedef struct mp4_tag_type* mp4_tag_type_t;

struct mp4_tag_type mp4_string = {};
struct mp4_tag_type mp4_year = {};
struct mp4_tag_type mp4_isrc = {};
struct mp4_tag_type mp4_number = {};
struct mp4_tag_type mp4_bool = {};
struct mp4_tag_type mp4_genre = {};
struct mp4_tag_type mp4_tracknumber = {};
struct mp4_tag_type mp4_tracktotal = {};
struct mp4_tag_type mp4_discnumber = {};
struct mp4_tag_type mp4_disctotal = {};

struct mp4_tag {
    const char* vorbis_name;
    mp4_tag_type_t type;
    const char* mp4_code;
    const char* mp4_name;
};
typedef struct mp4_tag* mp4_tag_t;

struct mp4_tag mp4_tags[] = {
    {"TITLE",               &mp4_string,        "\251nam"},
    {"TITLESORT",           &mp4_string,        "sonm"},
    {"ARTIST",              &mp4_string,        "\251ART"},
    {"ARTISTSORT",          &mp4_string,        "soar"},
    {"ALBUMARTIST",         &mp4_string,        "aART"},
    {"ALBUMARTISTSORT",     &mp4_string,        "soaa"},
    {"ALBUM",               &mp4_string,        "\251alb"},
    {"ALBUMSORT",           &mp4_string,        "soal"},
    {"COMPOSER",            &mp4_string,        "\251wrt"},
    {"COMPOSERSORT",        &mp4_string,        "soco"},
    {"GENRE",               &mp4_string,        "\251gen"},
    {"SUBTITLE",            &mp4_string,        "\251st3"},
    {"GROUPING",            &mp4_string,        "\251grp"},
    {"DESCRIPTION",         &mp4_string,        "desc"},
    {"COMMENTS",            &mp4_string,        "\251cmt"},
    {"ENCODER",             &mp4_string,        "\251too"},
    {"ENCODEDBY",           &mp4_string,        "\251enc"},
    {"COPYRIGHT",           &mp4_string,        "cprt"},

    {"DATE",                &mp4_year,          "\251day"},
    {"GENRE",               &mp4_genre,         "gnre"},
    {"TRACKNUMBER",         &mp4_tracknumber,   "trkn"},
    {"TRACKTOTAL",          &mp4_tracktotal,    "trkn"},
    {"DISCNUMBER",          &mp4_discnumber,    "disk"},
    {"DISCTOTAL",           &mp4_disctotal,     "disk"},
    {"BPM",                 &mp4_number,        "tmpo"},
    {"COMPILATION",         &mp4_bool,          "cpil"},
    {"GAPLESS",             &mp4_bool,          "pgap"},

    {"ISRC",                &mp4_isrc,          "----",     "ISRC"},
    {"MCN",                 &mp4_string,        "----",     "MCN"},
    {"MUSICBRAINZ_DISCID",  &mp4_string,        "----",     "MusicBrainz Disc Id"},

    {},
};

#define DOWN_CAST(TYPE, PTR) \
    ((TYPE*)((void*)PTR - offsetof(TYPE, super)))

struct rsvc_mp4_tags {
    struct rsvc_tags super;
    MP4FileHandle file;
    MP4ItmfItemList* tags;
};
typedef struct rsvc_mp4_tags* rsvc_mp4_tags_t;

static bool rsvc_mp4_tags_remove(rsvc_tags_t tags, const char* name,
                                 void (^fail)(rsvc_error_t error)) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    (void)self;
    rsvc_errorf(fail, __FILE__, __LINE__, "cannot modify MP4 tags");
    return false;
}

static bool rsvc_mp4_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                              void (^fail)(rsvc_error_t error)) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    (void)self;
    rsvc_errorf(fail, __FILE__, __LINE__, "cannot modify MP4 tags");
    return false;
}

static bool rsvc_mp4_tags_each(rsvc_tags_t tags,
                         void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    __block bool loop = true;
    rsvc_stop_t stop = ^{
        loop = false;
    };

    for (uint32_t i = 0; loop && (i < self->tags->size); ++i) {
        MP4ItmfItem* item = &self->tags->elements[i];
        for (mp4_tag_t tag = mp4_tags; tag->vorbis_name; ++tag) {
            if ((strcmp(item->code, tag->mp4_code) != 0)
                    || (item->dataList.size == 0)) {
                continue;
            } else if (tag->mp4_name
                    && (!item->name || (strcmp(item->name, tag->mp4_name) != 0))) {
                continue;
            }
            MP4ItmfData* data = &item->dataList.elements[0];
            switch (data->typeCode) {
                case MP4_ITMF_BT_IMPLICIT: {
                    uint16_t number = 0;
                    if ((tag->type == &mp4_tracknumber) || (tag->type == &mp4_discnumber)) {
                        if (data->valueSize >= 6) {
                            number = (data->value[2] << 8) | data->value[3];
                        }
                    } else if ((tag->type == &mp4_tracktotal) || (tag->type == &mp4_disctotal)) {
                        if (data->valueSize >= 6) {
                            number = (data->value[4] << 8) | data->value[5];
                        }
                    } else if (tag->type == &mp4_genre) {
                        if (data->valueSize == 2) {
                            number = ((data->value[0] << 8) | data->value[1]) - 1;
                        }
                    }
                    if (number) {
                        char value[16];
                        sprintf(value, "%hu", number);
                        block(tag->vorbis_name, value, stop);
                    }
                }
                break;

                case MP4_ITMF_BT_UTF8:
                case MP4_ITMF_BT_ISRC:
                case MP4_ITMF_BT_MI3P:
                case MP4_ITMF_BT_URL:
                case MP4_ITMF_BT_UPC: {
                    char* value = strndup((char*)data->value, data->valueSize);
                    block(tag->vorbis_name, value, stop);
                    free(value);
                }
                break;

                case MP4_ITMF_BT_UTF16:
                case MP4_ITMF_BT_SJIS: {
                    // Unsupported encoding.
                }
                break;

                case MP4_ITMF_BT_UUID:
                case MP4_ITMF_BT_DURATION:
                case MP4_ITMF_BT_DATETIME:
                case MP4_ITMF_BT_INTEGER:
                case MP4_ITMF_BT_RIAA_PA: {
                    int64_t number = 0;
                    if (data->value[0] & 0x80) {
                        number = -1;
                    }
                    for (int i = 0; i < data->valueSize; ++i) {
                        number = ((number & 0x0fffffffffffffffLL) << 8) | data->value[i];
                    }
                    char value[16];
                    sprintf(value, "%lld", number);
                    block(tag->vorbis_name, value, stop);
                }
                break;

                case MP4_ITMF_BT_HTML:
                case MP4_ITMF_BT_XML:
                case MP4_ITMF_BT_GIF:
                case MP4_ITMF_BT_JPEG:
                case MP4_ITMF_BT_PNG:
                case MP4_ITMF_BT_GENRES:
                case MP4_ITMF_BT_BMP:
                case MP4_ITMF_BT_UNDEFINED:
                break;
            }
        }
    }
    return loop;
}

static void rsvc_mp4_tags_save(rsvc_tags_t tags, void (^done)(rsvc_error_t)) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        done(NULL);
    });
}

static void rsvc_mp4_tags_destroy(rsvc_tags_t tags) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    MP4ItmfItemListFree(self->tags);
    MP4Close(self->file, MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
    free(self);
}

static struct rsvc_tags_methods mp4_vptr = {
    .remove = rsvc_mp4_tags_remove,
    .add = rsvc_mp4_tags_add,
    .each = rsvc_mp4_tags_each,
    .save = rsvc_mp4_tags_save,
    .destroy = rsvc_mp4_tags_destroy,
};

void rsvc_mp4_read_tags(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t)) {
    done = Block_copy(done);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        struct rsvc_mp4_tags tags = {
            .super = {
                .vptr   = &mp4_vptr,
            },
            .file = MP4Modify(path, 0),
        };
        if (tags.file == MP4_INVALID_FILE_HANDLE) {
            rsvc_errorf(^(rsvc_error_t error){
                done(NULL, error);
            }, __FILE__, __LINE__, "MP4_INVALID_FILE_HANDLE");
            goto cleanup;
        }
        tags.tags = MP4ItmfGetItems(tags.file);
        rsvc_mp4_tags_t copy = memdup(&tags, sizeof(tags));
        done(&copy->super, NULL);
        return;
cleanup:
        ;
    });
}
