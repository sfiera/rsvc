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

#define DEFAULT_PATH "./%b/%A/%d%k%t"

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

struct string_list {
    size_t nstrings;
    char** strings;
};
typedef struct string_list string_list_t;

struct add_image_node {
    const char* path;
    int fd;
    rsvc_format_t format;
    struct add_image_node *prev, *next;
};

struct add_image_list {
    struct add_image_node *head, *tail;
};

struct remove_image_list {
    struct remove_image_node {
        int index;
        struct remove_image_node *prev, *next;
    } *head, *tail;
};

struct write_image_list {
    struct write_image_node {
        int index;
        const char* path;
        char temp_path[MAXPATHLEN];
        int fd;
        struct write_image_node *prev, *next;
    } *head, *tail;
};

struct ops {
    bool            remove_all_tags;
    string_list_t   remove_tags;

    string_list_t   add_tag_names;
    string_list_t   add_tag_values;

    int             image_index;
    bool            remove_all_images;
    struct add_image_list       add_images;
    struct remove_image_list    remove_images;
    struct write_image_list     write_images;

    bool            auto_mode;

    bool            dry_run;

    enum list_mode  list_mode;
    bool            list_tags;
    bool            list_images;

    bool            move_mode;
    char*           move_format;
};
typedef struct ops* ops_t;

bool cloak_options(int argc, char* const* argv, ops_t ops, string_list_t* files, rsvc_done_t fail);
int cloak_mode(ops_t ops);

#endif  // SRC_BIN_CLOAK_H_
