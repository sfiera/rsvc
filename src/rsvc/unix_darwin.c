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
#include <sys/errno.h>
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

typedef struct memopen_data* memopen_data_t;
struct memopen_data {
    const char*  data;
    size_t       size;
    size_t       fpos;
};

int mem_read(void* cookie, char* data, int size) {
    memopen_data_t u = cookie;
    if (u->fpos >= u->size) {
        return 0;
    }
    size_t remainder = u->size - u->fpos;
    if (remainder < size) {
        size = remainder;
    }
    memcpy(data, u->data + u->fpos, size);
    u->fpos += size;
    return size;
};

fpos_t mem_seek(void* cookie, fpos_t off, int whence) {
    memopen_data_t u = cookie;
    switch (whence) {
        case SEEK_SET:
          u->fpos = off;
          break;
        case SEEK_END:
          if (off > u->size) {
              u->fpos = 0;
          } else {
              u->fpos = u->size - off;
          }
          break;
        case SEEK_CUR:
          u->fpos += off;
          break;
        default:
            errno = EINVAL;
            return -1;
    }
    return u->fpos;
}

int mem_close(void* cookie) {
    memopen_data_t u = cookie;
    free(u);
    return 0;
}

bool rsvc_memopen(const void* data, size_t size, FILE** file, rsvc_done_t fail) {
    struct memopen_data cookie = {
        .data  = data,
        .size  = size,
        .fpos  = 0,
    };
    *file = funopen(memdup(&cookie, sizeof(cookie)), mem_read, NULL, mem_seek, mem_close);
    if (!*file) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    return true;
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
