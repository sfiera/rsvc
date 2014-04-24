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

#include <rsvc/disc.h>

#include <Block.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

void rsvc_disc_watch(rsvc_disc_watch_callbacks_t callbacks) {
    __block rsvc_stop_t stop = Block_copy(^(){
        Block_release(stop);
    });
    callbacks.initialized(stop);
}

void rsvc_disc_eject(const char* path, rsvc_done_t done) {
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_async(queue, ^{
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            rsvc_strerrorf(done, __FILE__, __LINE__, "%s", path);
            return;
        }
        if (ioctl(fd, CDROMEJECT) < 0) {
            rsvc_strerrorf(done, __FILE__, __LINE__, "%s", path);
            close(fd);
            return;
        }
        close(fd);
        done(NULL);
    });
}
