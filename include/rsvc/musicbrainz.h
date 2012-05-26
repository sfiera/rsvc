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

#ifndef RSVC_MUSICBRAINZ_H_
#define RSVC_MUSICBRAINZ_H_

#include <musicbrainz5/mb5_c.h>

#include <rsvc/common.h>
#include <rsvc/tag.h>

void rsvc_apply_musicbrainz_tags(rsvc_tags_t tags, rsvc_done_t done);

#endif  // RSVC_MUSICBRAINZ_H_
