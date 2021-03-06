/* --------------------------------------------------------------------------

   libmusicbrainz5 - Client library to access MusicBrainz

   Copyright (C) 2012 Andrew Hawkins

   This file is part of libmusicbrainz5.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   libmusicbrainz5 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

   THIS FILE IS AUTOMATICALLY GENERATED - DO NOT EDIT IT!

----------------------------------------------------------------------------*/


/* --------------------------------------------------------------------------

   libmusicbrainz5 - Client library to access MusicBrainz

   Copyright (C) 2012 Andrew Hawkins

   This file is part of libmusicbrainz5.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   libmusicbrainz5 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

     $Id$

----------------------------------------------------------------------------*/

#include "musicbrainz5/defines.h"

#include "musicbrainz5/Query.h"

#include "musicbrainz5/mb5_c.h"

#include <string.h>

#include "musicbrainz5/Alias.h"
#include "musicbrainz5/AliasList.h"
#include "musicbrainz5/Annotation.h"
#include "musicbrainz5/AnnotationList.h"
#include "musicbrainz5/Artist.h"
#include "musicbrainz5/ArtistList.h"
#include "musicbrainz5/ArtistCredit.h"
#include "musicbrainz5/Attribute.h"
#include "musicbrainz5/AttributeList.h"
#include "musicbrainz5/CDStub.h"
#include "musicbrainz5/CDStubList.h"
#include "musicbrainz5/Collection.h"
#include "musicbrainz5/CollectionList.h"
#include "musicbrainz5/Disc.h"
#include "musicbrainz5/DiscList.h"
#include "musicbrainz5/FreeDBDisc.h"
#include "musicbrainz5/FreeDBDiscList.h"
#include "musicbrainz5/IPI.h"
#include "musicbrainz5/ISRC.h"
#include "musicbrainz5/ISRCList.h"
#include "musicbrainz5/ISWC.h"
#include "musicbrainz5/ISWCList.h"
#include "musicbrainz5/Label.h"
#include "musicbrainz5/LabelList.h"
#include "musicbrainz5/LabelInfo.h"
#include "musicbrainz5/LabelInfoList.h"
#include "musicbrainz5/Lifespan.h"
#include "musicbrainz5/Medium.h"
#include "musicbrainz5/MediumList.h"
#include "musicbrainz5/Message.h"
#include "musicbrainz5/Metadata.h"
#include "musicbrainz5/NameCredit.h"
#include "musicbrainz5/NameCreditList.h"
#include "musicbrainz5/NonMBTrack.h"
#include "musicbrainz5/NonMBTrackList.h"
#include "musicbrainz5/PUID.h"
#include "musicbrainz5/PUIDList.h"
#include "musicbrainz5/Rating.h"
#include "musicbrainz5/Recording.h"
#include "musicbrainz5/RecordingList.h"
#include "musicbrainz5/Relation.h"
#include "musicbrainz5/RelationList.h"
#include "musicbrainz5/RelationListList.h"
#include "musicbrainz5/Release.h"
#include "musicbrainz5/ReleaseGroup.h"
#include "musicbrainz5/ReleaseGroupList.h"
#include "musicbrainz5/SecondaryType.h"
#include "musicbrainz5/SecondaryTypeList.h"
#include "musicbrainz5/Tag.h"
#include "musicbrainz5/TagList.h"
#include "musicbrainz5/TextRepresentation.h"
#include "musicbrainz5/Track.h"
#include "musicbrainz5/TrackList.h"
#include "musicbrainz5/UserRating.h"
#include "musicbrainz5/UserTag.h"
#include "musicbrainz5/UserTagList.h"
#include "musicbrainz5/Work.h"
#include "musicbrainz5/WorkList.h"

std::string GetMapName(std::map<std::string,std::string> Map, int Item)
{
	std::string Ret;

	if (Item<(int)Map.size())
	{
		std::map<std::string,std::string>::const_iterator ThisItem=Map.begin();

		int count=0;

		while (count<Item)
		{
			++count;
			++ThisItem;
		}

		Ret=(*ThisItem).first;
	}

	return Ret;
}

std::string GetMapValue(std::map<std::string,std::string> Map, int Item)
{
	std::string Ret;

	if (Item<(int)Map.size())
	{
		std::map<std::string,std::string>::const_iterator ThisItem=Map.begin();

		int count=0;

		while (count<Item)
		{
			++count;
			++ThisItem;
		}

		Ret=(*ThisItem).second;
	}

	return Ret;
}

#define MB5_C_DELETE(TYPE1, TYPE2) \
	void \
	mb5_##TYPE2##_delete(Mb5##TYPE1 o) \
	{ \
		delete (MusicBrainz5::C##TYPE1 *)o; \
	}

#define MB5_C_CLONE(TYPE1, TYPE2) \
	Mb5##TYPE1 \
	mb5_##TYPE2##_clone(Mb5##TYPE1 o) \
	{ \
		if (o) \
			return (Mb5##TYPE1)new MusicBrainz5::C##TYPE1(*(MusicBrainz5::C##TYPE1 *)o); \
		return 0;\
	}

#define MB5_C_STR_SETTER(TYPE1, TYPE2, PROP1, PROP2) \
	void \
	mb5_##TYPE2##_set_##PROP2(Mb5##TYPE1 o, const char *str) \
	{ \
		if (o) \
		{ \
			try { \
				((MusicBrainz5::C##TYPE1 *)o)->Set##PROP1(str); \
			} \
			catch (...) { \
			} \
		} \
	}

#define MB5_C_INT_SETTER(TYPE1, TYPE2, PROP1, PROP2) \
	void \
	mb5_##TYPE2##_set_##PROP2(Mb5##TYPE1 o, int i) \
	{ \
		if (o) \
		{ \
			try { \
				((MusicBrainz5::C##TYPE1 *)o)->Set##PROP1(i); \
			} \
			catch (...) { \
			} \
		} \
	}

#define MB5_C_STR_GETTER(TYPE1, TYPE2, PROP1, PROP2) \
	int \
	mb5_##TYPE2##_get_##PROP2(Mb5##TYPE1 o, char *str, int len) \
	{ \
		int ret=0; \
		if (str) \
			*str=0; \
		if (o) \
		{ \
			try { \
				ret=((MusicBrainz5::C##TYPE1 *)o)->PROP1().length(); \
				if (str && len) \
				{ \
					strncpy(str, ((MusicBrainz5::C##TYPE1 *)o)->PROP1().c_str(), len); \
					str[len-1]='\0'; \
				} \
			} \
			catch (...) { \
				str[0] = '\0'; \
			} \
		} \
		return ret; \
	}

#define MB5_C_INT_GETTER(TYPE1, TYPE2, PROP1, PROP2) \
	int \
	mb5_##TYPE2##_get_##PROP2(Mb5##TYPE1 o) \
	{ \
		if (o) \
		{ \
			try { \
				return ((MusicBrainz5::C##TYPE1 *)o)->PROP1(); \
			} \
			catch (...) { \
			} \
		} \
		return 0; \
	}

#define MB5_C_DOUBLE_GETTER(TYPE1, TYPE2, PROP1, PROP2) \
	double \
	mb5_##TYPE2##_get_##PROP2(Mb5##TYPE1 o) \
	{ \
		if (o) \
		{ \
			try { \
				return ((MusicBrainz5::C##TYPE1 *)o)->PROP1(); \
			} \
			catch (...) { \
			} \
		} \
		return 0; \
	}

#define MB5_C_BOOL_GETTER(TYPE1, TYPE2, PROP1, PROP2) \
	unsigned char \
	mb5_##TYPE2##_get_##PROP2(Mb##TYPE1 o) \
	{ \
		if (o) \
		{ \
			try { \
				return ((TYPE1 *)o)->PROP1() ? 1 : 0; \
			} \
			catch (...) { \
				return 0; \
			} \
		} \
	}

#define MB5_C_OBJ_GETTER(TYPE1, TYPE2, PROP1, PROP2) \
	Mb5##PROP1 \
	mb5_##TYPE2##_get_##PROP2(Mb5##TYPE1 o) \
	{ \
		if (o) \
		{ \
			try { \
				return (Mb5##PROP1)((MusicBrainz5::C##TYPE1 *)o)->PROP1(); \
			} \
			catch (...) { \
			} \
		} \
		return (Mb5##PROP1)0; \
	}

#define MB5_C_LIST_GETTER(TYPE1, TYPE2) \
	void \
	mb5_##TYPE2##_list_delete(Mb5##TYPE1 o) \
	{ \
		delete (MusicBrainz5::C##TYPE1##List *)o; \
	} \
	int \
	mb5_##TYPE2##_list_size(Mb5##TYPE1##List List) \
	{ \
		if (List) \
		{ \
			try { \
				return ((MusicBrainz5::C##TYPE1##List *)List)->NumItems(); \
			} \
			catch (...) { \
				return 0; \
			} \
		} \
		return 0; \
	} \
 \
	Mb5##TYPE1 \
	mb5_##TYPE2##_list_item(Mb5##TYPE1##List List, int Item) \
	{ \
		if (List) \
		{ \
			try { \
				return ((MusicBrainz5::C##TYPE1##List *)List)->Item(Item); \
			} \
			catch (...) { \
				return (Mb5##TYPE1)0; \
			} \
		} \
		return (Mb5##TYPE1)0; \
	} \
	int \
	mb5_##TYPE2##_list_get_count(Mb5##TYPE1##List List) \
	{ \
		if (List) \
		{ \
			try { \
				return ((MusicBrainz5::C##TYPE1##List *)List)->Count(); \
			} \
			catch (...) { \
				return 0; \
			} \
		} \
		return 0; \
	} \
 \
	int \
	mb5_##TYPE2##_list_get_offset(Mb5##TYPE1##List List) \
	{ \
		if (List) \
		{ \
			try { \
				return ((MusicBrainz5::C##TYPE1##List *)List)->Offset(); \
			} \
			catch (...) { \
				return 0; \
			} \
		} \
		return 0; \
	} \
 \

#define MB5_C_EXT_GETTER(PROP1, PROP2) \
	int \
	mb5_entity_ext_##PROP2##s_size(Mb5Entity o) \
	{ \
		if (o) \
		{ \
			return ((MusicBrainz5::CEntity *)o)->Ext##PROP1##s().size(); \
		} \
		return 0; \
	} \
	int \
	mb5_entity_ext_##PROP2##_name(Mb5Entity o, int Item, char *str, int len) \
	{ \
		int ret=0; \
		if (str) \
			*str=0; \
		if (o) \
		{ \
			std::map<std::string,std::string> Items=((MusicBrainz5::CEntity *)o)->Ext##PROP1##s(); \
			std::string Name=GetMapName(Items,Item); \
			ret=Name.length(); \
			if (str && len) \
			{ \
				strncpy(str, Name.c_str(), len); \
				str[len-1]='\0'; \
			} \
		} \
		return ret; \
	} \
	int \
	mb5_entity_ext_##PROP2##_value(Mb5Entity o, int Item, char *str, int len) \
	{ \
		int ret=0; \
		if (str) \
			*str=0; \
		if (o) \
		{ \
			std::map<std::string,std::string> Items=((MusicBrainz5::CEntity *)o)->Ext##PROP1##s(); \
			std::string Name=GetMapValue(Items,Item); \
			ret=Name.length(); \
			if (str && len) \
			{ \
				strncpy(str, Name.c_str(), len); \
				str[len-1]='\0'; \
			} \
		} \
		return ret; \
	} \



  MB5_C_EXT_GETTER(Attribute,attribute)
  MB5_C_EXT_GETTER(Element,element)

  MB5_C_DELETE(Alias,alias)
  MB5_C_CLONE(Alias,alias)
  MB5_C_STR_GETTER(Alias,alias,Locale,locale)
  MB5_C_STR_GETTER(Alias,alias,Text,text)
  MB5_C_STR_GETTER(Alias,alias,SortName,sortname)
  MB5_C_STR_GETTER(Alias,alias,Type,type)
  MB5_C_STR_GETTER(Alias,alias,Primary,primary)
  MB5_C_STR_GETTER(Alias,alias,BeginDate,begindate)
  MB5_C_STR_GETTER(Alias,alias,EndDate,enddate)

  MB5_C_DELETE(Annotation,annotation)
  MB5_C_CLONE(Annotation,annotation)
  MB5_C_STR_GETTER(Annotation,annotation,Type,type)
  MB5_C_STR_GETTER(Annotation,annotation,Entity,entity)
  MB5_C_STR_GETTER(Annotation,annotation,Name,name)
  MB5_C_STR_GETTER(Annotation,annotation,Text,text)

  MB5_C_DELETE(Artist,artist)
  MB5_C_CLONE(Artist,artist)
  MB5_C_STR_GETTER(Artist,artist,ID,id)
  MB5_C_STR_GETTER(Artist,artist,Type,type)
  MB5_C_STR_GETTER(Artist,artist,Name,name)
  MB5_C_STR_GETTER(Artist,artist,SortName,sortname)
  MB5_C_STR_GETTER(Artist,artist,Gender,gender)
  MB5_C_STR_GETTER(Artist,artist,Country,country)
  MB5_C_STR_GETTER(Artist,artist,Disambiguation,disambiguation)
  MB5_C_OBJ_GETTER(Artist,artist,IPIList,ipilist)
  MB5_C_OBJ_GETTER(Artist,artist,Lifespan,lifespan)
  MB5_C_OBJ_GETTER(Artist,artist,AliasList,aliaslist)
  MB5_C_OBJ_GETTER(Artist,artist,RecordingList,recordinglist)
  MB5_C_OBJ_GETTER(Artist,artist,ReleaseList,releaselist)
  MB5_C_OBJ_GETTER(Artist,artist,ReleaseGroupList,releasegrouplist)
  MB5_C_OBJ_GETTER(Artist,artist,LabelList,labellist)
  MB5_C_OBJ_GETTER(Artist,artist,WorkList,worklist)
  MB5_C_OBJ_GETTER(Artist,artist,RelationListList,relationlistlist)
  MB5_C_OBJ_GETTER(Artist,artist,TagList,taglist)
  MB5_C_OBJ_GETTER(Artist,artist,UserTagList,usertaglist)
  MB5_C_OBJ_GETTER(Artist,artist,Rating,rating)
  MB5_C_OBJ_GETTER(Artist,artist,UserRating,userrating)

  MB5_C_DELETE(ArtistCredit,artistcredit)
  MB5_C_CLONE(ArtistCredit,artistcredit)
  MB5_C_OBJ_GETTER(ArtistCredit,artistcredit,NameCreditList,namecreditlist)

  MB5_C_DELETE(Attribute,attribute)
  MB5_C_CLONE(Attribute,attribute)
  MB5_C_STR_GETTER(Attribute,attribute,Text,text)

  MB5_C_DELETE(CDStub,cdstub)
  MB5_C_CLONE(CDStub,cdstub)
  MB5_C_STR_GETTER(CDStub,cdstub,ID,id)
  MB5_C_STR_GETTER(CDStub,cdstub,Title,title)
  MB5_C_STR_GETTER(CDStub,cdstub,Artist,artist)
  MB5_C_STR_GETTER(CDStub,cdstub,Barcode,barcode)
  MB5_C_STR_GETTER(CDStub,cdstub,Comment,comment)
  MB5_C_OBJ_GETTER(CDStub,cdstub,NonMBTrackList,nonmbtracklist)

  MB5_C_DELETE(Collection,collection)
  MB5_C_CLONE(Collection,collection)
  MB5_C_STR_GETTER(Collection,collection,ID,id)
  MB5_C_STR_GETTER(Collection,collection,Name,name)
  MB5_C_STR_GETTER(Collection,collection,Editor,editor)
  MB5_C_OBJ_GETTER(Collection,collection,ReleaseList,releaselist)

  MB5_C_DELETE(Disc,disc)
  MB5_C_CLONE(Disc,disc)
  MB5_C_STR_GETTER(Disc,disc,ID,id)
  MB5_C_INT_GETTER(Disc,disc,Sectors,sectors)
  MB5_C_OBJ_GETTER(Disc,disc,ReleaseList,releaselist)

  MB5_C_DELETE(FreeDBDisc,freedbdisc)
  MB5_C_CLONE(FreeDBDisc,freedbdisc)
  MB5_C_STR_GETTER(FreeDBDisc,freedbdisc,ID,id)
  MB5_C_STR_GETTER(FreeDBDisc,freedbdisc,Title,title)
  MB5_C_STR_GETTER(FreeDBDisc,freedbdisc,Artist,artist)
  MB5_C_STR_GETTER(FreeDBDisc,freedbdisc,Category,category)
  MB5_C_STR_GETTER(FreeDBDisc,freedbdisc,Year,year)
  MB5_C_OBJ_GETTER(FreeDBDisc,freedbdisc,NonMBTrackList,nonmbtracklist)

  MB5_C_DELETE(IPI,ipi)
  MB5_C_CLONE(IPI,ipi)
  MB5_C_STR_GETTER(IPI,ipi,IPI,ipi)

  MB5_C_DELETE(ISRC,isrc)
  MB5_C_CLONE(ISRC,isrc)
  MB5_C_STR_GETTER(ISRC,isrc,ID,id)
  MB5_C_OBJ_GETTER(ISRC,isrc,RecordingList,recordinglist)

  MB5_C_DELETE(ISWC,iswc)
  MB5_C_CLONE(ISWC,iswc)
  MB5_C_STR_GETTER(ISWC,iswc,ISWC,iswc)

  MB5_C_DELETE(Label,label)
  MB5_C_CLONE(Label,label)
  MB5_C_STR_GETTER(Label,label,ID,id)
  MB5_C_STR_GETTER(Label,label,Type,type)
  MB5_C_STR_GETTER(Label,label,Name,name)
  MB5_C_STR_GETTER(Label,label,SortName,sortname)
  MB5_C_INT_GETTER(Label,label,LabelCode,labelcode)
  MB5_C_OBJ_GETTER(Label,label,IPIList,ipilist)
  MB5_C_STR_GETTER(Label,label,Disambiguation,disambiguation)
  MB5_C_STR_GETTER(Label,label,Country,country)
  MB5_C_OBJ_GETTER(Label,label,Lifespan,lifespan)
  MB5_C_OBJ_GETTER(Label,label,AliasList,aliaslist)
  MB5_C_OBJ_GETTER(Label,label,ReleaseList,releaselist)
  MB5_C_OBJ_GETTER(Label,label,RelationListList,relationlistlist)
  MB5_C_OBJ_GETTER(Label,label,TagList,taglist)
  MB5_C_OBJ_GETTER(Label,label,UserTagList,usertaglist)
  MB5_C_OBJ_GETTER(Label,label,Rating,rating)
  MB5_C_OBJ_GETTER(Label,label,UserRating,userrating)

  MB5_C_DELETE(LabelInfo,labelinfo)
  MB5_C_CLONE(LabelInfo,labelinfo)
  MB5_C_STR_GETTER(LabelInfo,labelinfo,CatalogNumber,catalognumber)
  MB5_C_OBJ_GETTER(LabelInfo,labelinfo,Label,label)

  MB5_C_DELETE(Lifespan,lifespan)
  MB5_C_CLONE(Lifespan,lifespan)
  MB5_C_STR_GETTER(Lifespan,lifespan,Begin,begin)
  MB5_C_STR_GETTER(Lifespan,lifespan,End,end)
  MB5_C_STR_GETTER(Lifespan,lifespan,Ended,ended)

  MB5_C_DELETE(Medium,medium)
  MB5_C_CLONE(Medium,medium)
  MB5_C_STR_GETTER(Medium,medium,Title,title)
  MB5_C_INT_GETTER(Medium,medium,Position,position)
  MB5_C_STR_GETTER(Medium,medium,Format,format)
  MB5_C_OBJ_GETTER(Medium,medium,DiscList,disclist)
  MB5_C_OBJ_GETTER(Medium,medium,TrackList,tracklist)

/* --------------------------------------------------------------------------

   libmusicbrainz5 - Client library to access MusicBrainz

   Copyright (C) 2012 Andrew Hawkins

   This file is part of libmusicbrainz5.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   libmusicbrainz5 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

     $Id$

----------------------------------------------------------------------------*/

unsigned char mb5_medium_contains_discid(Mb5Medium Medium, const char *DiscID)
{
	unsigned char Ret=0;

	if (Medium)
	{
		MusicBrainz5::CMedium *TheMedium=reinterpret_cast<MusicBrainz5::CMedium *>(Medium);
		if (TheMedium)
			Ret=TheMedium->ContainsDiscID(DiscID);
	}

	return Ret;
}


  MB5_C_DELETE(Message,message)
  MB5_C_CLONE(Message,message)
  MB5_C_STR_GETTER(Message,message,Text,text)

  MB5_C_DELETE(Metadata,metadata)
  MB5_C_CLONE(Metadata,metadata)
  MB5_C_STR_GETTER(Metadata,metadata,XMLNS,xmlns)
  MB5_C_STR_GETTER(Metadata,metadata,XMLNSExt,xmlnsext)
  MB5_C_STR_GETTER(Metadata,metadata,Generator,generator)
  MB5_C_STR_GETTER(Metadata,metadata,Created,created)
  MB5_C_OBJ_GETTER(Metadata,metadata,Artist,artist)
  MB5_C_OBJ_GETTER(Metadata,metadata,Release,release)
  MB5_C_OBJ_GETTER(Metadata,metadata,ReleaseGroup,releasegroup)
  MB5_C_OBJ_GETTER(Metadata,metadata,Recording,recording)
  MB5_C_OBJ_GETTER(Metadata,metadata,Label,label)
  MB5_C_OBJ_GETTER(Metadata,metadata,Work,work)
  MB5_C_OBJ_GETTER(Metadata,metadata,PUID,puid)
  MB5_C_OBJ_GETTER(Metadata,metadata,ISRC,isrc)
  MB5_C_OBJ_GETTER(Metadata,metadata,Disc,disc)
  MB5_C_OBJ_GETTER(Metadata,metadata,LabelInfoList,labelinfolist)
  MB5_C_OBJ_GETTER(Metadata,metadata,Rating,rating)
  MB5_C_OBJ_GETTER(Metadata,metadata,UserRating,userrating)
  MB5_C_OBJ_GETTER(Metadata,metadata,Collection,collection)
  MB5_C_OBJ_GETTER(Metadata,metadata,ArtistList,artistlist)
  MB5_C_OBJ_GETTER(Metadata,metadata,ReleaseList,releaselist)
  MB5_C_OBJ_GETTER(Metadata,metadata,ReleaseGroupList,releasegrouplist)
  MB5_C_OBJ_GETTER(Metadata,metadata,RecordingList,recordinglist)
  MB5_C_OBJ_GETTER(Metadata,metadata,LabelList,labellist)
  MB5_C_OBJ_GETTER(Metadata,metadata,WorkList,worklist)
  MB5_C_OBJ_GETTER(Metadata,metadata,ISRCList,isrclist)
  MB5_C_OBJ_GETTER(Metadata,metadata,AnnotationList,annotationlist)
  MB5_C_OBJ_GETTER(Metadata,metadata,CDStubList,cdstublist)
  MB5_C_OBJ_GETTER(Metadata,metadata,FreeDBDiscList,freedbdisclist)
  MB5_C_OBJ_GETTER(Metadata,metadata,TagList,taglist)
  MB5_C_OBJ_GETTER(Metadata,metadata,UserTagList,usertaglist)
  MB5_C_OBJ_GETTER(Metadata,metadata,CollectionList,collectionlist)
  MB5_C_OBJ_GETTER(Metadata,metadata,CDStub,cdstub)
  MB5_C_OBJ_GETTER(Metadata,metadata,Message,message)

  MB5_C_DELETE(NameCredit,namecredit)
  MB5_C_CLONE(NameCredit,namecredit)
  MB5_C_STR_GETTER(NameCredit,namecredit,JoinPhrase,joinphrase)
  MB5_C_STR_GETTER(NameCredit,namecredit,Name,name)
  MB5_C_OBJ_GETTER(NameCredit,namecredit,Artist,artist)

  MB5_C_DELETE(NonMBTrack,nonmbtrack)
  MB5_C_CLONE(NonMBTrack,nonmbtrack)
  MB5_C_STR_GETTER(NonMBTrack,nonmbtrack,Title,title)
  MB5_C_STR_GETTER(NonMBTrack,nonmbtrack,Artist,artist)
  MB5_C_INT_GETTER(NonMBTrack,nonmbtrack,Length,length)

  MB5_C_DELETE(PUID,puid)
  MB5_C_CLONE(PUID,puid)
  MB5_C_STR_GETTER(PUID,puid,ID,id)
  MB5_C_OBJ_GETTER(PUID,puid,RecordingList,recordinglist)

  MB5_C_DELETE(Query,query)
  MB5_C_CLONE(Query,query)
  MB5_C_INT_GETTER(Query,query,LastHTTPCode,lasthttpcode)
  MB5_C_STR_GETTER(Query,query,LastErrorMessage,lasterrormessage)
  MB5_C_STR_GETTER(Query,query,Version,version)

/* --------------------------------------------------------------------------

   libmusicbrainz5 - Client library to access MusicBrainz

   Copyright (C) 2012 Andrew Hawkins

   This file is part of libmusicbrainz5.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   libmusicbrainz5 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

     $Id$

----------------------------------------------------------------------------*/

Mb5Query mb5_query_new(const char *UserAgent, const char *Server, int Port)
{
	return new MusicBrainz5::CQuery(UserAgent, Server ? Server : "musicbrainz.org", Port!=0 ? Port : 80);
}

MB5_C_STR_SETTER(Query,query,UserName,username)
MB5_C_STR_SETTER(Query,query,Password,password)
MB5_C_STR_SETTER(Query,query,ProxyHost,proxyhost)
MB5_C_INT_SETTER(Query,query,ProxyPort,proxyport)
MB5_C_STR_SETTER(Query,query,ProxyUserName,proxyusername)
MB5_C_STR_SETTER(Query,query,ProxyPassword,proxypassword)

Mb5ReleaseList mb5_query_lookup_discid(Mb5Query Query, const char *DiscID)
{
	if (Query)
	{
		try
		{
			MusicBrainz5::CQuery *TheQuery=reinterpret_cast<MusicBrainz5::CQuery *>(Query);
			if (TheQuery)
				return new MusicBrainz5::CReleaseList(TheQuery->LookupDiscID(DiscID));
		}

		catch(...)
		{
		}
	}

	return 0;
}

Mb5Release mb5_query_lookup_release(Mb5Query Query, const char *Release)
{
	if (Query)
	{
		try
		{
			MusicBrainz5::CQuery *TheQuery=reinterpret_cast<MusicBrainz5::CQuery *>(Query);
			if (TheQuery)
				return new MusicBrainz5::CRelease(TheQuery->LookupRelease(Release));
		}

		catch(...)
		{
		}
	}

	return 0;
}

Mb5Metadata mb5_query_query(Mb5Query Query, const char *Entity, const char *ID, const char *Resource, int NumParams, char **ParamName, char **ParamValue)
{
	if (Query)
	{
		try
		{
			MusicBrainz5::CQuery::tParamMap Params;

			for (int count=0;count<NumParams;count++)
			{
				if (ParamName[count] && ParamValue[count])
					Params[ParamName[count]]=ParamValue[count];
			}

			MusicBrainz5::CQuery *TheQuery=reinterpret_cast<MusicBrainz5::CQuery *>(Query);
			if (TheQuery)
				return new MusicBrainz5::CMetadata(TheQuery->Query(Entity?Entity:"",
																											ID?ID:"",
																											Resource?Resource:"",
																											Params));
		}

		catch(...)
		{
		}
	}

	return 0;
}

unsigned char mb5_query_add_collection_entries(Mb5Query Query, const char *Collection, int NumEntries, const char **Entries)
{
	if (Query)
	{
		try
		{
			std::vector<std::string> VecEntries;

			MusicBrainz5::CQuery *TheQuery=reinterpret_cast<MusicBrainz5::CQuery *>(Query);
			if (TheQuery)
			{
				for (int count=0;count<NumEntries;count++)
				{
					if (Entries && Entries[count])
					{
						VecEntries.push_back(Entries[count]);
					}
				}

				return TheQuery->AddCollectionEntries(Collection,VecEntries)?1:0;
			}
		}

		catch(...)
		{
		}
	}

	return 0;
}

unsigned char mb5_query_delete_collection_entries(Mb5Query Query, const char *Collection, int NumEntries, const char **Entries)
{
	if (Query)
	{
		try
		{
			std::vector<std::string> VecEntries;

			MusicBrainz5::CQuery *TheQuery=reinterpret_cast<MusicBrainz5::CQuery *>(Query);
			if (TheQuery)
			{
				for (int count=0;count<NumEntries;count++)
				{
					if (Entries && Entries[count])
					{
						VecEntries.push_back(Entries[count]);
					}
				}

				return TheQuery->AddCollectionEntries(Collection,VecEntries)?1:0;
			}
		}

		catch(...)
		{
		}
	}

	return 0;
}

tQueryResult mb5_query_get_lastresult(Mb5Query o)
{
	if (o)
	{
		try
		{
			return (tQueryResult)((MusicBrainz5::CQuery *)o)->LastResult();
		}

		catch (...)
		{
		}
	}

	return eQuery_FetchError;
}



  MB5_C_DELETE(Rating,rating)
  MB5_C_CLONE(Rating,rating)
  MB5_C_INT_GETTER(Rating,rating,VotesCount,votescount)
  MB5_C_DOUBLE_GETTER(Rating,rating,Rating,rating)

  MB5_C_DELETE(Recording,recording)
  MB5_C_CLONE(Recording,recording)
  MB5_C_STR_GETTER(Recording,recording,ID,id)
  MB5_C_STR_GETTER(Recording,recording,Title,title)
  MB5_C_INT_GETTER(Recording,recording,Length,length)
  MB5_C_STR_GETTER(Recording,recording,Disambiguation,disambiguation)
  MB5_C_OBJ_GETTER(Recording,recording,ArtistCredit,artistcredit)
  MB5_C_OBJ_GETTER(Recording,recording,ReleaseList,releaselist)
  MB5_C_OBJ_GETTER(Recording,recording,PUIDList,puidlist)
  MB5_C_OBJ_GETTER(Recording,recording,ISRCList,isrclist)
  MB5_C_OBJ_GETTER(Recording,recording,RelationListList,relationlistlist)
  MB5_C_OBJ_GETTER(Recording,recording,TagList,taglist)
  MB5_C_OBJ_GETTER(Recording,recording,UserTagList,usertaglist)
  MB5_C_OBJ_GETTER(Recording,recording,Rating,rating)
  MB5_C_OBJ_GETTER(Recording,recording,UserRating,userrating)

  MB5_C_DELETE(Relation,relation)
  MB5_C_CLONE(Relation,relation)
  MB5_C_STR_GETTER(Relation,relation,Type,type)
  MB5_C_STR_GETTER(Relation,relation,Target,target)
  MB5_C_STR_GETTER(Relation,relation,Direction,direction)
  MB5_C_OBJ_GETTER(Relation,relation,AttributeList,attributelist)
  MB5_C_STR_GETTER(Relation,relation,Begin,begin)
  MB5_C_STR_GETTER(Relation,relation,End,end)
  MB5_C_OBJ_GETTER(Relation,relation,Artist,artist)
  MB5_C_OBJ_GETTER(Relation,relation,Release,release)
  MB5_C_OBJ_GETTER(Relation,relation,ReleaseGroup,releasegroup)
  MB5_C_OBJ_GETTER(Relation,relation,Recording,recording)
  MB5_C_OBJ_GETTER(Relation,relation,Label,label)
  MB5_C_OBJ_GETTER(Relation,relation,Work,work)

  MB5_C_DELETE(Release,release)
  MB5_C_CLONE(Release,release)
  MB5_C_STR_GETTER(Release,release,ID,id)
  MB5_C_STR_GETTER(Release,release,Title,title)
  MB5_C_STR_GETTER(Release,release,Status,status)
  MB5_C_STR_GETTER(Release,release,Quality,quality)
  MB5_C_STR_GETTER(Release,release,Disambiguation,disambiguation)
  MB5_C_STR_GETTER(Release,release,Packaging,packaging)
  MB5_C_OBJ_GETTER(Release,release,TextRepresentation,textrepresentation)
  MB5_C_OBJ_GETTER(Release,release,ArtistCredit,artistcredit)
  MB5_C_OBJ_GETTER(Release,release,ReleaseGroup,releasegroup)
  MB5_C_STR_GETTER(Release,release,Date,date)
  MB5_C_STR_GETTER(Release,release,Country,country)
  MB5_C_STR_GETTER(Release,release,Barcode,barcode)
  MB5_C_STR_GETTER(Release,release,ASIN,asin)
  MB5_C_OBJ_GETTER(Release,release,LabelInfoList,labelinfolist)
  MB5_C_OBJ_GETTER(Release,release,MediumList,mediumlist)
  MB5_C_OBJ_GETTER(Release,release,RelationListList,relationlistlist)
  MB5_C_OBJ_GETTER(Release,release,CollectionList,collectionlist)

/* --------------------------------------------------------------------------

   libmusicbrainz5 - Client library to access MusicBrainz

   Copyright (C) 2012 Andrew Hawkins

   This file is part of libmusicbrainz5.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   libmusicbrainz5 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

     $Id$

----------------------------------------------------------------------------*/

Mb5MediumList mb5_release_media_matching_discid(Mb5Release Release, const char *DiscID)
{
	if (Release)
	{
		MusicBrainz5::CRelease *TheRelease=reinterpret_cast<MusicBrainz5::CRelease *>(Release);
		if (TheRelease)
			return new MusicBrainz5::CMediumList(TheRelease->MediaMatchingDiscID(DiscID));
	}

	return 0;
}



  MB5_C_DELETE(ReleaseGroup,releasegroup)
  MB5_C_CLONE(ReleaseGroup,releasegroup)
  MB5_C_STR_GETTER(ReleaseGroup,releasegroup,ID,id)
  MB5_C_STR_GETTER(ReleaseGroup,releasegroup,PrimaryType,primarytype)
  MB5_C_STR_GETTER(ReleaseGroup,releasegroup,Title,title)
  MB5_C_STR_GETTER(ReleaseGroup,releasegroup,Disambiguation,disambiguation)
  MB5_C_STR_GETTER(ReleaseGroup,releasegroup,FirstReleaseDate,firstreleasedate)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,ArtistCredit,artistcredit)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,ReleaseList,releaselist)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,RelationListList,relationlistlist)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,TagList,taglist)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,UserTagList,usertaglist)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,Rating,rating)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,UserRating,userrating)
  MB5_C_OBJ_GETTER(ReleaseGroup,releasegroup,SecondaryTypeList,secondarytypelist)

  MB5_C_DELETE(SecondaryType,secondarytype)
  MB5_C_CLONE(SecondaryType,secondarytype)
  MB5_C_STR_GETTER(SecondaryType,secondarytype,SecondaryType,secondarytype)

  MB5_C_DELETE(Tag,tag)
  MB5_C_CLONE(Tag,tag)
  MB5_C_INT_GETTER(Tag,tag,Count,count)
  MB5_C_STR_GETTER(Tag,tag,Name,name)

  MB5_C_DELETE(TextRepresentation,textrepresentation)
  MB5_C_CLONE(TextRepresentation,textrepresentation)
  MB5_C_STR_GETTER(TextRepresentation,textrepresentation,Language,language)
  MB5_C_STR_GETTER(TextRepresentation,textrepresentation,Script,script)

  MB5_C_DELETE(Track,track)
  MB5_C_CLONE(Track,track)
  MB5_C_INT_GETTER(Track,track,Position,position)
  MB5_C_STR_GETTER(Track,track,Title,title)
  MB5_C_OBJ_GETTER(Track,track,Recording,recording)
  MB5_C_INT_GETTER(Track,track,Length,length)
  MB5_C_OBJ_GETTER(Track,track,ArtistCredit,artistcredit)
  MB5_C_STR_GETTER(Track,track,Number,number)

  MB5_C_DELETE(UserRating,userrating)
  MB5_C_CLONE(UserRating,userrating)
  MB5_C_INT_GETTER(UserRating,userrating,UserRating,userrating)

  MB5_C_DELETE(UserTag,usertag)
  MB5_C_CLONE(UserTag,usertag)
  MB5_C_STR_GETTER(UserTag,usertag,Name,name)

  MB5_C_DELETE(Work,work)
  MB5_C_CLONE(Work,work)
  MB5_C_STR_GETTER(Work,work,ID,id)
  MB5_C_STR_GETTER(Work,work,Type,type)
  MB5_C_STR_GETTER(Work,work,Title,title)
  MB5_C_OBJ_GETTER(Work,work,ArtistCredit,artistcredit)
  MB5_C_OBJ_GETTER(Work,work,ISWCList,iswclist)
  MB5_C_STR_GETTER(Work,work,Disambiguation,disambiguation)
  MB5_C_OBJ_GETTER(Work,work,AliasList,aliaslist)
  MB5_C_OBJ_GETTER(Work,work,RelationListList,relationlistlist)
  MB5_C_OBJ_GETTER(Work,work,TagList,taglist)
  MB5_C_OBJ_GETTER(Work,work,UserTagList,usertaglist)
  MB5_C_OBJ_GETTER(Work,work,Rating,rating)
  MB5_C_OBJ_GETTER(Work,work,UserRating,userrating)
  MB5_C_STR_GETTER(Work,work,Language,language)

  MB5_C_LIST_GETTER(Alias,alias)
  MB5_C_CLONE(AliasList,alias_list)

  MB5_C_LIST_GETTER(Annotation,annotation)
  MB5_C_CLONE(AnnotationList,annotation_list)

  MB5_C_LIST_GETTER(Artist,artist)
  MB5_C_CLONE(ArtistList,artist_list)

  MB5_C_LIST_GETTER(Attribute,attribute)
  MB5_C_CLONE(AttributeList,attribute_list)

  MB5_C_LIST_GETTER(CDStub,cdstub)
  MB5_C_CLONE(CDStubList,cdstub_list)

  MB5_C_LIST_GETTER(Collection,collection)
  MB5_C_CLONE(CollectionList,collection_list)

  MB5_C_LIST_GETTER(Disc,disc)
  MB5_C_CLONE(DiscList,disc_list)

  MB5_C_LIST_GETTER(FreeDBDisc,freedbdisc)
  MB5_C_CLONE(FreeDBDiscList,freedbdisc_list)

  MB5_C_LIST_GETTER(IPI,ipi)
  MB5_C_CLONE(IPIList,ipi_list)

  MB5_C_LIST_GETTER(ISRC,isrc)
  MB5_C_CLONE(ISRCList,isrc_list)

  MB5_C_LIST_GETTER(ISWC,iswc)
  MB5_C_CLONE(ISWCList,iswc_list)

  MB5_C_LIST_GETTER(Label,label)
  MB5_C_CLONE(LabelList,label_list)

  MB5_C_LIST_GETTER(LabelInfo,labelinfo)
  MB5_C_CLONE(LabelInfoList,labelinfo_list)

  MB5_C_LIST_GETTER(Medium,medium)
  MB5_C_CLONE(MediumList,medium_list)
  MB5_C_INT_GETTER(MediumList,medium_list,TrackCount,trackcount)

  MB5_C_LIST_GETTER(NameCredit,namecredit)
  MB5_C_CLONE(NameCreditList,namecredit_list)

  MB5_C_LIST_GETTER(NonMBTrack,nonmbtrack)
  MB5_C_CLONE(NonMBTrackList,nonmbtrack_list)

  MB5_C_LIST_GETTER(PUID,puid)
  MB5_C_CLONE(PUIDList,puid_list)

  MB5_C_LIST_GETTER(Recording,recording)
  MB5_C_CLONE(RecordingList,recording_list)

  MB5_C_LIST_GETTER(Relation,relation)
  MB5_C_CLONE(RelationList,relation_list)
  MB5_C_STR_GETTER(RelationList,relation_list,TargetType,targettype)

  MB5_C_LIST_GETTER(RelationList,relationlist)
  MB5_C_CLONE(RelationListList,relationlist_list)

  MB5_C_LIST_GETTER(Release,release)
  MB5_C_CLONE(ReleaseList,release_list)

  MB5_C_LIST_GETTER(ReleaseGroup,releasegroup)
  MB5_C_CLONE(ReleaseGroupList,releasegroup_list)

  MB5_C_LIST_GETTER(SecondaryType,secondarytype)
  MB5_C_CLONE(SecondaryTypeList,secondarytype_list)

  MB5_C_LIST_GETTER(Tag,tag)
  MB5_C_CLONE(TagList,tag_list)

  MB5_C_LIST_GETTER(Track,track)
  MB5_C_CLONE(TrackList,track_list)

  MB5_C_LIST_GETTER(UserTag,usertag)
  MB5_C_CLONE(UserTagList,usertag_list)

  MB5_C_LIST_GETTER(Work,work)
  MB5_C_CLONE(WorkList,work_list)

