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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <rsvc/common.h>
#include <rsvc/forward.h>

/// Images
/// -------

struct rsvc_image_info {
    size_t width, height;
    size_t depth;
    size_t palette_size;
};

typedef bool (*rsvc_image_info_f)(
        const char* path, FILE* file,
        rsvc_image_info_t info, rsvc_done_t fail);

/// Formats
/// -------
void rsvc_image_formats_register();

extern const struct rsvc_format rsvc_gif;
extern const struct rsvc_format rsvc_jpeg;
extern const struct rsvc_format rsvc_png;

#endif  // RSVC_IMAGE_H_
