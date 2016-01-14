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

#include "audio.h"

#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <Block.h>
#include <dispatch/dispatch.h>
#include <rsvc/common.h>
#include <rsvc/format.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "unix.h"

OSStatus core_audio_read(
        void* userdata, SInt64 offset, UInt32 size, void* data, UInt32* actual_size) {
    FILE* file = userdata;
    ssize_t read = pread(fileno(file), data, size, offset);
    if (read < 0) {
        return kAudioFileUnspecifiedError;
    }
    *actual_size = read;
    return noErr;
}

OSStatus core_audio_write(
        void* userdata, SInt64 offset, UInt32 size, const void* data, UInt32* actual_size) {
    FILE* file = userdata;
    ssize_t wrote = pwrite(fileno(file), data, size, offset);
    if (wrote < 0) {
        return kAudioFileUnspecifiedError;
    }
    *actual_size = wrote;
    return noErr;
}

SInt64 core_audio_get_size(void* userdata) {
    FILE* file = userdata;
    struct stat st;
    if (fstat(fileno(file), &st) < 0) {
        return -1;
    }
    return st.st_size;
}

OSStatus core_audio_set_size(void* userdata, SInt64 size) {
    FILE* file = userdata;
    if (ftruncate(fileno(file), size) < 0) {
        return kAudioFileUnspecifiedError;
    }
    return noErr;
}

typedef struct four_char {
    char string[5];
} four_char_t;

static four_char_t fourcc(uint32_t value) {
    four_char_t result = {{
        0xff & (value >> 24),
        0xff & (value >> 16),
        0xff & (value >> 8),
        0xff & value,
        '\0',
    }};
    return result;
}

bool core_audio_encode_options_validate(rsvc_encode_options_t opts, rsvc_done_t fail) {
    if (!rsvc_audio_info_validate(&opts->info, fail)) {
        return false;
    }
    return true;
}

static bool core_audio_encode(
        FILE* src_file, FILE* dst_file, rsvc_encode_options_t options,
        int container_id, int codec_id, rsvc_done_t fail) {
    int32_t                 bitrate   = options->bitrate;
    struct rsvc_audio_info  info      = options->info;
    rsvc_encode_progress_f  progress  = options->progress;
    if (!core_audio_encode_options_validate(options, fail)) {
        return false;
    }

    OSStatus err;
    AudioStreamBasicDescription asbd_out = {
        .mFormatID          = codec_id,
        .mSampleRate        = info.sample_rate,
        .mChannelsPerFrame  = info.channels,
    };
    if (codec_id == kAudioFormatAppleLossless) {
        asbd_out.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
    }

    FILE* file = dst_file;
    AudioFileID file_id = NULL;
    err = AudioFileInitializeWithCallbacks(
        file, core_audio_read, core_audio_write, core_audio_get_size, core_audio_set_size,
        container_id, &asbd_out, 0, &file_id);
    if (err != noErr) {
        rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
        return false;
    }
    void (^cleanup)() = ^{
        AudioFileClose(file_id);
    };
    ExtAudioFileRef file_ref = NULL;
    err = ExtAudioFileWrapAudioFileID(file_id, true, &file_ref);
    if (err != noErr) {
        cleanup();
        rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
        return false;
    }
    cleanup = ^{
        ExtAudioFileDispose(file_ref);
        cleanup();
    };

    const AudioStreamBasicDescription asbd_in = {
        .mFormatID          = kAudioFormatLinearPCM,
        .mFormatFlags       = kAudioFormatFlagIsSignedInteger
                            | kAudioFormatFlagsNativeEndian
                            | kAudioFormatFlagIsPacked,
        .mBitsPerChannel    = info.bits_per_sample,
        .mSampleRate        = info.sample_rate,
        .mChannelsPerFrame  = info.channels,
        .mFramesPerPacket   = 1,
        .mBytesPerPacket    = info.block_align,
        .mBytesPerFrame     = info.block_align,
    };
    err = ExtAudioFileSetProperty(
            file_ref, kExtAudioFileProperty_ClientDataFormat, sizeof(asbd_in), &asbd_in);
    if (err != noErr) {
        cleanup();
        rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
        return false;
    }

    // Set the bitrate if encoding with a lossy codec.
    if (codec_id != kAudioFormatAppleLossless) {
        AudioConverterRef converter = NULL;
        UInt32 converter_size = sizeof(converter);
        err = ExtAudioFileGetProperty(
                file_ref, kExtAudioFileProperty_AudioConverter, &converter_size, &converter);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return false;
        }
        if (converter == NULL) {
            cleanup();
            rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return false;
        }
        err = AudioConverterSetProperty(
                converter, kAudioConverterEncodeBitRate, sizeof(bitrate), &bitrate);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return false;
        }
        CFArrayRef converter_properties;
        UInt32 converter_properties_size = sizeof(converter_properties);
        err = AudioConverterGetProperty(
                converter, kAudioConverterPropertySettings,
                &converter_properties_size, &converter_properties);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return false;
        }
        err = ExtAudioFileSetProperty(
                file_ref, kExtAudioFileProperty_ConverterConfig,
                sizeof(converter_properties), &converter_properties);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return false;
        }
    }

    size_t samples_per_channel_read = 0;
    static const int kSamples = 4096;
    uint8_t buffer[kSamples];
    while (true) {
        bool eof;
        size_t nsamples;
        if (!rsvc_cread(NULL, src_file, buffer, kSamples / info.block_align, info.block_align,
                &nsamples, NULL, &eof, fail)) {
            return false;
        } else if (eof) {
            break;
        }
        samples_per_channel_read += nsamples;
        AudioBufferList buffer_list = {
            .mNumberBuffers = 1,
            .mBuffers = {{
                .mData = buffer,
                .mNumberChannels = info.channels,
                .mDataByteSize = nsamples * info.block_align,
            }},
        };
        err = ExtAudioFileWrite(file_ref, nsamples, &buffer_list);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(fail, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return false;
        }
        progress(samples_per_channel_read * 1.0 / info.samples_per_channel);
    }
    ExtAudioFileDispose(file_ref);
    AudioFileClose(file_id);
    cleanup();
    return true;
}

bool rsvc_aac_encode(FILE* src_file, FILE* dst_file, rsvc_encode_options_t options, rsvc_done_t fail) {
    return core_audio_encode(  src_file, dst_file, options,
                               kAudioFileM4AType, kAudioFormatMPEG4AAC, fail);
}

bool rsvc_alac_encode(FILE* src_file, FILE* dst_file, rsvc_encode_options_t options, rsvc_done_t fail) {
    return core_audio_encode(  src_file, dst_file, options,
                               kAudioFileM4AType, kAudioFormatAppleLossless, fail);
}
