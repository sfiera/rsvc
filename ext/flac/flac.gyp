{ "target_defaults":
  { "include_dirs":
    [ "flac-1.2.1/include"
    , "flac-1.2.1/src/libFLAC/include"
    ]
  , "direct_dependent_settings":
    { "include_dirs": ["flac-1.2.1/include"]
    }
  }

, "targets": []

, "conditions":
  [ [ "<(BUNDLED_FLAC) != 0"
    , { "targets":
        [ { "target_name": "libFLAC"
          , "type": "static_library"
          , "sources":
            [ "flac-1.2.1/src/libFLAC/bitmath.c"
            , "flac-1.2.1/src/libFLAC/bitreader.c"
            , "flac-1.2.1/src/libFLAC/bitwriter.c"
            , "flac-1.2.1/src/libFLAC/cpu.c"
            , "flac-1.2.1/src/libFLAC/crc.c"
            , "flac-1.2.1/src/libFLAC/fixed.c"
            , "flac-1.2.1/src/libFLAC/float.c"
            , "flac-1.2.1/src/libFLAC/format.c"
            , "flac-1.2.1/src/libFLAC/lpc.c"
            , "flac-1.2.1/src/libFLAC/md5.c"
            , "flac-1.2.1/src/libFLAC/memory.c"
            , "flac-1.2.1/src/libFLAC/metadata_iterators.c"
            , "flac-1.2.1/src/libFLAC/metadata_object.c"
            , "flac-1.2.1/src/libFLAC/ogg_decoder_aspect.c"
            , "flac-1.2.1/src/libFLAC/ogg_encoder_aspect.c"
            , "flac-1.2.1/src/libFLAC/ogg_helper.c"
            , "flac-1.2.1/src/libFLAC/ogg_mapping.c"
            , "flac-1.2.1/src/libFLAC/stream_decoder.c"
            , "flac-1.2.1/src/libFLAC/stream_encoder.c"
            , "flac-1.2.1/src/libFLAC/stream_encoder_framing.c"
            , "flac-1.2.1/src/libFLAC/window.c"
            ]
          , "defines": ["VERSION=\"1.2.1\""]
          , "dependencies":
            [ "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
            ]
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libFLAC"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(FLAC_INCLUDE_DIRS)"]
            , "defines": ["<@(FLAC_DEFINES)"]
            , "cflags": ["<@(FLAC_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(FLAC_LIBRARY_DIRS)"]
            , "libraries": ["<@(FLAC_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
