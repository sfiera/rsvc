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

#include <rsvc/core-audio.h>

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

OSStatus core_audio_read(
        void* userdata, SInt64 offset, UInt32 size, void* data, UInt32* actual_size) {
    int fd = *(int*)userdata;
    ssize_t read = pread(fd, data, size, offset);
    if (read < 0) {
        return kAudioFileUnspecifiedError;
    }
    *actual_size = read;
    return noErr;
}

OSStatus core_audio_write(
        void* userdata, SInt64 offset, UInt32 size, const void* data, UInt32* actual_size) {
    int fd = *(int*)userdata;
    ssize_t wrote = pwrite(fd, data, size, offset);
    if (wrote < 0) {
        return kAudioFileUnspecifiedError;
    }
    *actual_size = wrote;
    return noErr;
}

SInt64 core_audio_get_size(void* userdata) {
    int fd = *(int*)userdata;
    struct stat st;
    if (fstat(fd, &st) < 0) {
        return -1;
    }
    return st.st_size;
}

OSStatus core_audio_set_size(void* userdata, SInt64 size) {
    int fd = *(int*)userdata;
    if (ftruncate(fd, size) < 0) {
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

static void core_audio_encode(
        int src_fd, int dst_fd, rsvc_encode_options_t options,
        int container_id, int codec_id, rsvc_done_t done) {
    int32_t bitrate                     = options->bitrate;
    size_t samples_per_channel          = options->samples_per_channel;
    size_t channels                     = options->channels;
    rsvc_encode_progress_t progress     = options->progress;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        OSStatus err;
        AudioStreamBasicDescription asbd_out = {
            .mFormatID          = codec_id,
            .mSampleRate        = 44100.0,
            .mChannelsPerFrame  = channels,
        };
        if (codec_id == kAudioFormatAppleLossless) {
            asbd_out.mFormatFlags       = kAppleLosslessFormatFlag_16BitSourceData;
        }

        int fd = dst_fd;
        AudioFileID file_id = NULL;
        err = AudioFileInitializeWithCallbacks(
            &fd, core_audio_read, core_audio_write, core_audio_get_size, core_audio_set_size,
            container_id, &asbd_out, 0, &file_id);
        if (err != noErr) {
            rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return;
        }
        void (^cleanup)() = ^{
            AudioFileClose(file_id);
        };
        ExtAudioFileRef file_ref = NULL;
        err = ExtAudioFileWrapAudioFileID(file_id, true, &file_ref);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return;
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
            .mBitsPerChannel    = 16,
            .mSampleRate        = 44100.0,
            .mChannelsPerFrame  = channels,
            .mFramesPerPacket   = 1,
            .mBytesPerPacket    = 2 * channels,
            .mBytesPerFrame     = 2 * channels,
        };
        err = ExtAudioFileSetProperty(
                file_ref, kExtAudioFileProperty_ClientDataFormat, sizeof(asbd_in), &asbd_in);
        if (err != noErr) {
            cleanup();
            rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
            return;
        }

        // Set the bitrate if encoding with a lossy codec.
        if (codec_id != kAudioFormatAppleLossless) {
            AudioConverterRef converter = NULL;
            UInt32 converter_size = sizeof(converter);
            err = ExtAudioFileGetProperty(
                    file_ref, kExtAudioFileProperty_AudioConverter, &converter_size, &converter);
            if (err != noErr) {
                cleanup();
                rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
                return;
            }
            if (converter == NULL) {
                cleanup();
                rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
                return;
            }
            err = AudioConverterSetProperty(
                    converter, kAudioConverterEncodeBitRate, sizeof(bitrate), &bitrate);
            if (err != noErr) {
                cleanup();
                rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
                return;
            }
            CFArrayRef converter_properties;
            UInt32 converter_properties_size = sizeof(converter_properties);
            err = AudioConverterGetProperty(
                    converter, kAudioConverterPropertySettings,
                    &converter_properties_size, &converter_properties);
            if (err != noErr) {
                cleanup();
                rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
                return;
            }
            err = ExtAudioFileSetProperty(
                    file_ref, kExtAudioFileProperty_ConverterConfig,
                    sizeof(converter_properties), &converter_properties);
            if (err != noErr) {
                cleanup();
                rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
                return;
            }
        }

        size_t samples_per_channel_read = 0;
        size_t start = 0;
        static const int kSamples = 4096;
        uint8_t buffer[kSamples];
        while (true) {
            ssize_t result = read(src_fd, buffer + start, sizeof(buffer) - start);
            if (result > 0) {
                result += start;
                size_t remainder = result % (2 * channels);
                result -= remainder;
                size_t nsamples = result / (2 * channels);
                samples_per_channel_read += nsamples;
                if (result > 0) {
                    AudioBufferList buffer_list = {
                        .mNumberBuffers = 1,
                        .mBuffers = {{
                            .mData = buffer,
                            .mNumberChannels = channels,
                            .mDataByteSize = nsamples * channels * 2,
                        }},
                    };
                    err = ExtAudioFileWrite(file_ref, nsamples, &buffer_list);
                    if (err != noErr) {
                        cleanup();
                        rsvc_errorf(done, __FILE__, __LINE__, "some error: %s", fourcc(err).string);
                        return;
                    }
                    memcpy(buffer, buffer + result, remainder);
                }
                start = remainder;
                progress(samples_per_channel_read * 1.0 / samples_per_channel);
            } else if (result == 0) {
                break;
            } else if (errno != EINTR) {
                cleanup();
                rsvc_strerrorf(done, __FILE__, __LINE__, "pipe");
                return;
            }
        }
        ExtAudioFileDispose(file_ref);
        AudioFileClose(file_id);
        cleanup();
        done(NULL);
    });
}

void rsvc_aac_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t done) {
    core_audio_encode(src_fd, dst_fd, options, kAudioFileM4AType, kAudioFormatMPEG4AAC, done);
}

void rsvc_alac_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t done) {
    core_audio_encode(src_fd, dst_fd, options, kAudioFileM4AType, kAudioFormatAppleLossless, done);
}

void rsvc_core_audio_format_register() {
    struct rsvc_format aac = {
        .name = "aac",
        .mime = "audio/mp4",
        .extension = "m4a",
        .lossless = false,
        .encode = rsvc_aac_encode,
    };
    struct rsvc_format alac = {
        .name = "alac",
        .mime = "audio/mp4",
        .extension = "m4a",
        .lossless = true,
        .encode = rsvc_alac_encode,
    };
    rsvc_format_register(&aac);
    rsvc_format_register(&alac);
}
