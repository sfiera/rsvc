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

#include "ogg.h"

#include <string.h>

#include "common.h"
#include "unix.h"

void rsvc_ogg_page_clear(ogg_page* og) {
    if (og->header) {
        free(og->header);
    }
    if (og->body) {
        free(og->body);
    }
    memset(og, 0, sizeof(*og));
}

void rsvc_ogg_page_copy(ogg_page* dst, const ogg_page* src) {
    rsvc_ogg_page_clear(dst);
    dst->header         = memdup(src->header, src->header_len);
    dst->header_len     = src->header_len;
    dst->body           = memdup(src->body, src->body_len);
    dst->body_len       = src->body_len;
}

bool rsvc_ogg_page_read(FILE* file, ogg_sync_state* oy, ogg_page* og, bool* eof, rsvc_done_t fail) {
    while (true) {
        const int status = ogg_sync_pageout(oy, og);
        if (status == 1) {
            if (eof) {
                *eof = false;
            }
            return true;
        } else if (status < 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
            return false;
        }

        char* data = ogg_sync_buffer(oy, 4096);
        size_t size;
        bool inner_eof;
        if (!rsvc_read(NULL, file, data, 4096, 1, &size, &inner_eof, fail)) {
            return false;
        } else if (inner_eof) {
            if (eof) {
                *eof = inner_eof;
                return true;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "unexpected end of file");
                return false;
            }
        }
        ogg_sync_wrote(oy, size);
    }
}

void rsvc_ogg_packet_clear(ogg_packet* op) {
    if (op->packet) {
        free(op->packet);
    }
    memset(op, 0, sizeof(*op));
}

void rsvc_ogg_packet_copy(ogg_packet* dst, const ogg_packet* src) {
    rsvc_ogg_packet_clear(dst);
    dst->packet         = memdup(src->packet, src->bytes);
    dst->bytes          = src->bytes;
    dst->b_o_s          = src->b_o_s;
    dst->e_o_s          = src->e_o_s;
    dst->granulepos     = src->granulepos;
    dst->packetno       = src->packetno;
}

bool rsvc_ogg_packet_out(ogg_stream_state* os, ogg_packet* op, bool* have_op, rsvc_done_t fail) {
    int packetout_status = ogg_stream_packetout(os, op);
    if (packetout_status < 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg packet");
        return false;
    } else if (packetout_status == 0) {
        *have_op = false;
        return true;
    } else {
        *have_op = true;
        return true;
    }
}

bool rsvc_ogg_flush(FILE* file, ogg_stream_state *os, ogg_page* og, rsvc_done_t fail) {
    if (ogg_stream_flush(os, og)) {
        return rsvc_write(NULL, file, og->header, og->header_len, fail)
            && rsvc_write(NULL, file, og->body, og->body_len, fail);
    }
    return true;
}

bool rsvc_ogg_align_packet(FILE* file, ogg_stream_state *os,
                           ogg_page* og, ogg_packet* op, rsvc_done_t fail) {
    ogg_stream_packetin(os, op);
    while (ogg_stream_pageout(os, og)) {
        if (!(rsvc_write(NULL, file, og->header, og->header_len, fail) &&
              rsvc_write(NULL, file, og->body, og->body_len, fail))) {
            return false;
        }
    }
    return rsvc_ogg_flush(file, os, og, fail);
}
