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

#include <rsvc/lame.h>

#include <lame/lame.h>
#include <rsvc/encode.h>
#include <rsvc/format.h>
#include "unix.h"

void rsvc_lame_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t done) {
    int32_t bitrate                     = options->bitrate;
    size_t sample_rate                  = options->sample_rate;
    size_t samples_per_channel          = options->samples_per_channel;
    size_t channels                     = options->channels;
    rsvc_encode_progress_t progress     = options->progress;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        lame_global_flags* lame = lame_init();
        lame_set_num_channels(lame, channels);
        lame_set_brate(lame, bitrate >> 10);
        lame_set_in_samplerate(lame, sample_rate);
        lame_set_bWriteVbrTag(lame, 0);  // TODO(sfiera): write the tag.
        if (lame_init_params(lame) < 0) {
            rsvc_errorf(done, __FILE__, __LINE__, "init error");
            return;
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
            if (!rsvc_cread("pipe", src_fd, buffer, kSamples, 2 * sizeof(int16_t),
                            &nsamples, &size_inout, &eof, done)) {
                return;
            } else if (nsamples) {
                samples_per_channel_read += nsamples;
                int mp3buf_written = lame_encode_buffer_interleaved(
                            lame, buffer, nsamples, mp3buf, kMp3BufSize);
                if (mp3buf_written < 0) {
                    rsvc_errorf(done, __FILE__, __LINE__, "encode error");
                    return;
                }
                if (!rsvc_write("pipe", dst_fd, mp3buf, mp3buf_written, NULL, NULL, done)) {
                    return;
                }
                progress(samples_per_channel_read * 1.0 / samples_per_channel);
            }
        }
        if (lame_encode_flush(lame, mp3buf, kMp3BufSize) < 0) {
            rsvc_errorf(done, __FILE__, __LINE__, "flush error");
            return;
        }
        lame_close(lame);
        free(mp3buf);
        done(NULL);
    });
}

void rsvc_lame_format_register() {
    struct rsvc_format lame = {
        .name = "mp3",
        .mime = "audio/mpeg",
        .magic = "ID3",
        .magic_size = 3,
        .extension = "mp3",
        .lossless = false,
        .encode = rsvc_lame_encode,
    };
    rsvc_format_register(&lame);
}
