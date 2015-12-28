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

#include <rsvc/mp4.h>

#include <Block.h>
#include <mp4v2/mp4v2.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "common.h"

typedef struct mp4_tag* mp4_tag_t;
typedef struct mp4_tag_type* mp4_tag_type_t;

struct mp4_tag_type {
    bool (*add)(MP4FileHandle* file, mp4_tag_t tag, const char* value, rsvc_done_t fail);
    bool (*remove)(MP4FileHandle* file, mp4_tag_t tag, rsvc_done_t fail);
};
struct mp4_tag {
    const char* vorbis_name;
    mp4_tag_type_t type;
    const char* mp4_code;
    const char* mp4_name;
};

static bool add_binary_tag(MP4FileHandle* file, mp4_tag_t tag,
                           const void* value, size_t value_size, int type_code,
                           rsvc_done_t fail) {
    // Check that the tag does not already exist.
    MP4ItmfItemList* items;
    if (tag->mp4_name) {
        items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", tag->mp4_name);
    } else {
        items = MP4ItmfGetItemsByCode(file, tag->mp4_code);
    }
    if (items->size > 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "only one mp4 %s tag permitted", tag->vorbis_name);
        return false;
    }
    MP4ItmfItemListFree(items);

    // Add the tag.
    MP4ItmfItem* item = MP4ItmfItemAlloc(tag->mp4_code, 1);
    if (tag->mp4_name) {
        item->mean = strdup("com.apple.iTunes");
        item->name = strdup(tag->mp4_name);
    }
    MP4ItmfData* data = &item->dataList.elements[0];
    data->typeSetIdentifier = 0;
    data->typeCode          = type_code;
    data->locale            = 0;
    data->value             = memdup(value, value_size);
    data->valueSize         = value_size;
    MP4ItmfAddItem(file, item);
    MP4ItmfItemFree(item);

    return true;
}

static bool add_string_tag(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                           rsvc_done_t fail) {
    return add_binary_tag(file, tag, value, strlen(value), MP4_ITMF_BT_UTF8, fail);
}

static bool add_date_tag(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                         rsvc_done_t fail) {
    bool valid = false;
    size_t size = strlen(value);
    if ((size == 4) || (size == 7) || (size == 10)) {
        valid = true;
        for (int i = 0; i < size; ++i) {
            if ((i == 4) || (i == 7)) {
                if (value[i] != '-') {
                    valid = false;
                    break;
                }
            } else {
                if ((value[i] < '0') || ('9' < value[i])) {
                    valid = false;
                    break;
                }
            }
        }
    }

    if (valid) {
        return add_binary_tag(file, tag, value, size, MP4_ITMF_BT_UTF8, fail);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
}

static bool add_isrc_tag(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                         rsvc_done_t fail) {
    bool valid = false;
    size_t size = strlen(value);
    if (size == 12) {
        valid = true;
        for (int i = 0; i < size; ++i) {
            char ch = value[i];
            if (i < 2) {
                if ((ch < 'A') || ('Z' < ch)) {
                    valid = false;
                    break;
                }
            } else if (i < 5) {
                if ((ch < '0') || (('9' < ch) && (ch < 'A')) || ('Z' < ch)) {
                    valid = false;
                    break;
                }
            } else {
                if ((ch < '0') || ('9' < ch)) {
                    valid = false;
                    break;
                }
            }
        }
    }

    if (valid) {
        return add_binary_tag(file, tag, value, size, MP4_ITMF_BT_ISRC, fail);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
}

static bool parse_number(const char* value, int64_t* number) {
    char* end;
    *number = strtol(value, &end, 10);
    if (*end) {
        return false;
    }
    return true;
}

static bool add_uint16_tag(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                           rsvc_done_t fail) {
    int64_t number;
    if (!parse_number(value, &number) || (number < 0) || (number >= UINT16_MAX)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
    uint8_t data[2] = {
        (number >> 8) & 0xff,
        number & 0xff,
    };
    return add_binary_tag(file, tag, data, 2, MP4_ITMF_BT_INTEGER, fail);
}

static bool add_boolean_tag(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                            rsvc_done_t fail) {
    if ((strlen(value) != 1) || (value[0] < '0') || (value[1] > '1')) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
    uint8_t data = value[0] - '0';
    return add_binary_tag(file, tag, &data, 1, MP4_ITMF_BT_INTEGER, fail);
}

static void read_trkn_or_disc(const uint8_t* bytes, size_t size,
                              uint16_t* number, uint16_t* total) {
    *number = 0;
    *total = 0;
    if (size < 2) {
        return;
    }
    bytes++;  // reserved1
    uint8_t reserved2 = *(bytes++);

    if (size < reserved2 + 2) {
        return;
    }
    bytes += reserved2;  // reserved3

    if (size < reserved2 + 6) {
        return;
    }
    *number |= *(bytes++);
    *number <<= 8;
    *number |= *(bytes++);
    *total |= *(bytes++);
    *total <<= 8;
    *total |= *(bytes++);
}

static void write_trkn_or_disc(uint8_t* bytes, size_t size, uint16_t number, uint16_t total) {
    if (size < 2) {
        return;
    }
    bytes++;  // reserved1
    uint8_t reserved2 = *(bytes++);

    if (size < reserved2 + 2) {
        return;
    }
    bytes += reserved2;  // reserved3

    if (size < reserved2 + 6) {
        return;
    }
    *(bytes++) = number >> 8;
    *(bytes++) = number;
    *(bytes++) = total >> 8;
    *(bytes++) = total;
}

static bool set_trkn_or_disc(MP4FileHandle* file, mp4_tag_t tag,
                             uint8_t* default_data, size_t default_size,
                             uint16_t* new_number, uint16_t* new_total,
                             rsvc_done_t fail) {
    bool result = true;
    MP4ItmfItemList* items = MP4ItmfGetItemsByCode(file, tag->mp4_code);
    if (items->size && items->elements[0].dataList.size) {
        MP4ItmfItem* item = &items->elements[0];
        MP4ItmfData* data = &item->dataList.elements[0];
        uint16_t number, total;
        read_trkn_or_disc(data->value, data->valueSize, &number, &total);
        number = new_number ? *new_number : number;
        total = new_total ? *new_total : total;
        if (number || total) {
            write_trkn_or_disc(data->value, data->valueSize, number, total);
            MP4ItmfSetItem(file, item);
        } else {
            MP4ItmfRemoveItem(file, item);
        }
    } else {
        uint8_t* value = default_data;
        size_t size = default_size;
        uint16_t number, total;
        number = new_number ? *new_number : 0;
        total = new_total ? *new_total : 0;
        if (number || total) {
            write_trkn_or_disc(value, size, number, total);
            result = add_binary_tag(file, tag, value, size, MP4_ITMF_BT_IMPLICIT, fail);
        }
    }
    MP4ItmfItemListFree(items);
    return result;
}

static bool add_track_number(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                             rsvc_done_t fail) {
    int64_t n;
    if (!parse_number(value, &n) || (n < 1) || (n >= UINT16_MAX)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
    uint16_t number = n;
    uint8_t default_data[8] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), &number, NULL, fail);
}

static bool add_track_total(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                             rsvc_done_t fail) {
    int64_t n;
    if (!parse_number(value, &n) || (n < 1) || (n >= UINT16_MAX)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
    uint16_t total = n;
    uint8_t default_data[8] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), NULL, &total, fail);
}

static bool add_disc_number(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                             rsvc_done_t fail) {
    int64_t n;
    if (!parse_number(value, &n) || (n < 1) || (n >= UINT16_MAX)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
    uint16_t number = n;
    uint8_t default_data[6] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), &number, NULL, fail);
}

static bool add_disc_total(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                             rsvc_done_t fail) {
    int64_t n;
    if (!parse_number(value, &n) || (n < 1) || (n >= UINT16_MAX)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
        return false;
    }
    uint16_t total = n;
    uint8_t default_data[6] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), NULL, &total, fail);
}

struct media_kind {
    const char* name;
    uint8_t value;
};
static struct media_kind kMediaKinds[] = {
    {"Home Video",  0},
    {"Music",       1},
    {"Audiobook",   2},
    {"Music Video", 6},
    {"Movie",       9},
    {"Short Film",  9},
    {"TV Show",     10},
    {"Booklet",     11},
    {"Ringtone",    14},
    {NULL}
};

static bool add_media_kind(MP4FileHandle* file, mp4_tag_t tag, const char* value,
                           rsvc_done_t fail) {
    for (struct media_kind* mk = kMediaKinds; mk->name; ++mk) {
        if (strcmp(mk->name, value) == 0) {
            return add_binary_tag(file, tag, &mk->value, 1, MP4_ITMF_BT_INTEGER, fail);
        }
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "invalid mp4 %s %s", tag->vorbis_name, value);
    return false;
}

static bool delete_tag(MP4FileHandle* file, mp4_tag_t tag,
                       rsvc_done_t fail) {
    MP4ItmfItemList* items = MP4ItmfGetItems(file);
    for (int i = 0; i < items->size; ++i) {
        MP4ItmfItem* item = &items->elements[i];
        if (strcmp(item->code, tag->mp4_code) != 0) {
            continue;
        } else if (tag->mp4_name
                && (!item->name || (strcmp(item->name, tag->mp4_name) != 0))) {
            continue;
        }
        MP4ItmfRemoveItem(file, item);
    }
    MP4ItmfItemListFree(items);
    return true;
}

static bool remove_track_number(MP4FileHandle* file, mp4_tag_t tag, rsvc_done_t fail) {
    uint16_t number = 0;
    uint8_t default_data[8] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), &number, NULL, fail);
}

static bool remove_track_total(MP4FileHandle* file, mp4_tag_t tag, rsvc_done_t fail) {
    uint16_t total = 0;
    uint8_t default_data[8] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), NULL, &total, fail);
}

static bool remove_disc_number(MP4FileHandle* file, mp4_tag_t tag, rsvc_done_t fail) {
    uint16_t number = 0;
    uint8_t default_data[8] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), &number, NULL, fail);
}

static bool remove_disc_total(MP4FileHandle* file, mp4_tag_t tag, rsvc_done_t fail) {
    uint16_t total = 0;
    uint8_t default_data[8] = {};
    return set_trkn_or_disc(file, tag, default_data, sizeof(default_data), NULL, &total, fail);
}

static struct mp4_tag_type mp4_string = {
    .add = add_string_tag,
    .remove = delete_tag,
};

static struct mp4_tag_type mp4_year = {
    .add = add_date_tag,
    .remove = delete_tag,
};

static struct mp4_tag_type mp4_isrc = {
    .add = add_isrc_tag,
    .remove = delete_tag,
};

static struct mp4_tag_type mp4_uint16 = {
    .add = add_uint16_tag,
    .remove = delete_tag,
};

static struct mp4_tag_type mp4_bool = {
    .add = add_boolean_tag,
    .remove = delete_tag,
};

static struct mp4_tag_type mp4_genre = {
    .add = NULL,
    .remove = delete_tag,
};

static struct mp4_tag_type mp4_tracknumber = {
    .add = add_track_number,
    .remove = remove_track_number,
};

static struct mp4_tag_type mp4_tracktotal = {
    .add = add_track_total,
    .remove = remove_track_total,
};

static struct mp4_tag_type mp4_discnumber = {
    .add = add_disc_number,
    .remove = remove_disc_number,
};

static struct mp4_tag_type mp4_disctotal = {
    .add = add_disc_total,
    .remove = remove_disc_total,
};

static struct mp4_tag_type mp4_media_kind = {
    .add = add_media_kind,
    .remove = delete_tag,
};

static struct mp4_tag mp4_tags[] = {
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
    {"BPM",                 &mp4_uint16,        "tmpo"},
    {"COMPILATION",         &mp4_bool,          "cpil"},
    {"GAPLESS",             &mp4_bool,          "pgap"},

    {"SHOW",                &mp4_string,        "tvsh"},
    {"SHOWSORT",            &mp4_string,        "sosn"},
    {"EPISODEID",           &mp4_string,        "tven"},
    {"EPISODENUMBER",       &mp4_uint16,        "tves"},
    {"EPISODETOTAL",        &mp4_string,        "----",     "EPISODETOTAL"},
    {"SEASONNUMBER",        &mp4_uint16,        "tvsn"},
    {"SEASONTOTAL",         &mp4_string,        "----",     "SEASONTOTAL"},

    {"ISRC",                &mp4_isrc,          "----",     "ISRC"},
    {"MCN",                 &mp4_string,        "----",     "MCN"},
    {"MUSICBRAINZ_DISCID",  &mp4_string,        "----",     "MusicBrainz Disc Id"},

    {"MEDIAKIND",           &mp4_media_kind,    "stik"},

    {},
};

struct rsvc_mp4_tags {
    struct rsvc_tags    super;
    char*               path;
    MP4FileHandle       file;
};
typedef struct rsvc_mp4_tags* rsvc_mp4_tags_t;

static bool rsvc_mp4_tags_remove(rsvc_tags_t tags, const char* name,
                                 rsvc_done_t fail) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    if (name) {
        for (mp4_tag_t tag = mp4_tags; tag->vorbis_name; ++tag) {
            if ((strcmp(tag->vorbis_name, name) != 0)) {
                continue;
            }
            if (!tag->type->remove(self->file, tag, fail)) {
                return false;
            }
        }
        return true;
    } else {
        MP4ItmfItemList* items = MP4ItmfGetItems(self->file);
        for (int i = 0; i < items->size; ++i) {
            MP4ItmfItem* item = &items->elements[i];
            MP4ItmfRemoveItem(self->file, item);
        }
        MP4ItmfItemListFree(items);
    }
    return true;
}

static bool rsvc_mp4_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                              rsvc_done_t fail) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    for (mp4_tag_t tag = mp4_tags; tag->vorbis_name; ++tag) {
        if (strcmp(tag->vorbis_name, name) != 0) {
            continue;
        }
        return tag->type->add(self->file, tag, value, fail);
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "no mp4 tag %s", name);
    return false;
}

static bool rsvc_mp4_tags_each(rsvc_tags_t tags,
                         void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    __block bool loop = true;
    rsvc_stop_t stop = ^{
        loop = false;
    };

    MP4ItmfItemList* items = MP4ItmfGetItems(self->file);
    for (uint32_t i = 0; loop && (i < items->size); ++i) {
        MP4ItmfItem* item = &items->elements[i];
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
                    uint16_t total = 0;
                    if ((tag->type == &mp4_tracknumber) || (tag->type == &mp4_discnumber)) {
                        read_trkn_or_disc(data->value, data->valueSize, &number, &total);
                    } else if ((tag->type == &mp4_tracktotal) || (tag->type == &mp4_disctotal)) {
                        read_trkn_or_disc(data->value, data->valueSize, &number, &total);
                        number = total;
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
                    if (tag->type == &mp4_media_kind) {
                        for (struct media_kind* mk = kMediaKinds; mk->name; ++mk) {
                            if (mk->value == number) {
                                block(tag->vorbis_name, mk->name, stop);
                                goto skip;
                            }
                        }
                    }
                    char value[16];
                    sprintf(value, "%lld", (long long)number);
                    block(tag->vorbis_name, value, stop);
                }
skip:
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
    MP4ItmfItemListFree(items);
    return loop;
}

static bool rsvc_mp4_tags_image_each(
        rsvc_tags_t tags,
        void (^block)(rsvc_format_t format, const uint8_t* data, size_t size, rsvc_stop_t stop)) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    __block bool loop = true;
    rsvc_stop_t stop = ^{
        loop = false;
    };

    MP4ItmfItemList* items = MP4ItmfGetItems(self->file);
    for (uint32_t i = 0; loop && (i < items->size); ++i) {
        MP4ItmfItem* item = &items->elements[i];
        if ((item->dataList.size == 0)
                || (strcmp(item->code, "covr") != 0)) {
            continue;
        }
        MP4ItmfData* data = &item->dataList.elements[0];
        rsvc_format_t format = NULL;
        switch (data->typeCode) {
            case MP4_ITMF_BT_GIF:
                format = rsvc_format_named("gif", 0);
                break;
            case MP4_ITMF_BT_JPEG:
                format = rsvc_format_named("jpeg", 0);
                break;
            case MP4_ITMF_BT_PNG:
                format = rsvc_format_named("png", 0);
                break;
            case MP4_ITMF_BT_BMP:
                format = rsvc_format_named("bmp", 0);
                break;
            default:
                continue;
        }
        if (format) {
            block(format, data->value, data->valueSize, stop);
        }
    }
    MP4ItmfItemListFree(items);
    return loop;
}

static bool rsvc_mp4_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    // The mp4v2 C API doesn't have a way to say "clean up this file
    // without saving its contents to disk", so rsvc_mp4_open_tags
    // always opens the file in read-only mode.  To save the file,
    // we reopen it in read-write mode, and copy over the tags.
    MP4FileHandle* file = MP4Modify(self->path, 0);
    if (file == MP4_INVALID_FILE_HANDLE) {
        rsvc_errorf(fail, __FILE__, __LINE__, "MP4_INVALID_FILE_HANDLE");
        return false;
    }

    // Clear existing tags from the file.
    MP4ItmfItemList* dst_items = MP4ItmfGetItems(file);
    for (int i = 0; i < dst_items->size; ++i) {
        MP4ItmfItem* item = &dst_items->elements[i];
        MP4ItmfRemoveItem(file, item);
    }
    MP4ItmfItemListFree(dst_items);

    // Copy tags from self->file.
    MP4ItmfItemList* src_items = MP4ItmfGetItems(self->file);
    for (uint32_t i = 0; i < src_items->size; ++i) {
        const MP4ItmfItem* src_item = &src_items->elements[i];
        MP4ItmfItem* dst_item = MP4ItmfItemAlloc(src_item->code, src_item->dataList.size);
        if (src_item->mean) {
            dst_item->mean = strdup(src_item->mean);
            dst_item->name = strdup(src_item->name);
        }
        for (uint32_t j = 0; j < src_item->dataList.size; ++j) {
            const MP4ItmfData* src_data = &src_item->dataList.elements[0];
            MP4ItmfData* dst_data = &dst_item->dataList.elements[0];
            dst_data->typeSetIdentifier = src_data->typeSetIdentifier;
            dst_data->typeCode          = src_data->typeCode;
            dst_data->locale            = src_data->locale;
            dst_data->value             = memdup(src_data->value, src_data->valueSize);
            dst_data->valueSize         = src_data->valueSize;
        }
        MP4ItmfAddItem(file, dst_item);
        MP4ItmfItemFree(dst_item);
    }
    MP4ItmfItemListFree(src_items);

    MP4Close(file, MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
    return true;
}

static void rsvc_mp4_tags_destroy(rsvc_tags_t tags) {
    rsvc_mp4_tags_t self = DOWN_CAST(struct rsvc_mp4_tags, tags);
    MP4Close(self->file, MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
    free(self->path);
    free(self);
}

static struct rsvc_tags_methods mp4_vptr = {
    .remove = rsvc_mp4_tags_remove,
    .add = rsvc_mp4_tags_add,
    .each = rsvc_mp4_tags_each,
    .image_each = rsvc_mp4_tags_image_each,
    .save = rsvc_mp4_tags_save,
    .destroy = rsvc_mp4_tags_destroy,
};

bool rsvc_mp4_open_tags(const char* path, int flags, rsvc_tags_t* tags, rsvc_done_t fail) {
    struct rsvc_mp4_tags mp4 = {
        .super = {
            .vptr   = &mp4_vptr,
            .flags  = flags,
        },
        .path = strdup(path),
    };
    mp4.file = MP4Read(mp4.path);
    if (mp4.file == MP4_INVALID_FILE_HANDLE) {
        free(mp4.path);
        rsvc_errorf(fail, __FILE__, __LINE__, "MP4_INVALID_FILE_HANDLE");
        return false;
    }

    rsvc_mp4_tags_t copy = memdup(&mp4, sizeof(mp4));
    *tags = &copy->super;
    return true;
}

void rsvc_mp4_format_register() {
    struct rsvc_format m4a = {
        .name = "mp4",
        .mime = "audio/mp4",
        .magic = "????ftypM4A ",
        .magic_size = 12,
        .extension = "m4a",
        .open_tags = rsvc_mp4_open_tags,
    };
    struct rsvc_format m4v = {
        .name = "mp4",
        .mime = "audio/mp4",
        .magic = "????ftypM4V ",
        .magic_size = 12,
        .extension = "m4a",
        .open_tags = rsvc_mp4_open_tags,
    };
    struct rsvc_format mp42 = {
        .name = "mp4",
        .mime = "audio/mp4",
        .magic = "????ftypmp42",
        .magic_size = 12,
        .extension = "m4v",
        .open_tags = rsvc_mp4_open_tags,
    };
    rsvc_format_register(&m4a);
    rsvc_format_register(&m4v);
    rsvc_format_register(&mp42);
}
