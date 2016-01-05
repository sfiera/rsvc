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

struct rsvc_tags_methods {
    bool (*remove)(rsvc_tags_t tags, const char* name, rsvc_done_t fail);
    bool (*add)(rsvc_tags_t tags, const char* name, const char* value, rsvc_done_t fail);
    bool (*each)(rsvc_tags_t tags,
                 void (^block)(const char* name, const char* value, rsvc_stop_t stop));
    bool (*save)(rsvc_tags_t tags, rsvc_done_t fail);
    void (*destroy)(rsvc_tags_t tags);

    bool (*image_remove)(rsvc_tags_t tags, size_t* index, rsvc_done_t fail);
    bool (*image_add)(rsvc_tags_t tags, rsvc_format_t format, const uint8_t* data, size_t size,
                      rsvc_done_t fail);
    bool (*image_each)(rsvc_tags_t tags,
                       void (^block)(rsvc_format_t format, const uint8_t* data, size_t size,
                                     rsvc_stop_t stop));
};
struct rsvc_tags {
    struct rsvc_tags_methods*   vptr;
    int                         flags;
};

enum {
    RSVC_TAG_RDONLY = 0,
    RSVC_TAG_RDWR = 1,
};

typedef bool (*rsvc_open_tags_f)(const char* path, int flags, rsvc_tags_t* tags, rsvc_done_t fail);

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
/// ..  function:: bool rsvc_tags_save(rsvc_tags_t, rsvc_done_t fail)
///
/// ..  function:: void rsvc_tags_destroy(rsvc_tags_t tags)
///
///     Destroys a :type:`rsvc_tags_t`, reclaiming its resources.
bool                    rsvc_tags_save(rsvc_tags_t tags, rsvc_done_t fail);
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

bool                    rsvc_tags_image_clear(rsvc_tags_t tags, rsvc_done_t fail);
bool                    rsvc_tags_image_remove(rsvc_tags_t tags, size_t index, rsvc_done_t fail);
bool                    rsvc_tags_image_add(rsvc_tags_t tags, rsvc_format_t format,
                                            const uint8_t* data, size_t size, rsvc_done_t fail);

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
bool                    rsvc_tags_image_each(rsvc_tags_t tags,
                                             void (^block)(rsvc_format_t format,
                                                           const uint8_t* data, size_t size,
                                                           rsvc_stop_t stop));
size_t                  rsvc_tags_image_size(rsvc_tags_t tags);

/// ..  function:: bool rsvc_tags_strf(const char* format, rsvc_tags_t tags, const char* extension, char** path, rsvc_done_t fail)
///
///     Formatting codes:
///
///     *   %a: ARTIST
///     *   %A: ALBUM
///     *   %b: ALBUMARTIST, or ARTIST if there is no ALBUMARTIST
///     *   %c: SEASONNUMBER, padded to the width of SEASONTOTAL if an
///         integer in canonical form
///     *   %C: SEASONTOTAL
///     *   %d: DISCNUMBER, padded to the width of DISCTOTAL if an
///         integer in canonical form
///     *   %D: DISCTOTAL
///     *   %e: EPISODENUMBER, padded to the width of EPISODETOTAL if an
///         integer in canonical form
///     *   %E: EPISODETOTAL
///     *   %g: GENRE
///     *   %k: TRACKNUMBER, padded to the width of TRACKTOTAL if an
///         integer in canonical form
///     *   %K: TRACKTOTAL
///     *   %t: TITLE
///     *   %y: DATE
///     *   %%: literal '%'
///
///     An integer is considered to be in canonical form if it matches
///     "0|[1-9][0-9]*".  For example, if TRACKTOTAL is "1000", then a
///     TRACKNUMBER of "21" will be printed as "0021", but a TRACKNUMBER
///     of "021" will be printed as "021".
///
///     If the format string contains two formatting codes next to each
///     other (e.g. "%d%k%t"), then a separator will be put between
///     them: usually a space, but a hyphen when TRACKNUMBER follows
///     DISCNUMBER or EPISODENUMBER follows SEASONNUMBER.  Also, if a
///     tag has multiple values, those values will be separated by a
///     comma and a space.
///
///     Also, if formatting results in an empty directory name (e.g.
///     "%b/%A/%k" when ARTIST and ALBUMARTIST or ALBUM is not set),
///     then the empty directories will be removed.
///
///     :param format:      a formatting string.
///     :param extension:   an extension, such as "flac".  If non-NULL,
///                         it will be appended after the result of the
///                         formatting string.
///     :param path:        on success, set to the path.  Memory is
///                         owned by the caller.
///     :param fail:        on failure, invoked with an rsvc_error_t
bool rsvc_tags_strf(rsvc_tags_t tags, const char* format, const char* extension,
                    char** path, rsvc_done_t fail);
bool rsvc_tags_validate_strf(const char* format, rsvc_done_t fail);

/// ..  function:: bool rsvc_tags_copy(rsvc_tags_t dst, rsvc_tags_t src, rsvc_done_t fail)
bool rsvc_tags_copy(rsvc_tags_t dst, rsvc_tags_t src, rsvc_done_t fail);

/// ..  function:: rsvc_tags_t rsvc_tags_new()
rsvc_tags_t rsvc_tags_new();

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
#define RSVC_GROUPING               "GROUPING"
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

/// ..  var:: RSVC_ISRC
///           RSVC_MCN
///           RSVC_ENCODER
#define RSVC_SHOW                   "SHOW"
#define RSVC_EPISODENUMBER          "EPISODENUMBER"
#define RSVC_EPISODETOTAL           "EPISODETOTAL"
#define RSVC_SEASONNUMBER           "SEASONNUMBER"
#define RSVC_SEASONTOTAL            "SEASONTOTAL"

/// ..  var:: RSVC_MUSICBRAINZ_DISCID
#define RSVC_MUSICBRAINZ_DISCID     "MUSICBRAINZ_DISCID"

enum rsvc_tag_code {
    RSVC_CODE_ARTIST            = 'a',
    RSVC_CODE_ALBUM             = 'A',
    RSVC_CODE_ALBUMARTIST       = 'b',
    RSVC_CODE_TITLE             = 't',
    RSVC_CODE_GENRE             = 'g',
    RSVC_CODE_GROUPING          = 'G',
    RSVC_CODE_DATE              = 'y',
    RSVC_CODE_TRACKNUMBER       = 'k',
    RSVC_CODE_TRACKTOTAL        = 'K',
    RSVC_CODE_DISCNUMBER        = 'd',
    RSVC_CODE_DISCTOTAL         = 'D',
    RSVC_CODE_SHOW              = 'S',
    RSVC_CODE_EPISODENUMBER     = 'e',
    RSVC_CODE_EPISODETOTAL      = 'E',
    RSVC_CODE_SEASONNUMBER      = 'c',
    RSVC_CODE_SEASONTOTAL       = 'C',
};

const char* rsvc_tag_code_get(int code);

#endif  // RSVC_TAG_H_
