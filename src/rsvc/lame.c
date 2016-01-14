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

#include "audio.h"

#include <lame/lame.h>
#include <rsvc/audio.h>
#include <rsvc/format.h>
#include "unix.h"

bool rsvc_lame_encode_options_validate(rsvc_encode_options_t opts, rsvc_done_t fail) {
    if (!rsvc_audio_info_validate(&opts->info, fail)) {
        return false;
    } else if (opts->info.bits_per_sample != 16) {
        rsvc_errorf(  fail, __FILE__, __LINE__,
                      "can't encode %zu-bit mp3", opts->info.bits_per_sample);
        return false;
    } else if (opts->info.block_align != 4) {
        rsvc_errorf(  fail, __FILE__, __LINE__,
                      "can't encode mp3 from %zu-byte blocks", opts->info.block_align);
        return false;
    } else if (opts->info.channels > 2) {
        rsvc_errorf(  fail, __FILE__, __LINE__,
                      "can't encode %zu-channel mp3", opts->info.channels);
        return false;
    }

    return true;
}

bool rsvc_lame_encode(FILE* src_file, FILE* dst_file, rsvc_encode_options_t options, rsvc_done_t fail) {
    int32_t                 bitrate   = options->bitrate;
    struct rsvc_audio_info  info      = options->info;
    rsvc_encode_progress_f  progress  = options->progress;

    if (!rsvc_lame_encode_options_validate(options, fail)) {
        return false;
    }

    lame_global_flags* lame = lame_init();
    lame_set_num_channels(lame, info.channels);
    lame_set_brate(lame, bitrate >> 10);
    lame_set_in_samplerate(lame, info.sample_rate);
    lame_set_bWriteVbrTag(lame, 0);  // TODO(sfiera): write the tag.
    if (lame_init_params(lame) < 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "init error");
        return false;
    }

    size_t samples_per_channel_read = 0;
    static const int kSamples = 2048;
    static const int kMp3BufSize = kSamples * 5 + 7200;
    unsigned char* mp3buf = malloc(kMp3BufSize);
    int16_t buffer[kSamples * 2];
    size_t size_inout = 0;
    bool eof = false;
    while (!eof) {
        size_t nsamples;
        if (!rsvc_cread("pipe", fileno(src_file), buffer, kSamples, 2 * sizeof(int16_t),
                        &nsamples, &size_inout, &eof, fail)) {
            return false;
        } else if (nsamples) {
            samples_per_channel_read += nsamples;
            int mp3buf_written = -1;
            if (info.channels == 2) {
                mp3buf_written = lame_encode_buffer_interleaved(
                        lame, buffer, nsamples, mp3buf, kMp3BufSize);
            } else {
                mp3buf_written = lame_encode_buffer(
                        lame, buffer, NULL, nsamples * 2, mp3buf, kMp3BufSize);
            }
            if (mp3buf_written < 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "encode error");
                return false;
            }
            if (!rsvc_write("pipe", fileno(dst_file), mp3buf, mp3buf_written, NULL, NULL, fail)) {
                return false;
            }
            progress(samples_per_channel_read * 2.0 / info.channels / info.samples_per_channel);
        }
    }
    if (lame_encode_flush(lame, mp3buf, kMp3BufSize) < 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "flush error");
        return false;
    }
    lame_close(lame);
    free(mp3buf);
    return true;
}
