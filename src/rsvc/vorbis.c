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

#include <rsvc/vorbis.h>

#include <Block.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <rsvc/format.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vorbis/vorbisenc.h>
#include <sys/param.h>

#include "common.h"

void rsvc_vorbis_encode(int read_fd, int write_fd, size_t samples_per_channel,
                        int bitrate,
                        rsvc_encode_progress_t progress, rsvc_done_t done) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Largely cribbed from libvorbis's examples/encoder_example.c.
        // Some comments there imply that it is doing things a certain
        // way for exposition, and that normally different practices
        // would be followed, but it's perfectly functional.
        ogg_stream_state    os;
        ogg_page            og;
        ogg_packet          op;
        vorbis_info         vi;
        vorbis_comment      vc;
        vorbis_dsp_state    vd;
        vorbis_block        vb;
        int                 ret;
        size_t              samples_per_channel_read = 0;

        // Initialize the encoder.
        vorbis_info_init(&vi);
        ret = vorbis_encode_init(&vi, 2, 44100, -1, bitrate, -1);
        if (ret != 0 ) {
            rsvc_errorf(done, __FILE__, __LINE__, "couldn't init vorbis encoder");
            return;
        }
        vorbis_analysis_init(&vd, &vi);
        vorbis_block_init(&vd, &vb);

        // Pick a random-ish serial number.  rand_r() is thread-safe,
        // and we don't need strong guarantees about randomness, so this
        // is probably sufficient.
        unsigned seed = time(NULL);
        ogg_stream_init(&os, rand_r(&seed));

        // Copy `tags` into `vc`.
        vorbis_comment_init(&vc);
        // vorbis_comment* vcp = &vc;
        // rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        //     vorbis_comment_add_tag(vcp, name, value);
        // });

        {
            ogg_packet header;
            ogg_packet header_comm;
            ogg_packet header_code;

            vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
            ogg_stream_packetin(&os, &header);
            ogg_stream_packetin(&os, &header_comm);
            ogg_stream_packetin(&os, &header_code);

            while (true) {
                int result = ogg_stream_flush(&os, &og);
                if (result == 0) {
                    break;
                }
                write(write_fd, og.header, og.header_len);
                write(write_fd, og.body, og.body_len);
            }
        }

        bool eos = false;
        size_t start = 0;
        uint8_t in[1024];
        while (!eos) {
            ssize_t size = read(read_fd, in, sizeof(in));
            if (size < 0) {
                rsvc_strerrorf(done, __FILE__, __LINE__, "pipe");
                return;
            } else if (size == 0) {
                vorbis_analysis_wrote(&vd, 0);
            } else {
                size += start;
                size_t remainder = size % 4;
                size -= remainder;
                size_t nsamples = size / 4;
                start = remainder;
                if (nsamples == 0) {
                    continue;
                }
                samples_per_channel_read += nsamples;
                float** out = vorbis_analysis_buffer(&vd, sizeof(in));
                float* channels[] = {out[0], out[1]};
                int16_t* in16 = (int16_t*)in;
                for (int16_t* p = in16; p < in16 + (nsamples * 2); p += 2) {
                    *(channels[0]++) = p[0] / 32768.f;
                    *(channels[1]++) = p[1] / 32768.f;
                }
                vorbis_analysis_wrote(&vd, size / 4);
                memcpy(in, in + size, start);
            }

            while (vorbis_analysis_blockout(&vd, &vb) == 1) {
                vorbis_analysis(&vb, NULL);
                vorbis_bitrate_addblock(&vb);
                while (vorbis_bitrate_flushpacket(&vd, &op)) {
                    ogg_stream_packetin(&os, &op);
                    while (!eos) {
                        int result = ogg_stream_pageout(&os, &og);
                        if (result == 0) {
                            break;
                        }
                        write(write_fd, og.header, og.header_len);
                        write(write_fd, og.body, og.body_len);
                        if (ogg_page_eos(&og)) {
                            eos = true;
                        }
                    }
                }
            }
            progress(samples_per_channel_read * 1.0 / samples_per_channel);
        }

        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);

        done(NULL);
    });
}

typedef struct rsvc_ogg_page_node* rsvc_ogg_page_node_t;
struct rsvc_ogg_page_node {
    ogg_page                page;
    rsvc_ogg_page_node_t    next;
};

typedef struct rsvc_vorbis_tags* rsvc_vorbis_tags_t;
struct rsvc_vorbis_tags {
    struct rsvc_tags        super;
    char*                   path;
    uint32_t                serial;
    vorbis_comment          vc;

    ogg_packet              header;
    ogg_packet              header_code;
    rsvc_ogg_page_node_t    page_head;
    rsvc_ogg_page_node_t    page_tail;
};

static void rsvc_ogg_packet_clear(ogg_packet* op) {
    if (op->packet) {
        free(op->packet);
    }
    memset(op, 0, sizeof(*op));
}

static void rsvc_ogg_packet_copy(ogg_packet* dst, const ogg_packet* src) {
    rsvc_ogg_packet_clear(dst);
    dst->packet         = memdup(src->packet, src->bytes);
    dst->bytes          = src->bytes;
    dst->b_o_s          = src->b_o_s;
    dst->e_o_s          = src->e_o_s;
    dst->granulepos     = src->granulepos;
    dst->packetno       = src->packetno;
}

static void rsvc_ogg_page_clear(ogg_page* og) {
    if (og->header) {
        free(og->header);
    }
    if (og->body) {
        free(og->body);
    }
    memset(og, 0, sizeof(*og));
}

static void rsvc_ogg_page_copy(ogg_page* dst, const ogg_page* src) {
    rsvc_ogg_page_clear(dst);
    dst->header         = memdup(src->header, src->header_len);
    dst->header_len     = src->header_len;
    dst->body           = memdup(src->body, src->body_len);
    dst->body_len       = src->body_len;
}

// Copies `op` and appends it to the list of pages in `tags`.
static void rsvc_ogg_page_push(rsvc_vorbis_tags_t tags, ogg_page* og) {
    struct rsvc_ogg_page_node node = {
        .page = {},
        .next = NULL,
    };
    rsvc_ogg_page_copy(&node.page, og);
    if (tags->page_head) {
        tags->page_tail->next = memdup(&node, sizeof(node));
        tags->page_tail = tags->page_tail->next;
    } else {
        tags->page_head = memdup(&node, sizeof(node));
        tags->page_tail = tags->page_head;
    }
}

static bool rsvc_vorbis_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    // I bet this is not the most efficient thing to do.
    vorbis_comment vc;
    vorbis_comment_init(&vc);
    size_t size = strlen(name);
    for (int i = 0; i < self->vc.comments; ++i) {
        const char* comment = self->vc.user_comments[i];
        if ((strncmp(name, comment, size) != 0) && (comment[size] != '=')) {
            vorbis_comment_add(&vc, comment);
        }
    }
    vorbis_comment_clear(&self->vc);
    memcpy(&self->vc, &vc, sizeof(vc));
    return true;
}

static bool rsvc_vorbis_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                                 rsvc_done_t fail) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    vorbis_comment_add_tag(&self->vc, name, value);
    return true;
}

static bool rsvc_vorbis_tags_each(
        rsvc_tags_t tags,
        void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    __block bool loop = true;
    for (size_t i = 0; loop && (i < self->vc.comments); ++i) {
        const char* comment = self->vc.user_comments[i];
        char* eq = strchr(comment, '=');
        if (!eq) {
            // TODO(sfiera): report failure.
            continue;
        }
        char* name = strndup(comment, eq - comment);
        char* value = strdup(eq + 1);
        block(name, value, ^{
            loop = false;
        });
        free(name);
        free(value);
    }
    return loop;
}

static void rsvc_vorbis_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    rsvc_done_t done_copy = done;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        rsvc_done_t done = done_copy;
        int fd;
        char tmp_path[PATH_MAX + 5] = {'\0'};
        strcat(tmp_path, self->path);
        strcat(tmp_path, ".tmp");
        if (!rsvc_open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644, &fd, done)) {
            return;
        }

        ogg_page                    og;
        __block ogg_stream_state    os;
        ogg_stream_init(&os, self->serial);
        done = ^(rsvc_error_t error){
            ogg_stream_clear(&os);
            done(error);
        };

        ogg_packet header_comm;
        vorbis_commentheader_out(&self->vc, &header_comm);

        ogg_stream_packetin(&os, &self->header);
        ogg_stream_packetin(&os, &header_comm);
        ogg_stream_packetin(&os, &self->header_code);

        while (true) {
            int result = ogg_stream_flush(&os, &og);
            if (result == 0) {
                break;
            }
            write(fd, og.header, og.header_len);
            write(fd, og.body, og.body_len);
        }

        for (rsvc_ogg_page_node_t curr = self->page_head; curr; curr = curr->next) {
            write(fd, curr->page.header, curr->page.header_len);
            write(fd, curr->page.body, curr->page.body_len);
        }
        rename(tmp_path, self->path);

        done(NULL);
    });
}

static void rsvc_vorbis_tags_clear(rsvc_tags_t tags) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    vorbis_comment_clear(&self->vc);
    rsvc_ogg_packet_clear(&self->header);
    rsvc_ogg_packet_clear(&self->header_code);
    while (self->page_head) {
        rsvc_ogg_page_node_t old = self->page_head;
        self->page_head = self->page_head->next;
        rsvc_ogg_page_clear(&old->page);
        free(old);
    }
    self->page_tail = NULL;
}

static void rsvc_vorbis_tags_destroy(rsvc_tags_t tags) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    rsvc_vorbis_tags_clear(tags);
    free(self);
}

static struct rsvc_tags_methods vorbis_vptr = {
    .remove = rsvc_vorbis_tags_remove,
    .add = rsvc_vorbis_tags_add,
    .each = rsvc_vorbis_tags_each,
    .save = rsvc_vorbis_tags_save,
    .destroy = rsvc_vorbis_tags_destroy,
};

static int read_ogg_page(int fd, ogg_sync_state* oy, ogg_page* og, rsvc_done_t fail) {
    while (true) {
        char* data = ogg_sync_buffer(oy, 4096);
        ssize_t size = read(fd, data, 4096);
        if (size < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return -1;
        }
        ogg_sync_wrote(oy, size);
        const int status = ogg_sync_pageout(oy, og);
        if (status == 1) {
            return 1;
        } else if (status < 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
            return status;
        }
        if (size == 0) {
            return 0;
        }
    }
}

#define FAIL \
    ^(rsvc_error_t error){ \
        done(NULL, error); \
    }

void rsvc_vorbis_read_tags(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t)) {
    __block struct rsvc_vorbis_tags tags = {
        .super = {
            .vptr = &vorbis_vptr,
        },
        .path = strdup(path),
    };
    done = ^(rsvc_tags_t tags, rsvc_error_t error){
        if (error) {
            rsvc_vorbis_tags_clear(tags);
        }
        done(tags, error);
    };

    void (^done_copy)(rsvc_tags_t, rsvc_error_t) = done;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Make it possible to reassign `done` within this block.
        void (^done)(rsvc_tags_t, rsvc_error_t) = done_copy;

        int fd;
        if (!rsvc_open(tags.path, O_RDONLY, 0644, &fd, FAIL)) {
            return;
        }
        done = ^(rsvc_tags_t tags, rsvc_error_t error){
            close(fd);
            done(tags, error);
        };

        __block ogg_sync_state      oy;
        __block ogg_stream_state    os;
        __block vorbis_info         vi;
        ogg_page                    og;
        ogg_packet                  op;

        ogg_sync_init(&oy);
        ogg_stream_reset(&os);  // Init after first page.  For now, just make sure it's zeroed out.
        vorbis_info_init(&vi);
        vorbis_comment_init(&tags.vc);

        done = ^(rsvc_tags_t tags, rsvc_error_t error){
            ogg_sync_clear(&oy);
            ogg_stream_clear(&os);
            vorbis_info_clear(&vi);
            done(tags, error);
        };

        bool first_page = true;
        int header_packets = 3;

        // Read all of the pages from the ogg file in a loop.
        while (true) {
            int page_status = read_ogg_page(fd, &oy, &og, FAIL);
            if (page_status < 0) {
                return;
            } else if (page_status == 0) {
                if (header_packets > 0) {
                    rsvc_errorf(FAIL, __FILE__, __LINE__, "unexpected end of file");
                    return;
                }
                break;
            }

            if (first_page) {
                // First page must be a bos page.
                if (!ogg_page_bos(&og)) {
                    rsvc_errorf(FAIL, __FILE__, __LINE__, "ogg file does not begin with bos page");
                    return;
                } else if (ogg_stream_init(&os, ogg_page_serialno(&og)) < 0) {
                    rsvc_errorf(FAIL, __FILE__, __LINE__, "couldn't initialize ogg stream");
                    return;
                }
                tags.serial = ogg_page_serialno(&og);
                first_page = false;
            } else {
                // Subsequent pages must not be bos pages, and must have
                // the same serial as the first page.
                if (ogg_page_bos(&og) || (ogg_page_serialno(&og) != tags.serial)) {
                    rsvc_errorf(FAIL, __FILE__, __LINE__, "multiplexed ogg files not supported");
                    return;
                }
            }

            if (header_packets > 0) {
                // The first three packets are special and define the
                // parameters of the vorbis stream within the ogg file.
                // Parse them out of the pages.
                if (ogg_stream_pagein(&os, &og) < 0) {
                    rsvc_errorf(FAIL, __FILE__, __LINE__, "error reading ogg page");
                    return;
                }

                while (true) {
                    int packetout_status = ogg_stream_packetout(&os, &op);
                    if (packetout_status == 0) {
                        break;
                    } else if (packetout_status < 0) {
                        rsvc_errorf(FAIL, __FILE__, __LINE__, "error reading ogg packet");
                        return;
                    }

                    if (header_packets <= 0) {
                        rsvc_errorf(FAIL, __FILE__, __LINE__, "extra packets in ogg header pages");
                        return;
                    } else if (vorbis_synthesis_headerin(&vi, &tags.vc, &op) < 0) {
                        rsvc_errorf(FAIL, __FILE__, __LINE__, "ogg file is not a vorbis file");
                        return;
                    }
                    if (header_packets == 3) {
                        rsvc_ogg_packet_copy(&tags.header, &op);
                    } else if (header_packets == 1) {
                        rsvc_ogg_packet_copy(&tags.header_code, &op);
                    }
                    --header_packets;
                }
            } else {
                rsvc_logf(2, "read ogg page (header_len=%lu, body_len=%lu)",
                          og.header_len, og.body_len);
                rsvc_ogg_page_push(&tags, &og);
            }
        }

        if (header_packets > 0) {
            rsvc_errorf(FAIL, __FILE__, __LINE__, "unexpected end of file");
            return;
        }

        done(memdup(&tags, sizeof(tags)), NULL);
    });
}

#undef FAIL

void rsvc_vorbis_format_register() {
    rsvc_container_format_register("vorbis", 4, "OggS", rsvc_vorbis_read_tags);
}
