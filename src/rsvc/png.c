//
// This file is part of Rip Service.
//
// Copyright (C) 2014 Chris Pickel <sfiera@sfzmail.com>
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

#define PNG_IHDR 0x49484452
#define PNG_PLTE 0x504c5445

#define PNG_HEADER_SIZE     8
#define PNG_CHECKSUM_SIZE   4
#define PNG_IHDR_SIZE       13

#define PNG_PALETTE_USED 0x01

static size_t read_size(const uint8_t data[4]) {
    return (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + (data[3] << 0);
}

static bool png_consume_file_header(const char* path, FILE* file, rsvc_done_t fail) {
    uint8_t data[8];
    if (!rsvc_read(path, file, data, 1, 8, NULL, NULL, fail)) {
        return false;
    }
    return true;
}

static bool png_consume_block_header(  const char* path, FILE* file, size_t* block_type,
                                       size_t* block_size, rsvc_done_t fail) {
    uint8_t data[PNG_HEADER_SIZE];
    if (!rsvc_read(path, file, data, 1, PNG_HEADER_SIZE, NULL, NULL, fail)) {
        return false;
    }
    *block_size = read_size(&data[0]);
    *block_type = read_size(&data[4]);
    return true;
}

static bool png_info(const char* path, FILE* file, rsvc_image_info_t info, rsvc_done_t fail) {
    if (!png_consume_file_header(path, file, fail)) {
        return false;
    }

    bool got_ihdr = false;
    while (true) {
        size_t block_type, block_size;
        if (!png_consume_block_header(path, file, &block_type, &block_size, fail)) {
            return false;
        }
        rsvc_logf(3, "png header %08zx", block_type);

        if (!got_ihdr) {
            if (block_type != PNG_IHDR) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (IHDR not first)", path);
                return false;
            }
            got_ihdr = true;

            uint8_t header[PNG_IHDR_SIZE];
            if (block_size != PNG_IHDR_SIZE) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (bad IHDR size)", path);
                return false;
            } else if (!rsvc_read(path, file, header, 1, PNG_IHDR_SIZE, NULL, NULL, fail)) {
                return false;
            }

            info->width = read_size(&header[0]);
            info->height = read_size(&header[4]);
            if (header[9] & PNG_PALETTE_USED) {
                info->depth = 8;
                // keep looping until we find a PLTE chunk.
            } else {
                info->depth = header[8];
                info->palette_size = 0;
                return true;
            }
        } else {
            if (block_type == PNG_PLTE) {
                if ((block_size % 3) != 0) {
                    rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (bad PLTE size)", path);
                    return false;
                }
                info->palette_size = block_size / 3;
                return true;
            }
        }
        if (!rsvc_seek(file, block_size + PNG_CHECKSUM_SIZE, SEEK_CUR, fail)) {
            return false;
        }
    }

    if (got_ihdr) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (missing PLTE)", path);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (missing IHDR)", path);
    }
    return false;
}

const struct rsvc_format rsvc_png = {
    .format_group = RSVC_IMAGE,
    .name = "png",
    .mime = "image/png",
    .magic = {"\211PNG\015\012\032\012"},
    .magic_size = 8,
    .extension = "png",
    .image_info = png_info,
};
