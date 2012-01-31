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

     $Id: Tag.h 13259 2011-08-10 12:02:50Z adhawkins $

----------------------------------------------------------------------------*/

#ifndef _MUSICBRAINZ4_TAG_H
#define _MUSICBRAINZ4_TAG_H

#include <string>
#include <iostream>

#include "musicbrainz4/Entity.h"

#include "musicbrainz4/xmlParser.h"

namespace MusicBrainz4
{
	class CTagPrivate;

	class CTag: public CEntity
	{
	public:
		CTag(const XMLNode& Node);
		CTag(const CTag& Other);
		CTag& operator =(const CTag& Other);
		virtual ~CTag();

		virtual CTag *Clone();

		int Count() const;
		std::string Name() const;

		virtual std::ostream& Serialise(std::ostream& os) const;
		static std::string GetElementName();

	protected:
		virtual bool ParseAttribute(const std::string& Name, const std::string& Value);
		virtual bool ParseElement(const XMLNode& Node);

	private:
		CTagPrivate * const m_d;
	};
}

#endif
