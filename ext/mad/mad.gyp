{ "conditions":
  [ [ "<(BUNDLED_MAD) != 0"
    , { "targets":
        [ { "target_name": "libmad"
          , "type": "static_library"
          , "sources":
            [ "libmad-0.15.1b/bit.c"
            , "libmad-0.15.1b/decoder.c"
            , "libmad-0.15.1b/fixed.c"
            , "libmad-0.15.1b/frame.c"
            , "libmad-0.15.1b/huffman.c"
            , "libmad-0.15.1b/layer12.c"
            , "libmad-0.15.1b/layer3.c"
            , "libmad-0.15.1b/stream.c"
            , "libmad-0.15.1b/synth.c"
            , "libmad-0.15.1b/timer.c"
            , "libmad-0.15.1b/version.c"
            ]
          , "defines":
            [ "ASO_ZEROCHECK"
            , "FPM_64BIT"
            , "HAVE_CONFIG_H"
            ]
          , "include_dirs": ["libmad-0.15.1b"]
          , "cflags": ["-fasm"]
          , "direct_dependent_settings":
            { "include_dirs": ["libmad-0.15.1b"]
            }
          , "conditions":
            [ [ "OS == 'mac'"
              , { "include_dirs": ["darwin", "darwin/include"]
                , "direct_dependent_settings":
                  { "include_dirs": ["darwin/include"]
                  }
                }
              ]
            ]
          }
        ]
      }
    , { "targets":
        [ { "target_name": "libmad"
          , "type": "static_library"
          , "link_settings":
            { "libraries": ["-lmad"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
