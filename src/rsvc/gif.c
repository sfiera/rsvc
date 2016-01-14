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

#define _POSIX_C_SOURCE 200809L

#include <rsvc/image.h>

#include <rsvc/format.h>
#include <string.h>
#include <unistd.h>
#include "unix.h"

static bool gif_info(const char* path, FILE* file, rsvc_image_info_t info, rsvc_done_t fail) {
    char data[11];
    if (!rsvc_read(path, file, data, 1, 11, NULL, NULL, fail)) {
        return false;
    }
    info->width = (data[7] << 8) + data[6];
    info->height = (data[9] << 8) + data[8];
    info->depth = 8;
    info->palette_size = 2 << (data[10] & 0x08);
    return true;
}

const struct rsvc_format rsvc_gif = {
    .format_group = RSVC_IMAGE,
    .name = "gif",
    .mime = "image/gif",
    .magic = {"GIF87a", "GIF89a"},
    .magic_size = 6,
    .extension = "gif",
    .image_info = gif_info,
};
