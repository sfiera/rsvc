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

#ifndef SRC_BIN_CLOAK_H_
#define SRC_BIN_CLOAK_H_

#include <ctype.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sysexits.h>
#include <unistd.h>

#include <rsvc/audio.h>
#include <rsvc/format.h>
#include <rsvc/image.h>
#include <rsvc/musicbrainz.h>
#include <rsvc/tag.h>

#include "../rsvc/common.h"
#include "../rsvc/list.h"
#include "../rsvc/options.h"
#include "../rsvc/unix.h"
#include "strlist.h"

#define DEFAULT_PATH "./%@/%A/%d%k%t"

enum short_flag {
    HELP                = 'h',
    FORMATS             = -5,
    DRY_RUN             = 'n',
    VERBOSE             = 'v',
    VERSION             = 'V',

    LIST                = 'l',
    LIST_IMAGES         = 'L',
    REMOVE              = 'r',
    REMOVE_ALL          = 'R',
    ADD                 = 'x',
    SET                 = 's',

    ARTIST              = 'a',
    ALBUM               = 'A',
    ALBUM_ARTIST        = 'b',
    TITLE               = 't',
    GENRE               = 'g',
    GROUPING            = 'G',
    DATE                = 'y',
    TRACK               = 'k',
    TRACK_TOTAL         = 'K',
    DISC                = 'd',
    DISC_TOTAL          = 'D',

    MOVIE               = -6,
    TV                  = -7,
    SHOW                = 'S',
    EPISODE             = 'e',
    EPISODE_TOTAL       = 'E',
    SEASON              = 'c',
    SEASON_TOTAL        = 'C',

    IMAGE               = 'i',
    WRITE_IMAGE         = 'o',
    WRITE_IMAGE_DEFAULT = 'O',
    SELECT_IMAGE        = 'I',
    REMOVE_IMAGE        = -1,
    REMOVE_ALL_IMAGES   = -2,
    ADD_IMAGE           = -3,

    AUTO                = -4,

    MOVE                = 'm',
    PATH                = 'p',
};

enum list_mode {
    LIST_MODE_NONE = 0,
    LIST_MODE_SHORT,
    LIST_MODE_LONG,
};

typedef struct add_image_list*          add_image_list_t;
typedef struct add_image_list_node*     add_image_list_node_t;
typedef struct remove_image_list*       remove_image_list_t;
typedef struct remove_image_list_node*  remove_image_list_node_t;
typedef struct write_image_list*        write_image_list_t;
typedef struct write_image_list_node*   write_image_list_node_t;
typedef struct format_path_list*        format_path_list_t;
typedef struct format_path_list_node*   format_path_list_node_t;

struct add_image_list {
    struct add_image_list_node {
        const char*            path;
        int                    fd;
        rsvc_format_t          format;
        add_image_list_node_t  prev, next;
    } *head, *tail;
};

struct remove_image_list {
    struct remove_image_list_node {
        int                       index;
        remove_image_list_node_t  prev, next;
    } *head, *tail;
};

struct write_image_list {
    struct write_image_list_node {
        int                      index;
        const char*              path;
        char                     temp_path[MAXPATHLEN];
        int                      fd;
        write_image_list_node_t  prev, next;
    } *head, *tail;
};

enum fpath_priority {
    FPATH_DEFAULT = 0,
    FPATH_ALL,
    FPATH_GROUP,
    FPATH_MEDIAKIND,
};

struct format_path_list {
    struct format_path_list_node {
        enum fpath_priority      priority;
        const char*              mediakind;
        enum rsvc_format_group   group;
        const char*              path;
        format_path_list_node_t  prev, next;
    } *head, *tail;
};

struct ops {
    bool                      remove_all_tags;
    struct string_list        remove_tags;

    struct string_list        add_tag_names;
    struct string_list        add_tag_values;

    int                       image_index;
    bool                      remove_all_images;
    struct add_image_list     add_images;
    struct remove_image_list  remove_images;
    struct write_image_list   write_images;

    bool                      auto_mode;

    bool                      dry_run;

    enum list_mode            list_mode;
    bool                      list_tags;
    bool                      list_images;

    bool                      move_mode;
    struct format_path_list   paths;
};
typedef struct ops* ops_t;

bool cloak_options(int argc, char* const* argv, ops_t ops, string_list_t files, rsvc_done_t fail);
int cloak_mode(ops_t ops);
bool cloak_move_file(const char* path, rsvc_format_t format, rsvc_tags_t tags, ops_t ops,
                     rsvc_done_t fail);

#endif  // SRC_BIN_CLOAK_H_
