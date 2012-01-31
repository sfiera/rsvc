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

     $Id: Message.cc 13259 2011-08-10 12:02:50Z adhawkins $

----------------------------------------------------------------------------*/

#include "musicbrainz4/Message.h"

class MusicBrainz4::CMessagePrivate
{
public:
		std::string m_Text;
};

MusicBrainz4::CMessage::CMessage(const XMLNode& Node)
:	CEntity(),
	m_d(new CMessagePrivate)
{
	if (!Node.isEmpty())
	{
		//std::cout << "Message node: " << std::endl << Node.createXMLString(true) << std::endl;

		Parse(Node);
	}
}

MusicBrainz4::CMessage::CMessage(const CMessage& Other)
:	CEntity(),
	m_d(new CMessagePrivate)
{
	*this=Other;
}

MusicBrainz4::CMessage& MusicBrainz4::CMessage::operator =(const CMessage& Other)
{
	if (this!=&Other)
	{
		CEntity::operator =(Other);

		m_d->m_Text=Other.m_d->m_Text;
	}

	return *this;
}

MusicBrainz4::CMessage::~CMessage()
{
	delete m_d;
}

MusicBrainz4::CMessage *MusicBrainz4::CMessage::Clone()
{
	return new CMessage(*this);
}

bool MusicBrainz4::CMessage::ParseAttribute(const std::string& Name, const std::string& /*Value*/)
{
	bool RetVal=true;

	std::cerr << "Unrecognised message attribute: '" << Name << "'" << std::endl;
	RetVal=false;

	return RetVal;
}

bool MusicBrainz4::CMessage::ParseElement(const XMLNode& Node)
{
	bool RetVal=true;

	std::string NodeName=Node.getName();

	if (NodeName=="text")
		RetVal=ProcessItem(Node,m_d->m_Text);
	else
	{
		std::cerr << "Unrecognised message element: '" << NodeName << "'" << std::endl;
		RetVal=false;
	}

	return RetVal;
}

std::string MusicBrainz4::CMessage::GetElementName()
{
	return "message";
}

std::string MusicBrainz4::CMessage::Text() const
{
	return m_d->m_Text;
}

std::ostream& MusicBrainz4::CMessage::Serialise(std::ostream& os) const
{
	os << "Message:" << std::endl;

	CEntity::Serialise(os);

	os << "\tText: " << Text() << std::endl;

	return os;
}
