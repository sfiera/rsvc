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

#ifndef SRC_RSVC_MB5_H_
#define SRC_RSVC_MB5_H_

#include <musicbrainz5/mb5_c.h>

#define MbArtist            Mb5Artist
#define MbArtistCredit      Mb5ArtistCredit
#define MbDisc              Mb5Disc
#define MbMedium            Mb5Medium
#define MbMediumList        Mb5MediumList
#define MbMetadata          Mb5Metadata
#define MbNameCredit        Mb5NameCredit
#define MbNameCreditList    Mb5NameCreditList
#define MbQuery             Mb5Query
#define MbRecording         Mb5Recording
#define MbRelease           Mb5Release
#define MbReleaseList       Mb5ReleaseList
#define MbTrack             Mb5Track
#define MbTrackList         Mb5TrackList

#define mb_artist_get_name                  mb5_artist_get_name
#define mb_artistcredit_get_namecreditlist  mb5_artistcredit_get_namecreditlist
#define mb_disc_get_releaselist             mb5_disc_get_releaselist
#define mb_medium_contains_discid           mb5_medium_contains_discid
#define mb_medium_get_tracklist             mb5_medium_get_tracklist
#define mb_medium_list_item                 mb5_medium_list_item
#define mb_medium_list_size                 mb5_medium_list_size
#define mb_metadata_get_disc                mb5_metadata_get_disc
#define mb_namecredit_get_artist            mb5_namecredit_get_artist
#define mb_namecredit_list_item             mb5_namecredit_list_item
#define mb_namecredit_list_size             mb5_namecredit_list_size
#define mb_query_delete                     mb5_query_delete
#define mb_query_new                        mb5_query_new
#define mb_query_query                      mb5_query_query
#define mb_recording_get_title              mb5_recording_get_title
#define mb_release_get_artistcredit         mb5_release_get_artistcredit
#define mb_release_get_date                 mb5_release_get_date
#define mb_release_get_mediumlist           mb5_release_get_mediumlist
#define mb_release_get_title                mb5_release_get_title
#define mb_release_list_item                mb5_release_list_item
#define mb_release_list_size                mb5_release_list_size
#define mb_track_get_position               mb5_track_get_position
#define mb_track_get_recording              mb5_track_get_recording
#define mb_track_list_item                  mb5_track_list_item
#define mb_track_list_size                  mb5_track_list_size

#endif  // RSVC_MB5_H_
