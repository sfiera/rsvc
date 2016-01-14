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

#include "unix.h"

#include <copyfile.h>
#include <string.h>
#include <util.h>
#include "common.h"

bool rsvc_opendev(const char* path, int oflag, mode_t mode, FILE** file, rsvc_done_t fail) {
    rsvc_logf(3, "open %s", path);
    bool ok = false;
    char* dup_path = strdup(path);
    int fd = opendev(dup_path, oflag, mode, NULL);
    if (fd < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
    } else {
        const char* fmode = "r";
        if (mode & O_RDWR) {
            if (mode & O_APPEND) {
                fmode = "a+";
            } else {
                fmode = "r+";
            }
        } else if (mode & O_WRONLY) {
            if (mode & O_APPEND) {
                fmode = "a";
            } else {
                fmode = "w";
            }
        }
        *file = fdopen(fd, fmode);
        ok = true;
    }
    free(dup_path);
    return ok;
}

bool rsvc_cp(const char* src, const char* dst, rsvc_done_t fail) {
    rsvc_logf(3, "cp %s %s", src, dst);
    FILE* src_file;
    FILE* dst_file;
    if (!rsvc_open(src, O_RDONLY, 0644, &src_file, fail)) {
        return false;
    } else if (!rsvc_open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600, &dst_file, fail)) {
        fclose(src_file);
        return false;
    } else if (fcopyfile(fileno(src_file), fileno(dst_file), NULL, COPYFILE_ALL) < 0) {
        fclose(src_file);
        fclose(dst_file);
        rsvc_strerrorf(fail, __FILE__, __LINE__, "copy %s to %s", src, dst);
        return false;
    }
    fclose(src_file);
    fclose(dst_file);
    return true;
}
