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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vorbis/vorbisenc.h>

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

struct rsvc_vorbis_tags {
    struct rsvc_tags    super;
    vorbis_comment      vc;
};
typedef struct rsvc_vorbis_tags* rsvc_vorbis_tags_t;

static bool rsvc_vorbis_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    // rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    rsvc_errorf(fail, __FILE__, __LINE__, "can't edit vorbis tags");
    return false;
}

static bool rsvc_vorbis_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                                 rsvc_done_t fail) {
    // rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    rsvc_errorf(fail, __FILE__, __LINE__, "can't edit vorbis tags");
    return false;
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
    // rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        done(NULL);
    });
}

static void rsvc_vorbis_tags_destroy(rsvc_tags_t tags) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    vorbis_comment_clear(&self->vc);
    free(self);
}

static struct rsvc_tags_methods vorbis_vptr = {
    .remove = rsvc_vorbis_tags_remove,
    .add = rsvc_vorbis_tags_add,
    .each = rsvc_vorbis_tags_each,
    .save = rsvc_vorbis_tags_save,
    .destroy = rsvc_vorbis_tags_destroy,
};

static bool read_ogg_page(int fd, ogg_sync_state* oy, ogg_page* og, rsvc_done_t fail) {
    while (true) {
        char* data = ogg_sync_buffer(oy, 4096);
        ssize_t size = read(fd, data, 4096);
        if (size < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return false;
        } else if (size == 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "no ogg data found");
            return false;
        }
        ogg_sync_wrote(oy, size);
        if (ogg_sync_pageout(oy, og) == 1) {
            return true;
        }
    }
}

void rsvc_vorbis_read_tags(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t)) {
    char* path_copy = strdup(path);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        void (^done_copy)(rsvc_tags_t, rsvc_error_t) = done;
        void (^done)(rsvc_tags_t, rsvc_error_t) = ^(rsvc_tags_t tags, rsvc_error_t error){
            free(path_copy);
            done_copy(tags, error);
        };
        void (^*done_ptr)(rsvc_tags_t, rsvc_error_t) = &done;
        rsvc_done_t fail = ^(rsvc_error_t error){
            (*done_ptr)(NULL, error);
        };
        int fd;
        if (!rsvc_open(path_copy, O_RDONLY, 0644, &fd, fail)) {
            return;
        }

        ogg_sync_state oy;
        ogg_sync_state* oyp = &oy;
        ogg_page og;

        ogg_sync_init(&oy);
        done = ^(rsvc_tags_t tags, rsvc_error_t error){
            ogg_sync_clear(oyp);
            done(tags, error);
        };

        // Find and read first ogg page.
        if (!read_ogg_page(fd, &oy, &og, fail)) {
            return;
        }
        uint32_t serial = ogg_page_serialno(&og);

        struct rsvc_vorbis_tags tags = {
            .super = {
                .vptr = &vorbis_vptr,
            },
        };
        ogg_stream_state    os;
        ogg_stream_state*   osp = &os;
        vorbis_info         vi;
        vorbis_info*        vip = &vi;
        vorbis_comment*     vcp = &tags.vc;
        ogg_packet          header;

        ogg_stream_init(&os, ogg_page_serialno(&og));
        vorbis_info_init(&vi);
        vorbis_comment_init(&tags.vc);
        done = ^(rsvc_tags_t tags, rsvc_error_t error){
            ogg_stream_clear(osp);
            vorbis_info_clear(vip);
            if (error) {
                vorbis_comment_clear(vcp);
            }
            done(tags, error);
        };

        if (!ogg_page_bos(&og)) {
            rsvc_errorf(fail, __FILE__, __LINE__, "ogg file does not begin with bos page");
            return;
        } else if (ogg_stream_pagein(&os, &og) < 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
            return;
        } else if (ogg_stream_packetout(&os, &header) < 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg packet");
            return;
        } else if (vorbis_synthesis_headerin(&vi, &tags.vc, &header) < 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "ogg file is not a vorbis file");
            return;
        }

        for (int i = 0; i < 2; ++i) {
            if (!read_ogg_page(fd, &oy, &og, fail)) {
                return;
            } else if (ogg_stream_pagein(&os, &og) < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
                return;
            } else if (ogg_page_serialno(&og) != serial) {
                rsvc_errorf(fail, __FILE__, __LINE__, "multiplexed ogg files not supported");
                return;
            } else if (ogg_stream_packetout(&os, &header) < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg packet");
                return;
            } else if (vorbis_synthesis_headerin(&vi, &tags.vc, &header) < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "ogg file is not a vorbis file");
                return;
            }
        }

        done(memdup(&tags, sizeof(tags)), NULL);
    });
}
