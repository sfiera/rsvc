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

#include <rsvc/tag.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rsvc_tags_save(rsvc_tags_t tags, rsvc_done_t done) {
    tags->vptr->save(tags, done);
}

void rsvc_tags_destroy(rsvc_tags_t tags) {
    tags->vptr->destroy(tags);
}

bool rsvc_tags_clear(rsvc_tags_t tags, rsvc_done_t fail) {
    return tags->vptr->remove(tags, NULL, fail);
}

bool rsvc_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail) {
    return tags->vptr->remove(tags, name, fail);
}

static bool tag_name_is_valid(const char* name) {
    return name[strspn(name, "ABCDEFGHIJ" "KLMNOPQRST" "UVWXYZ" "_")] == '\0';
}

bool rsvc_tags_add(rsvc_tags_t tags, rsvc_done_t fail,
                   const char* name, const char* value) {
    if (!tag_name_is_valid(name)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid tag name: %s", name);
        return false;
    }
    return tags->vptr->add(tags, name, value, fail);
}

bool rsvc_tags_addf(rsvc_tags_t tags, rsvc_done_t fail,
                    const char* name, const char* format, ...) {
    if (!tag_name_is_valid(name)) {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid tag name: %s", name);
        return false;
    }
    char* value;
    va_list vl;
    va_start(vl, format);
    vasprintf(&value, format, vl);
    va_end(vl);
    bool result = tags->vptr->add(tags, name, value, fail);
    free(value);
    return result;
}

bool rsvc_tags_each(rsvc_tags_t tags,
                    void (^block)(const char*, const char*, rsvc_stop_t)) {
    return tags->vptr->each(tags, block);
}
