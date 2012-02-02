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

#ifndef RSVC_FLAC_H_
#define RSVC_FLAC_H_

#include <stdlib.h>
#include <rsvc/encode.h>
#include <rsvc/tag.h>

/// FLAC
/// ====
/// Encoder for :abbr:`FLAC (Free Lossless Audio Codec)`.
///
/// ..  function:: void rsvc_flac_encode(int read_fd, int file, size_t samples_per_channel, rsvc_tags_t tags, rsvc_encode_progress_t progress, rsvc_encode_done_t done)
void                    rsvc_flac_encode(int read_fd, int file, size_t samples_per_channel,
                                         rsvc_tags_t tags,
                                         rsvc_encode_progress_t progress,
                                         rsvc_encode_done_t done);
void                    rsvc_flac_read_tags(const char* path, rsvc_tags_t tags,
                                            void (^done)(rsvc_error_t));
void                    rsvc_flac_write_tags(const char* path, rsvc_tags_t tags,
                                             void (^done)(rsvc_error_t));

#endif  // RSVC_FLAC_H_
