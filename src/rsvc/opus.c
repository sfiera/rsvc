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

#define _POSIX_C_SOURCE 200809L

#include "audio.h"

#include <opus.h>
#include <rsvc/format.h>

const struct rsvc_format rsvc_opus = {
    .format_group = RSVC_AUDIO,
    .name = "opus",
    .mime = "audio/opus",
    .magic = {"OggS????????????????????????OpusHead"},
    .magic_size = 36,
    .extension = "opus",
    .lossless = false,
};
