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

bool rsvc_ogg_page_read(int fd, ogg_sync_state* oy, ogg_page* og, bool* eof, rsvc_done_t fail) {
    while (true) {
        const int status = ogg_sync_pageout(oy, og);
        if (status == 1) {
            *eof = false;
            return true;
        } else if (status < 0) {
            rsvc_errorf(fail, __FILE__, __LINE__, "error reading ogg page");
            return false;
        }

        char* data = ogg_sync_buffer(oy, 4096);
        size_t size;
        if (!rsvc_read(NULL, fd, data, 4096, &size, eof, fail)) {
            return false;
        } else if (*eof) {
            return true;
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

bool rsvc_ogg_flush(int dst_fd, ogg_stream_state *os, ogg_page* og, rsvc_done_t fail) {
    if (ogg_stream_flush(os, og)) {
        return rsvc_write(NULL, dst_fd, og->header, og->header_len, NULL, NULL, fail)
            && rsvc_write(NULL, dst_fd, og->body, og->body_len, NULL, NULL, fail);
    }
    return true;
}

bool rsvc_ogg_align_packet(int dst_fd, ogg_stream_state *os,
                           ogg_page* og, ogg_packet* op, rsvc_done_t fail) {
    ogg_stream_packetin(os, op);
    while (ogg_stream_pageout(os, og)) {
        if (!(rsvc_write(NULL, dst_fd, og->header, og->header_len, NULL, NULL, fail) &&
              rsvc_write(NULL, dst_fd, og->body, og->body_len, NULL, NULL, fail))) {
            return false;
        }
    }
    return rsvc_ogg_flush(dst_fd, os, og, fail);
}