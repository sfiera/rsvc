{ "target_defaults":
  { "variables":
    { "RSVC_VERSION": "0.0.0"
    }
  , "cflags":
    [ "-Wall"
    , "-Werror"
    , "-Wno-sign-compare"
    , "-fblocks"
    ]
  , "include_dirs": ["include"]
  }

, "targets":
  [ { "target_name": "rsvc"
    , "type": "executable"
    , "sources":
      [ "src/bin/rsvc.c"
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
      [ "src/bin/cloak.c"
      , "src/bin/cloak_options.c"
      ]
    , "dependencies": ["librsvc"]
    }

  , { "target_name": "librsvc"
    , "type": "static_library"
    , "sources":
      [ "src/rsvc/cancel.c"
      , "src/rsvc/common.c"
      , "src/rsvc/disc.c"
      , "src/rsvc/encoding.c"
      , "src/rsvc/flac.c"
      , "src/rsvc/format.c"
      , "src/rsvc/group.c"
      , "src/rsvc/id3.c"
      , "src/rsvc/jpeg.c"
      , "src/rsvc/lame.c"
      , "src/rsvc/mad.c"
      , "src/rsvc/mp4.c"
      , "src/rsvc/musicbrainz.c"
      , "src/rsvc/options.c"
      , "src/rsvc/png.c"
      , "src/rsvc/progress.c"
      , "src/rsvc/tag.c"
      , "src/rsvc/unix.c"
      , "src/rsvc/vorbis.c"
      ]
    , "defines": ["MB_VERSION=<(MB_VERSION)"]
    , "dependencies":
      [ "<(DEPTH)/ext/discid/discid.gyp:libdiscid"
      , "<(DEPTH)/ext/flac/flac.gyp:libflac"
      , "<(DEPTH)/ext/lame/lame.gyp:libmp3lame"
      , "<(DEPTH)/ext/mp4v2/mp4v2.gyp:libmp4v2"
      , "<(DEPTH)/ext/mad/mad.gyp:libmad"
      , "<(DEPTH)/ext/libmusicbrainz/libmusicbrainz.gyp:libmusicbrainz"
      , "<(DEPTH)/ext/vorbis/vorbis.gyp:libvorbis"
      , "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
      ]
    , "export_dependent_settings":
      [ "<(DEPTH)/ext/discid/discid.gyp:libdiscid"
      , "<(DEPTH)/ext/flac/flac.gyp:libflac"
      , "<(DEPTH)/ext/lame/lame.gyp:libmp3lame"
      , "<(DEPTH)/ext/mp4v2/mp4v2.gyp:libmp4v2"
      , "<(DEPTH)/ext/mad/mad.gyp:libmad"
      , "<(DEPTH)/ext/libmusicbrainz/libmusicbrainz.gyp:libmusicbrainz"
      , "<(DEPTH)/ext/vorbis/vorbis.gyp:libvorbis"
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
              [ "$(SDKROOT)/System/Library/Frameworks/DiskArbitration.framework"
              , "$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework"
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
          }
        ]
      ]
    }
  ]
}
# -*- mode: python; tab-width: 2 -*-
