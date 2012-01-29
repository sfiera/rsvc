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

#ifndef RSVC_TAG_H_
#define RSVC_TAG_H_

#include <stdlib.h>
#include <stdbool.h>
#include <rsvc/common.h>

typedef struct rsvc_tags* rsvc_tags_t;

/// Tags
/// ====
///
/// ..  type:: rsvc_tags_t
///
///     Represents a collection of name-value pairs patterned after
///     Vorbis comments.  Models a one-to-many map of strings to
///     strings.  The names may have any uppercase ASCII string as their
///     value, but see :ref:`tag_constants` for well-known values.
///
///     :type:`rsvc_tags_t` objects are created with
///     :func:`rsvc_tags_create()` or :func:`rsvc_tags_copy()` and
///     destroyed with :func:`rsvc_tags_destroy()`.
///
/// ..  function:: rsvc_tags_t rsvc_tags_create()
///
///     :returns:       An empty set of tags.
///
/// ..  function:: rsvc_tags_t rsvc_tags_copy(rsvc_tags_t tags)
///
///     :returns:       A copy of `tags`.
///
/// ..  function:: void rsvc_tags_destroy(rsvc_tags_t tags)
///
///     Destroys a :type:`rsvc_tags_t`, reclaiming its resources.
rsvc_tags_t             rsvc_tags_create();
rsvc_tags_t             rsvc_tags_copy(rsvc_tags_t tags);
void                    rsvc_tags_destroy(rsvc_tags_t tags);

/// ..  function:: void rsvc_tags_clear(rsvc_tags_t tags, const char* name)
///
///     Removes all tags with name `name`.
///
/// ..  function:: bool rsvc_tags_add(rsvc_tags_t tags, const char* name, const char* value)
/// ..  function:: bool rsvc_tags_addf(rsvc_tags_t tags, const char* name, const char* format, ...)
///
///     Adds a tag with name `name` and value `value`.  Does not modify
///     or overwrite any existing tag with name `name`.
///     :func:`rsvc_tags_addf()` constructs `value` from a
///     `printf`-style format string.
///
///     :returns:       true iff `name` was valid and the tag was added.
void                    rsvc_tags_clear(rsvc_tags_t tags, const char* name);
bool                    rsvc_tags_add(rsvc_tags_t tags,
                                      const char* name, const char* value);
bool                    rsvc_tags_addf(rsvc_tags_t tags,
                                       const char* name, const char* format, ...);

/// ..  function:: size_t rsvc_tags_size(rsvc_tags_t tags)
///
///     :returns:       The total number of tags in `tags`.
///
/// ..  function:: bool rsvc_tags_get(rsvc_tags_t tags, const char* names[], const char* values[], size_t* ntags)
///
///     Attempts to place all tags into the arrays `names` and `values`.
///     If there is enough space to read all tags into the arrays, reads
///     all of them and returns true; otherwise, reads as many as
///     possible then returns false.
///
///     :param names:   An array of size at least ``*ntags``.
///     :param values:  An array of size at least ``*ntags``.
///     :param ntags:   Initially, the size of `names` and `values`; on
///                     return, the number of tags read.
///     :returns:       true iff all tags were read.
///
/// ..  function:: size_t rsvc_tags_count(rsvc_tags_t tags, const char* name)
///
///     :param name:    A tag name.
///     :returns:       The number of tags with name `name`.
///
/// ..  function:: bool rsvc_tags_find(rsvc_tags_t tags, const char* name, const char* values[], size_t* nvalues)
///
///     Attempts to read all values of the tag `name` into `values`.  If
///     there is enough space to read all tags into the arrays, reads
///     all of them and returns true; otherwise, reads as many as
///     possible then returns false.
///
///     :param name:    A tag name.
///     :param values:  An array of size at least ``*nvalues``.
///     :param nvalues: Initially, the size of `values`; on return, the
///                     number of tags read.
///     :returns:       true iff all tags were read.
///
/// ..  function:: bool rsvc_tags_each(rsvc_tags_t tags, void (^block)(const char*, const char*, rsvc_stop_t))
///
///     Iterates over tags in `tags`.  See :type:`rsvc_stop_t` for a
///     description of the iterator interface.
size_t                  rsvc_tags_size(rsvc_tags_t tags);
bool                    rsvc_tags_get(rsvc_tags_t tags, const char* names[],
                                      const char* values[], size_t* ntags);
size_t                  rsvc_tags_count(rsvc_tags_t tags, const char* name);
bool                    rsvc_tags_find(rsvc_tags_t tags, const char* name,
                                       const char* values[], size_t* nvalues);
bool                    rsvc_tags_each(rsvc_tags_t tags,
                                       void (^block)(const char*, const char*, rsvc_stop_t));

/// ..  _tag_constants:
///
/// Constants
/// ---------
/// ..  var:: RSVC_TITLE
///           RSVC_ALBUM
///           RSVC_ARTIST
///           RSVC_ALBUMARTIST
///           RSVC_GENRE
///           RSVC_DATE
#define RSVC_TITLE                  "TITLE"
#define RSVC_ALBUM                  "ALBUM"
#define RSVC_ARTIST                 "ARTIST"
#define RSVC_ALBUMARTIST            "ALBUMARTIST"
#define RSVC_GENRE                  "GENRE"
#define RSVC_DATE                   "DATE"

/// ..  var:: RSVC_TRACKNUMBER
///           RSVC_TRACKTOTAL
///           RSVC_DISCNUMBER
///           RSVC_DISCTOTAL
#define RSVC_TRACKNUMBER            "TRACKNUMBER"
#define RSVC_TRACKTOTAL             "TRACKTOTAL"
#define RSVC_DISCNUMBER             "DISCNUMBER"
#define RSVC_DISCTOTAL              "DISCTOTAL"

/// ..  var:: RSVC_ISRC
///           RSVC_MCN
///           RSVC_ENCODER
#define RSVC_ISRC                   "ISRC"
#define RSVC_MCN                    "MCN"
#define RSVC_ENCODER                "ENCODER"

/// ..  var:: RSVC_MUSICBRAINZ_DISCID
#define RSVC_MUSICBRAINZ_DISCID     "MUSICBRAINZ_DISCID"

#endif  // RSVC_TAG_H_
