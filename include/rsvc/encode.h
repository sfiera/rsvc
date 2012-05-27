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

#ifndef RSVC_ENCODE_H_
#define RSVC_ENCODE_H_

#include <rsvc/common.h>

/// Encoding
/// ========
/// ..  toctree::
///
///     flac.h
///     vorbis.h
///
/// ..  type:: void (^rsvc_encode_progress_t)(double progress)
typedef void (^rsvc_encode_progress_t)(double progress);

typedef struct rsvc_encode_options* rsvc_encode_options_t;
struct rsvc_encode_options {
    int32_t                 bitrate;
    size_t                  samples_per_channel;
    rsvc_encode_progress_t  progress;
};

typedef void (*rsvc_encode_f)(
        int src_fd,
        int dst_fd,
        rsvc_encode_options_t options,
        rsvc_done_t done);

/// Encode Formats
/// --------------

typedef struct rsvc_encode_format* rsvc_encode_format_t;
struct rsvc_encode_format {
    const char*     name;
    const char*     extension;
    bool            lossless;
    rsvc_encode_f   encode;
};

void rsvc_encode_format_register(
        const char* name,
        const char* extension,
        bool lossless,
        rsvc_encode_f encode);

rsvc_encode_format_t rsvc_encode_format_named(
        const char* name);

bool rsvc_encode_formats_each(
        void (^block)(rsvc_encode_format_t format, rsvc_stop_t stop));

#endif  // RSVC_ENCODE_H_
