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

#define _POSIX_C_SOURCE 200809L

#include "rsvc.h"

#include <rsvc/disc.h>
#include "../rsvc/common.h"

static char* eject_disk;

struct rsvc_command rsvc_eject = {
    .name = "eject",

    .usage = ^{
        errf("usage: %s eject [DEVICE]\n", rsvc_progname);
    },

    .run = ^(rsvc_done_t done){
        if (eject_disk == NULL) {
            rsvc_default_disk(^(rsvc_error_t error, char* disk){
                if (error) {
                    done(error);
                } else {
                    eject_disk = disk;
                    rsvc_eject.run(done);
                }
            });
            return;
        }

        rsvc_disc_eject(eject_disk, done);
    },

    .argument = ^bool (char* arg, rsvc_done_t fail) {
        if (!eject_disk) {
            eject_disk = arg;
            return true;
        }
        return false;
    },
};
