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
#include <rsvc/audio.h>
#include <rsvc/common.h>
#include <rsvc/image.h>
#include <rsvc/tag.h>

/// Formats
/// -------

enum rsvc_format_group {
    RSVC_AUDIO,
    RSVC_VIDEO,
    RSVC_IMAGE,
};

struct rsvc_format {
    enum rsvc_format_group
                        format_group;
    const char*         name;
    const char*         mime;

    const char*         magic[4];
    size_t              magic_size;
    const char*         extension;
    bool                lossless;

    rsvc_open_tags_f    open_tags;
    rsvc_encode_f       encode;
    rsvc_decode_f       decode;
    rsvc_audio_info_f   audio_info;

    rsvc_image_info_f   image_info;
};

rsvc_format_t*          rsvc_formats;
size_t                  rsvc_nformats;

void                    rsvc_format_register(rsvc_format_t format);

rsvc_format_t           rsvc_format_named(const char* name);
rsvc_format_t           rsvc_format_with_mime(const char* mime);
bool                    rsvc_format_detect(const char* path, int fd,
                                           rsvc_format_t* format, rsvc_done_t fail);
const char*             rsvc_format_group_name(enum rsvc_format_group format_group);

#define rsvc_formats_foreach(FORMAT) \
    for (rsvc_format_t *__iter = rsvc_formats, FORMAT = *__iter; \
         __iter < (rsvc_formats + rsvc_nformats); \
         FORMAT = *(++__iter))

#endif  // RSVC_FORMAT_H_
