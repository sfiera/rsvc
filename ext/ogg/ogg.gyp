{ "conditions":
  [ [ "<(BUNDLED_OGG) != 0"
    , { "targets":
        [ { "target_name": "libogg"
          , "type": "static_library"
          , "sources":
            [ "libogg-1.3.0/src/bitwise.c"
            , "libogg-1.3.0/src/framing.c"
            ]
          , "conditions":
            [ [ "OS == 'mac'"
              , { "include_dirs": ["darwin"]
                }
              ]
            ]
          , "include_dirs": ["libogg-1.3.0/include"]
          , "direct_dependent_settings":
            { "include_dirs": ["libogg-1.3.0/include"]
            }
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libogg"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(OGG_INCLUDE_DIRS)"]
            , "defines": ["<@(OGG_DEFINES)"]
            , "cflags": ["<@(OGG_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(OGG_LIBRARY_DIRS)"]
            , "libraries": ["<@(OGG_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
