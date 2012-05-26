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

#ifndef SRC_RSVC_POSIX_H_
#define SRC_RSVC_POSIX_H_

#include <fcntl.h>
#include <stdbool.h>
#include <rsvc/common.h>

bool rsvc_open(const char* path, int oflag, mode_t mode, int* fd, rsvc_done_t fail);
bool rsvc_rename(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_rm(const char* path, rsvc_done_t fail);
bool rsvc_mkdir(const char* path, mode_t mode, rsvc_done_t fail);
bool rsvc_rmdir(const char* path, rsvc_done_t fail);

bool rsvc_cp(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_mv(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_makedirs(const char* path, mode_t mode, rsvc_done_t fail);
void rsvc_trimdirs(const char* path);

void rsvc_dirname(const char* path, void (^block)(const char* parent));

#endif  // SRC_RSVC_POSIX_H_
