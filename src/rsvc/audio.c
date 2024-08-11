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

#include "audio.h"

#include <rsvc/format.h>

static bool valid_channels(size_t channels) {
    return (1 <= channels) && (channels <= 8);
}

static bool valid_sample_rate(size_t sample_rate) {
    return sample_rate >= 1;
}

static bool valid_bit_depth(size_t depth) {
    return (depth == 32)
        || (depth == 24)
        || (depth == 16)
        || (depth == 8)
        || (depth == 4)
        || (depth == 2)
        || (depth == 1);
}

static bool valid_block_align(size_t block_align, size_t channels, size_t bit_depth) {
    // TODO(sfiera): allow packing for bit_depth < 8
    if (bit_depth <= 8) {
        return block_align == (channels * sizeof(int8_t));
    } else if (bit_depth == 16) {
        return block_align == (channels * sizeof(int16_t));
    } else if (bit_depth <= 32) {
        return block_align == (channels * sizeof(int32_t));
    } else {
        return false;
    }
}

bool rsvc_audio_info_validate(rsvc_audio_info_t info, rsvc_done_t fail) {
    if (!valid_channels(info->channels)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid channel count: %zu", info->channels);
        return false;
    } else if (!valid_sample_rate(info->sample_rate)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid sample rate: %zu", info->sample_rate);
        return false;
    } else if (!valid_bit_depth(info->bits_per_sample)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid bit depth: %zu", info->bits_per_sample);
        return false;
    } else if (!valid_block_align(info->block_align, info->channels, info->bits_per_sample)) {
        rsvc_errorf(  fail, __FILE__, __LINE__,
                      "invalid %zu-bit block align: %zu",
                      info->bits_per_sample, info->block_align);
        return false;
    }
    return true;
}

const struct rsvc_format rsvc_mp3 = {
    .format_group = RSVC_AUDIO,
    .name = "mp3",
    .mime = "audio/mpeg",
    .magic = {"ID3"},
    .magic_size = 3,
    .extension = "mp3",
    .lossless = false,
    .open_tags = rsvc_id3_open_tags,
    .encode = rsvc_lame_encode,
    .decode = rsvc_mad_decode,
    .audio_info = rsvc_mad_audio_info,
};

// TODO(sfiera): allow alac encoding too.
const struct rsvc_format rsvc_m4a = {
    .format_group = RSVC_AUDIO,
    .name = "m4a",
    .mime = "audio/mp4",
    .magic = {"????ftypM4A "},
    .magic_size = 12,
    .extension = "m4a",
    .lossless = false,
    .encode = rsvc_aac_encode,
    .audio_info = rsvc_m4a_audio_info,
    .open_tags = rsvc_mp4_open_tags,
};

const struct rsvc_format rsvc_m4v = {
    .format_group = RSVC_VIDEO,
    .name = "m4v",
    .mime = "video/mp4",
    .magic = {
        "????ftypM4V ",
        "????ftypmp42",
        "????ftypisom",
    },
    .magic_size = 12,
    .extension = "m4v",
    .open_tags = rsvc_mp4_open_tags,
};

void rsvc_audio_formats_register() {
    rsvc_format_register(&rsvc_flac);
    rsvc_format_register(&rsvc_mp3);
    rsvc_format_register(&rsvc_m4a);
    rsvc_format_register(&rsvc_m4v);  // TODO(sfiera): video.c
    rsvc_format_register(&rsvc_opus);
    rsvc_format_register(&rsvc_vorbis);
    rsvc_format_register(&rsvc_wav);
}
