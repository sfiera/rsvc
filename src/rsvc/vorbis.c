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
#include <vorbis/vorbisenc.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void rsvc_vorbis_encode(int read_fd, int file, size_t samples_per_channel,
                        rsvc_comments_t comments, int bitrate, void (^done)(rsvc_error_t error)) {
    done = Block_copy(done);
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

        // Initialize the encoder.
        vorbis_info_init(&vi);
        ret = vorbis_encode_init(&vi, 2, 44100, -1, bitrate, -1);
        if (ret != 0 ) {
            rsvc_const_error(done, __FILE__, __LINE__, "couldn't init vorbis encoder");
            goto encode_cleanup;
        }
        vorbis_analysis_init(&vd, &vi);
        vorbis_block_init(&vd, &vb);

        // Pick a random-ish serial number.  rand_r() is thread-safe,
        // and we don't need strong guarantees about randomness, so this
        // is probably sufficient.
        unsigned seed = time(NULL);
        ogg_stream_init(&os, rand_r(&seed));

        // Copy `comments` into `vc`.
        vorbis_comment_init(&vc);
        vorbis_comment* vcp = &vc;
        rsvc_comments_each(comments, ^(const char* name, const char* value, rsvc_stop_t stop){
            vorbis_comment_add_tag(vcp, name, value);
        });

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
                write(file, og.header, og.header_len);
                write(file, og.body, og.body_len);
            }
        }

        bool eos = false;
        size_t start = 0;
        uint8_t in[1024];
        while (!eos) {
            ssize_t size = read(read_fd, in, sizeof(in));
            if (size < 0) {
                rsvc_strerror(done, __FILE__, __LINE__);
                goto encode_cleanup;
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
                float** out = vorbis_analysis_buffer(&vd, sizeof(in));
                float* channels[] = {out[0], out[1]};
                for (uint8_t* p = in; p < in + size; p += 4) {
                    *(channels[0]++) = (p[0] | ((int)(int8_t)p[1] << 8)) / 32768.f;
                    *(channels[1]++) = (p[2] | ((int)(int8_t)p[3] << 8)) / 32768.f;
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
                        write(file, og.header, og.header_len);
                        write(file, og.body, og.body_len);
                        if (ogg_page_eos(&og)) {
                            eos = true;
                        }
                    }
                }
            }
        }

        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);

        done(NULL);
encode_cleanup:
        Block_release(done);
    });
}
