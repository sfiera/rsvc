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

#include <Block.h>
#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>
#include <ctype.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audio.h"
#include "common.h"
#include "unix.h"

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

bool rsvc_flac_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t fail) {
    struct rsvc_audio_meta meta         = options->meta;
    rsvc_encode_progress_f progress     = options->progress;

    FLAC__StreamEncoder *encoder = NULL;
    // FLAC__StreamMetadata* comment_metadata = NULL;
    // FLAC__StreamMetadata* padding_metadata = NULL;
    size_t samples_per_channel_read = 0;

    encoder = FLAC__stream_encoder_new();
    if (encoder == NULL) {
        rsvc_errorf(fail, __FILE__, __LINE__, "couldn't allocate FLAC encoder");
        return false;
    }
    void (^cleanup)() = ^{
        FLAC__stream_encoder_delete(encoder);
    };

    if (!(FLAC__stream_encoder_set_verify(encoder, true) &&
          FLAC__stream_encoder_set_compression_level(encoder, 8) &&
          FLAC__stream_encoder_set_channels(encoder, meta.channels) &&
          FLAC__stream_encoder_set_bits_per_sample(encoder, 16) &&
          FLAC__stream_encoder_set_sample_rate(encoder, meta.sample_rate) &&
          FLAC__stream_encoder_set_total_samples_estimate(encoder, meta.samples_per_channel))) {
        FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
        const char* message = FLAC__StreamEncoderStateString[state];
        rsvc_errorf(fail, __FILE__, __LINE__, "%s", message);
    }

    // comment_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    // padding_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
    // padding_metadata->length = 1024;
    // if (!comment_metadata || !rsvc_tags_to_vorbis_comments(tags, comment_metadata)
    //         || !padding_metadata) {
    //     cleanup();
    //     rsvc_errorf(fail, __FILE__, __LINE__, "comment failure");
    //     return false;
    // }
    //
    // FLAC__StreamMetadata* metadata[2] = {comment_metadata, padding_metadata};
    // if (!FLAC__stream_encoder_set_metadata(encoder, metadata, 2)) {
    //     cleanup();
    //     rsvc_errorf(fail, __FILE__, __LINE__, "comment failure");
    //     return false;
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
        rsvc_errorf(fail, __FILE__, __LINE__, "%s", message);
        return false;
    }

    static const int kSamples = 2048;
    int16_t buffer[kSamples * 2];
    size_t size_inout = 0;
    bool eof = false;
    while (!eof) {
        size_t nsamples;
        if (!rsvc_cread("pipe", src_fd, buffer, kSamples, 2 * sizeof(int16_t),
                        &nsamples, &size_inout, &eof, fail)) {
            return false;
        } else if (nsamples) {
            samples_per_channel_read += nsamples;
            FLAC__int32 samples[kSamples * 2];
            FLAC__int32* sp = samples;
            for (int16_t* p = buffer; p < buffer + (nsamples * 2); ++p) {
                *(sp++) = *p;
            }
            if (!FLAC__stream_encoder_process_interleaved(encoder, samples, nsamples)) {
                FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
                const char* message = FLAC__StreamEncoderStateString[state];
                cleanup();
                rsvc_errorf(fail, __FILE__, __LINE__, "pipe: %s", message);
                return false;
            }
            progress(samples_per_channel_read * 1.0 / meta.samples_per_channel);
        }
    }
    if (!FLAC__stream_encoder_finish(encoder)) {
        FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
        const char* message = FLAC__StreamEncoderStateString[state];
        cleanup();
        rsvc_errorf(fail, __FILE__, __LINE__, "%s", message);
        return false;
    }
    cleanup();
    return true;
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
    FLAC__StreamMetadata* block;
    FLAC__StreamMetadata_VorbisComment* comments;
};
typedef struct rsvc_flac_tags* rsvc_flac_tags_t;

static bool rsvc_flac_tags_remove(rsvc_tags_t tags, const char* name,
                                  rsvc_done_t fail) {
    (void)fail;  // Always succeeds.
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
    if (!(FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, name, value)
          && FLAC__metadata_object_vorbiscomment_append_comment(self->block, entry, false))) {
        rsvc_errorf(fail, __FILE__, __LINE__, "FLAC tag error");
        return false;
    }
    return true;
}

static void flac_break(rsvc_iter_t super_it);
static bool flac_next(rsvc_iter_t super_it);

static struct rsvc_iter_methods flac_iter_vptr = {
    .next    = flac_next,
    .break_  = flac_break,
};

typedef struct flac_tags_iter* flac_tags_iter_t;
struct flac_tags_iter {
    struct rsvc_tags_iter                super;
    FLAC__StreamMetadata_VorbisComment*  comments;
    int                                  i;
    char*                                name_storage;
    char*                                value_storage;
};

static rsvc_tags_iter_t flac_begin(rsvc_tags_t tags) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    struct flac_tags_iter iter = {
        .super = {
            .vptr = &flac_iter_vptr,
        },
        .comments  = self->comments,
        .i         = 0,
    };
    flac_tags_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static void flac_break(rsvc_iter_t super_it) {
    flac_tags_iter_t it = DOWN_CAST(struct flac_tags_iter, (rsvc_tags_iter_t)super_it);
    if (it->name_storage) {
        free(it->name_storage);
        free(it->value_storage);
        it->name_storage = 0;
        it->value_storage = 0;
    }
    free(it);
}

static bool flac_next(rsvc_iter_t super_it) {
    flac_tags_iter_t it = DOWN_CAST(struct flac_tags_iter, (rsvc_tags_iter_t)super_it);
    if (it->name_storage) {
        free(it->name_storage);
        free(it->value_storage);
        it->name_storage = 0;
        it->value_storage = 0;
    }
    while (it->i < it->comments->num_comments) {
        if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(
                it->comments->comments[it->i++], &it->name_storage, &it->value_storage)) {
            for (char* cp = it->name_storage; *cp; ++cp) {
                *cp = toupper(*cp);
            }
            it->super.name = it->name_storage;
            it->super.value = it->value_storage;
            return true;
        }
        // else TODO(sfiera): report failure.
    }
    flac_break(super_it);
    return false;
}

static void flac_image_break(rsvc_iter_t super_it);
static bool flac_image_next(rsvc_iter_t super_it);

static struct rsvc_iter_methods flac_image_iter_vptr = {
    .next    = flac_image_next,
    .break_  = flac_image_break,
};

typedef struct flac_tags_image_iter* flac_tags_image_iter_t;
struct flac_tags_image_iter {
    struct rsvc_tags_image_iter  super;
    FLAC__Metadata_Iterator*     it;
};

static rsvc_tags_image_iter_t flac_image_begin(rsvc_tags_t tags) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    struct flac_tags_image_iter iter = {
        .super = {
            .vptr = &flac_image_iter_vptr,
        },
        .it  = FLAC__metadata_iterator_new(),
    };
    FLAC__metadata_iterator_init(iter.it, self->chain);
    flac_tags_image_iter_t copy = memdup(&iter, sizeof(iter));
    return &copy->super;
}

static void flac_image_break(rsvc_iter_t super_it) {
    flac_tags_image_iter_t it = DOWN_CAST(struct flac_tags_image_iter, (rsvc_tags_image_iter_t)super_it);
    FLAC__metadata_iterator_delete(it->it);
    free(it);
}

static bool flac_image_next(rsvc_iter_t super_it) {
    flac_tags_image_iter_t it = DOWN_CAST(struct flac_tags_image_iter, (rsvc_tags_image_iter_t)super_it);
    // First block is STREAMINFO, which we can skip.
    while (FLAC__metadata_iterator_next(it->it)) {
        if (FLAC__metadata_iterator_get_block_type(it->it) != FLAC__METADATA_TYPE_PICTURE) {
            continue;
        }
        FLAC__StreamMetadata* metadata = FLAC__metadata_iterator_get_block(it->it);
        FLAC__StreamMetadata_Picture* picture = &metadata->data.picture;
        rsvc_format_t format = rsvc_format_with_mime(picture->mime_type);
        if (!format || !format->image_info) {
            continue;
        }
        it->super.format  = format;
        it->super.data    = picture->data;
        it->super.size    = picture->data_length;
        return true;
    }
    flac_image_break(super_it);
    return false;
}

static bool rsvc_flac_tags_image_remove(
        rsvc_tags_t tags, size_t* index, rsvc_done_t fail) {
    (void)fail;  // Always succeeds.
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    FLAC__Metadata_Iterator* it = FLAC__metadata_iterator_new();
    FLAC__metadata_iterator_init(it, self->chain);
    size_t i = 0;
    // First block is STREAMINFO, which we can skip.
    while (FLAC__metadata_iterator_next(it)) {
        if (FLAC__metadata_iterator_get_block_type(it) != FLAC__METADATA_TYPE_PICTURE) {
            continue;
        }
        if (index) {
            if (*index == i++) {
                FLAC__metadata_iterator_delete_block(it, false);
                break;
            }
        } else {
            FLAC__metadata_iterator_delete_block(it, false);
        }
    }
    FLAC__metadata_iterator_delete(it);
    return true;
}

static bool rsvc_flac_tags_image_add(
        rsvc_tags_t tags, rsvc_format_t format, const uint8_t* data, size_t size,
        rsvc_done_t fail) {
    struct rsvc_image_info info;
    if (!format->image_info("image", data, size, &info, fail)) {
        return false;
    }

    FLAC__StreamMetadata *metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE);
    metadata->data.picture.type = FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER;
    metadata->data.picture.width = info.width;
    metadata->data.picture.height = info.height;
    metadata->data.picture.depth = info.depth;
    metadata->data.picture.colors = info.palette_size;
    if (!(FLAC__metadata_object_picture_set_mime_type(metadata, (char*)format->mime, true)
          && FLAC__metadata_object_picture_set_description(metadata, (unsigned char*)"", true)
          && FLAC__metadata_object_picture_set_data(metadata, (uint8_t*)data, size, true))) {
        rsvc_errorf(fail, __FILE__, __LINE__, "memory error");
        return false;
    }
    const char* err;
    if (!FLAC__format_picture_is_legal(&metadata->data.picture, &err)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s", err);
        return false;
    }

    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    FLAC__Metadata_Iterator* it = FLAC__metadata_iterator_new();
    FLAC__metadata_iterator_init(it, self->chain);
    while (FLAC__metadata_iterator_next(it)) {
        continue;
    }
    if (!FLAC__metadata_iterator_insert_block_after(it, metadata)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "error inserting picture block");
        FLAC__metadata_iterator_delete(it);
        return false;
    }
    FLAC__metadata_iterator_delete(it);
    return true;
}

static bool rsvc_flac_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    if (!FLAC__metadata_chain_write(self->chain, true, false)) {
        rsvc_errorf(done, __FILE__, __LINE__, "comment error");
        return false;
    }
    return true;
}

static void rsvc_flac_tags_destroy(rsvc_tags_t tags) {
    rsvc_flac_tags_t self = DOWN_CAST(struct rsvc_flac_tags, tags);
    FLAC__metadata_chain_delete(self->chain);
    free(self);
}

static struct rsvc_tags_methods flac_vptr = {
    .remove            = rsvc_flac_tags_remove,
    .add               = rsvc_flac_tags_add,
    .save              = rsvc_flac_tags_save,
    .destroy           = rsvc_flac_tags_destroy,
    .image_remove      = rsvc_flac_tags_image_remove,
    .image_add         = rsvc_flac_tags_image_add,

    .iter_begin        = flac_begin,
    .image_iter_begin  = flac_image_begin,
};

bool rsvc_flac_open_tags(const char* path, int flags, rsvc_tags_t* tags, rsvc_done_t fail) {
    struct rsvc_flac_tags flac = {
        .super = {
            .vptr   = &flac_vptr,
            .flags  = flags,
        },
        .chain      = FLAC__metadata_chain_new(),
        .block      = NULL,
        .comments   = NULL,
    };
    FLAC__Metadata_Iterator* it = FLAC__metadata_iterator_new();
    if (!FLAC__metadata_chain_read(flac.chain, path)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s",
           FLAC__Metadata_ChainStatusString[FLAC__metadata_chain_status(flac.chain)]);
        FLAC__metadata_iterator_delete(it);
        FLAC__metadata_chain_delete(flac.chain);
        return false;
    }
    FLAC__metadata_iterator_init(it, flac.chain);
    // First block is STREAMINFO, which we can skip.
    while (FLAC__metadata_iterator_next(it)) {
        if (FLAC__metadata_iterator_get_block_type(it) != FLAC__METADATA_TYPE_VORBIS_COMMENT) {
            continue;
        }
        flac.block = FLAC__metadata_iterator_get_block(it);
        flac.comments = &flac.block->data.vorbis_comment;
        break;
    }
    FLAC__metadata_iterator_delete(it);

    rsvc_flac_tags_t copy = memdup(&flac, sizeof(flac));
    *tags = &copy->super;
    return true;
}

static write_encode_status_t flac_encode_write(const FLAC__StreamEncoder* encoder,
                                               const FLAC__byte bytes[], size_t nbytes,
                                               unsigned samples, unsigned current_frame,
                                               void* userdata) {
    (void)encoder;
    (void)samples;
    (void)current_frame;
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
    (void)encoder;
    flac_encode_userdata_t u = (flac_encode_userdata_t)userdata;
    if (lseek(u->fd, absolute_byte_offset, SEEK_SET) < 0) {
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    }
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static tell_encode_status_t flac_encode_tell(const FLAC__StreamEncoder* encoder,
                                             FLAC__uint64* absolute_byte_offset, void* userdata) {
    (void)encoder;
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
    (void)decoder;
    if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
        return;
    }
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    struct rsvc_audio_meta meta = {
        .channels = metadata->data.stream_info.channels,
        .sample_rate = metadata->data.stream_info.sample_rate,
        .samples_per_channel = metadata->data.stream_info.total_samples,
    };
    u->metadata(&meta);
}

static read_decode_status_t flac_decode_read(const FLAC__StreamDecoder* encoder,
                                             FLAC__byte bytes[], size_t* nbytes, void* userdata) {
    (void)encoder;
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    void* data = bytes;
    while (true) {
        ssize_t bytes_read = read(u->read_fd, data, *nbytes);
        if (bytes_read > 0) {
            *nbytes = bytes_read;
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        } else if (bytes_read == 0) {
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        } else if (errno != EINTR) {
            rsvc_strerrorf(u->done, __FILE__, __LINE__, NULL);
            u->called_done = true;
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }
    }
}

// TODO(sfiera): support 24-bit better.
// This function simply clips samples larger than 16 bits.  It should
// instead do dithering and ideally noise-shaping.  Or we should allow
// 24-bit samples through the pipe.
static write_decode_status_t flac_decode_write(const FLAC__StreamDecoder* decoder,
                                               const FLAC__Frame* frame,
                                               const FLAC__int32* const* data,
                                               void* userdata) {
    (void)decoder;
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    int shift = frame->header.bits_per_sample - 16;
    size_t size = frame->header.channels * frame->header.blocksize * sizeof(int16_t);
    int16_t* s16 = malloc(size);
    int16_t* p = s16;
    for (int i = 0; i < frame->header.blocksize; ++i) {
        for (int c = 0; c < frame->header.channels; ++c) {
            *(p++) = data[c][i] >> shift;
        }
    }
    while (size > 0) {
        ssize_t written = write(u->write_fd, s16, size);
        if (written <= 0) {
            rsvc_strerrorf(u->done, __FILE__, __LINE__, NULL);
            u->called_done = true;
            free(s16);
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        } else {
            size -= written;
        }
    }
    free(s16);
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static seek_decode_status_t flac_decode_seek(const FLAC__StreamDecoder* decoder,
                                             FLAC__uint64 absolute_byte_offset,
                                             void* userdata) {
    (void)decoder;
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    if (lseek(u->read_fd, absolute_byte_offset, SEEK_SET) < 0) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static tell_decode_status_t flac_decode_tell(const FLAC__StreamDecoder* decoder,
                                             FLAC__uint64* absolute_byte_offset,
                                             void* userdata) {
    (void)decoder;
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
    (void)decoder;
    flac_decode_userdata_t u = (flac_decode_userdata_t)userdata;
    struct stat st;
    if (fstat(u->read_fd, &st) < 0) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }
    *absolute_byte_offset = st.st_size;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flac_decode_eof(const FLAC__StreamDecoder* decoder, void* userdata) {
    (void)decoder;
    (void)userdata;
    return false;
}

static void flac_decode_error(const FLAC__StreamDecoder* decoder,
                              FLAC__StreamDecoderErrorStatus error, void* userdata) {
    (void)decoder;
    (void)userdata;
    if (error != FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC) {
        rsvc_logf(0, "FLAC error: %s", FLAC__StreamDecoderErrorStatusString[error]);
    }
}

const struct rsvc_format rsvc_flac = {
    .format_group = RSVC_AUDIO,
    .name = "flac",
    .mime = "audio/x-flac",
    .magic = {"fLaC"},
    .magic_size = 4,
    .extension = "flac",
    .lossless = true,
    .open_tags = rsvc_flac_open_tags,
    .encode = rsvc_flac_encode,
    .decode = rsvc_flac_decode,
};
