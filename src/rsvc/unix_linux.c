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

#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include "unix.h"

#include <linux/limits.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"

bool rsvc_opendev(const char* path, int oflag, mode_t mode, FILE** file, rsvc_done_t fail) {
    if (strchr(path, '/')) {
        return rsvc_open(path, oflag, mode, file, fail);
    }
    char devpath[PATH_MAX] = "/dev/";
    if (strlen(devpath) + strlen(path) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    strcat(devpath, path);
    return rsvc_open(devpath, oflag, mode, file, fail);
}

bool rsvc_memopen(const void* data, size_t size, FILE** file, rsvc_done_t fail) {
    if (!(*file = fmemopen((void*)data, size, "r"))) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    return true;
}

static bool rsvc_futimes(int fd, int64_t atime_sec, int64_t mtime_sec) {
    struct timeval tv[2] = {};
    tv[0].tv_sec = atime_sec;
    tv[1].tv_sec = mtime_sec;
    return futimes(fd, tv) >= 0;
}

bool rsvc_cp(const char* src, const char* dst, rsvc_done_t fail) {
    rsvc_logf(3, "cp %s %s", src, dst);
    FILE* src_file = NULL;
    FILE* dst_file = NULL;
    struct stat st;
    bool success = true;

    if (!rsvc_open(src, O_RDONLY, 0644, &src_file, fail)) {
        success = false;
    } else if (fstat(fileno(src_file), &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "stat %s", src);
        success = false;
    } else if (!rsvc_open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600, &dst_file, fail)) {
        success = false;
    } else if (fchmod(fileno(dst_file), st.st_mode) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "copy %s to %s", src, dst);
        success = false;
    } else if (!rsvc_futimes(fileno(dst_file), st.st_atime, st.st_mtime)) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "copy %s to %s", src, dst);
        success = false;
    } else if (sendfile(fileno(dst_file), fileno(src_file), NULL, st.st_size) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "copy %s to %s", src, dst);
        success = false;
    }

    fclose(src_file);
    fclose(dst_file);
    return success;
}
