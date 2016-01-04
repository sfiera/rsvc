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

#define JPEG_SOI 0xD8
#define JPEG_EOI 0xD9
#define JPEG_SOF0 0xC0
#define JPEG_SOF2 0xC2

static size_t read_short(const uint8_t data[2]) {
    return (data[0] << 8) + (data[1] << 0);
}

static bool jpeg_consume_marker(
        const char* path, const uint8_t** data, size_t* size,
        uint8_t marker[2], rsvc_done_t fail) {
    if (*size < 2) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
        return false;
    }
    memcpy(marker, *data, 2);
    *data += 2;
    *size -= 2;
    return true;
}

static bool jpeg_info(
        const char* path, const uint8_t* data, size_t size,
        struct rsvc_image_info* info, rsvc_done_t fail) {
    bool got_soi = false;
    uint8_t marker[2];
    while (true) {
        if (!jpeg_consume_marker(path, &data, &size, marker, fail)) {
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
            data += 1;
            size -= 1;
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
            data += 2;
            size -= 2;
            continue;

          case JPEG_SOI:
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (unexpected SOI)", path);
            return false;

          case JPEG_EOI:
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (unexpected EOI)", path);
            return false;
        }

        if (size < 2) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
            return false;
        }
        size_t segment_size = read_short(data);
        data += 2;
        size -= 2;

        if ((marker[1] == JPEG_SOF0) || (marker[1] == JPEG_SOF2)) {
            if (segment_size < 5) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (bad SOFn size)", path);
                return false;
            }
            if (size < segment_size) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
                return false;
            }
            info->depth = data[0];
            info->width = read_short(data + 1);
            info->height = read_short(data + 3);
            info->palette_size = 0;
            return true;
        }

        data += segment_size - 2;
        size -= segment_size - 2;
    }

    if (got_soi) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (missing SOFn)", path);
    } else {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (missing SOI)", path);
    }
    return false;
}

void rsvc_jpeg_format_register() {
    struct rsvc_format jpeg = {
        .name = "jpeg",
        .mime = "image/jpeg",
        .magic = {"??????JFIF"},
        .magic_size = 10,
        .extension = "jpg",
        .image_info = jpeg_info,
    };
    rsvc_format_register(&jpeg);
}
