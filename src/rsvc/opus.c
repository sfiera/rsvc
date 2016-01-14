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
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <rsvc/format.h>

#include "common.h"
#include "ogg.h"
#include "unix.h"

typedef struct opus_tags* opus_tags_t;
struct opus_tags {
    struct rsvc_tags  super;

    char*             path;
    FILE*             file;
    off_t             data_offset;

    uint32_t          serial;
    OpusHead          head;
    OpusTags          tags;
};

static bool rsvc_opus_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                          rsvc_done_t fail) {
    opus_tags_t self = DOWN_CAST(struct opus_tags, tags);
    opus_tags_add(&self->tags, name, value);
    (void)fail;
    return true;
}

static bool rsvc_opus_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    opus_tags_t self = DOWN_CAST(struct opus_tags, tags);
    const size_t name_size = strlen(name);
    int o = 0;
    if (name) {
        for (int i = 0; i < self->tags.comments; ++i) {
            char* comment = self->tags.user_comments[i];
            int comment_length = self->tags.comment_lengths[i];
            if ((strncmp(name, comment, name_size) != 0) || (comment[name_size] != '=')) {
                self->tags.user_comments[i] = self->tags.user_comments[o];
                self->tags.comment_lengths[i] = self->tags.comment_lengths[o];
                self->tags.user_comments[o] = comment;
                self->tags.comment_lengths[o] = comment_length;
                ++o;
            }
        }
    }
    for (int i = o; i < self->tags.comments; ++i) {
        free(self->tags.user_comments[i]);
    }
    self->tags.comments = o;
    (void)fail;
    return true;
}

static size_t memcat(unsigned char** p, void* data, size_t len) {
    if (*p) {
        memcpy(*p, data, len);
        *p += len;
    }
    return len;
}

static size_t u1le(unsigned char** p, uint8_t u) {
    if (*p) {
        *((*p)++) = 0xff & (u >> 0);
    }
    return 1;
}

static size_t u2le(unsigned char** p, uint16_t u) {
    if (*p) {
        *((*p)++) = 0xff & (u >> 0);
        *((*p)++) = 0xff & (u >> 8);
    }
    return 2;
}

static size_t u4le(unsigned char** p, uint32_t u) {
    if (*p) {
        *((*p)++) = 0xff & (u >> 0);
        *((*p)++) = 0xff & (u >> 8);
        *((*p)++) = 0xff & (u >> 16);
        *((*p)++) = 0xff & (u >> 24);
    }
    return 4;
}

static void rsvc_opus_head_out(OpusHead* head, ogg_packet* op) {
    unsigned char data[29];
    op->bytes = 0;

    unsigned char* p = data;
    op->bytes += memcat(&p, "OpusHead", 8);
    op->bytes += u1le(&p, head->version);
    op->bytes += u1le(&p, head->channel_count);
    op->bytes += u2le(&p, head->pre_skip);
    op->bytes += u4le(&p, head->input_sample_rate);
    op->bytes += u2le(&p, head->output_gain);
    op->bytes += u1le(&p, head->mapping_family);

    if (head->mapping_family == 1) {
        op->bytes += u1le(&p, head->stream_count);
        op->bytes += u1le(&p, head->coupled_count);
        int n = (head->channel_count <= 8) ? head->channel_count : 8;
        for (int i = 0; i < n; ++i) {
            op->bytes += u1le(&p, head->mapping[i]);
        }
    }

    op->packet = memdup(data, op->bytes);
    op->b_o_s = 1;
    op->e_o_s = 0;
    op->packetno = 0;
    op->granulepos = 0;
}

size_t write_opus_tags(unsigned char* dst, OpusTags* tags) {
    size_t size = 0;
    unsigned char* p = dst;
    size += memcat(&p, "OpusTags", 8);
    size += u4le(&p, strlen(RSVC_VERSION));
    size += memcat(&p, RSVC_VERSION, strlen(RSVC_VERSION));
    size += u4le(&p, tags->comments);
    for (int i = 0; i < tags->comments; ++i) {
        size += u4le(&p, tags->comment_lengths[i]);
        size += memcat(&p, tags->user_comments[i], tags->comment_lengths[i]);
    }
    return size;
}

static void rsvc_opus_tags_out(OpusTags* tags, ogg_packet* op) {
    op->bytes = write_opus_tags(NULL, tags);
    op->packet = malloc(op->bytes);
    write_opus_tags(op->packet, tags);
    op->b_o_s = op->e_o_s = 0;
    op->packetno = 1;
    op->granulepos = 0;
}

static bool rsvc_opus_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    opus_tags_t self = DOWN_CAST(struct opus_tags, tags);

    FILE* file;
    char tmp_path[MAXPATHLEN];
    if (!rsvc_temp(self->path, tmp_path, &file, fail)) {
        return false;
    }

    ogg_page          og;
    ogg_stream_state  os;
    ogg_stream_init(&os, self->serial);

    ogg_packet op_head;
    rsvc_opus_head_out(&self->head, &op_head);
    ogg_packet op_tags;
    rsvc_opus_tags_out(&self->tags, &op_tags);
    if (!(rsvc_ogg_align_packet(file, &os, &og, &op_head, fail) &&
          rsvc_ogg_align_packet(file, &os, &og, &op_tags, fail))) {
        // TODO(sfiera): cleanup
        return false;
    }

    if (lseek(fileno(self->file), self->data_offset, SEEK_SET) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }

    rsvc_logf(2, "copying opus data from %s to %s", self->path, tmp_path);
    uint8_t buffer[4096];
    bool eof = false;
    while (!eof) {
        size_t size;
        if (!(rsvc_read(self->path, fileno(self->file), buffer, 4096, &size, &eof, fail) &&
              (eof || rsvc_write(tmp_path, fileno(file), buffer, size, NULL, NULL, fail)))) {
            return false;
        }
    }

    if (!rsvc_refile(tmp_path, self->path, fail)) {
        return false;
    }
    return true;
}

void rsvc_opus_tags_clear(opus_tags_t self) {
    free(self->path);
    fclose(self->file);
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

    if (!rsvc_open(opus.path, O_RDONLY, 0644, &opus.file, fail)) {
        return false;
    }
    fail = ^(rsvc_error_t error){
        fclose(opus.file);
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
    while (packetno < 2) {
        bool eof;
        if (!rsvc_ogg_page_read(opus.file, &oy, &og, &eof, fail)) {
            return false;
        } else if (eof) {
            if (packetno < 2) {
                rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of file");
                return false;
            }
            break;
        }
        opus.data_offset += og.header_len + og.body_len;

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
    }

    ogg_sync_clear(&oy);
    ogg_stream_clear(&os);

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
