//
// This file is part of Rip Service.
//
// Copyright (C) 2014 Chris Pickel <sfiera@sfzmail.com>
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

#ifndef SRC_RSVC_CANCEL_H_
#define SRC_RSVC_CANCEL_H_

#include "common.h"

typedef struct rsvc_cancel* rsvc_cancel_t;
typedef struct rsvc_cancel_handle* rsvc_cancel_handle_t;

extern struct rsvc_cancel rsvc_sigint;

rsvc_cancel_t rsvc_cancel_new();
void rsvc_cancel_destroy(rsvc_cancel_t cancel);
rsvc_cancel_handle_t rsvc_cancel_add(rsvc_cancel_t cancel, rsvc_stop_t fn);
void rsvc_cancel(rsvc_cancel_t cancel);
void rsvc_cancel_remove(rsvc_cancel_t cancel, rsvc_cancel_handle_t handle);

#endif  // RSVC_CANCEL_H_
