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
#include "common.h"

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
