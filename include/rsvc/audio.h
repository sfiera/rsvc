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
typedef struct rsvc_audio_meta* rsvc_audio_meta_t;
struct rsvc_audio_meta {
    int32_t                 bitrate;
    size_t                  sample_rate;
    size_t                  channels;
    size_t                  samples_per_channel;
};

/// Encoding
/// --------
/// ..  type:: void (^rsvc_encode_progress_t)(double progress)
typedef void (^rsvc_encode_progress_t)(double progress);

typedef struct rsvc_encode_options* rsvc_encode_options_t;
struct rsvc_encode_options {
    struct rsvc_audio_meta  meta;
    rsvc_encode_progress_t  progress;
};

typedef void (*rsvc_encode_f)(
        int src_fd,
        int dst_fd,
        rsvc_encode_options_t options,
        rsvc_done_t done);

/// Decoding
/// --------
typedef void (^rsvc_decode_metadata_f)(rsvc_audio_meta_t meta);

typedef void (*rsvc_decode_f)(
        int src_fd,
        int dst_fd,
        rsvc_decode_metadata_f metadata,
        rsvc_done_t done);

#endif  // RSVC_AUDIO_H_
