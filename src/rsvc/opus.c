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

#include <opus.h>
#include <opusfile.h>
#include <string.h>
#include <rsvc/format.h>

#include "common.h"
#include "ogg.h"
#include "unix.h"

typedef struct opus_tags* opus_tags_t;
struct opus_tags {
    struct rsvc_tags  super;
    char*             path;
    uint32_t          serial;
    OpusHead          head;
    OpusTags          tags;
};

static bool rsvc_opus_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                          rsvc_done_t fail) {
    (void)tags;
    (void)name;
    (void)value;
    rsvc_errorf(fail, __FILE__, __LINE__, "opus tags are read-only");
    return false;
}

static bool rsvc_opus_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    (void)tags;
    (void)name;
    rsvc_errorf(fail, __FILE__, __LINE__, "opus tags are read-only");
    return false;
}

static bool rsvc_opus_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    (void)tags;
    (void)fail;
    return true;
}

void rsvc_opus_tags_clear(opus_tags_t self) {
    free(self->path);
}

static void rsvc_opus_tags_destroy(rsvc_tags_t tags) {
    opus_tags_t self = DOWN_CAST(struct opus_tags, tags);
    rsvc_opus_tags_clear(self);
    free(self);
}

typedef struct opus_tags_iter* opus_tags_iter_t;
struct opus_tags_iter {
    struct rsvc_tags_iter  super;
    OpusTags*              tags;
    int                    i;
    char*                  storage;
};

static void rsvc_opus_break(rsvc_iter_t super_it) {
    opus_tags_iter_t it = DOWN_CAST(struct opus_tags_iter, (rsvc_tags_iter_t)super_it);
    if (it->storage) {
        free(it->storage);
    }
    free(it);
}

static bool rsvc_opus_next(rsvc_iter_t super_it) {
    opus_tags_iter_t it = DOWN_CAST(struct opus_tags_iter, (rsvc_tags_iter_t)super_it);
    while (it->i < it->tags->comments) {
        if (it->storage) {
            free(it->storage);
        }
        it->storage = strdup(it->tags->user_comments[it->i++]);
        char* eq = strchr(it->storage, '=');
        if (eq) {
            *eq = '\0';
            it->super.name = it->storage;
            it->super.value = eq + 1;
            return true;
        }
        // else TODO(sfiera): report failure.
    }
    rsvc_opus_break(super_it);
    return false;
}

static struct rsvc_iter_methods opus_iter_vptr = {
    .next    = rsvc_opus_next,
    .break_  = rsvc_opus_break,
};

static rsvc_tags_iter_t rsvc_opus_begin(rsvc_tags_t tags) {
    opus_tags_t self = DOWN_CAST(struct opus_tags, tags);
    struct opus_tags_iter iter = {
        .super = {
            .vptr = &opus_iter_vptr,
        },
        .tags  = &self->tags,
        .i     = 0,
    };
    opus_tags_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static struct rsvc_tags_methods opus_vptr = {
    .remove      = rsvc_opus_tags_remove,
    .add         = rsvc_opus_tags_add,
    .save        = rsvc_opus_tags_save,
    .destroy     = rsvc_opus_tags_destroy,

    .iter_begin  = rsvc_opus_begin,
};

bool rsvc_opus_open_tags(const char* path, int flags, rsvc_tags_t* tags, rsvc_done_t fail) {
    __block struct opus_tags opus = {
        .super = {
            .vptr   = &opus_vptr,
            .flags  = flags,
        },
        .path = strdup(path),
    };
    fail = ^(rsvc_error_t error){
        rsvc_opus_tags_clear(&opus);
        fail(error);
    };

    int fd;
    if (!rsvc_open(opus.path, O_RDONLY, 0644, &fd, fail)) {
        return false;
    }
    fail = ^(rsvc_error_t error){
        close(fd);
        fail(error);
    };

    __block ogg_sync_state      oy;
    __block ogg_stream_state    os;
    ogg_page                    og;
    ogg_packet                  op;

    ogg_sync_init(&oy);

    fail = ^(rsvc_error_t error){
        ogg_sync_clear(&oy);
        ogg_stream_clear(&os);
        fail(error);
    };

    int packetno  = 0;
    int pageno    = 0;

    // Read all of the pages from the ogg file in a loop.
    while (true) {
        bool eof;
        if (!rsvc_ogg_page_read(fd, &oy, &og, &eof, fail)) {
            return false;
        } else if (eof) {
            if (packetno < 2) {
                rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of file");
                return false;
            }
            break;
        }

        if (pageno++ == 0) {
            // First page must be a bos page.
            if (!ogg_page_bos(&og)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "ogg file does not begin with bos page");
                return false;
            } else if (ogg_stream_init(&os, ogg_page_serialno(&og)) < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "couldn't initialize ogg stream");
                return false;
            }
            opus.serial = ogg_page_serialno(&og);
        } else {
            // Subsequent pages must not be bos pages, and must have
            // the same serial as the first page.
            if (ogg_page_bos(&og) || (ogg_page_serialno(&og) != opus.serial)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "multiplexed ogg files not supported");
                return false;
            }
        }

        if (packetno < 2) {
            if (ogg_stream_pagein(&os, &og) < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
                return false;
            }

            while (true) {
                int packetout_status = ogg_stream_packetout(&os, &op);
                if (packetout_status == 0) {
                    break;
                } else if (packetout_status < 0) {
                    rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg packet");
                    return false;
                }

                switch (packetno++) {
                    case 0:  // OpusHead
                        if (opus_head_parse(&opus.head, op.packet, op.bytes) != 0) {
                            rsvc_errorf(fail, __FILE__, __LINE__, "bad opus header");
                            return false;
                        }
                        break;
                    case 1:  // OpusTags
                        if (opus_tags_parse(&opus.tags, op.packet, op.bytes) != 0) {
                            rsvc_errorf(fail, __FILE__, __LINE__, "bad opus tags");
                            return false;
                        }
                        break;
                    default:
                        rsvc_errorf(fail, __FILE__, __LINE__, "extra packets in header pages");
                        return false;
                }
            }
        } else {
            rsvc_logf(2, "read ogg page (header_len=%lu, body_len=%lu)",
                      og.header_len, og.body_len);
        }
    }

    ogg_sync_clear(&oy);
    ogg_stream_clear(&os);
    close(fd);

    opus_tags_t copy = memdup(&opus, sizeof(opus));
    *tags = &copy->super;
    return true;
}

const struct rsvc_format rsvc_opus = {
    .format_group = RSVC_AUDIO,
    .name = "opus",
    .mime = "audio/opus",
    .magic = {"OggS????????????????????????OpusHead"},
    .magic_size = 36,
    .extension = "opus",
    .lossless = false,
    .open_tags = rsvc_opus_open_tags,
};
