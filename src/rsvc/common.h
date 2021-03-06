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

#ifndef SRC_RSVC_COMMON_H_
#define SRC_RSVC_COMMON_H_

#include <rsvc/common.h>

void                    rsvc_prefix_error(const char* prefix, rsvc_error_t error,
                                          rsvc_done_t done);

int                     rsvc_vasprintf(char** value, const char* format, va_list ap);
void                    rsvc_outf(const char* format, ...)
                        __attribute__((format(printf, 1, 2)));
void                    rsvc_errf(const char* format, ...)
                        __attribute__((format(printf, 1, 2)));

void*                   rsvc_memdup(const void* data, size_t size);

// Keep "rsvc_" in the symbol names, but shorten for convenience.
#define                 memdup      rsvc_memdup
#define                 outf        rsvc_outf
#define                 errf        rsvc_errf

#define DOWN_CAST(TYPE, PTR) \
    ((TYPE*)((void*)PTR - offsetof(TYPE, super)))

#endif  // RSVC_COMMON_H_
