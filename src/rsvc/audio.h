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

#ifndef SRC_RSVC_AUDIO_H_
#define SRC_RSVC_AUDIO_H_

#include <rsvc/audio.h>
#include <rsvc/tag.h>

#ifdef __APPLE__
bool rsvc_aac_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t fail);
bool rsvc_alac_encode(int src_fd, int dst_fd, rsvc_encode_options_t options, rsvc_done_t fail);
#else
#define rsvc_aac_encode NULL
#define rsvc_alac_encode NULL
#endif

bool                    rsvc_mp4_open_tags(const char* path, int flags,
                                           rsvc_tags_t* tags, rsvc_done_t fail);

bool                    rsvc_id3_open_tags(const char* path, int flags,
                                           rsvc_tags_t* tags, rsvc_done_t fail);
bool                    rsvc_id3_skip_tags(int fd, rsvc_done_t fail);
bool                    rsvc_lame_encode(  int src_fd, int dst_fd,
                                           rsvc_encode_options_t options, rsvc_done_t fail);
bool                    rsvc_mad_decode(int src_fd, int dst_fd,
                                        rsvc_decode_info_f info, rsvc_done_t fail);
bool                    rsvc_mad_audio_info(int fd, rsvc_audio_info_t info, rsvc_done_t fail);

#endif  // RSVC_AUDIO_H_
