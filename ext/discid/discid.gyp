{ "conditions":
  [ [ "<(BUNDLED_DISCID) != 0"
    , { "targets":
        [ { "target_name": "libdiscid"
          , "type": "static_library"
          , "sources":
            [ "libdiscid-0.2.2/src/base64.c"
            , "libdiscid-0.2.2/src/disc.c"
            , "libdiscid-0.2.2/src/sha1.c"
            ]
          , "conditions":
            [ [ "OS == 'mac'"
              , { "sources": ["libdiscid-0.2.2/src/disc_darwin.c"]
                , "include_dirs": ["darwin"]
                , "link_settings":
                  { "libraries":
                    [ "$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework"
                    , "$(SDKROOT)/System/Library/Frameworks/IOKit.framework"
                    ]
                  }
                }
              ]
            ]
          , "include_dirs": ["libdiscid-0.2.2/include"]
          , "direct_dependent_settings":
            { "include_dirs": ["libdiscid-0.2.2/include"]
            }
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libdiscid"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(DISCID_INCLUDE_DIRS)"]
            , "defines": ["<@(DISCID_DEFINES)"]
            , "cflags": ["<@(DISCID_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(DISCID_LIBRARY_DIRS)"]
            , "libraries": ["<@(DISCID_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
