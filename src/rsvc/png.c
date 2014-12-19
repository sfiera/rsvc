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

#include <rsvc/format.h>

#include <string.h>
#include <unistd.h>

#define PNG_IHDR "IHDR"
#define PNG_PLTE "PLTE"
#define PNG_IDAT "IDAT"

#define PNG_HEADER_SIZE     8
#define PNG_CHECKSUM_SIZE   4
#define PNG_IHDR_SIZE       13

#define PNG_PALETTE_USED 0x01

static size_t read_size(char data[4]) {
    return ((uint8_t)data[0] << 24)
        + ((uint8_t)data[1] << 16)
        + ((uint8_t)data[2] << 8)
        + ((uint8_t)data[3] << 0);
}

static bool png_info(
        const char* path, int fd,
        size_t* width, size_t* height, size_t* depth, size_t* palette_size,
        rsvc_done_t fail) {
    size_t offset = 8;
    char block_header[PNG_HEADER_SIZE + 1] = {};  // one extra for '\0' after block type.
    bool got_ihdr = false;
    while (true) {
        ssize_t size = pread(fd, block_header, PNG_HEADER_SIZE, offset);
        if (size < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
            return false;
        } else if (size == 0) {
            break;
        } else if (size < PNG_HEADER_SIZE) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
            return false;
        }
        size_t block_size = read_size(block_header);
        char* block_type = block_header + 4;

        if (!got_ihdr) {
            if (strcmp(block_type, PNG_IHDR) != 0) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (IHDR not first)", path);
                return false;
            }
            got_ihdr = true;

            if (block_size != PNG_IHDR_SIZE) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (bad IHDR size)", path);
                return false;
            }

            char block[PNG_IHDR_SIZE];
            size = pread(fd, block, PNG_IHDR_SIZE, offset + PNG_HEADER_SIZE);
            if (size < 0) {
                rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
                return false;
            } else if (size < PNG_IHDR_SIZE) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
                return false;
            }
            *width = read_size(block + 0);
            *height = read_size(block + 4);
            if (block[9] & PNG_PALETTE_USED) {
                *depth = 8;
                // keep looping until we find a PLTE chunk.
            } else {
                *depth = block[8];
                *palette_size = 0;
                return true;
            }
        } else {
            if (strcmp(block_type, PNG_PLTE) == 0) {
                if ((block_size % 3) != 0) {
                    rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (bad PLTE size)", path);
                    return false;
                }
                *palette_size = block_size / 3;
                return true;
            }
        }
        offset += PNG_HEADER_SIZE + block_size + PNG_CHECKSUM_SIZE;
    }

    if (got_ihdr) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (missing PLTE)", path);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (missing IHDR)", path);
    }
    return false;
}

void rsvc_png_format_register() {
    struct rsvc_format png = {
        .name = "png",
        .mime = "image/png",
        .magic = "\211PNG\015\012\032\012",
        .magic_size = 8,
        .extension = "png",
        .image_info = png_info,
    };
    rsvc_format_register(&png);
}
