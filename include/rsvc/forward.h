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

#ifndef RSVC_FORWARD_H_
#define RSVC_FORWARD_H_

typedef const  struct rsvc_format*          rsvc_format_t;

typedef        struct rsvc_audio_meta*      rsvc_audio_meta_t;
typedef        struct rsvc_cd*              rsvc_cd_t;
typedef        struct rsvc_cd_session*      rsvc_cd_session_t;
typedef        struct rsvc_cd_track*        rsvc_cd_track_t;
typedef        struct rsvc_encode_options*  rsvc_encode_options_t;
typedef        struct rsvc_error*           rsvc_error_t;
typedef        struct rsvc_tags*            rsvc_tags_t;
typedef        struct rsvc_cancel*          rsvc_cancel_t;
typedef        struct rsvc_cancel_handle*   rsvc_cancel_handle_t;

#endif  // RSVC_FORWARD_H_
