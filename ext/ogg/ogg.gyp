{ "target_defaults":
  { "include_dirs": ["libogg-1.3.0/include"]
  , "direct_dependent_settings":
    { "include_dirs": ["libogg-1.3.0/include"]
    }
  }

, "targets": []

, "conditions":
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
          }
        ]
      }
    , { "targets":
        [ { "target_name": "libogg"
          , "type": "static_library"
          , "link_settings":
            { "libraries": ["-logg"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
