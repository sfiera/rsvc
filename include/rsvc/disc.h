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

#ifndef RSVC_DISC_H_
#define RSVC_DISC_H_

#include <dispatch/dispatch.h>
#include <rsvc/common.h>

/// Disc
/// ====
/// ..  toctree::
///
///     cd.h
///
/// ..  type:: enum rsvc_disc_type_t
typedef enum rsvc_disc_type {
    /// ..  var:: RSVC_DISC_TYPE_NONE
    ///
    ///     Denotes the absence of a disc.
    RSVC_DISC_TYPE_NONE = 0,

    /// ..  var:: RSVC_DISC_TYPE_CD
    ///
    ///     Denotes a :abbr:`CD (Compact Disc)`.  See :doc:`cd.h`.
    RSVC_DISC_TYPE_CD,

    /// ..  var:: RSVC_DISC_TYPE_DVD
    ///
    ///     Denotes a :abbr:`DVD (Digital Video Disc)`.
    RSVC_DISC_TYPE_DVD,

    /// ..  var:: RSVC_DISC_TYPE_BD
    ///
    ///     Denotes a Blu-Ray disc.
    RSVC_DISC_TYPE_BD,

    /// ..  var:: RSVC_DISC_TYPE_HDDVD
    ///
    ///     Denotes a HD-DVD disc.
    RSVC_DISC_TYPE_HDDVD,
} rsvc_disc_type_t;

extern const char* rsvc_disc_type_name[];

typedef struct rsvc_disc_callbacks {
    void (^appeared)(rsvc_disc_type_t, const char*);
    void (^disappeared)(rsvc_disc_type_t, const char*);
    void (^initialized)(rsvc_stop_t);
} rsvc_disc_watch_callbacks_t;

/// ..  function:: void rsvc_disc_watch(void (^appeared)(rsvc_disc_type_t, const char*), void (^disappeared)(rsvc_disc_type_t, const char*), void (^initialized)())
///
///     When first called, invokes `callbacks.appeared` for every disc
///     which is currently available, then invokes
///     `callbacks.initialized`.  After that, continues to invoke
///     `callbacks.appeared` and `callbacks.disappeared` as discs appear
///     and disappear from the computer.  Stops watching for discs when
///     the closure passed to initialized is invoked.  It may be called
///     from `callbacks.initialized` to get a simple list of discs.
///
///     All callbacks are serialized on an internal, single-threaded
///     dispatch queue to maintain consistency.
void                    rsvc_disc_watch(rsvc_disc_watch_callbacks_t callbacks);

void                    rsvc_disc_eject(const char* path, rsvc_done_t done);

#endif  // RSVC_DISC_H_
