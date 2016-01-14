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

#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "audio.h"

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
#include <sys/errno.h>
#include <sys/param.h>

#include "common.h"
#include "list.h"
#include "ogg.h"
#include "unix.h"

bool rsvc_vorbis_encode_options_validate(rsvc_encode_options_t opts, rsvc_done_t fail) {
    if (!rsvc_audio_info_validate(&opts->info, fail)) {
        return false;
    }
    return true;
}

bool rsvc_vorbis_encode(  FILE* src_file, FILE* dst_file, rsvc_encode_options_t options,
                          rsvc_done_t fail) {
    int32_t                 bitrate   = options->bitrate;
    struct rsvc_audio_info  info      = options->info;
    rsvc_encode_progress_f  progress  = options->progress;
    float                   div       = 1 << (info.bits_per_sample - 1);
    if (!rsvc_vorbis_encode_options_validate(options, fail)) {
        return false;
    }

    ogg_stream_state  os;
    ogg_page          og;
    ogg_packet        op;
    vorbis_info       vi;
    vorbis_comment    vc;
    vorbis_dsp_state  vd;
    vorbis_block      vb;
    size_t            samples_per_channel_read = 0;

    // Initialize the encoder.
    vorbis_info_init(&vi);
    if (vorbis_encode_init(&vi, info.channels, info.sample_rate, -1, bitrate, -1)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "couldn't init vorbis encoder");
        return false;
    }
    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    // Pick a random-ish serial number.  rand_r() is thread-safe,
    // and we don't need strong guarantees about randomness, so this
    // is probably sufficient.
    unsigned seed = time(NULL);
    ogg_stream_init(&os, rand_r(&seed));

    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    vorbis_comment_init(&vc);
    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

    if (!(rsvc_ogg_align_packet(dst_file, &os, &og, &header, fail) &&
          rsvc_ogg_align_packet(dst_file, &os, &og, &header_comm, fail) &&
          rsvc_ogg_align_packet(dst_file, &os, &og, &header_code, fail))) {
        // TODO(sfiera): cleanup
        return false;
    }

    bool eos = false;
    int16_t in[2048];
    size_t size_inout = 0;
    while (!eos) {
        bool eof = false;
        size_t nsamples;
        if (!rsvc_cread("pipe", fileno(src_file), in, 2048 / info.channels, info.channels * sizeof(int16_t),
                        &nsamples, &size_inout, &eof, fail)) {
            return false;
        } else if (nsamples) {
            samples_per_channel_read += nsamples;
            float** out = vorbis_analysis_buffer(&vd, sizeof(in));
            for (int16_t* p = in; p < in + (nsamples * info.channels); ) {
                for (int i = 0; i < info.channels; ++i) {
                    *(out[i]++) = *(p++) / div;
                }
            }
            vorbis_analysis_wrote(&vd, nsamples);
        } else if (eof) {
            vorbis_analysis_wrote(&vd, 0);
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
                    if (!(rsvc_write(NULL, fileno(dst_file), og.header, og.header_len, NULL, NULL, fail) &&
                          rsvc_write(NULL, fileno(dst_file), og.body, og.body_len, NULL, NULL, fail))) {
                        return false;  // TODO(sfiera): cleanup?
                    }
                    if (ogg_page_eos(&og)) {
                        eos = true;
                    }
                }
            }
        }
        progress(samples_per_channel_read * 1.0 / info.samples_per_channel);
    }

    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);

    return true;
}

typedef struct rsvc_ogg_page_node* rsvc_ogg_page_node_t;
struct rsvc_ogg_page_node {
    ogg_page                page;
    rsvc_ogg_page_node_t    prev, next;
};

typedef struct rsvc_ogg_page_list* rsvc_ogg_page_list_t;
struct rsvc_ogg_page_list {
    rsvc_ogg_page_node_t    head, tail;
};

typedef struct rsvc_vorbis_tags* rsvc_vorbis_tags_t;
struct rsvc_vorbis_tags {
    struct rsvc_tags            super;
    char*                       path;
    uint32_t                    serial;
    vorbis_comment              vc;

    ogg_packet                  header;
    ogg_packet                  header_code;
    struct rsvc_ogg_page_list   pages;
};

static bool rsvc_vorbis_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    (void)fail;
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    // I bet this is not the most efficient thing to do.
    vorbis_comment vc;
    vorbis_comment_init(&vc);
    if (name) {
        size_t size = strlen(name);
        for (int i = 0; i < self->vc.comments; ++i) {
            const char* comment = self->vc.user_comments[i];
            if ((strncmp(name, comment, size) != 0) && (comment[size] != '=')) {
                vorbis_comment_add(&vc, comment);
            }
        }
    }
    vorbis_comment_clear(&self->vc);
    memcpy(&self->vc, &vc, sizeof(vc));
    return true;
}

static bool rsvc_vorbis_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                                 rsvc_done_t fail) {
    (void)fail;
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    vorbis_comment_add_tag(&self->vc, name, value);
    return true;
}

static void vorbis_break(rsvc_iter_t super_it);
static bool vorbis_next(rsvc_iter_t super_it);

static struct rsvc_iter_methods vorbis_iter_vptr = {
    .next    = vorbis_next,
    .break_  = vorbis_break,
};

typedef struct vorbis_tags_iter* vorbis_tags_iter_t;
struct vorbis_tags_iter {
    struct rsvc_tags_iter  super;
    vorbis_comment*        vc;
    int                    i;
    char*                  storage;
};

static rsvc_tags_iter_t vorbis_begin(rsvc_tags_t tags) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    struct vorbis_tags_iter iter = {
        .super = {
            .vptr = &vorbis_iter_vptr,
        },
        .vc  = &self->vc,
        .i   = 0,
    };
    vorbis_tags_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static void vorbis_break(rsvc_iter_t super_it) {
    vorbis_tags_iter_t it = DOWN_CAST(struct vorbis_tags_iter, (rsvc_tags_iter_t)super_it);
    if (it->storage) {
        free(it->storage);
    }
    free(it);
}

static bool vorbis_next(rsvc_iter_t super_it) {
    vorbis_tags_iter_t it = DOWN_CAST(struct vorbis_tags_iter, (rsvc_tags_iter_t)super_it);
    while (it->i < it->vc->comments) {
        if (it->storage) {
            free(it->storage);
        }
        it->storage = strdup(it->vc->user_comments[it->i++]);
        char* eq = strchr(it->storage, '=');
        if (eq) {
            *eq = '\0';
            it->super.name = it->storage;
            it->super.value = eq + 1;
            return true;
        }
        // else TODO(sfiera): report failure.
    }
    vorbis_break(super_it);
    return false;
}

static bool rsvc_vorbis_tags_save(rsvc_tags_t tags, rsvc_done_t fail) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    FILE* file;
    char tmp_path[MAXPATHLEN];
    if (!rsvc_temp(self->path, tmp_path, &file, fail)) {
        return false;
    }
    rsvc_done_t done = ^(rsvc_error_t error){
        fclose(file);
        if (error) {
            fail(error);
        }
    };

    ogg_page                    og;
    __block ogg_stream_state    os;
    ogg_stream_init(&os, self->serial);
    done = ^(rsvc_error_t error){
        ogg_stream_clear(&os);
        done(error);
    };

    ogg_packet header_comm;
    vorbis_commentheader_out(&self->vc, &header_comm);
    if (!(rsvc_ogg_align_packet(file, &os, &og, &self->header, done) &&
          rsvc_ogg_align_packet(file, &os, &og, &header_comm, done) &&
          rsvc_ogg_align_packet(file, &os, &og, &self->header_code, done))) {
        // TODO(sfiera): cleanup
        return false;
    }

    while (true) {
        int result = ogg_stream_flush(&os, &og);
        if (result == 0) {
            break;
        }
        if (!(rsvc_write(NULL, fileno(file), og.header, og.header_len, NULL, NULL, done) &&
              rsvc_write(NULL, fileno(file), og.body, og.body_len, NULL, NULL, done))) {
            return false;  // TODO(sfiera): cleanup?
        }
    }

    for (rsvc_ogg_page_node_t curr = self->pages.head; curr; curr = curr->next) {
        if (!(rsvc_write(NULL, fileno(file), curr->page.header, curr->page.header_len, NULL, NULL, done) &&
              rsvc_write(NULL, fileno(file), curr->page.body, curr->page.body_len, NULL, NULL, done))) {
            return false;
        }
    }
    if (!rsvc_refile(tmp_path, self->path, done)) {
        return false;
    }
    done(NULL);
    return true;
}

static void rsvc_vorbis_tags_clear(rsvc_tags_t tags) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    vorbis_comment_clear(&self->vc);
    rsvc_ogg_packet_clear(&self->header);
    rsvc_ogg_packet_clear(&self->header_code);

    RSVC_LIST_CLEAR(&self->pages, ^(rsvc_ogg_page_node_t node){
        rsvc_ogg_page_clear(&node->page);
    });
}

static void rsvc_vorbis_tags_destroy(rsvc_tags_t tags) {
    rsvc_vorbis_tags_t self = DOWN_CAST(struct rsvc_vorbis_tags, tags);
    rsvc_vorbis_tags_clear(tags);
    free(self->path);
    free(self);
}

static struct rsvc_tags_methods vorbis_vptr = {
    .remove = rsvc_vorbis_tags_remove,
    .add = rsvc_vorbis_tags_add,
    .save = rsvc_vorbis_tags_save,
    .destroy = rsvc_vorbis_tags_destroy,

    .iter_begin  = vorbis_begin,
};

static bool start_stream(ogg_stream_state* os, ogg_page* og, uint32_t* serial, rsvc_done_t fail) {
    if (!ogg_page_bos(og)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "ogg file does not begin with bos page");
        return false;
    } else if (ogg_stream_init(os, ogg_page_serialno(og)) < 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "couldn't initialize ogg stream");
        return false;
    }
    *serial = ogg_page_serialno(og);
    return true;
}

static bool continue_stream(ogg_page* og, uint32_t serial, rsvc_done_t fail) {
    if (ogg_page_bos(og) || (ogg_page_serialno(og) != serial)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "multiplexed ogg files not supported");
        return false;
    }
    return true;
}

static bool rsvc_ogg_stream_pagein(ogg_stream_state* os, ogg_page* og, rsvc_done_t fail) {
    if (ogg_stream_pagein(os, og) < 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
        return false;
    }
    return true;
}

bool read_header(  FILE* file, rsvc_vorbis_tags_t ogv, vorbis_info* vi,
                   ogg_sync_state* oy, ogg_stream_state* os, ogg_page* og, ogg_packet* op,
                   rsvc_done_t fail) {
    bool first_page = true;
    int header_packets = 3;
    while (header_packets > 0) {
        if (!(rsvc_ogg_page_read(file, oy, og, NULL, fail) &&
              (first_page  ? start_stream(os, og, &ogv->serial, fail)
                           : continue_stream(og, ogv->serial, fail)) &&
              rsvc_ogg_stream_pagein(os, og, fail))) {
            return false;
        }
        first_page = false;

        while (true) {
            bool have_packet;
            if (!rsvc_ogg_packet_out(os, op, &have_packet, fail)) {
                return false;
            } else if (!have_packet) {
                break;
            }

            if (header_packets <= 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "extra packets in ogg header pages");
                return false;
            } else if (vorbis_synthesis_headerin(vi, &ogv->vc, op) < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "ogg file is not a vorbis file");
                return false;
            }
            if (header_packets == 3) {
                rsvc_ogg_packet_copy(&ogv->header, op);
            } else if (header_packets == 1) {
                rsvc_ogg_packet_copy(&ogv->header_code, op);
            }
            --header_packets;
        }
    }
    return true;
}

bool read_all_packets(  FILE* file, rsvc_vorbis_tags_t ogv, ogg_sync_state* oy, ogg_page* og,
                        rsvc_done_t fail) {
    while (true) {
        bool eof;
        if (!rsvc_ogg_page_read(file, oy, og, &eof, fail)) {
            return false;
        } else if (eof) {
            break;
        }
        rsvc_logf(2, "read ogg page (header_len=%lu, body_len=%lu)",
                  og->header_len, og->body_len);
        struct rsvc_ogg_page_node node = {};
        rsvc_ogg_page_copy(&node.page, og);
        RSVC_LIST_PUSH(&ogv->pages, memdup(&node, sizeof(node)));
    }

    return true;
}

bool rsvc_vorbis_open_tags(const char* path, int flags, rsvc_tags_t* tags, rsvc_done_t fail) {
    struct rsvc_vorbis_tags ogv = {
        .super = {
            .vptr   = &vorbis_vptr,
            .flags  = flags,
        },
        .path = strdup(path),
    };
    bool ok = false;

    FILE* file;
    if (rsvc_open(ogv.path, O_RDONLY, 0644, &file, fail)) {
        ogg_sync_state    oy = {};
        ogg_stream_state  os = {};
        vorbis_info       vi = {};
        ogg_page          og = {};
        ogg_packet        op = {};

        ogg_sync_init(&oy);
        vorbis_info_init(&vi);
        vorbis_comment_init(&ogv.vc);

        ok =  read_header(file, &ogv, &vi, &oy, &os, &og, &op, fail) &&
              read_all_packets(file, &ogv, &oy, &og, fail);

        ogg_sync_clear(&oy);
        ogg_stream_clear(&os);
        vorbis_info_clear(&vi);
        fclose(file);
    }
    if (ok) {
        struct rsvc_vorbis_tags* copy = memdup(&ogv, sizeof(ogv));
        *tags = &copy->super;
    } else {
        rsvc_vorbis_tags_clear(&ogv.super);
    }
    return ok;
}

#define MAX_READ_BACK          65536
#define READ_BACK_CHUNK_SIZE   4096
#define READ_BACK_CHUNK_COUNT  (MAX_READ_BACK / READ_BACK_CHUNK_SIZE)

bool read_last_page(  int fd, ogg_sync_state* oy, ogg_page* og,
                      rsvc_done_t fail) {
    const off_t end = lseek(fd, 0, SEEK_END);
    if (end < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }

    bool ok = false;
    off_t chunk_end = end;
    uint8_t* chunk_data[READ_BACK_CHUNK_COUNT] = {};
    size_t chunk_sizes[READ_BACK_CHUNK_COUNT] = {};
    for (int i = 0; ; ++i) {
        // If we've read 64kiB (or the whole file), then give up.
        if ((chunk_end == 0) || (i == READ_BACK_CHUNK_COUNT)) {
            rsvc_errorf(fail, __FILE__, __LINE__, "couldn't find last ogg page");
            break;
        }

        // Seek back and read another new chunk.
        off_t chunk_start = (end > READ_BACK_CHUNK_SIZE) ? end - READ_BACK_CHUNK_SIZE : 0;
        if (lseek(fd, chunk_start, SEEK_SET) < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            break;
        }

        chunk_sizes[i] = chunk_end - chunk_start;
        chunk_data[i] = malloc(chunk_sizes[i]);
        if (!rsvc_read(NULL, fd, chunk_data[i], chunk_sizes[i], NULL, NULL, fail)) {
            break;
        }

        // Read back the chunks in `chunk_data` in reverse order.
        ogg_sync_reset(oy);
        for (int j = 0; j < i; ++j) {
            char* buf = ogg_sync_buffer(oy, READ_BACK_CHUNK_SIZE);
            memcpy(buf, chunk_data[i-j-1], chunk_sizes[i-j-1]);
            ogg_sync_wrote(oy, chunk_sizes[i-j-1]);
        }

        while (true) {
            int result = ogg_sync_pageseek(oy, og);
            if (result == 0) {
                break;
            } else if (result < 0) {
                continue;
            }

            ok = true;  // have one, but keep looping.
        }

        if (ok) {
            break;
        }

        chunk_end = chunk_start;
    }

    for (int i = 0; i < READ_BACK_CHUNK_COUNT; ++i) {
        free(chunk_data[i]);
    }

    return ok;
}

bool rsvc_vorbis_audio_info(FILE* file, rsvc_audio_info_t info, rsvc_done_t fail) {
    struct rsvc_vorbis_tags ogv = {};
    bool ok = false;

    ogg_sync_state    oy = {};
    ogg_stream_state  os = {};
    vorbis_info       vi = {};
    ogg_page          og = {};
    ogg_packet        op = {};

    ogg_sync_init(&oy);
    vorbis_info_init(&vi);
    vorbis_comment_init(&ogv.vc);

    if (read_header(file, &ogv, &vi, &oy, &os, &og, &op, fail) &&
        read_last_page(fileno(file), &oy, &og, fail)) {
        struct rsvc_audio_info i = {
            .sample_rate = vi.rate,
            .channels = vi.channels,
            .samples_per_channel = ogg_page_granulepos(&og),
            .bits_per_sample = 16,
            .block_align = 2 * vi.channels,
        };
        *info = i;
        ok = true;
    }

    ogg_sync_clear(&oy);
    ogg_stream_clear(&os);
    vorbis_info_clear(&vi);
    rsvc_vorbis_tags_clear(&ogv.super);
    return ok;
}

const struct rsvc_format rsvc_vorbis = {
    .format_group = RSVC_AUDIO,
    .name = "vorbis",
    .mime = "application/ogg",
    .magic = {"OggS????????????????????????\001vorbis"},
    .magic_size = 35,
    .extension = "ogv",
    .lossless = false,
    .open_tags = rsvc_vorbis_open_tags,
    .audio_info = rsvc_vorbis_audio_info,
    .encode = rsvc_vorbis_encode,
};
