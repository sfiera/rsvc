{ "target_defaults":
  { "variables":
    { "RSVC_VERSION": "0.0.0"
    }
  , "cflags":
    [ "-Wall"
    , "-Wextra"
    , "-Werror"
    , "-Wno-sign-compare"                # TODO(sfiera): remove
    , "-Wno-missing-field-initializers"  # TODO(sfiera): remove
    , "-fblocks"
    ]
  , "xcode_settings":
    { "OTHER_CFLAGS":
      [ "-Wall"
      , "-Wextra"
      , "-Werror"
      , "-Wno-sign-compare"
      , "-Wno-missing-field-initializers"
      ]
    }
  , "include_dirs": ["include"]
  }

, "targets":
  [ { "target_name": "rsvc"
    , "type": "executable"
    , "sources":
      [ "src/bin/rsvc.h"
      , "src/bin/strlist.h"
      , "src/bin/rsvc.c"
      , "src/bin/rsvc_convert.c"
      , "src/bin/rsvc_eject.c"
      , "src/bin/rsvc_ls.c"
      , "src/bin/rsvc_print.c"
      , "src/bin/rsvc_rip.c"
      , "src/bin/rsvc_watch.c"
      ]
    , "dependencies": ["librsvc"]
    }

  , { "target_name": "cloak"
    , "type": "executable"
    , "sources":
      [ "src/bin/cloak.h"
      , "src/bin/strlist.h"
      , "src/bin/cloak.c"
      , "src/bin/cloak_options.c"
      ]
    , "dependencies": ["librsvc"]
    }

  , { "target_name": "rsvctest"
    , "type": "executable"
    , "sources":
      [ "src/rsvc/unix.test.c"
      ]
    , "dependencies": ["librsvc"]
    }

  , { "target_name": "librsvc"
    , "type": "static_library"
    , "sources":
      [ "include/rsvc/audio.h"
      , "include/rsvc/cancel.h"
      , "include/rsvc/cd.h"
      , "include/rsvc/common.h"
      , "include/rsvc/disc.h"
      , "include/rsvc/format.h"
      , "include/rsvc/forward.h"
      , "include/rsvc/image.h"
      , "include/rsvc/musicbrainz.h"
      , "include/rsvc/tag.h"

      , "src/rsvc/audio.h"
      , "src/rsvc/common.h"
      , "src/rsvc/disc.h"
      , "src/rsvc/encoding.h"
      , "src/rsvc/group.h"
      , "src/rsvc/list.h"
      , "src/rsvc/mb4.h"
      , "src/rsvc/mb5.h"
      , "src/rsvc/options.h"
      , "src/rsvc/progress.h"
      , "src/rsvc/unix.h"

      , "src/rsvc/audio.c"
      , "src/rsvc/cancel.c"
      , "src/rsvc/common.c"
      , "src/rsvc/disc.c"
      , "src/rsvc/encoding.c"
      , "src/rsvc/flac.c"
      , "src/rsvc/format.c"
      , "src/rsvc/gif.c"
      , "src/rsvc/group.c"
      , "src/rsvc/id3.c"
      , "src/rsvc/image.c"
      , "src/rsvc/jpeg.c"
      , "src/rsvc/lame.c"
      , "src/rsvc/mad.c"
      , "src/rsvc/mp4.c"
      , "src/rsvc/musicbrainz.c"
      , "src/rsvc/options.c"
      , "src/rsvc/opus.c"
      , "src/rsvc/png.c"
      , "src/rsvc/progress.c"
      , "src/rsvc/tag.c"
      , "src/rsvc/unix.c"
      , "src/rsvc/vorbis.c"
      ]
    , "dependencies":
      [ "<(DEPTH)/ext/discid/discid.gyp:libdiscid"
      , "<(DEPTH)/ext/flac/flac.gyp:libFLAC"
      , "<(DEPTH)/ext/lame/lame.gyp:libmp3lame"
      , "<(DEPTH)/ext/mp4v2/mp4v2.gyp:libmp4v2"
      , "<(DEPTH)/ext/mad/mad.gyp:libmad"
      , "<(DEPTH)/ext/mb5/mb5.gyp:libmusicbrainz"
      , "<(DEPTH)/ext/vorbis/vorbis.gyp:libvorbis"
      , "<(DEPTH)/ext/opus/opus.gyp:libopus"
      , "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
      ]
    , "export_dependent_settings":
      [ "<(DEPTH)/ext/discid/discid.gyp:libdiscid"
      , "<(DEPTH)/ext/flac/flac.gyp:libFLAC"
      , "<(DEPTH)/ext/lame/lame.gyp:libmp3lame"
      , "<(DEPTH)/ext/mp4v2/mp4v2.gyp:libmp4v2"
      , "<(DEPTH)/ext/mad/mad.gyp:libmad"
      , "<(DEPTH)/ext/mb5/mb5.gyp:libmusicbrainz"
      , "<(DEPTH)/ext/vorbis/vorbis.gyp:libvorbis"
      , "<(DEPTH)/ext/opus/opus.gyp:libopus"
      , "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
      ]
    , "conditions":
      [ [ "OS == 'mac'"
        , { "sources":
            [ "src/rsvc/cd_darwin.c"
            , "src/rsvc/core-audio.c"
            , "src/rsvc/disc_darwin.c"
            , "src/rsvc/unix_darwin.c"
            ]
          , "link_settings":
            { "libraries":
              [ "$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework"
              , "$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework"
              , "$(SDKROOT)/System/Library/Frameworks/DiskArbitration.framework"
              , "$(SDKROOT)/System/Library/Frameworks/IOKit.framework"
              ]
            }
          }
        ]
      , [ "OS == 'linux'"
        , { "sources":
            [ "src/rsvc/cd_linux.c"
            , "src/rsvc/unix_linux.c"
            , "src/rsvc/disc_linux.c"
            ]
          , "link_settings":
            { "libraries": ["-lBlocksRuntime", "-ldispatch", "-ludev"]
            }
          }
        ]
      ]
    }
  ]
}
# -*- mode: python; tab-width: 2 -*-
