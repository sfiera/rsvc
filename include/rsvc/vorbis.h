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

#ifndef RSVC_VORBIS_H_
#define RSVC_VORBIS_H_

#include <stdlib.h>
#include <rsvc/common.h>
#include <rsvc/encode.h>
#include <rsvc/tag.h>

/// Ogg Vorbis
/// ==========
/// Encoder for Ogg Vorbis.
///
/// ..  function:: void rsvc_vorbis_encode(int read_fd, int file, size_t samples_per_channel, int bitrate, rsvc_encode_progress_t progress, rsvc_done_t done)
void                    rsvc_vorbis_encode(int read_fd, int write_fd, size_t samples_per_channel,
                                           int bitrate,
                                           rsvc_encode_progress_t progress,
                                           rsvc_done_t done);

/// ..  function:: void rsvc_vorbis_read_tags(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t));
void                    rsvc_vorbis_read_tags(const char* path,
                                              void (^done)(rsvc_tags_t, rsvc_error_t));

#endif  // RSVC_VORBIS_H_
