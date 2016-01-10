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
    .open_tags = rsvc_mp4_open_tags,
};

const struct rsvc_format rsvc_m4v = {
    .format_group = RSVC_VIDEO,
    .name = "m4v",
    .mime = "video/mp4",
    .magic = {
        "????ftypM4V ",
        "????ftypmp42",
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
