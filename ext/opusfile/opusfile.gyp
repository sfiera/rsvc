{ "conditions":
  [ [ "<(BUNDLED_OPUSFILE) != 0"
    , { "targets":
        [ { "target_name": "libopusfile"
          , "type": "static_library"
          , "sources":
            [ "opusfile-0.7/src/internal.h"
	          , "opusfile-0.7/src/info.c"
	          , "opusfile-0.7/src/internal.c"
	          , "opusfile-0.7/src/opusfile.c"
            , "opusfile-0.7/src/stream.c"
            ]
          , "dependencies":
            [ "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
            , "<(DEPTH)/ext/opus/opus.gyp:libopus"
            ]
          , "defines":
            [ "HAVE_CONFIG_H"
            ]
          , "include_dirs": ["opusfile-0.7/include"]
          , "direct_dependent_settings":
            { "include_dirs": ["opusfile-0.7/include"]
            }
          , "conditions":
            [ [ "OS == 'mac'"
              , {"include_dirs": ["darwin"]}
              ]
            ]
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libopusfile"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(OPUSFILE_INCLUDE_DIRS)"]
            , "defines": ["<@(OPUSFILE_DEFINES)"]
            , "cflags": ["<@(OPUSFILE_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(OPUSFILE_LIBRARY_DIRS)"]
            , "libraries": ["<@(OPUSFILE_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
