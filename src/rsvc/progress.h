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

#ifndef RSVC_PROGRESS_H_
#define RSVC_PROGRESS_H_

#include <stdlib.h>

typedef struct rsvc_progress* rsvc_progress_t;

rsvc_progress_t rsvc_progress_start(const char* name);
void rsvc_progress_update(rsvc_progress_t item, double fraction);
void rsvc_progress_done(rsvc_progress_t node);
void rsvc_outf(const char* format, ...);
void rsvc_errf(const char* format, ...);

#endif  // RSVC_PROGRESS_H_
