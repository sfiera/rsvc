//
// This file is part of Rip Service.
//
// Copyright (C) 2012 Chris Pickel <sfiera@sfzmail.com>
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

#include "encoding.h"

static void utf8_out(uint32_t rune, char** out) {
    if (rune < 0x80) {
        *((*out)++) = rune;
    } else if (rune < 0x800) {
        *((*out)++) = 0xc0 | (rune & 0x1f); rune >>= 6;
        *((*out)++) = 0x80 | (rune & 0x3f);
    } else if (rune < 0x10000) {
        *((*out)++) = 0xe0 | (rune & 0x0f); rune >>= 6;
        *((*out)++) = 0x80 | (rune & 0x3f); rune >>= 6;
        *((*out)++) = 0x80 | (rune & 0x3f);
    } else if (rune < 0x110000) {
        *((*out)++) = 0xf0 | (rune & 0x07); rune >>= 6;
        *((*out)++) = 0x80 | (rune & 0x3f); rune >>= 6;
        *((*out)++) = 0x80 | (rune & 0x3f); rune >>= 6;
        *((*out)++) = 0x80 | (rune & 0x3f);
    }
}

void rsvc_decode_latin1(uint8_t* data, size_t size,
                        void (^done)(const char* text, size_t size, rsvc_error_t error)) {
    char* utf8 = malloc((2 * size) - 1);
    char* out = utf8;
    while (size) {
        utf8_out(*(data++), &out);
        --size;
    }
    done(utf8, out - utf8, NULL);
    free(utf8);
}

void rsvc_decode_utf16(uint8_t* data, size_t size, bool big_endian,
                       void (^done)(const char* text, size_t size, rsvc_error_t error)) {
    rsvc_done_t fail = ^(rsvc_error_t error){
        done(NULL, 0, error);
    };
    if (size % 2) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-16 data");
        return;
    }
    char* utf8 = malloc(2 * size);
    char* out = utf8;
    bool surrogate = false;
    uint32_t surrogate_head;
    for (uint8_t* it = data, *end = data + size; it != end; it += 2) {
        uint32_t rune = 0;
        if (big_endian) {
            rune |= it[0]; rune <<= 8;
            rune |= it[1];
        } else {
            rune |= it[1]; rune <<= 8;
            rune |= it[0];
        }
        if (surrogate) {
            if ((0xd800 <= rune) && (rune < 0xdc00)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-16 data");
                return;
            } else if ((0xdc00 <= rune) && (rune < 0xe000)) {
                surrogate = false;
                rune |= surrogate_head;
            }
        } else {
            if ((0xd800 <= rune) && (rune < 0xdc00)) {
                surrogate = true;
                surrogate_head = (rune & 0x3ff) << 10;
                continue;
            } else if ((0xdc00 <= rune) && (rune < 0xe000)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-16 data");
                return;
            }
        }
        utf8_out(rune, &out);
        data += 2;
        size -= 2;
    }

    if (surrogate) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-16 data");
    } else {
        done(utf8, out - utf8, NULL);
        free(utf8);
    }
}

void rsvc_decode_utf16be(uint8_t* data, size_t size,
                         void (^done)(const char* text, size_t size, rsvc_error_t error)) {
    return rsvc_decode_utf16(data, size, true, done);
}

void rsvc_decode_utf16le(uint8_t* data, size_t size,
                         void (^done)(const char* text, size_t size, rsvc_error_t error)) {
    return rsvc_decode_utf16(data, size, false, done);
}

void rsvc_decode_utf16bom(uint8_t* data, size_t size,
                          void (^done)(const char* text, size_t size, rsvc_error_t error)) {
    if ((size >= 2) && (data[0] == 0xfe) && (data[1] == 0xff)) {
        return rsvc_decode_utf16be(data + 2, size - 2, done);
    } else if ((size >= 2) && (data[0] == 0xff) && (data[1] == 0xfe)) {
        return rsvc_decode_utf16le(data + 2, size - 2, done);
    } else {
        rsvc_done_t fail = ^(rsvc_error_t error){
            done(NULL, 0, error);
        };
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-16 data");
    }
}

void rsvc_decode_utf8(uint8_t* data, size_t size,
                      void (^done)(const char* text, size_t size, rsvc_error_t error)) {
    rsvc_done_t fail = ^(rsvc_error_t error){
        done(NULL, 0, error);
    };
    int continuation = 0;
    uint8_t* curr = data;
    while (size) {
        uint8_t byte = *(curr++);
        --size;
        if (continuation) {
            if ((byte & 0xc0) != 0x80) {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-8 data");
                return;
            }
            --continuation;
        } else if (byte & 0x80) {
            if ((byte & 0xe0) == 0xc0) {
                continuation = 1;
            } else if ((byte & 0xf0) == 0xe0) {
                continuation = 2;
            } else if ((byte & 0xf1) == 0xf0) {
                continuation = 3;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-8 data");
                return;
            }
        }
    }

    if (continuation) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid UTF-8 data");
    } else {
        done((const char*)data, curr - data, NULL);
    }
}
