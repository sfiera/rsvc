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

#ifndef RSVC_COMMENT_H_
#define RSVC_COMMENT_H_

#include <stdlib.h>
#include <stdbool.h>
#include <rsvc/common.h>

typedef struct rsvc_comments* rsvc_comments_t;

#define RSVC_TITLE      "TITLE"
#define RSVC_ALBUM      "ALBUM"
#define RSVC_ARTIST     "ARTIST"
#define RSVC_GENRE      "GENRE"
#define RSVC_DATE       "DATE"

#define RSVC_TRACKNUMBER \
                        "TRACKNUMBER"
#define RSVC_TRACKTOTAL "TRACKTOTAL"
#define RSVC_DISCNUMBER "DISCNUMBER"
#define RSVC_DISCTOTAL  "DISCTOTAL"

#define RSVC_ISRC       "ISRC"
#define RSVC_MCN        "MCN"
#define RSVC_ENCODER    "ENCODER"

rsvc_comments_t         rsvc_comments_create();
rsvc_comments_t         rsvc_comments_copy(rsvc_comments_t comments);
void                    rsvc_comments_destroy(rsvc_comments_t comments);

void                    rsvc_comments_clear(rsvc_comments_t comments, const char* name);
void                    rsvc_comments_add(rsvc_comments_t comments,
                                          const char* name, const char* value);
void                    rsvc_comments_add_int(rsvc_comments_t comments,
                                              const char* name, int value);
void                    rsvc_comments_set(rsvc_comments_t comments,
                                          const char* name, const char* value);

size_t                  rsvc_comments_size(rsvc_comments_t comments);
bool                    rsvc_comments_get(rsvc_comments_t comments, const char* names[],
                                          const char* values[], size_t* ncomments);
size_t                  rsvc_comments_count(rsvc_comments_t comments, const char* name);
bool                    rsvc_comments_find(rsvc_comments_t comments, const char* name,
                                           const char* values[], size_t* nvalues);

#endif  // RSVC_COMMENT_H_
