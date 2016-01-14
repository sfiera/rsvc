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

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <rsvc/common.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool rsvc_open(const char* path, int oflag, mode_t mode, FILE** file, rsvc_done_t fail) {
    rsvc_logf(3, "open %s", path);
    int fd = open(path, oflag, mode);
    if (fd < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    if (mode & O_RDWR) {
        if (mode & O_APPEND) {
            *file = fdopen(fd, "a+");
        } else {
            *file = fdopen(fd, "r+");
        }
    } else if (mode & O_WRONLY) {
        if (mode & O_APPEND) {
            *file = fdopen(fd, "a");
        } else {
            *file = fdopen(fd, "w");
        }
    } else {
        *file = fdopen(fd, "r");
    }
    return true;
}

bool rsvc_temp(const char* base, char* path, FILE** file, rsvc_done_t fail) {
    if ((strlen(base) + 12) >= MAXPATHLEN) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: File name too long", base);
        return false;
    }

    char result[MAXPATHLEN];
    char* dst = result;
    const char* src = base;

    const char* slash = strrchr(src, '/');
    if (slash) {
        size_t size = slash - src + 1;
        memcpy(dst, src, size);
        src += size;
        dst += size;
    }
    *(dst++) = '.';

    char* dot = strrchr(src, '.');
    if (dot && (dot != src)) {
        size_t size = dot - src;
        memcpy(dst, src, size);
        src += size;
        dst += size;
    }
    *(dst++) = '.';
    size_t prefix_size = dst - result;
    strcpy(dst, "XXXXXXXXXX");
    size_t suffix_size = strlen(src);
    strcat(dst, src);

    while (true) {
        memset(result + prefix_size, 'X', 10);
        int fd = mkstemps(result, suffix_size);
        rsvc_logf(3, "temp %s", result);
        if (fd < 0) {
            switch (errno) {
              case EEXIST:
              case EINTR:
              case EISDIR:
                // Ignore errors that will be resolved by picking a new
                // file name.  EACCES sometimes can, but not always.
                continue;
              default:
                rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", result);
                return false;
            }
        }
        *file = fdopen(fd, "r+");
        break;
    }

    strcpy(path, result);
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

bool rsvc_refile(const char* src, const char* dst, rsvc_done_t fail) {
    rsvc_logf(3, "refile %s %s", src, dst);
    struct stat st;
    if (stat(dst, &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "stat %s", dst);
        return false;
    }
    if ((chmod(src, st.st_mode) < 0) && (errno != EPERM)) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "chmod %s", src);
        return false;
    }
    if ((chown(src, st.st_uid, st.st_gid) < 0) && (errno != EPERM)) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "chmod %s", src);
        return false;
    }
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

bool rsvc_mmap(const char* path, FILE* file, uint8_t** data, size_t* size, rsvc_done_t fail) {
    rsvc_logf(3, "mmap %s", path);
    struct stat st;
    if (fstat(fileno(file), &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "stat %s", path);
        return false;
    }
    *size = st.st_size;
    *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);
    if (*data == MAP_FAILED) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "mmap %s", path);
        return false;
    }
    return true;
}

bool rsvc_seek(FILE* file, off_t where, int whence, rsvc_done_t fail) {
    if (fseek(file, where, whence) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    return true;
}

bool rsvc_tell(FILE* file, off_t* where, rsvc_done_t fail) {
    if ((*where = ftell(file)) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    return true;
}

static int compare_names(const FTSENT** x, const FTSENT** y) {
    return strcmp((*x)->fts_name, (*y)->fts_name);
}

bool rsvc_walk(char* path, int options, rsvc_done_t fail,
               bool (^callback)(unsigned short info, const char* dirname, const char* basename,
                                struct stat* st, rsvc_done_t fail)) {
    char* const path_argv[] = {path, NULL};
    const int pathlen = strlen(path);
    FTS* fts = fts_open(path_argv, options, compare_names);
    if (!fts) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
    FTSENT* ftsent;
    while ((ftsent = fts_read(fts))) {
        char path[MAXPATHLEN];
        const char* dirname = NULL;
        const char* basename = NULL;
        if (ftsent->fts_level > 0) {
            basename = ftsent->fts_name;
        }
        if (ftsent->fts_level > 1) {
            int size = ftsent->fts_parent->fts_pathlen - pathlen;
            const char* data = ftsent->fts_path + pathlen;
            int slashes = strspn(data, "/");
            size -= slashes;
            data += slashes;
            memcpy(path, data, size);
            path[size] = '\0';
            dirname = path;
        }

        rsvc_logf(3, "rsvc_walk: %hu %s %s", ftsent->fts_info, dirname, basename);
        if (!callback(ftsent->fts_info, dirname, basename, ftsent->fts_statp, fail)) {
            fts_close(fts);
            return false;
        }
    }
    if (errno != 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
    }
    fts_close(fts);
    return true;
}

bool rsvc_mv(const char* src, const char* dst, rsvc_done_t fail) {
    if (rsvc_rename(src, dst, ^(rsvc_error_t error){ (void)error; })) {
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
    char parent[MAXPATHLEN];
    rsvc_dirname(path, parent);
    if (strcmp(path, parent) != 0) {
        if (!rsvc_makedirs(parent, mode, fail)) {
            return false;
        }
    }

    if (rsvc_mkdir(path, mode, ^(rsvc_error_t error){ (void)error; })) {
        return true;
    } else if ((errno == EEXIST) || (errno == EISDIR)) {
        return true;
    } else {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        return false;
    }
}

void rsvc_trimdirs(const char* path) {
    if (rsvc_rmdir(path, ^(rsvc_error_t error){ (void)error; })) {
        char parent[MAXPATHLEN];
        rsvc_dirname(path, parent);
        rsvc_trimdirs(parent);
    }
}

char* rsvc_dirname(const char* path, char* dirname) {
    char* slash = strrchr(path, '/');
    if (slash == NULL) {
        strcpy(dirname, ".");
    } else if (slash == path) {
        strcpy(dirname, "/");
    } else if (slash[1] == '\0') {
        char stripped[MAXPATHLEN];
        strncpy(stripped, path, slash - path);
        stripped[slash - path] = '\0';
        rsvc_dirname(stripped, dirname);
    } else {
        strncpy(dirname, path, slash - path);
        dirname[slash - path] = '\0';
    }
    return dirname;
}

char* rsvc_basename(const char* path, char* basename) {
    const char* end = path + strlen(path);
    if (path == end) {
        *basename = '\0';
        return basename;
    }
    while ((end > path) && (*(end - 1) == '/')) {
        --end;
    }
    if (path == end) {
        strcpy(basename, "/");
        return basename;
    }
    const char* begin = end - 1;
    while ((begin > path) && (*(begin - 1) != '/')) {
        --begin;
    }
    size_t len = end - begin;
    memcpy(basename, begin, len);
    basename[len] = '\0';
    return basename;
}

char* rsvc_ext(const char* path, char* ext) {
    rsvc_basename(path, ext);
    const char* base = ext;
    const char* end = base + strlen(base);
    base += strspn(base, ".");
    char* begin = strrchr(base, '.');
    if (begin) {
        ++begin;
        size_t len = end - begin;
        memmove(ext, begin, len);
        ext[len] = '\0';
        return ext;
    }
    *ext = '\0';
    return NULL;
}

bool rsvc_pipe(FILE** read_pipe, FILE** write_pipe, rsvc_done_t fail) {
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, "pipe");
        return false;
    }
    *read_pipe = fdopen(pipe_fd[0], "r");
    *write_pipe = fdopen(pipe_fd[1], "w");
    return true;
}

bool rsvc_read(const char* name, FILE* file, void* data, size_t size,
               size_t* size_out, bool* eof, rsvc_done_t fail) {
    return rsvc_cread(name, file, data, size, 1, size_out, eof, fail);
}

bool rsvc_cread(const char* name, FILE* file, void* data, size_t count, size_t size,
                size_t* count_out, bool* eof, rsvc_done_t fail) {
    if (!!eof != !!count_out) {
        rsvc_errorf(fail, __FILE__, __LINE__, "passing !!eof != !!count_out is nonsensical");
        return false;
    }

    size_t n = fread(data, size, count, file);
    if (n < count) {
        if (feof(file)) {
            if (!eof) {
                rsvc_errorf(fail, __FILE__, __LINE__, "%s: unexpected eof", name);
                return false;
            } else if (n == 0) {
                *eof = true;
            }
        } else if (ferror(file)) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: read error", name);
            return false;
        }
    }
    if (count_out) {
        *count_out = n;
    }
    return true;
}

bool rsvc_write(const char* name, FILE* file, const void* data, size_t size,
                rsvc_done_t fail) {
    size_t n = fwrite(data, 1, size, file);
    if (n < size) {
        rsvc_errorf(fail, __FILE__, __LINE__, "%s: write error", name);
        return false;
    }
    return true;
}
