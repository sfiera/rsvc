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

#include <rsvc/flac.h>

#include <Block.h>
#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

typedef FLAC__StreamEncoderWriteStatus write_encode_status_t;
typedef FLAC__StreamEncoderSeekStatus seek_encode_status_t;
typedef FLAC__StreamEncoderTellStatus tell_encode_status_t;
typedef struct flac_encode_userdata* flac_encode_userdata_t;
struct flac_encode_userdata {
    int fd;
};

static write_encode_status_t   flac_encode_write(const FLAC__StreamEncoder* encoder,
                                                 const FLAC__byte bytes[], size_t nbytes,
                                                 unsigned samples, unsigned current_frame,
                                                 void* userdata);
static seek_encode_status_t    flac_encode_seek(const FLAC__StreamEncoder* encoder,
                                                FLAC__uint64 absolute_byte_offset,
                                                void* userdata);
static tell_encode_status_t    flac_encode_tell(const FLAC__StreamEncoder* encoder,
                                                FLAC__uint64* absolute_byte_offset,
                                                void* userdata);

typedef FLAC__StreamDecoderReadStatus read_decode_status_t;
typedef FLAC__StreamDecoderWriteStatus write_decode_status_t;
typedef FLAC__StreamDecoderSeekStatus seek_decode_status_t;
typedef FLAC__StreamDecoderTellStatus tell_decode_status_t;
typedef FLAC__StreamDecoderLengthStatus length_decode_status_t;
typedef struct flac_decode_userdata* flac_decode_userdata_t;
struct flac_decode_userdata {
    int                     read_fd;
    int                     write_fd;
    rsvc_decode_metadata_f  metadata;
    rsvc_done_t             done;
    bool                    called_done;
};

static void                    flac_decode_metadata(const FLAC__StreamDecoder* decoder,
                                                    const FLAC__StreamMetadata* metadata,
                                                    void* userdata);
static read_decode_status_t    flac_decode_read(const FLAC__StreamDecoder* decoder,
                                                FLAC__byte bytes[], size_t* nbytes,
                                                void* userdata);
static write_decode_status_t   flac_decode_write(const FLAC__StreamDecoder* decoder,
                                                 const FLAC__Frame* frame,
                                                 const FLAC__int32* const* data,
                                                 void* userdata);
static seek_decode_status_t    flac_decode_seek(const FLAC__StreamDecoder* decoder,
                                                FLAC__uint64 absolute_byte_offset,
                                                void* userdata);
static tell_decode_status_t    flac_decode_tell(const FLAC__StreamDecoder* decoder,
                                                FLAC__uint64* absolute_byte_offset,
                                                void* userdata);
static length_decode_status_t  flac_decode_length(const FLAC__StreamDecoder* decoder,
                                                  FLAC__uint64* absolute_byte_offset,
                                                  void* userdata);
static FLAC__bool              flac_decode_eof(const FLAC__StreamDecoder* decoder,
                                               void* userdata);
static void                    flac_decode_error(const FLAC__StreamDecoder* decoder,
                                                 FLAC__StreamDecoderErrorStatus error,
                                                 void* userdata);

// static bool rsvc_tags_to_vorbis_comments(rsvc_tags_t tags, FLAC__StreamMetadata* metadata) {
//     return rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
//         FLAC__StreamMetadata_VorbisComment_Entry entry;
//         if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, name, value) ||
//             !FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, false)) {
//             stop();
//         }
//     });
// }

void rsvc_flac_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t done) {
    size_t samples_per_channel          = options->samples_per_channel;
    rsvc_encode_progress_t progress     = options->progress;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        FLAC__StreamEncoder *encoder = NULL;
        // FLAC__StreamMetadata* comment_metadata = NULL;
        // FLAC__StreamMetadata* padding_metadata = NULL;
        size_t samples_per_channel_read = 0;

        encoder = FLAC__stream_encoder_new();
        if (encoder == NULL) {
            rsvc_errorf(done, __FILE__, __LINE__, "couldn't allocate FLAC encoder");
            return;
        }
        void (^cleanup)() = ^{
            FLAC__stream_encoder_delete(encoder);
        };

        if (!(FLAC__stream_encoder_set_verify(encoder, true) &&
              FLAC__stream_encoder_set_compression_level(encoder, 8) &&
              FLAC__stream_encoder_set_channels(encoder, 2) &&
              FLAC__stream_encoder_set_bits_per_sample(encoder, 16) &&
              FLAC__stream_encoder_set_sample_rate(encoder, 44100) &&
              FLAC__stream_encoder_set_total_samples_estimate(encoder, samples_per_channel))) {
            FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
            const char* message = FLAC__StreamEncoderStateString[state];
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
        }

        // comment_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
        // padding_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
        // padding_metadata->length = 1024;
        // if (!comment_metadata || !rsvc_tags_to_vorbis_comments(tags, comment_metadata)
        //         || !padding_metadata) {
        //     cleanup();
        //     rsvc_errorf(done, __FILE__, __LINE__, "comment failure");
        //     return;
        // }
        //
        // FLAC__StreamMetadata* metadata[2] = {comment_metadata, padding_metadata};
        // if (!FLAC__stream_encoder_set_metadata(encoder, metadata, 2)) {
        //     cleanup();
        //     rsvc_errorf(done, __FILE__, __LINE__, "comment failure");
        //     return;
        // }

        struct flac_encode_userdata userdata = {
            .fd = dst_fd,
        };
        FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(
                encoder, flac_encode_write, flac_encode_seek, flac_encode_tell, NULL, &userdata);
        if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
            const char* message;
            if (init_status == FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR) {
                FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
                message = FLAC__StreamEncoderStateString[state];
            } else {
                message = FLAC__StreamEncoderInitStatusString[init_status];
            }
            cleanup();
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            return;
        }

        size_t start = 0;
        static const int kSamples = 4096;
        uint8_t buffer[kSamples];
        while (true) {
            ssize_t result = read(src_fd, buffer + start, sizeof(buffer) - start);
            if (result < 0) {
                cleanup();
                rsvc_strerrorf(done, __FILE__, __LINE__, "pipe");
                return;
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
                    cleanup();
                    rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
                    return;
                }
                memcpy(buffer, buffer + result, remainder);
            }
            start = remainder;
            progress(samples_per_channel_read * 1.0 / samples_per_channel);
        }
        if (!FLAC__stream_encoder_finish(encoder)) {
            FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
            const char* message = FLAC__StreamEncoderStateString[state];
            cleanup();
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            return;
        }
        cleanup();
        done(NULL);
    });
}

void rsvc_flac_decode(int src_fd, int dst_fd,
                      rsvc_decode_metadata_f metadata, rsvc_done_t done) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        FLAC__StreamDecoder *decoder = NULL;

        decoder = FLAC__stream_decoder_new();
        if (decoder == NULL) {
            rsvc_errorf(done, __FILE__, __LINE__, "couldn't allocate FLAC decoder");
            return;
        }
        void (^cleanup)() = ^{
            FLAC__stream_decoder_delete(decoder);
        };

        struct flac_decode_userdata userdata = {
            .read_fd   = src_fd,
            .write_fd  = dst_fd,
            .metadata  = metadata,
            .done      = done,
        };
        FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
                decoder, flac_decode_read, flac_decode_seek, flac_decode_tell,
                flac_decode_length, flac_decode_eof, flac_decode_write, flac_decode_metadata,
                flac_decode_error, &userdata);
        if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            const char* message = FLAC__StreamDecoderInitStatusString[init_status];
            cleanup();
            rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            return;
        }

        if (!(FLAC__stream_decoder_process_until_end_of_stream(decoder)
              && FLAC__stream_decoder_finish(decoder))) {
            FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(decoder);
            const char* message = FLAC__StreamDecoderStateString[state];
            cleanup();
            if (!userdata.called_done) {
                rsvc_errorf(done, __FILE__, __LINE__, "%s", message);
            }
            return;
        }
        cleanup();
        done(NULL);
    });
}

struct rsvc_flac_tags {
    struct rsvc_tags super;
    FLAC__Metadata_Chain* chain;
    FLAC__Metadata_Iterator* it;
    FLAC__StreamMetadata* block;
    FLAC__StreamMetadata_VorbisComment* comments;
};
typedef struct rsvc_flac_tags* rsvc_flac_tags_t;

static bool rsvc_flac_tags_remove(rsvc_tags_t tags, const char* name,
                                  rsvc_done_t fail) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    if (name) {
        FLAC__metadata_object_vorbiscomment_remove_entries_matching(self->block, name);
    } else {
        FLAC__metadata_object_vorbiscomment_resize_comments(self->block, 0);
    }
    return true;
}

static bool rsvc_flac_tags_add(rsvc_tags_t tags, const char* name, const char* value,
                               rsvc_done_t fail) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, name, value) ||
        !FLAC__metadata_object_vorbiscomment_append_comment(self->block, entry, false)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "FLAC tag error");
        return false;
    }
    return true;
}

static bool rsvc_flac_tags_each(rsvc_tags_t tags,
                         void (^block)(const char* name, const char* value, rsvc_stop_t stop)) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    __block bool loop = true;
    for (size_t i = 0; loop && (i < self->comments->num_comments); ++i) {
        char* name;
        char* value;
        if (!FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(
                self->comments->comments[i], &name, &value)) {
            // TODO(sfiera): report failure.
            continue;
        }
        block(name, value, ^{
            loop = false;
        });
        free(name);
        free(value);
    }
    return loop;
}

static void rsvc_flac_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if (!FLAC__metadata_chain_write(self->chain, true, false)) {
            rsvc_errorf(done, __FILE__, __LINE__, "comment error");
            return;
        }
        done(NULL);
    });
}

static void rsvc_flac_tags_destroy(rsvc_tags_t tags) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    FLAC__metadata_iterator_delete(self->it);
    FLAC__metadata_chain_delete(self->chain);
    free(self);
}

static struct rsvc_tags_methods flac_vptr = {
    .remove = rsvc_flac_tags_remove,
    .add = rsvc_flac_tags_add,
    .each = rsvc_flac_tags_each,
    .save = rsvc_flac_tags_save,
    .destroy = rsvc_flac_tags_destroy,
};

void rsvc_flac_open_tags(const char* path, int flags,
                         void (^done)(rsvc_tags_t, rsvc_error_t)) {
    char* path_copy = strdup(path);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        void (^cleanup)() = ^{
            free(path_copy);
        };
        struct rsvc_flac_tags tags = {
            .super = {
                .vptr   = &flac_vptr,
                .flags  = flags,
            },
            .chain      = FLAC__metadata_chain_new(),
            .it         = FLAC__metadata_iterator_new(),
            .block      = NULL,
            .comments   = NULL,
        };
        if (!FLAC__metadata_chain_read(tags.chain, path_copy)) {
            cleanup();
            FLAC__metadata_iterator_delete(tags.it);
            FLAC__metadata_chain_delete(tags.chain);
            rsvc_errorf(^(rsvc_error_t error){
                done(NULL, error);
            }, __FILE__, __LINE__, "%s",
               FLAC__Metadata_ChainStatusString[FLAC__metadata_chain_status(tags.chain)]);
            return;
        }
        FLAC__metadata_iterator_init(tags.it, tags.chain);
        // First block is STREAMINFO, which we can skip.
        while (FLAC__metadata_iterator_next(tags.it)) {
            if (FLAC__metadata_iterator_get_block_type(tags.it) != FLAC__METADATA_TYPE_VORBIS_COMMENT) {
                continue;
            }
            tags.block = FLAC__metadata_iterator_get_block(tags.it);
            tags.comments = &tags.block->data.vorbis_comment;
            break;
        }
        rsvc_flac_tags_t copy = memdup(&tags, sizeof(tags));
        cleanup();
        done(&copy->super, NULL);
    });
}

static write_encode_status_t flac_encode_write(const FLAC__StreamEncoder* encoder,
                                               const FLAC__byte bytes[], size_t nbytes,
                                               unsigned samples, unsigned current_frame,
                                               void* userdata) {
    flac_encode_userdata_t u = (flac_encode_userdata_t)userdata;
    const void* data = bytes;
    while (nbytes > 0) {
        ssize_t written = write(u->fd, data, nbytes);
        if (written <= 0) {
            return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }
        nbytes -= written;
    }
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static seek_encode_status_t flac_encode_seek(const FLAC__StreamEncoder* encoder,
                                             FLAC__uint64 absolute_byte_offset, void* userdata) {
    flac_encode_userdata_t u = (flac_encode_userdata_t)userdata;
    if (lseek(u->fd, absolute_byte_offset, SEEK_SET) < 0) {
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    }
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static tell_encode_status_t flac_encode_tell(const FLAC__StreamEncoder* encoder,
                                             FLAC__uint64* absolute_byte_offset, void* userdata) {
    flac_encode_userdata_t u = (flac_encode_userdata_t)userdata;
    off_t offset = lseek(u->fd, 0, SEEK_CUR);
    if (offset < 0) {
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    }
    *absolute_byte_offset = offset;
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static void flac_decode_metadata(const FLAC__StreamDecoder* decoder,
                                 const FLAC__StreamMetadata* metadata,
                                 void* userdata) {
    if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
        return;
    }
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    u->metadata(
            metadata->data.stream_info.bits_per_sample * metadata->data.stream_info.sample_rate,
            metadata->data.stream_info.total_samples);
}

static read_decode_status_t flac_decode_read(const FLAC__StreamDecoder* encoder,
                                             FLAC__byte bytes[], size_t* nbytes, void* userdata) {
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    void* data = bytes;
    ssize_t bytes_read = read(u->read_fd, data, *nbytes);
    if (bytes_read < 0) {
        rsvc_strerrorf(u->done, __FILE__, __LINE__, NULL);
        u->called_done = true;
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    } else if (bytes_read == 0) {
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    } else {
        *nbytes = bytes_read;
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
}

static write_decode_status_t flac_decode_write(const FLAC__StreamDecoder* decoder,
                                               const FLAC__Frame* frame,
                                               const FLAC__int32* const* data,
                                               void* userdata) {
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    size_t size = frame->header.channels * frame->header.blocksize * sizeof(int16_t);
    int16_t* s16 = malloc(size);
    int16_t* p = s16;
    for (int i = 0; i < frame->header.blocksize; ++i) {
        for (int c = 0; c < frame->header.channels; ++c) {
            *(p++) = data[c][i];
        }
    }
    while (size > 0) {
        ssize_t written = write(u->write_fd, s16, size);
        if (written <= 0) {
            rsvc_strerrorf(u->done, __FILE__, __LINE__, NULL);
            u->called_done = true;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        } else {
            size -= written;
        }
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static seek_decode_status_t flac_decode_seek(const FLAC__StreamDecoder* decoder,
                                             FLAC__uint64 absolute_byte_offset,
                                             void* userdata) {
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    if (lseek(u->read_fd, absolute_byte_offset, SEEK_SET) < 0) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static tell_decode_status_t flac_decode_tell(const FLAC__StreamDecoder* decoder,
                                             FLAC__uint64* absolute_byte_offset,
                                             void* userdata) {
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    off_t offset = lseek(u->read_fd, 0, SEEK_CUR);
    if (offset < 0) {
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }
    *absolute_byte_offset = offset;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static length_decode_status_t flac_decode_length(const FLAC__StreamDecoder* decoder,
                                                 FLAC__uint64* absolute_byte_offset,
                                                 void* userdata) {
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    struct stat st;
    if (fstat(u->read_fd, &st) < 0) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }
    *absolute_byte_offset = st.st_size;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flac_decode_eof(const FLAC__StreamDecoder* decoder, void* userdata) {
    return false;
}

static void flac_decode_error(const FLAC__StreamDecoder* decoder,
                              FLAC__StreamDecoderErrorStatus error, void* userdata) {
    fprintf(stderr, "FLAC error: %d\n", error);
}

void rsvc_flac_format_register() {
    struct rsvc_format flac = {
        .name = "flac",
        .magic = "fLaC",
        .magic_size = 4,
        .extension = "flac",
        .lossless = true,
        .open_tags = rsvc_flac_open_tags,
        .encode = rsvc_flac_encode,
        .decode = rsvc_flac_decode,
    };
    rsvc_format_register(&flac);
}
