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

     $Id: ReleaseGroup.h 13259 2011-08-10 12:02:50Z adhawkins $

----------------------------------------------------------------------------*/

#ifndef _MUSICBRAINZ4_RELEASE_GROUP_H
#define _MUSICBRAINZ4_RELEASE_GROUP_H

#include <string>
#include <iostream>

#include "musicbrainz4/Entity.h"
#include "musicbrainz4/ReleaseList.h"
#include "musicbrainz4/RelationList.h"
#include "musicbrainz4/TagList.h"
#include "musicbrainz4/UserTagList.h"

#include "musicbrainz4/xmlParser.h"

namespace MusicBrainz4
{
	class CReleaseGroupPrivate;

	class CArtistCredit;
	class CRating;
	class CUserRating;

	class CReleaseGroup: public CEntity
	{
	public:
		CReleaseGroup(const XMLNode& Node=XMLNode::emptyNode());
		CReleaseGroup(const CReleaseGroup& Other);
		CReleaseGroup& operator =(const CReleaseGroup& Other);
		virtual ~CReleaseGroup();

		virtual CReleaseGroup *Clone();

		std::string ID() const;
		std::string Type() const;
		std::string Title() const;
		std::string Disambiguation() const;
		std::string FirstReleaseDate() const;
		CArtistCredit *ArtistCredit() const;
		CReleaseList *ReleaseList() const;
		CRelationList *RelationList() const;
		CTagList *TagList() const;
		CUserTagList *UserTagList() const;
		CRating *Rating() const;
		CUserRating *UserRating() const;

		virtual std::ostream& Serialise(std::ostream& os) const;
		static std::string GetElementName();

	protected:
		virtual bool ParseAttribute(const std::string& Name, const std::string& Value);
		virtual bool ParseElement(const XMLNode& Node);

	private:
		void Cleanup();

		CReleaseGroupPrivate * const m_d;
	};
}

#endif
