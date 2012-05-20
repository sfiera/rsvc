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

#ifndef RSVC_FORMAT_H_
#define RSVC_FORMAT_H_

#include <rsvc/tag.h>

typedef void (*rsvc_open_tags_f)(const char* path, int flags,
                                 void (^done)(rsvc_tags_t, rsvc_error_t));

typedef struct rsvc_container_format* rsvc_container_format_t;
struct rsvc_container_format {
    const char*             name;

    size_t                  magic_size;
    const char*             magic;

    rsvc_open_tags_f        open_tags;
};

void                        rsvc_container_format_register(const char* name,
                                                           size_t magic_size, const char* magic,
                                                           rsvc_open_tags_f open_tags);

rsvc_container_format_t     rsvc_container_format_named(const char* name);
bool                        rsvc_container_format_detect(int fd, rsvc_container_format_t* format,
                                                         rsvc_done_t fail);
                     
#endif  // RSVC_FORMAT_H_
