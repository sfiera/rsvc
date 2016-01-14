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

#ifndef SRC_RSVC_OGG_H_
#define SRC_RSVC_OGG_H_

#include <ogg/ogg.h>
#include <stdio.h>

#include <rsvc/common.h>

void rsvc_ogg_page_clear(ogg_page* og);
void rsvc_ogg_page_copy(ogg_page* dst, const ogg_page* src);
bool rsvc_ogg_page_read(FILE* file, ogg_sync_state* oy, ogg_page* og, bool* eof, rsvc_done_t fail);

void rsvc_ogg_packet_clear(ogg_packet* op);
void rsvc_ogg_packet_copy(ogg_packet* dst, const ogg_packet* src);
bool rsvc_ogg_packet_out(ogg_stream_state* os, ogg_packet* op, bool* have_op, rsvc_done_t fail);

bool rsvc_ogg_flush(FILE* file, ogg_stream_state *os, ogg_page* og, rsvc_done_t fail);
bool rsvc_ogg_align_packet(FILE* file, ogg_stream_state *os,
                           ogg_page* og, ogg_packet* op, rsvc_done_t fail);

#endif  // RSVC_OGG_H_
