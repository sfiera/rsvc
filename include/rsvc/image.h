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

#ifndef RSVC_IMAGE_H_
#define RSVC_IMAGE_H_

#include <stdlib.h>
#include <stdbool.h>
#include <rsvc/common.h>

/// Images
/// -------

struct rsvc_image_info {
    size_t width, height;
    size_t depth;
    size_t palette_size;
};

typedef bool (*rsvc_image_info_f)(
        const char* path, const uint8_t* data, size_t size,
        struct rsvc_image_info* info, rsvc_done_t fail);

void rsvc_image_formats_register();

void rsvc_gif_format_register();
void rsvc_jpeg_format_register();
void rsvc_png_format_register();

#endif  // RSVC_IMAGE_H_
