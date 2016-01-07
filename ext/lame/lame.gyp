{ "conditions":
  [ [ "<(BUNDLED_LAME) != 0"
    , { "targets":
        [ { "target_name": "libmp3lame"
          , "type": "static_library"
          , "sources":
            [ "lame-3.99.5/libmp3lame/VbrTag.c"
            , "lame-3.99.5/libmp3lame/bitstream.c"
            , "lame-3.99.5/libmp3lame/encoder.c"
            , "lame-3.99.5/libmp3lame/fft.c"
            , "lame-3.99.5/libmp3lame/gain_analysis.c"
            , "lame-3.99.5/libmp3lame/id3tag.c"
            , "lame-3.99.5/libmp3lame/lame.c"
            , "lame-3.99.5/libmp3lame/mpglib_interface.c"
            , "lame-3.99.5/libmp3lame/newmdct.c"
            , "lame-3.99.5/libmp3lame/presets.c"
            , "lame-3.99.5/libmp3lame/psymodel.c"
            , "lame-3.99.5/libmp3lame/quantize.c"
            , "lame-3.99.5/libmp3lame/quantize_pvt.c"
            , "lame-3.99.5/libmp3lame/reservoir.c"
            , "lame-3.99.5/libmp3lame/set_get.c"
            , "lame-3.99.5/libmp3lame/tables.c"
            , "lame-3.99.5/libmp3lame/takehiro.c"
            , "lame-3.99.5/libmp3lame/util.c"
            , "lame-3.99.5/libmp3lame/vbrquantize.c"
            , "lame-3.99.5/libmp3lame/vector/xmm_quantize_sub.c"
            , "lame-3.99.5/libmp3lame/version.c"
            , "lame-3.99.5/mpglib/common.c"
            , "lame-3.99.5/mpglib/dct64_i386.c"
            , "lame-3.99.5/mpglib/decode_i386.c"
            , "lame-3.99.5/mpglib/interface.c"
            , "lame-3.99.5/mpglib/layer1.c"
            , "lame-3.99.5/mpglib/layer2.c"
            , "lame-3.99.5/mpglib/layer3.c"
            , "lame-3.99.5/mpglib/tabinit.c"
            ]
          , "defines": ["HAVE_CONFIG_H"]
          , "cflags":
            [ "-Wno-tautological-pointer-compare"
            , "-Wno-absolute-value"
            ]
          , "xcode_settings":
            { "OTHER_CFLAGS":
              [ "-Wno-tautological-pointer-compare"
              , "-Wno-absolute-value"
              ]
            }
          , "include_dirs":
            [ "lame-3.99.5/include"
            , "lame-3.99.5/libmp3lame"
            , "lame-3.99.5/mpglib"
            ]
          , "direct_dependent_settings":
            { "include_dirs": ["include"]
            }
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
        [ { "target_name": "libmp3lame"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(LAME_INCLUDE_DIRS)"]
            , "defines": ["<@(LAME_DEFINES)"]
            , "cflags": ["<@(LAME_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(LAME_LIBRARY_DIRS)"]
            , "libraries": ["<@(LAME_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
