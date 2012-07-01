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

#ifndef SRC_RSVC_MB4_H_
#define SRC_RSVC_MB4_H_

#include <musicbrainz4/mb4_c.h>

#define MbArtist            Mb4Artist
#define MbArtistCredit      Mb4ArtistCredit
#define MbDisc              Mb4Disc
#define MbMedium            Mb4Medium
#define MbMediumList        Mb4MediumList
#define MbMetadata          Mb4Metadata
#define MbNameCredit        Mb4NameCredit
#define MbNameCreditList    Mb4NameCreditList
#define MbQuery             Mb4Query
#define MbRecording         Mb4Recording
#define MbRelease           Mb4Release
#define MbReleaseList       Mb4ReleaseList
#define MbTrack             Mb4Track
#define MbTrackList         Mb4TrackList

#define mb_artist_get_name                  mb4_artist_get_name
#define mb_artistcredit_get_namecreditlist  mb4_artistcredit_get_namecreditlist
#define mb_disc_get_releaselist             mb4_disc_get_releaselist
#define mb_medium_contains_discid           mb4_medium_contains_discid
#define mb_medium_get_tracklist             mb4_medium_get_tracklist
#define mb_medium_list_item                 mb4_medium_list_item
#define mb_medium_list_size                 mb4_medium_list_size
#define mb_metadata_get_disc                mb4_metadata_get_disc
#define mb_namecredit_get_artist            mb4_namecredit_get_artist
#define mb_namecredit_list_item             mb4_namecredit_list_item
#define mb_namecredit_list_size             mb4_namecredit_list_size
#define mb_query_delete                     mb4_query_delete
#define mb_query_new                        mb4_query_new
#define mb_query_query                      mb4_query_query
#define mb_recording_get_title              mb4_recording_get_title
#define mb_release_get_artistcredit         mb4_release_get_artistcredit
#define mb_release_get_date                 mb4_release_get_date
#define mb_release_get_mediumlist           mb4_release_get_mediumlist
#define mb_release_get_title                mb4_release_get_title
#define mb_release_list_item                mb4_release_list_item
#define mb_release_list_size                mb4_release_list_size
#define mb_track_get_position               mb4_track_get_position
#define mb_track_get_recording              mb4_track_get_recording
#define mb_track_list_item                  mb4_track_list_item
#define mb_track_list_size                  mb4_track_list_size

#endif  // RSVC_MB4_H_
