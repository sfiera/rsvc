//
// This file is part of Rip Service.
//
// Copyright (C) 2016 Chris Pickel <sfiera@sfzmail.com>
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

#include <rsvc/format.h>

#include "common.h"
#include "unix.h"

#define WAV_RIFF  0x46464952
#define WAV_WAVE  0x45564157
#define WAV_FMT   0x20746d66
#define WAV_DATA  0x61746164

typedef struct riff_chunk* riff_chunk_t;
struct riff_chunk {
    uint32_t  code;
    size_t    size;
};

typedef struct wav_fmt* wav_fmt_t;
struct wav_fmt {
    uint16_t  audio_format;
    uint16_t  channels;
    uint32_t  sample_rate;
    uint32_t  byte_rate;
    uint16_t  block_align;
    uint16_t  bits_per_sample;
};

static uint32_t u32le(uint8_t data[4]) {
    return (data[0] << 0) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t u16le(uint8_t data[2]) {
    return (data[0] << 0) | (data[1] << 8);
}

static void u32le_out(uint8_t* data, uint32_t v) {
    data[0] = v >> 0;
    data[1] = v >> 8;
    data[2] = v >> 16;
    data[3] = v >> 24;
}

static void u16le_out(uint8_t* data, uint16_t v) {
    data[0] = v >> 0;
    data[1] = v >> 8;
}

static bool read_riff_chunk_header(int fd, riff_chunk_t rc, rsvc_done_t fail) {
    uint8_t header[8];
    if (!rsvc_read(NULL, fd, header, 8, NULL, NULL, fail)) {
        return false;
    }
    rc->code   = u32le(header + 0);
    rc->size   = u32le(header + 4);
    return true;
}

static bool check_is_wav(int fd, riff_chunk_t header, rsvc_done_t fail) {
    if (header->code != WAV_RIFF) {
        rsvc_errorf(fail, __FILE__, __LINE__, "not a wav file");
        return false;
    }
    uint8_t file_type[4];
    if (!rsvc_read(NULL, fd, file_type, 4, NULL, NULL, fail)) {
        return false;
    } else if (u32le(file_type) != WAV_WAVE) {
        rsvc_errorf(fail, __FILE__, __LINE__, "not a wav file");
        return false;
    }
    return true;
}

static bool read_wav_fmt(int fd, riff_chunk_t rc, wav_fmt_t wf, rsvc_done_t fail) {
    if (rc->size < 16) {
        rsvc_errorf(fail, __FILE__, __LINE__, "malformed wav fmt chunk");
        return false;
    }
    uint8_t data[16];
    if (!rsvc_read(NULL, fd, data, 16, NULL, NULL, fail)) {
        return false;
    }

    wf->audio_format     = u16le(data + 0);
    wf->channels         = u16le(data + 2);
    wf->sample_rate      = u32le(data + 4);
    wf->byte_rate        = u32le(data + 8);
    wf->block_align      = u16le(data + 12);
    wf->bits_per_sample  = u16le(data + 14);

    if (wf->audio_format != 1) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported audio format: %hu", wf->audio_format);
        return false;
    } else if (wf->channels == 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "bad channel count: %hu", wf->audio_format);
        return false;
    } else if (wf->bits_per_sample != 16) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported bits: %hu", wf->bits_per_sample);
        return false;
    } else if (wf->block_align != (2 * wf->channels)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported block align: %hu", wf->block_align);
        return false;
    } else if (wf->byte_rate != (wf->block_align * wf->sample_rate)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "unsupported byte rate: %u", wf->byte_rate);
        return false;
    }
    if (lseek(fd, rc->size - 16, SEEK_CUR) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    return true;
}

// Leaves `fd` positioned at the start of the data chunk.
bool wav_audio_info(int fd, rsvc_audio_info_t info, rsvc_done_t fail) {
    struct riff_chunk header;
    if (!(read_riff_chunk_header(fd, &header, fail) &&
          check_is_wav(fd, &header, fail))) {
        return false;
    }

    struct wav_fmt wf;
    bool have_fmt = false;
    while (true) {
        struct riff_chunk rc;
        if (!read_riff_chunk_header(fd, &rc, fail)) {
            return false;
        } else if (rc.code == WAV_FMT) {
            if (have_fmt) {
                rsvc_errorf(fail, __FILE__, __LINE__, "duplicate wav fmt chunk");
                return false;
            } else {
                have_fmt = true;
            }
            if (!read_wav_fmt(fd, &rc, &wf, fail)) {
                return false;
            }
            info->sample_rate      = wf.sample_rate;
            info->channels         = wf.channels;
            info->bits_per_sample  = wf.bits_per_sample;
        } else if (rc.code == WAV_DATA) {
            if (!have_fmt) {
                rsvc_errorf(fail, __FILE__, __LINE__, "missing wav fmt chunk");
                return false;
            } else if (rc.size % wf.block_align) {
                rsvc_errorf(fail, __FILE__, __LINE__, "trailing bytes in wav data");
                return false;
            }
            info->samples_per_channel = rc.size / wf.block_align;
            return true;
        } else {
            if (!lseek(fd, rc.size, SEEK_CUR)) {
                rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
                return false;
            }
        }
    }
}

bool wav_audio_decode(int src_fd, int dst_fd, rsvc_decode_info_f info, rsvc_done_t fail) {
    struct rsvc_audio_info i;
    if (!wav_audio_info(src_fd, &i, fail)) {
        return false;
    }
    info(&i);
    size_t remainder = (i.bits_per_sample / 8) * i.samples_per_channel * i.channels;
    while (remainder) {
        uint8_t data[4096];
        size_t size = (remainder > 4096) ? 4096 : remainder;
        if (!(rsvc_read(NULL, src_fd, data, size, NULL, NULL, fail) &&
              rsvc_write(NULL, dst_fd, data, size, NULL, NULL, fail))) {
            return false;
        }
        remainder -= size;
    }
    return true;
}

bool wav_audio_encode(int src_fd, int dst_fd, rsvc_encode_options_t opts, rsvc_done_t fail) {
    static const int header_size = 44;
    uint8_t header[header_size];
    size_t sampl = opts->info.sample_rate;
    size_t chans = opts->info.channels;
    size_t bytes = opts->info.bits_per_sample / 8;
    size_t data_size = opts->info.samples_per_channel * chans * bytes;
    size_t riff_size = header_size - 4 + data_size;
    u32le_out(header + 0,   WAV_RIFF);
    u32le_out(header + 4,   riff_size);
    u32le_out(header + 8,   WAV_WAVE);
    u32le_out(header + 12,  WAV_FMT);
    u32le_out(header + 16,  16);
    u16le_out(header + 20,  1);
    u16le_out(header + 22,  chans);
    u32le_out(header + 24,  sampl);
    u32le_out(header + 28,  sampl * chans * bytes);
    u16le_out(header + 32,  chans * bytes);
    u16le_out(header + 34,  bytes * 8);
    u32le_out(header + 36,  WAV_DATA);
    u32le_out(header + 40,  data_size);

    if (!rsvc_write(NULL, dst_fd, header, header_size, NULL, NULL, fail)) {
        return false;
    }

    size_t remainder = data_size;
    while (remainder) {
        uint8_t data[4096];
        size_t size = (remainder > 4096) ? 4096 : remainder;
        if (!(rsvc_read(NULL, src_fd, data, size, NULL, NULL, fail) &&
              rsvc_write(NULL, dst_fd, data, size, NULL, NULL, fail))) {
            return false;
        }
        remainder -= size;
        opts->progress((data_size - remainder) / (double)data_size);
    }
    return true;
}

const struct rsvc_format rsvc_wav = {
    .format_group = RSVC_AUDIO,
    .name = "wav",
    .mime = "audio/x-wav",
    .magic = {"RIFF????WAVE"},
    .magic_size = 12,
    .extension = "wav",
    .lossless = true,
    .audio_info = wav_audio_info,
    .decode = wav_audio_decode,
    .encode = wav_audio_encode,
};
