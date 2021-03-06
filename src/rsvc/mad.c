//
// This file is part of Rip Service.
//
// Copyright (C) 2014 Chris Pickel <sfiera@sfzmail.com>
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
#include <mad.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "unix.h"

struct mad_userdata {
    FILE* src_file;
    FILE* dst_file;
    unsigned char data[4096];
    unsigned char* end;
    size_t end_position;
    struct rsvc_audio_info info;
    rsvc_done_t fail;
};

static enum mad_flow mad_input(void* v, struct mad_stream* stream) {
    struct mad_userdata* userdata = v;

    unsigned char* data = userdata->data;
    size_t size = sizeof(userdata->data);
    if (stream->next_frame) {
        size_t prefix = userdata->end - stream->next_frame;
        memmove(userdata->data, stream->next_frame, prefix);
        data += prefix;
        size -= prefix;
    }

    bool eof = false;
    if (!rsvc_read("pipe", userdata->src_file, data, size, 1, &size, &eof, userdata->fail)) {
        return MAD_FLOW_BREAK;
    } else if (eof) {
        return MAD_FLOW_STOP;
    } else {
        userdata->end = data + size;
        userdata->end_position += size;
        mad_stream_buffer(stream, userdata->data, userdata->end - userdata->data);
        return MAD_FLOW_CONTINUE;
    }
}

static enum mad_flow mad_header(void* v, struct mad_header const* header) {
    struct mad_userdata* userdata = v;
    switch (header->mode) {
        case MAD_MODE_SINGLE_CHANNEL:
            userdata->info.channels = 1;
            break;

        case MAD_MODE_DUAL_CHANNEL:
        case MAD_MODE_STEREO:
        case MAD_MODE_JOINT_STEREO:
        default:
            userdata->info.channels = 2;
            break;
    }
    userdata->info.bits_per_sample = 16;  // TODO(sfiera): mad outputs 24
    userdata->info.block_align = 2 * userdata->info.channels;
    userdata->info.sample_rate = header->samplerate;

    return MAD_FLOW_CONTINUE;
}

// Taken directly from minimad.c example code.  Comments there suggest
// using a version with dithering and noise shaping instead.  Is this
// really relevant with most MP3s? (i.e. ones from 16-bit CDs).
// TODO(sfiera): investigate better 24-to-32 bit audio conversion.
static int16_t mad_scale(mad_fixed_t sample) {
    // round
    sample += (1L << (MAD_F_FRACBITS - 16));
    // clip
    if (sample >= MAD_F_ONE) {
        sample = MAD_F_ONE - 1;
    } else if (sample < -MAD_F_ONE) {
        sample = -MAD_F_ONE;
    }
    // quantize
    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static enum mad_flow mad_count(void* v, struct mad_header const* header, struct mad_pcm* pcm) {
    (void)header;
    struct mad_userdata* userdata = v;
    userdata->info.samples_per_channel += pcm->length;
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow mad_output(void* v, struct mad_header const* header, struct mad_pcm* pcm) {
    (void)header;
    struct mad_userdata* userdata = v;
    int16_t data[2 * 1152];
    size_t size = 0;
    mad_fixed_t* channels[2] = {pcm->samples[0], pcm->samples[1]};
    for (int i = 0; i < pcm->length; ++i) {
        for (int chan = 0; chan < pcm->channels; ++chan) {
            data[size++] = mad_scale(*(channels[chan]++));
        }
    }
    if (!rsvc_write("pipe", userdata->dst_file, &data, size * sizeof(int16_t), userdata->fail)) {
        return MAD_FLOW_BREAK;
    }
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow mad_error(void* v, struct mad_stream* stream, struct mad_frame* frame) {
    (void)frame;
    struct mad_userdata* userdata = v;
    size_t position = userdata->end_position - (userdata->end - stream->this_frame);
    if (MAD_RECOVERABLE(stream->error)) {
        return MAD_FLOW_CONTINUE;
    } else {
        rsvc_errorf(userdata->fail, __FILE__, __LINE__,
                "decoding error: %s at %08lx", mad_stream_errorstr(stream), position);
        return MAD_FLOW_BREAK;
    }
}

bool mad_get_audio_info(struct mad_userdata* userdata) {
    struct mad_decoder decoder;
    mad_decoder_init(&decoder, userdata,
                     mad_input,
                     mad_header,  // header
                     NULL,  // filter
                     mad_count,
                     mad_error,
                     NULL  // message
                     );
    int result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
    mad_decoder_finish(&decoder);
    return result == 0;
}

bool mad_decode(struct mad_userdata* userdata) {
    struct mad_decoder decoder;
    mad_decoder_init(&decoder, userdata,
                     mad_input,
                     NULL,  // header
                     NULL,  // filter
                     mad_output,
                     mad_error,
                     NULL  // message
                     );
    int result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
    mad_decoder_finish(&decoder);
    return result == 0;
}

bool rsvc_mad_decode(FILE* src_file, FILE* dst_file,
                     rsvc_decode_info_f info, rsvc_done_t fail) {
    if (!rsvc_id3_skip_tags(src_file, fail)) {
        return false;
    }
    off_t offset;
    if (!rsvc_tell(src_file, &offset, fail)) {
        return false;
    }
    struct mad_userdata userdata = {
        .src_file = src_file,
        .dst_file = dst_file,
        .fail = fail,
    };
    if (!mad_get_audio_info(&userdata)) {
        return false;
    }
    info(&userdata.info);

    if (!rsvc_seek(src_file, offset, SEEK_SET, fail)) {
        return false;
    }
    userdata.end_position = 0;
    return mad_decode(&userdata);
}

bool rsvc_mad_audio_info(FILE* file, rsvc_audio_info_t info, rsvc_done_t fail) {
    struct mad_userdata userdata = {
        .src_file = file,
        .fail = fail,
    };
    if (!(rsvc_id3_skip_tags(file, fail) &&
          mad_get_audio_info(&userdata))) {
        return false;
    }
    *info = userdata.info;
    return true;
}
