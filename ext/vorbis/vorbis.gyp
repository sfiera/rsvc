{ "conditions":
  [ [ "<(BUNDLED_VORBIS) != 0"
    , { "targets":
        [ { "target_name": "libvorbis"
          , "type": "static_library"
          , "sources":
            [ "libvorbis-1.3.2/lib/analysis.c"
            , "libvorbis-1.3.2/lib/bitrate.c"
            , "libvorbis-1.3.2/lib/block.c"
            , "libvorbis-1.3.2/lib/codebook.c"
            , "libvorbis-1.3.2/lib/envelope.c"
            , "libvorbis-1.3.2/lib/floor0.c"
            , "libvorbis-1.3.2/lib/floor1.c"
            , "libvorbis-1.3.2/lib/info.c"
            , "libvorbis-1.3.2/lib/lookup.c"
            , "libvorbis-1.3.2/lib/lpc.c"
            , "libvorbis-1.3.2/lib/lsp.c"
            , "libvorbis-1.3.2/lib/mapping0.c"
            , "libvorbis-1.3.2/lib/mdct.c"
            , "libvorbis-1.3.2/lib/psy.c"
            , "libvorbis-1.3.2/lib/registry.c"
            , "libvorbis-1.3.2/lib/res0.c"
            , "libvorbis-1.3.2/lib/smallft.c"
            , "libvorbis-1.3.2/lib/synthesis.c"
            , "libvorbis-1.3.2/lib/sharedbook.c"
            , "libvorbis-1.3.2/lib/vorbisenc.c"
            , "libvorbis-1.3.2/lib/vorbisfile.c"
            , "libvorbis-1.3.2/lib/window.c"
            ]
          , "dependencies":
            [ "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
            ]
          , "include_dirs":
            [ "libvorbis-1.3.2/include"
            , "libvorbis-1.3.2/lib"
            ]
          , "direct_dependent_settings":
            { "include_dirs": ["libvorbis-1.3.2/include"]
            }
          , "conditions":
            [ [ "OS == 'mac'"
              , { "include_dirs": ["darwin"]
                , "direct_dependent_settings":
                  { "include_dirs": ["darwin"]
                  }
                }
              ]
            ]
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libvorbis"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(VORBIS_INCLUDE_DIRS)"]
            , "defines": ["<@(VORBIS_DEFINES)"]
            , "cflags": ["<@(VORBIS_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(VORBIS_LIBRARY_DIRS)"]
            , "libraries": ["<@(VORBIS_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
