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

#ifndef RSVC_GROUP_H_
#define RSVC_GROUP_H_

#include <dispatch/dispatch.h>

#include "common.h"

typedef struct rsvc_group* rsvc_group_t;

rsvc_group_t rsvc_group_create(dispatch_queue_t queue, rsvc_done_t done);
rsvc_done_t rsvc_group_add(rsvc_group_t group);
void rsvc_group_ready(rsvc_group_t group);
void rsvc_group_on_cancel(rsvc_group_t group, rsvc_stop_t stop);
void rsvc_group_cancel(rsvc_group_t group, rsvc_error_t error);

#endif  // RSVC_GROUP_H_
