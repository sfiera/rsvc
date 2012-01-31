/* --------------------------------------------------------------------------

   libmusicbrainz4 - Client library to access MusicBrainz

   Copyright (C) 2011 Andrew Hawkins

   This file is part of libmusicbrainz4.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   libmusicbrainz4 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

     $Id: mbtest.cc 13254 2011-08-09 09:29:20Z adhawkins $

----------------------------------------------------------------------------*/

#include <iostream>

#include <strings.h>

#include "musicbrainz4/Query.h"
#include "musicbrainz4/Release.h"
#include "musicbrainz4/ReleaseGroup.h"
#include "musicbrainz4/Medium.h"
#include "musicbrainz4/MediumList.h"
#include "musicbrainz4/NameCredit.h"
#include "musicbrainz4/ArtistCredit.h"
#include "musicbrainz4/Artist.h"
#include "musicbrainz4/HTTPFetch.h"
#include "musicbrainz4/Track.h"
#include "musicbrainz4/Recording.h"
#include "musicbrainz4/Collection.h"
#include "musicbrainz4/CollectionList.h"

int main(int argc, const char *argv[])
{
	MusicBrainz4::CQuery MB("MBTest/v1.0");

	if (argc>1)
	{
		std::cout << "Setting username: '" << argv[1] << "'" << std::endl;
		MB.SetUserName(argv[1]);
	}

	if (argc>2)
	{
		std::cout << "Setting password: '" << argv[2] << "'" << std::endl;
		MB.SetPassword(argv[2]);
	}

	MusicBrainz4::CMetadata Metadata=MB.Query("collection");
	MusicBrainz4::CCollectionList *CollectionList=Metadata.CollectionList();
	if (CollectionList && 0!=CollectionList->NumItems())
	{
		MusicBrainz4::CCollection *Collection=CollectionList->Item(0);
		std::cout << "ID is " << Collection->ID() << std::endl;

		MB.Query("collection",Collection->ID(),"releases");

		std::vector<std::string> Releases;
		Releases.push_back("b5748ac9-f38e-48f7-a8a4-8b43cab025bc");
		Releases.push_back("f6335672-c521-4129-86c3-490d20533e08");
		bool Ret=MB.AddCollectionEntries(Collection->ID(),Releases);
		std::cout << "AddCollectionEntries returns " << std::boolalpha << Ret << std::endl;

		MB.Query("collection",Collection->ID(),"releases");

		Releases.clear();
		Releases.push_back("b5748ac9-f38e-48f7-a8a4-8b43cab025bc");
		Ret=MB.DeleteCollectionEntries(Collection->ID(),Releases);
		std::cout << "DeleteCollectionEntries returns " << std::boolalpha << Ret << std::endl;

		MB.Query("collection",Collection->ID(),"releases");
	}

	return 0;

	std::string DiscID="arIS30RPWowvwNEqsqdDnZzDGhk-";

	if (argc==2)
		DiscID=argv[1];

	MusicBrainz4::CReleaseList ReleaseList=MB.LookupDiscID(DiscID);

	for (int count=0;count<ReleaseList.NumItems();count++)
	{
		MusicBrainz4::CRelease *Release=ReleaseList.Item(count);

		MusicBrainz4::CRelease FullRelease=MB.LookupRelease(Release->ID());

		std::cout << "Full release: " << std::endl;

		std::cout << FullRelease << std::endl;

		std::cout << "Release group title: '" << FullRelease.ReleaseGroup()->Title() << "'" << std::endl;

		std::cout << std::endl << std::endl << "Media matching " << DiscID << ":" << std::endl;

		MusicBrainz4::CMediumList MediaList=FullRelease.MediaMatchingDiscID(DiscID);

		for (int count=0;count<MediaList.NumItems();count++)
		{
			MusicBrainz4::CMedium *Medium=MediaList.Item(count);

			std::cout << *Medium << std::endl;
		}

		std::cout << std::endl << std::endl;
	}

	return 0;
}

