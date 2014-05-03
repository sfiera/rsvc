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
#include <sys/stat.h>

bool rsvc_open(const char* path, int oflag, mode_t mode, int* fd, rsvc_done_t fail);
bool rsvc_opendev(const char* path, int oflag, mode_t mode, int* fd, rsvc_done_t fail);
bool rsvc_temp(const char* base, mode_t mode, char* path, int* fd, rsvc_done_t fail);
bool rsvc_rename(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_refile(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_rm(const char* path, rsvc_done_t fail);
bool rsvc_mkdir(const char* path, mode_t mode, rsvc_done_t fail);
bool rsvc_rmdir(const char* path, rsvc_done_t fail);

bool rsvc_walk(char* path, int options, rsvc_done_t fail,
               bool (^callback)(unsigned short info, const char* dirname, const char* basename,
                                struct stat* st, rsvc_done_t fail));

bool rsvc_cp(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_mv(const char* src, const char* dst, rsvc_done_t fail);
bool rsvc_makedirs(const char* path, mode_t mode, rsvc_done_t fail);
void rsvc_trimdirs(const char* path);

void rsvc_dirname(const char* path, void (^block)(const char* parent));

bool rsvc_pipe(int* read_pipe, int* write_pipe, rsvc_done_t fail);

bool rsvc_read(const char* name, int fd, void* data, size_t size,
               size_t* size_out, bool* eof, rsvc_done_t fail);
bool rsvc_cread(const char* name, int fd, void* data, size_t count, size_t size,
                size_t* count_out, size_t* size_inout, bool* eof, rsvc_done_t fail);
bool rsvc_write(const char* name, int fd, const void* data, size_t size,
                size_t* size_out, bool* eof, rsvc_done_t fail);

#endif  // SRC_RSVC_POSIX_H_
