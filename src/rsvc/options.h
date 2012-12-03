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

#ifndef RSVC_OPTIONS_H_
#define RSVC_OPTIONS_H_

#include <stdbool.h>
#include <stdlib.h>
#include <rsvc/common.h>

/// Option Parsing
/// --------------
///
/// ..  type:: rsvc_option_value_t
typedef bool (^rsvc_option_value_t)(char** value, rsvc_done_t fail);

/// ..  type:: rsvc_option_callbacks_t
///
///     A struct of blocks that can be used with :func:`rsvc_options()`.
typedef struct option_callbacks {
    /// ..  member:: bool (^short_option)(char opt, rsvc_option_value_t get_value, rsvc_done_t fail)
    ///
    ///     Called for each short option (-o) in the command-line.  May
    ///     return false to indicate that `opt` is not a valid option.
    ///
    ///     :param opt:     The name of the parsed option.
    ///     :param value:   A block which may optionally be invoked to
    ///                     retrieve the value of the option.
    ///     :returns:       true iff `opt` is a valid option.
    bool (^short_option)(char opt, rsvc_option_value_t get_value, rsvc_done_t fail);

    /// ..  member:: bool (^long_option)(char* opt, rsvc_option_value_t get_value, rsvc_done_t fail)
    ///
    ///     Called for each long option (--option) in the command-line.
    ///     May return false to indicate that `opt` is not a valid
    ///     option.
    ///
    ///     :param opt:     The name of the parsed option.
    ///     :param value:   A block which may optionally be invoked to
    ///                     retrieve the value of the option.
    ///     :returns:       true iff `opt` is a valid option.
    bool (^long_option)(char* opt, rsvc_option_value_t get_value, rsvc_done_t fail);

    /// ..  member:: bool (^argument)(char *arg)
    ///
    ///     Called for each argument in the command-line.  May return
    ///     false to indicate that too many arguments were provided.
    ///
    ///     :param arg:     The value of the argument.
    ///     :returns:       true iff `arg` was accepted.
    bool (^argument)(char* arg, rsvc_done_t fail);

    /// ..  member:: void (^usage)(const char* message, ...)
    ///
    ///     Called when a usage error is detected.  Arguments are passed
    ///     in the form of a `printf()` format string.
    ///
    ///     Must not return.
    ///
    ///     TODO(sfiera): support a softer failure mode.
    ///
    ///     :param format:  A `printf()`-style format string.
    ///     :param ...:     Format values for `format`.
    void (^usage)(const char* format, ...);
} rsvc_option_callbacks_t;

/// ..  function:: rsvc_options(size_t argc, char* const* argv, rsvc_option_callbacks_t* callbacks)
///
///     Given a list of strings `argv` and a size `argc`, parses
///     Unix-style command-line options.  Calls one of the callbacks
///     from `callbacks` each time an option or argument is seen.
///
///     :param argc:        The length of `argv`.
///     :param argv:        An array of argument strings.
///     :param callbacks:   A set of callbacks to invoke during parsing.
bool                    rsvc_options(size_t argc, char* const* argv,
                                     rsvc_option_callbacks_t* callbacks, rsvc_done_t fail);

#endif  // RSVC_OPTIONS_H_
