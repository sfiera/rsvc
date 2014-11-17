//
// This file is part of Rip Service.
//
// Copyright (C) 2013 Chris Pickel <sfiera@sfzmail.com>
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

#ifndef RSVC_FORMAT_H_
#define RSVC_FORMAT_H_

#include <stdlib.h>
#include <stdbool.h>
#include <rsvc/common.h>
#include <rsvc/encode.h>
#include <rsvc/decode.h>
#include <rsvc/tag.h>

/// Formats
/// -------

typedef void (*rsvc_open_tags_f)(const char* path, int flags,
                                 void (^done)(rsvc_tags_t, rsvc_error_t));

struct rsvc_format {
    const char*         name;
    const char*         mime;

    size_t              magic_size;
    const char*         magic;
    const char*         extension;
    bool                lossless;

    rsvc_open_tags_f    open_tags;
    rsvc_encode_f       encode;
    rsvc_decode_f       decode;

    bool                image;
};

enum rsvc_format_detect_flags {
    RSVC_FORMAT_OPEN_TAGS   = 1 << 0,
    RSVC_FORMAT_ENCODE      = 1 << 1,
    RSVC_FORMAT_DECODE      = 1 << 2,
    RSVC_FORMAT_IMAGE       = 1 << 3,
};

void                    rsvc_format_register(rsvc_format_t format);

rsvc_format_t           rsvc_format_named(const char* name, int flags);
rsvc_format_t           rsvc_format_with_mime(const char* mime, int flags);
bool                    rsvc_format_detect(const char* path, int fd, int flags,
                                           rsvc_format_t* format, rsvc_done_t fail);
bool                    rsvc_formats_each(void (^block)(rsvc_format_t format, rsvc_stop_t stop));

#endif  // RSVC_FORMAT_H_
