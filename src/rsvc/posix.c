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

#include "posix.h"

#include <copyfile.h>
#include <errno.h>
#include <fcntl.h>
#include <rsvc/common.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool rsvc_open(const char* path, int oflag, mode_t mode, int* fd, rsvc_done_t fail) {
    rsvc_logf(3, "open %s", path);
    *fd = open(path, oflag, mode);
    if (*fd < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    return true;
}

bool rsvc_rename(const char* src, const char* dst, rsvc_done_t fail) {
    rsvc_logf(3, "rename %s %s", src, dst);
    if (rename(src, dst) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "rename %s to %s", src, dst);
        return false;
    }
    return true;
}

bool rsvc_rm(const char* path, rsvc_done_t fail) {
    rsvc_logf(3, "rm %s", path);
    if (unlink(path) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    return true;
}

bool rsvc_mkdir(const char* path, mode_t mode, rsvc_done_t fail) {
    rsvc_logf(3, "mkdir %s", path);
    if (mkdir(path, mode) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    return true;
}

bool rsvc_rmdir(const char* path, rsvc_done_t fail) {
    rsvc_logf(3, "rmdir %s", path);
    if (rmdir(path) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    return true;
}

bool rsvc_cp(const char* src, const char* dst, rsvc_done_t fail) {
    rsvc_logf(3, "cp %s %s", src, dst);
    int src_fd, dst_fd;
    if (!rsvc_open(src, O_RDONLY, 0644, &src_fd, fail)) {
        return false;
    } else if (!rsvc_open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600, &dst_fd, fail)) {
        close(src_fd);
        return false;
    } else if (fcopyfile(src_fd, dst_fd, NULL, COPYFILE_ALL) < 0) {
        close(src_fd);
        close(dst_fd);
        rsvc_strerrorf(fail, __FILE__, __LINE__, "copy %s to %s", src, dst);
        return false;
    }
    close(src_fd);
    close(dst_fd);
    return true;
}

bool rsvc_mv(const char* src, const char* dst, rsvc_done_t fail) {
    if (rsvc_rename(src, dst, ^(rsvc_error_t error){})) {
        return true;
    } else if (errno == EXDEV) {
        return rsvc_cp(src, dst, fail)
            && rsvc_rm(src, fail);
    } else {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "rename %s to %s", src, dst);
        return false;
    }
}

bool rsvc_makedirs(const char* path, mode_t mode, rsvc_done_t fail) {
    __block bool ok = true;
    rsvc_dirname(path, ^(const char* parent){
        if (strcmp(path, parent) != 0) {
            ok = rsvc_makedirs(parent, mode, fail);
        }
    });
    if (!ok) {
        return false;
    }
    if (rsvc_mkdir(path, mode, ^(rsvc_error_t error){})) {
        return true;
    } else if ((errno == EEXIST) || (errno == EISDIR)) {
        return true;
    } else {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
}

void rsvc_trimdirs(const char* path) {
    if (rsvc_rmdir(path, ^(rsvc_error_t error){})) {
        rsvc_dirname(path, ^(const char* parent){
            rsvc_trimdirs(parent);
        });
    }
}

void rsvc_dirname(const char* path, void (^block)(const char* parent)) {
    char* slash = strrchr(path, '/');
    if (slash == NULL) {
        block(".");
    } else if (slash == path) {
        block("/");
    } else if (slash[1] == '\0') {
        char* parent = strndup(path, slash - path);
        rsvc_dirname(parent, block);
        free(parent);
    } else {
        char* parent = strndup(path, slash - path);
        block(parent);
        free(parent);
    }
}
