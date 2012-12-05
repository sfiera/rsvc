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

#ifndef RSVC_CD_H_
#define RSVC_CD_H_

#include <stdlib.h>
#include <rsvc/common.h>

typedef struct rsvc_cd* rsvc_cd_t;
typedef struct rsvc_cd_session* rsvc_cd_session_t;
typedef struct rsvc_cd_track* rsvc_cd_track_t;

/// Compact Disc
/// ============
///
/// Compact discs are the oldest form of optical media typically used by
/// computers.  Each sector of audio data on a compact disc contains
/// 2352 bytes, which comprises 588 samples of 2-channel, 16-bit PCM
/// data, or 1/75th of a second at 44.1 kHz.
///
/// ..  type:: rsvc_cd_t
///
///     Represents a compact disc.  Each :type:`rsvc_cd_t` object keeps
///     its own internal dispatch queue.  This is used to serialize
///     access to the disc, and to prevent operations that access the
///     disc from blocking.
///
///     :type:`rsvc_cd_t` objects are created with
///     :func:`rsvc_cd_create()` and destroyed with
///     :func:`rsvc_cd_destroy()`.
///
/// ..  function:: void rsvc_cd_create(char* path, void (^done)(rsvc_cd_t, rsvc_error_t))
///
///     Creates a :type:`rsvc_cd_t` object, reading its content off of
///     the CD drive.  If initialization completes successfully, calls
///     `done` with the result as the first argument and `NULL` as the
///     second.  If it fails, passes `NULL` as the first argument and an
///     error as the second.
///
///     :param path:    A short-form device name, such as "disk1".
///     :param done:    A completion callback.  Receives exactly one of
///                     an initialized :type:`rsvc_cd_t` or an
///                     :type:`rsvc_error_t`.
///
/// ..  function:: void rsvc_cd_destroy(rsvc_cd_t cd)
///
///     Destroys a :type:`rsvc_cd_t`, reclaiming its resources.
void                    rsvc_cd_create(char* path, void (^done)(rsvc_cd_t, rsvc_error_t));
void                    rsvc_cd_destroy(rsvc_cd_t cd);

/// ..  function:: const char* rsvc_cd_mcn(rsvc_cd_t cd)
///
///     :returns:   The :abbr:`MCN (Media Catalog Number)` of the CD, if
///                 any.  If the disc does not have one, returns an
///                 empty string.
const char*             rsvc_cd_mcn(rsvc_cd_t cd);

/// ..  function:: size_t rsvc_cd_nsessions(rsvc_cd_t cd)
///
///     :returns:   The number of sessions on the CD.
///
/// ..  function:: rsvc_cd_session_t rsvc_cd_session(rsvc_cd_t cd, size_t n)
///
///     Returns the session at index `n`.  Sessions are returned by
///     index, not by number, so on a typical, one-session CD, the
///     single session will be at index 0.
///
///     :param n:   An index.  Must be less than the CD's
///                 :func:`rsvc_cd_nsessions()`
///     :returns:   The session at index `n`.
///
/// ..  function:: bool rsvc_cd_each_session(rsvc_cd_t cd, void (^block)(rsvc_cd_session_t, rsvc_stop_t))
///
///     Iterates over sessions in the CD.  See :type:`rsvc_stop_t` for a
///     description of the iterator interface.
size_t                  rsvc_cd_nsessions(rsvc_cd_t cd);
rsvc_cd_session_t       rsvc_cd_session(rsvc_cd_t cd, size_t n);
bool                    rsvc_cd_each_session(rsvc_cd_t cd,
                                             void (^block)(rsvc_cd_session_t, rsvc_stop_t));

/// ..  function:: size_t rsvc_cd_ntracks(rsvc_cd_t cd)
///
///     :returns:   The number of tracks on the CD.
///
/// ..  function:: rsvc_cd_track_t rsvc_cd_track(rsvc_cd_t cd, size_t n)
///
///     Returns the track at index `n`.  Tracks are returned by index,
///     not by number, so the first track will be at index 0.
///
///     :param n:   An index.  Must be less than the CD's
///                 :func:`rsvc_cd_ntracks()`.
///
/// ..  function:: bool rsvc_cd_each_track(rsvc_cd_t cd, void (^block)(rsvc_cd_track_t, rsvc_stop_t))
///
///     Iterates over tracks in the CD.  See :type:`rsvc_stop_t` for a
///     description of the iterator interface.
size_t                  rsvc_cd_ntracks(rsvc_cd_t cd);
rsvc_cd_track_t         rsvc_cd_track(rsvc_cd_t cd, size_t n);
bool                    rsvc_cd_each_track(rsvc_cd_t cd,
                                           void (^block)(rsvc_cd_track_t, rsvc_stop_t));

/// Compact Disc Session
/// --------------------
///
/// ..  type:: rsvc_cd_session_t
///
///     Represents an individual session on a CD.  Typical CDs contain
///     exactly one session, and that session typically contains either
///     a single data track or one or more audio tracks.
///
///     All :type:`rsvc_cd_session_t` objects are owned by their parent
///     :type:`rsvc_cd_t`.
///
/// ..  function:: size_t rsvc_cd_session_number(rsvc_cd_session_t session)
///
///     :returns:   The number of the session.
size_t                  rsvc_cd_session_number(rsvc_cd_session_t session);

/// ..  function:: const char* rsvc_cd_session_discid(rsvc_cd_session_t session)
///
///     :returns:   The MusicBrainz disc ID of the session.  Typically,
///                 this only applies to the first session on a CD.
const char*             rsvc_cd_session_discid(rsvc_cd_session_t session);

/// ..  function:: size_t rsvc_cd_session_ntracks(rsvc_cd_session_t session)
///
///     :returns:   The number of tracks on the session.
///
/// ..  function:: rsvc_cd_track_t rsvc_cd_session_track(rsvc_cd_session_t session, size_t n)
///
///     Returns the track at index `n`.  Tracks are returned by index,
///     not by number, so the first track will be at index 0.
///
///     :param n:   An index.  Must be less than the session's
///                 :func:`rsvc_cd_session_ntracks()`.
///
/// ..  function:: bool rsvc_cd_session_each_track(rsvc_cd_session_t session, void (^block)(rsvc_cd_track_t, rsvc_stop_t))
///
///     Iterates over tracks in the session.  See :type:`rsvc_stop_t`
///     for a description of the iterator interface.
size_t                  rsvc_cd_session_ntracks(rsvc_cd_session_t session);
rsvc_cd_track_t         rsvc_cd_session_track(rsvc_cd_session_t session, size_t n);
bool                    rsvc_cd_session_each_track(rsvc_cd_session_t session,
                                                   void (^block)(rsvc_cd_track_t, rsvc_stop_t));

/// Compact Disc Track
/// ------------------
///
/// ..  type:: rsvc_cd_track_t
///
///     Represents an individual track on a CD.  Each track may contain
///     either data or audio; a typical CD has either exactly one data
///     track or one or more audio tracks.  Occasionally, a CD may have
///     both, usually with the audio tracks on the first session and the
///     data track on the second.
///
///     All :type:`rsvc_cd_track_t` objects are owned by their parent
///     :type:`rsvc_cd_t`.
///
/// ..  function:: size_t rsvc_cd_track_number(rsvc_cd_track_t track)
///
///     :returns:   The number of the track (1-100).
size_t                  rsvc_cd_track_number(rsvc_cd_track_t track);

/// ..  type:: enum rsvc_cd_track_type_t
typedef enum rsvc_cd_track_type_t {
    /// ..  var:: RSVC_CD_TRACK_AUDIO
    RSVC_CD_TRACK_AUDIO,
    /// ..  var:: RSVC_CD_TRACK_DATA
    RSVC_CD_TRACK_DATA,
} rsvc_cd_track_type_t;

/// ..  function:: rsvc_cd_track_type_t rsvc_cd_track_type(rsvc_cd_track_t track)
///
///     Indicates whether a particular track is an audio track or a data
///     track.
///
///     :returns:   :data:`RSVC_CD_TRACK_AUDIO` or
///                 :data:`RSVC_CD_TRACK_DATA`
rsvc_cd_track_type_t    rsvc_cd_track_type(rsvc_cd_track_t track);

/// ..  function:: size_t rsvc_cd_track_nchannels(rsvc_cd_track_t track)
///
///     :returns:   The number of channels in the track (should always
///                 be 2).
///
/// ..  function:: size_t rsvc_cd_track_nsectors(rsvc_cd_track_t track)
///
///     :returns:   The number of sectors in the track.  This is equal
///                 to the duration of the track in 1/75ths of a second.
///
/// ..  function:: size_t rsvc_cd_track_nsamples(rsvc_cd_track_t track)
///
///     :returns:   The number of samples in the track.  This is equal
///                 to the duration of the track in 1/44100ths of a
///                 second.
size_t                  rsvc_cd_track_nchannels(rsvc_cd_track_t track);
size_t                  rsvc_cd_track_nsectors(rsvc_cd_track_t track);
size_t                  rsvc_cd_track_nsamples(rsvc_cd_track_t track);

/// ..  function:: void rsvc_cd_track_isrc(rsvc_cd_track_t track, void (^done)(const char* isrc));
void                    rsvc_cd_track_isrc(rsvc_cd_track_t track,
                                           void (^done)(const char* isrc));

/// ..  function:: void rsvc_cd_track_rip(rsvc_cd_track_t track, int fd, rsvc_done_t done)
///
///     Begins ripping data from the track and writing it to `fd`.  The
///     data written will be a sequence of native-endian int16_t
///     samples, interleaved between channels.
///
///     The IO involved in ripping the CD takes place on the CD's
///     dedicated IO queue; only one rip may be active for a given CD at
///     a time.
///
///     :param fd:      An open, writable file descriptor.
///     :param done:    Invoked when the rip is complete; either with
///                     `NULL` to indicate success, or with an error to
///                     indicate failure.
///     :returns:       A block that can be invoked to cancel the rip. 
///                     Must not be called after the :type:`rsvc_cd_t`
///                     containing `track` is destroyed.
rsvc_stop_t             rsvc_cd_track_rip(rsvc_cd_track_t track, int fd, rsvc_done_t done);

#endif  // RSVC_CD_H_
