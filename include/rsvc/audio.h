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

#ifndef RSVC_AUDIO_H_
#define RSVC_AUDIO_H_

#include <rsvc/common.h>

/// Audio
/// =====
struct rsvc_audio_info {
    size_t  sample_rate;
    size_t  channels;
    size_t  samples_per_channel;
    size_t  bits_per_sample;
};

typedef bool (*rsvc_audio_info_f)(int fd, rsvc_audio_info_t info, rsvc_done_t fail);

bool rsvc_audio_info_validate(rsvc_audio_info_t info, rsvc_done_t fail);

/// Encoding
/// --------
/// ..  type:: void (^rsvc_encode_progress_f)(double progress)
typedef void (^rsvc_encode_progress_f)(double progress);

struct rsvc_encode_options {
    struct rsvc_audio_info  info;
    int32_t                 bitrate;
    rsvc_encode_progress_f  progress;
};

typedef bool (*rsvc_encode_f)(
        int src_fd,
        int dst_fd,
        rsvc_encode_options_t options,
        rsvc_done_t fail);

/// Decoding
/// --------
typedef void (^rsvc_decode_info_f)(rsvc_audio_info_t meta);

typedef bool (*rsvc_decode_f)(
        int src_fd,
        int dst_fd,
        rsvc_decode_info_f info,
        rsvc_done_t fail);

/// Formats
/// -------
void rsvc_audio_formats_register();

extern const struct rsvc_format rsvc_flac;
extern const struct rsvc_format rsvc_mp3;
extern const struct rsvc_format rsvc_m4a;
extern const struct rsvc_format rsvc_m4v;  // TODO(sfiera): video.h
extern const struct rsvc_format rsvc_opus;
extern const struct rsvc_format rsvc_vorbis;
extern const struct rsvc_format rsvc_wav;

#endif  // RSVC_AUDIO_H_
