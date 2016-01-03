{ "target_defaults":
  { "include_dirs":
    [ "include"
    , "libmusicbrainz-5.0.0/include"
    ]
  , "direct_dependent_settings":
    { "include_dirs":
      [ "include"
      , "libmusicbrainz-5.0.0/include"
      ]
    }
  , "conditions":
    [ [ "OS == 'mac'"
      , { "include_dirs": ["darwin"] }
      ]
    ]
  }

, "targets": []

, "conditions":
  [ [ "<(BUNDLED_MB5) != 0"
    , { "targets":
        [ { "target_name": "libmusicbrainz"
          , "type": "static_library"
          , "sources":
            [ "c/mb5_c.cc"
            , "libmusicbrainz-5.0.0/src/Alias.cc"
            , "libmusicbrainz-5.0.0/src/Annotation.cc"
            , "libmusicbrainz-5.0.0/src/Artist.cc"
            , "libmusicbrainz-5.0.0/src/ArtistCredit.cc"
            , "libmusicbrainz-5.0.0/src/Attribute.cc"
            , "libmusicbrainz-5.0.0/src/CDStub.cc"
            , "libmusicbrainz-5.0.0/src/Collection.cc"
            , "libmusicbrainz-5.0.0/src/Disc.cc"
            , "libmusicbrainz-5.0.0/src/Entity.cc"
            , "libmusicbrainz-5.0.0/src/FreeDBDisc.cc"
            , "libmusicbrainz-5.0.0/src/HTTPFetch.cc"
            , "libmusicbrainz-5.0.0/src/ISRC.cc"
            , "libmusicbrainz-5.0.0/src/IPI.cc"
            , "libmusicbrainz-5.0.0/src/ISWC.cc"
            , "libmusicbrainz-5.0.0/src/ISWCList.cc"
            , "libmusicbrainz-5.0.0/src/Label.cc"
            , "libmusicbrainz-5.0.0/src/LabelInfo.cc"
            , "libmusicbrainz-5.0.0/src/Lifespan.cc"
            , "libmusicbrainz-5.0.0/src/List.cc"
            , "libmusicbrainz-5.0.0/src/Medium.cc"
            , "libmusicbrainz-5.0.0/src/MediumList.cc"
            , "libmusicbrainz-5.0.0/src/Message.cc"
            , "libmusicbrainz-5.0.0/src/Metadata.cc"
            , "libmusicbrainz-5.0.0/src/NameCredit.cc"
            , "libmusicbrainz-5.0.0/src/NonMBTrack.cc"
            , "libmusicbrainz-5.0.0/src/PUID.cc"
            , "libmusicbrainz-5.0.0/src/Query.cc"
            , "libmusicbrainz-5.0.0/src/Rating.cc"
            , "libmusicbrainz-5.0.0/src/Recording.cc"
            , "libmusicbrainz-5.0.0/src/Relation.cc"
            , "libmusicbrainz-5.0.0/src/RelationList.cc"
            , "libmusicbrainz-5.0.0/src/RelationListList.cc"
            , "libmusicbrainz-5.0.0/src/Release.cc"
            , "libmusicbrainz-5.0.0/src/ReleaseGroup.cc"
            , "libmusicbrainz-5.0.0/src/SecondaryType.cc"
            , "libmusicbrainz-5.0.0/src/SecondaryTypeList.cc"
            , "libmusicbrainz-5.0.0/src/Tag.cc"
            , "libmusicbrainz-5.0.0/src/TextRepresentation.cc"
            , "libmusicbrainz-5.0.0/src/Track.cc"
            , "libmusicbrainz-5.0.0/src/UserRating.cc"
            , "libmusicbrainz-5.0.0/src/UserTag.cc"
            , "libmusicbrainz-5.0.0/src/Work.cc"
            , "libmusicbrainz-5.0.0/src/xmlParser.cpp"
            ]
          , "dependencies":
            [ "<(DEPTH)/ext/neon/neon.gyp:libneon"
            ]
          , "link_settings":
            { "libraries": ["-lstdc++"]
            }
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libmusicbrainz"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(MB5_INCLUDE_DIRS)"]
            , "defines": ["<@(MB5_DEFINES)"]
            , "cflags": ["<@(MB5_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(MB5_LIBRARY_DIRS)"]
            , "libraries": ["<@(MB5_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
