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

#ifndef RSVC_COMMON_H_
#define RSVC_COMMON_H_

#include <dispatch/dispatch.h>
#include <stdbool.h>
#include <stdlib.h>

/// Common Utilities
/// ================
///
/// ..  macro:: RSVC_VERSION
///
///     A string constant declaring the version of Rip Service compiled
///     against.
#define RSVC_VERSION "0.0.0"

/// Iteration
/// ---------
///
/// ..  type:: void (^rsvc_stop_t)()
///
///     Indicates that a for-each loop should stop.  A typical for-each
///     loop has a signature like::
///
///         bool rsvc_each_child(parent_t parent,
///                              void (^block)(child_t, rsvc_stop_t));
///
///     Calling the rsvc_stop_t object causes the loop to terminate,
///     and the function to return false::
///
///         if (!rsvc_each_child(p, ^(child_t c, rsvc_stop_t stop) {
///             if (process(c) == ERROR) {
///                 stop();
///             }
///         }) {
///             return false;
///         }
///
typedef void (^rsvc_stop_t)();

/// Error Handling
/// --------------
///
/// ..  type:: rsvc_error_t
///
///     Contains error information.  Typically provided as an argument
///     to a completion callback; either NULL to indicate success, or a
///     non-NULL value indicating the type and location of an error.
typedef struct rsvc_error* rsvc_error_t;
struct rsvc_error {
    /// ..  member:: const char* message
    ///
    ///     A description of the error that occurred.
    char* message;
    /// ..  member:: const char* file
    ///
    ///     The source file which reported the error.
    const char* file;
    /// ..  member:: int lineno
    ///
    ///     The line at which the error was reported.
    int lineno;
};

typedef void (^rsvc_done_t)(rsvc_error_t error);

/// ..  function:: void rsvc_errorf(rsvc_done_t callback, const char* file, int lineno, const char* format, ...)
///
///     Formats a printf-style error message, calling `callback` with an
///     :type:`rsvc_error_t` constructed from the formatted error
///     message, along with the file and line number.  The object passed
///     to the block is valid only for the duration of that call.
///
///     Typically, this function is used to call a completion callback.
///     If the callback takes only an error status, then the callback
///     can be passed directly; otherwise, the error object can be
///     passed through an intermediary::
///
///         rsvc_errorf(^(rsvc_error_t error){
///             done(NULL, error);
///         }, __FILE__, __LINE__, "error code %d", error_code);
///
///     :param callback:    Invoked with the :type:`rsvc_error_t`.
///     :param file:        Passed through to `callback`.
///     :param lineno:      Passed through to `callback`.
///     :param format:      A `printf()`-style format string.
///     :param ...:         Format values for `format`.
///
/// ..  function:: void rsvc_strerrorf(rsvc_done_t callback, const char* file, int lineno, const char* format, ...)
///
///     Like :func:`rsvc_errorf`, except that it describes the current
///     value of `errno`.  If `format` is non-NULL, then the formatted
///     string, plus a colon, is prepended.
///
///     :param callback:    Invoked with the :type:`rsvc_error_t`.
///     :param file:        Passed through to `callback`.
///     :param lineno:      Passed through to `callback`.
///     :param format:      A `printf()`-style format string.  May be
///                         `NULL`.
///     :param ...:         Format values for `format`.
void                    rsvc_errorf(rsvc_done_t callback,
                                    const char* file, int lineno, const char* format, ...);
void                    rsvc_strerrorf(rsvc_done_t callback,
                                       const char* file, int lineno, const char* format, ...);
rsvc_error_t            rsvc_error_copy(rsvc_error_t error);
void                    rsvc_error_destroy(rsvc_error_t error);
void                    rsvc_error_async(dispatch_queue_t queue, rsvc_error_t error,
                                         rsvc_done_t done);

bool rsvc_open(const char* path, int oflag, mode_t mode, int* fd, rsvc_done_t fail);

extern int verbosity;
void rsvc_logf(int level, const char* format, ...);

#endif  // RSVC_COMMON_H_
