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

#include <rsvc/flac.h>

#include <Block.h>
#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>
#include <dispatch/dispatch.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

typedef FLAC__StreamEncoderWriteStatus write_status_t;
typedef FLAC__StreamEncoderSeekStatus seek_status_t;
typedef FLAC__StreamEncoderTellStatus tell_status_t;

static write_status_t   flac_write(const FLAC__StreamEncoder* encoder,
                                   const FLAC__byte bytes[], size_t nbytes,
                                   unsigned samples, unsigned current_frame, void* userdata);
static seek_status_t    flac_seek(const FLAC__StreamEncoder* encoder,
                                  FLAC__uint64 absolute_byte_offset, void* userdata);
static tell_status_t    flac_tell(const FLAC__StreamEncoder* encoder,
                                  FLAC__uint64* absolute_byte_offset, void* userdata);


void rsvc_flac_encode(int read_fd, int file, size_t samples_per_channel, rsvc_comments_t comments,
                      void (^progress)(double fraction),
                      void (^done)(rsvc_error_t error)) {
    done = Block_copy(done);
    progress = Block_copy(progress);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        FLAC__StreamEncoder *encoder = NULL;
        FLAC__StreamMetadata* comment_metadata = NULL;
        FLAC__StreamMetadata* padding_metadata = NULL;
        size_t samples_per_channel_read = 0;

        encoder = FLAC__stream_encoder_new();
        if (encoder == NULL) {
            rsvc_errorf(done, __FILE__, __LINE__, "couldn't allocate FLAC encoder");
            goto encode_cleanup;
        }

        if (!(FLAC__stream_encoder_set_verify(encoder, true) &&
              FLAC__stream_encoder_set_compression_level(encoder, 8) &&
              FLAC__stream_encoder_set_channels(encoder, 2) &&
              FLAC__stream_encoder_set_bits_per_sample(encoder, 16) &&
              FLAC__stream_encoder_set_sample_rate(encoder, 44100) &&
              FLAC__stream_encoder_set_total_samples_estimate(encoder, samples_per_channel))) {
            FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
            const char* message = FLAC__StreamEncoderStateString[state];
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            goto encode_cleanup;
        }

        comment_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
        padding_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
        padding_metadata->length = 1024;
        if (!comment_metadata || !padding_metadata) {
            rsvc_errorf(done, __FILE__, __LINE__, "comment failure");
            goto encode_cleanup;
        }

        if (!rsvc_comments_each(comments, ^(const char* name, const char* value, rsvc_stop_t stop){
            FLAC__StreamMetadata_VorbisComment_Entry entry;
            if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
                        &entry, name, value) ||
                !FLAC__metadata_object_vorbiscomment_append_comment(
                        comment_metadata, entry, false)) {
                rsvc_errorf(done, __FILE__, __LINE__, "comment failure");
                stop();
            }
        })) {
            goto encode_cleanup;
        }
        FLAC__StreamMetadata* metadata[2] = {comment_metadata, padding_metadata};
        if (!FLAC__stream_encoder_set_metadata(encoder, metadata, 2)) {
            rsvc_errorf(done, __FILE__, __LINE__, "comment failure");
            goto encode_cleanup;
        }

        int fd = file;
        FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(
                encoder, flac_write, flac_seek, flac_tell, NULL, &fd);
        if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
            const char* message;
            if (init_status == FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR) {
                FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
                message = FLAC__StreamEncoderStateString[state];
            } else {
                message = FLAC__StreamEncoderInitStatusString[init_status];
            }
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            goto encode_cleanup;
        }

        size_t start = 0;
        static const int kSamples = 4096;
        uint8_t buffer[kSamples];
        while (true) {
            ssize_t result = read(read_fd, buffer + start, sizeof(buffer) - start);
            if (result < 0) {
                rsvc_strerrorf(done, __FILE__, __LINE__, "pipe");
                goto encode_cleanup;
            } else if (result == 0) {
                break;
            }

            result += start;
            size_t remainder = result % 4;
            result -= remainder;
            size_t nsamples = result / 4;
            samples_per_channel_read += nsamples;
            if (result > 0) {
                FLAC__int32 samples[2048];
                FLAC__int32* sp = samples;
                int16_t* buffer16 = (int16_t*)buffer;
                for (int16_t* p = buffer16; p < buffer16 + (nsamples * 2); ++p) {
                    *(sp++) = *p;
                }
                if (!FLAC__stream_encoder_process_interleaved(encoder, samples, nsamples)) {
                    FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
                    const char* message = FLAC__StreamEncoderStateString[state];
                    rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
                    goto encode_cleanup;
                }
                memcpy(buffer, buffer + result, remainder);
            }
            start = remainder;
            progress(samples_per_channel_read * 1.0 / samples_per_channel);
        }
        if (!FLAC__stream_encoder_finish(encoder)) {
            FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
            const char* message = FLAC__StreamEncoderStateString[state];
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            goto encode_cleanup;
        }
        done(NULL);
encode_cleanup:
        if (encoder) FLAC__stream_encoder_delete(encoder);
        if (padding_metadata) FLAC__metadata_object_delete(padding_metadata);
        if (comment_metadata) FLAC__metadata_object_delete(comment_metadata);
        Block_release(progress);
        Block_release(done);
    });
}

static write_status_t flac_write(const FLAC__StreamEncoder* encoder,
                                 const FLAC__byte bytes[], size_t nbytes,
                                 unsigned samples, unsigned current_frame, void* userdata) {
    int fd = *(int*)userdata;
    const void* data = bytes;
    while (nbytes > 0) {
        ssize_t written = write(fd, data, nbytes);
        if (written <= 0) {
            return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }
        nbytes -= written;
    }
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static seek_status_t flac_seek(const FLAC__StreamEncoder* encoder,
                               FLAC__uint64 absolute_byte_offset, void* userdata) {
    int fd = *(int*)userdata;
    if (lseek(fd, absolute_byte_offset, SEEK_SET) < 0) {
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    }
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static tell_status_t flac_tell(const FLAC__StreamEncoder* encoder,
                               FLAC__uint64* absolute_byte_offset, void* userdata) {
    int fd = *(int*)userdata;
    off_t offset = lseek(fd, 0, SEEK_CUR);
    if (offset < 0) {
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    }
    *absolute_byte_offset = offset;
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}
