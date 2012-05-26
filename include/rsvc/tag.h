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
struct rsvc_tags_methods {
    bool (*remove)(rsvc_tags_t tags, const char* name, rsvc_done_t fail);
    bool (*add)(rsvc_tags_t tags, const char* name, const char* value, rsvc_done_t fail);
    bool (*each)(rsvc_tags_t tags,
                 void (^block)(const char* name, const char* value, rsvc_stop_t stop));
    void (*save)(rsvc_tags_t tags, rsvc_done_t done);
    void (*destroy)(rsvc_tags_t tags);
};
struct rsvc_tags {
    struct rsvc_tags_methods*   vptr;
    int                         flags;
};

enum {
    RSVC_TAG_RDONLY = 0,
    RSVC_TAG_RDWR = 1,
};

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
/// ..  function:: void rsvc_tags_save(rsvc_tags_t, rsvc_done_t done)
///
/// ..  function:: void rsvc_tags_destroy(rsvc_tags_t tags)
///
///     Destroys a :type:`rsvc_tags_t`, reclaiming its resources.
void                    rsvc_tags_save(rsvc_tags_t tags, rsvc_done_t done);
void                    rsvc_tags_destroy(rsvc_tags_t tags);

/// ..  function:: bool rsvc_tags_clear(rsvc_tags_t tags, rsvc_done_t fail)
/// ..  function:: bool rsvc_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail)
///
///     Removes all tags with name `name`, or all tags.
///
/// ..  function:: bool rsvc_tags_add(rsvc_tags_t tags, rsvc_done_t fail, const char* name, const char* value)
/// ..  function:: bool rsvc_tags_addf(rsvc_tags_t tags, rsvc_done_t fail, const char* name, const char* format, ...)
///
///     Adds a tag with name `name` and value `value`.  Does not modify
///     or overwrite any existing tag with name `name`.
///     :func:`rsvc_tags_addf()` constructs `value` from a
///     `printf`-style format string.
///
///     :param fail:    invoked on failure with a description of the
///                     error.
///     :returns:       true on success
bool                    rsvc_tags_clear(rsvc_tags_t tags, rsvc_done_t fail);
bool                    rsvc_tags_remove(rsvc_tags_t tags, const char* name, rsvc_done_t fail);
bool                    rsvc_tags_add(rsvc_tags_t tags, rsvc_done_t fail,
                                      const char* name, const char* value);
bool                    rsvc_tags_addf(rsvc_tags_t tags, rsvc_done_t fail,
                                       const char* name, const char* format, ...)
                        __attribute__((format(printf, 4, 5)));

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
bool                    rsvc_tags_each(rsvc_tags_t tags,
                                       void (^block)(const char*, const char*, rsvc_stop_t));

/// Tag Formats
/// -----------

typedef void (*rsvc_open_tags_f)(const char* path, int flags,
                                 void (^done)(rsvc_tags_t, rsvc_error_t));

typedef struct rsvc_tag_format* rsvc_tag_format_t;
struct rsvc_tag_format {
    const char*         name;

    size_t              magic_size;
    const char*         magic;

    rsvc_open_tags_f    open_tags;
};

void                    rsvc_tag_format_register(const char* name,
                                                 size_t magic_size, const char* magic,
                                                 rsvc_open_tags_f open_tags);

rsvc_tag_format_t       rsvc_tag_format_named(const char* name);
bool                    rsvc_tag_format_detect(int fd, rsvc_tag_format_t* format,
                                               rsvc_done_t fail);
                     
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
