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

#include <rsvc/format.h>

#define JPEG_SOI 0xD8
#define JPEG_EOI 0xD9
#define JPEG_SOF0 0xC0
#define JPEG_SOF2 0xC2

static size_t read_short(char data[2]) {
    return ((uint8_t)data[0] << 8)
        + ((uint8_t)data[1] << 0);
}

static bool jpeg_info(
        const char* path, int fd,
        size_t* width, size_t* height, size_t* depth, size_t* palette_size,
        rsvc_done_t fail) {
    size_t offset = 0;
    bool got_soi = false;
    uint8_t marker[2];
    while (true) {
        ssize_t size = pread(fd, marker, 2, offset);
        if (size < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
            return false;
        } else if (size == 0) {
            break;
        } else if (size < 2) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
            return false;
        }
        if (marker[0] != 0xff) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (bad marker)", path);
            return false;
        }

        if (!got_soi) {
            if (marker[1] != JPEG_SOI) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (SOI not first)", path);
                return false;
            }
            got_soi = true;
            offset += 2;
            continue;
        }

        switch (marker[1]) {
          case 0xff:
            offset += 1;
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
            offset += 2;
            continue;

          case JPEG_SOI:
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (unexpected SOI)", path);
            return false;

          case JPEG_EOI:
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid jpeg (unexpected EOI)", path);
            return false;
        }

        char segment_size_bytes[2];
        size = pread(fd, segment_size_bytes, 2, offset + 2);
        if (size < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
            return false;
        } else if (size < 2) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
            return false;
        }
        size_t segment_size = read_short(segment_size_bytes);

        if ((marker[1] == JPEG_SOF0) || (marker[1] == JPEG_SOF2)) {
            if (segment_size < 7) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: invalid png (bad SOFn size)", path);
                return false;
            }
            char segment[5];
            size = pread(fd, segment, 5, offset + 4);
            if (size < 0) {
                rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
                return false;
            } else if (size < 2) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", path);
                return false;
            }
            *depth = segment[0];
            *width = read_short(segment + 1);
            *height = read_short(segment + 3);
            *palette_size = 0;
            return true;
        }

        offset += 2 + segment_size;
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
        .magic = "??????JFIF",
        .magic_size = 10,
        .extension = "jpg",
        .image_info = jpeg_info,
    };
    rsvc_format_register(&jpeg);
}
