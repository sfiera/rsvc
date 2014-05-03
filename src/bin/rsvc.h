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

#ifndef SRC_BIN_RSVC_H_
#define SRC_BIN_RSVC_H_

#include <stdint.h>
#include <rsvc/common.h>
#include <rsvc/format.h>

#include "../rsvc/options.h"

typedef struct rsvc_command* rsvc_command_t;
typedef struct rip_options* rip_options_t;

extern const char* rsvc_progname;
extern rsvc_option_callbacks_t callbacks;
extern rsvc_command_t command;

struct rsvc_command {
    const char* name;

    bool (^short_option)(int32_t opt, rsvc_option_value_t get_value, rsvc_done_t fail);
    bool (^long_option)(char* opt, rsvc_option_value_t get_value, rsvc_done_t fail);
    bool (^argument)(char* arg, rsvc_done_t fail);
    void (^usage)();

    void (^run)(rsvc_done_t done);
};

void rsvc_command_print(char* disk, rsvc_done_t done);
void rsvc_command_ls(rsvc_done_t done);
void rsvc_command_watch(rsvc_done_t done);
void rsvc_command_eject(char* disk, rsvc_done_t done);

struct encode_options {
    rsvc_format_t format;
    int64_t bitrate;
};
bool validate_encode_options(struct encode_options* encode, rsvc_done_t fail);

struct rip_options {
    char* disk;
    struct encode_options encode;
    bool eject;
    char* path_format;
};
void rsvc_command_rip(rip_options_t options, rsvc_done_t done);

typedef struct convert_options* convert_options_t;
struct convert_options {
    char* input;
    char* output;
    bool recursive;
    bool update;
    bool makedirs;
    struct encode_options encode;
};
void rsvc_command_convert(convert_options_t options, rsvc_done_t done);

void rsvc_usage(rsvc_done_t done);
void rsvc_default_disk(void (^done)(rsvc_error_t error, char* disk));

#endif  // SRC_BIN_RSVC_H_
