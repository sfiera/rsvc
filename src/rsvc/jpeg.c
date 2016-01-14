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

#define JPEG_SOI 0xD8
#define JPEG_EOI 0xD9
#define JPEG_SOF0 0xC0
#define JPEG_SOF2 0xC2

static size_t read_short(const uint8_t data[2]) {
    return (data[0] << 8) + (data[1] << 0);
}

static bool jpeg_consume_marker(  const char* path, FILE* file, uint8_t marker[2],
                                  rsvc_done_t fail) {
    if (!rsvc_read(path, file, marker, 1, 2, NULL, NULL, fail)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
        return false;
    }
    return true;
}

static bool jpeg_info(const char* path, FILE* file, rsvc_image_info_t info, rsvc_done_t fail) {
    bool got_soi = false;
    uint8_t marker[2];
    while (true) {
        if (!jpeg_consume_marker(path, file, marker, fail)) {
            return false;
        }
        if (marker[0] != 0xff) {
            rsvc_errorf(
                    fail, __FILE__, __LINE__, "%s: invalid jpeg (bad marker 0x%02x%02x)",
                    path, marker[0], marker[1]);
            return false;
        }
        rsvc_logf(3, "jpeg marker 0x%02x%02x", marker[0], marker[1]);

        if (!got_soi) {
            if (marker[1] != JPEG_SOI) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (SOI not first)", path);
                return false;
            }
            got_soi = true;
            continue;
        }

        switch (marker[1]) {
          case 0xff:
            if (!rsvc_seek(file, 1, SEEK_CUR, fail)) {
                return false;
            }
            continue;

          case 0x01:
          case 0xd0:
          case 0xd1:
          case 0xd2:
          case 0xd3:
          case 0xd4:
          case 0xd5:
          case 0xd6:
          case 0xd7:
            if (!rsvc_seek(file, 2, SEEK_CUR, fail)) {
                return false;
            }
            continue;

          case JPEG_SOI:
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (unexpected SOI)", path);
            return false;

          case JPEG_EOI:
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (unexpected EOI)", path);
            return false;
        }

        uint8_t ss_data[2];
        if (!rsvc_read(path, file, ss_data, 1, 2, NULL, NULL, fail)) {
            return false;
        }
        uint16_t segment_size = read_short(ss_data);

        if ((marker[1] == JPEG_SOF0) || (marker[1] == JPEG_SOF2)) {
            if (segment_size < 5) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (bad SOFn size)", path);
                return false;
            }
            uint8_t data[5];
            if (!rsvc_read(path, file, data, 1, 5, NULL, NULL, fail)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
                return false;
            }
            info->depth = data[0];
            info->width = read_short(data + 1);
            info->height = read_short(data + 3);
            info->palette_size = 0;
            return true;
        }

        if (!rsvc_seek(file, segment_size - 2, SEEK_CUR, fail)) {
            return false;
        }
    }

    if (got_soi) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (missing SOFn)", path);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (missing SOI)", path);
    }
    return false;
}

const struct rsvc_format rsvc_jpeg = {
    .format_group = RSVC_IMAGE,
    .name = "jpeg",
    .mime = "image/jpeg",
    .magic = {"??????JFIF"},
    .magic_size = 10,
    .extension = "jpg",
    .image_info = jpeg_info,
};
