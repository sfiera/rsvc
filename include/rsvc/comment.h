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

/// Comments
/// ========
///
/// ..  type:: rsvc_comments_t
///
///     Represents a collection of name-value pairs patterned after
///     Vorbis comments.  Models a one-to-many map of strings to
///     strings.  The names may have any uppercase ASCII string as their
///     value, but see :ref:`comments_constants` for well-known values.
///
///     :type:`rsvc_comments_t` objects are created with
///     :func:`rsvc_comments_create()` or :func:`rsvc_comments_copy()`
///     and destroyed with :func:`rsvc_comments_destroy()`.
///
/// ..  function:: rsvc_comments_t rsvc_comments_create()
///
///     :returns:       An empty set of comments.
///
/// ..  function:: rsvc_comments_t rsvc_comments_copy(rsvc_comments_t comments)
///
///     :returns:       A copy of `comments`.
///
/// ..  function:: void rsvc_comments_destroy(rsvc_comments_t comments)
///
///     Destroys a :type:`rsvc_comments_t`, reclaiming its resources.
rsvc_comments_t         rsvc_comments_create();
rsvc_comments_t         rsvc_comments_copy(rsvc_comments_t comments);
void                    rsvc_comments_destroy(rsvc_comments_t comments);

/// ..  function:: void rsvc_comments_clear(rsvc_comments_t comments, const char* name)
///
///     Removes all comments with name `name`.
///
/// ..  function:: void rsvc_comments_add(rsvc_comments_t comments,
///                                       const char* name, const char* value)
///
///     Adds a comment with name `name` and value `value`.  Does not
///     modify or overwrite any existing comment with name `name`.
///
/// ..  function:: void rsvc_comments_add_int(rsvc_comments_t comments,
///                                           const char* name, int value)
///
///     Calls :func:`rsvc_comments_add()` with a stringified form of
///     `value`.
///
/// ..  function:: void rsvc_comments_set(rsvc_comments_t comments,
///                                       const char* name, const char* value)
///
///     Removes any existing comments with name `name` and adds a new
///     one with name `name` and name `value`.
void                    rsvc_comments_clear(rsvc_comments_t comments, const char* name);
void                    rsvc_comments_add(rsvc_comments_t comments,
                                          const char* name, const char* value);
void                    rsvc_comments_add_int(rsvc_comments_t comments,
                                              const char* name, int value);
void                    rsvc_comments_set(rsvc_comments_t comments,
                                          const char* name, const char* value);

/// ..  function:: size_t rsvc_comments_size(rsvc_comments_t comments)
///
///     :returns:       The total number of comments in `comments`.
///
/// ..  function:: bool rsvc_comments_get(rsvc_comments_t comments, const char* names[],
///                                       const char* values[], size_t* ncomments)
///
///     Attempts to place all comments into the arrays `names` and
///     `values`.  If there is enough space to read all comments into
///     the arrays, reads all of them and returns true; otherwise,
///     reads as many as possible then returns false.
///
///     :param names:       An array of size at least ``*ncomments``.
///     :param values:      An array of size at least ``*ncomments``.
///     :param ncomments:   Initially, the size of `names` and `values`;
///                         on return, the number of comments read.
///     :returns:           true iff all comments were read.
///
/// ..  function:: size_t rsvc_comments_count(rsvc_comments_t comments, const char* name)
///
///     :param name:    A comment name.
///     :returns:       The number of comments with name `name`.
///
/// ..  function:: bool rsvc_comments_find(rsvc_comments_t comments, const char* name,
///                                        const char* values[], size_t* nvalues)
///
///     Attempts to read all values of the comment `name` into `values`.
///     If there is enough space to read all comments into the arrays,
///     reads all of them and returns true; otherwise, reads as many as
///     possible then returns false.
///
///     :param name:        A comment name.
///     :param values:      An array of size at least ``*nvalues``.
///     :param nvalues:     Initially, the size of `values`; on return,
///                         the number of comments read.
///     :returns:           true iff all comments were read.
///
/// ..  function:: bool rsvc_comments_each(rsvc_comments_t comments,
///                                        void (^block)(const char*, const char*, rsvc_stop_t))
///
///     Iterates over comments in `comments`.  See :type:`rsvc_stop_t`
///     for a description of the iterator interface.
size_t                  rsvc_comments_size(rsvc_comments_t comments);
bool                    rsvc_comments_get(rsvc_comments_t comments, const char* names[],
                                          const char* values[], size_t* ncomments);
size_t                  rsvc_comments_count(rsvc_comments_t comments, const char* name);
bool                    rsvc_comments_find(rsvc_comments_t comments, const char* name,
                                           const char* values[], size_t* nvalues);
bool                    rsvc_comments_each(rsvc_comments_t comments,
                                           void (^block)(const char*, const char*, rsvc_stop_t));

/// ..  _comments_constants:
///
/// Constants
/// ---------
///
/// ..  var:: RSVC_TITLE
/// ..  var:: RSVC_ALBUM
/// ..  var:: RSVC_ARTIST
/// ..  var:: RSVC_GENRE
/// ..  var:: RSVC_DATE
#define RSVC_TITLE                  "TITLE"
#define RSVC_ALBUM                  "ALBUM"
#define RSVC_ARTIST                 "ARTIST"
#define RSVC_GENRE                  "GENRE"
#define RSVC_DATE                   "DATE"

/// ..  var:: RSVC_TRACKNUMBER
/// ..  var:: RSVC_TRACKTOTAL
/// ..  var:: RSVC_DISCNUMBER
/// ..  var:: RSVC_DISCTOTAL
#define RSVC_TRACKNUMBER            "TRACKNUMBER"
#define RSVC_TRACKTOTAL             "TRACKTOTAL"
#define RSVC_DISCNUMBER             "DISCNUMBER"
#define RSVC_DISCTOTAL              "DISCTOTAL"

/// ..  var:: RSVC_ISRC
/// ..  var:: RSVC_MCN
/// ..  var:: RSVC_ENCODER
#define RSVC_ISRC                   "ISRC"
#define RSVC_MCN                    "MCN"
#define RSVC_ENCODER                "ENCODER"

/// ..  var:: RSVC_MUSICBRAINZ_DISCID
#define RSVC_MUSICBRAINZ_DISCID     "MUSICBRAINZ_DISCID"

#endif  // RSVC_COMMENT_H_
