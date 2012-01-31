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

     $Id: LabelInfo.cc 13259 2011-08-10 12:02:50Z adhawkins $

----------------------------------------------------------------------------*/

#include "musicbrainz4/LabelInfo.h"

#include "musicbrainz4/Label.h"

class MusicBrainz4::CLabelInfoPrivate
{
	public:
		CLabelInfoPrivate()
		:	m_Label(0)
		{
		}

		std::string m_CatalogNumber;
		CLabel *m_Label;
};

MusicBrainz4::CLabelInfo::CLabelInfo(const XMLNode& Node)
:	CEntity(),
	m_d(new CLabelInfoPrivate)
{
	if (!Node.isEmpty())
	{
		//std::cout << "Label info node: " << std::endl << Node.createXMLString(true) << std::endl;

		Parse(Node);
	}
}

MusicBrainz4::CLabelInfo::CLabelInfo(const CLabelInfo& Other)
:	CEntity(),
	m_d(new CLabelInfoPrivate)
{
	*this=Other;
}

MusicBrainz4::CLabelInfo& MusicBrainz4::CLabelInfo::operator =(const CLabelInfo& Other)
{
	if (this!=&Other)
	{
		Cleanup();

		CEntity::operator =(Other);

		m_d->m_CatalogNumber=Other.m_d->m_CatalogNumber;

		if (Other.m_d->m_Label)
			m_d->m_Label=new CLabel(*Other.m_d->m_Label);
	}

	return *this;
}

MusicBrainz4::CLabelInfo::~CLabelInfo()
{
	Cleanup();

	delete m_d;
}

void MusicBrainz4::CLabelInfo::Cleanup()
{
	delete m_d->m_Label;
	m_d->m_Label=0;
}

MusicBrainz4::CLabelInfo *MusicBrainz4::CLabelInfo::Clone()
{
	return new CLabelInfo(*this);
}

bool MusicBrainz4::CLabelInfo::ParseAttribute(const std::string& Name, const std::string& /*Value*/)
{
	bool RetVal=true;

	std::cerr << "Unrecognised labelinfo attribute: '" << Name << "'" << std::endl;
	RetVal=false;

	return RetVal;
}

bool MusicBrainz4::CLabelInfo::ParseElement(const XMLNode& Node)
{
	bool RetVal=true;

	std::string NodeName=Node.getName();

	if ("catalog-number"==NodeName)
	{
		RetVal=ProcessItem(Node,m_d->m_CatalogNumber);
	}
	else if ("label"==NodeName)
	{
		RetVal=ProcessItem(Node,m_d->m_Label);
	}
	else
	{
		std::cerr << "Unrecognised label info element: '" << NodeName << "'" << std::endl;
		RetVal=false;
	}

	return RetVal;
}

std::string MusicBrainz4::CLabelInfo::GetElementName()
{
	return "label-info";
}

std::string MusicBrainz4::CLabelInfo::CatalogNumber() const
{
	return m_d->m_CatalogNumber;
}

MusicBrainz4::CLabel *MusicBrainz4::CLabelInfo::Label() const
{
	return m_d->m_Label;
}

std::ostream& MusicBrainz4::CLabelInfo::Serialise(std::ostream& os) const
{
	os << "Label info:" << std::endl;

	CEntity::Serialise(os);

	os << "\tCatalog number: " << CatalogNumber() << std::endl;

	if (Label())
		os << *Label() << std::endl;

	return os;
}

