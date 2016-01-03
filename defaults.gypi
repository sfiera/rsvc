{ "make_global_settings":
  [ ["CC","/usr/bin/clang"]
  , ["LINK","/usr/bin/clang"]
  ]
, "target_defaults":
  { "default_configuration": "opt"
  , "configurations":
    { "dbg":
      { "cflags": ["-g", "-O0"]
      , "xcode_settings":
        { "GCC_OPTIMIZATION_LEVEL": "0"
        , "OTHER_CFLAGS": ["-g"]
        }
      }
    , "dev":
      { "cflags": ["-g", "-O0"]
      , "xcode_settings":
        { "GCC_OPTIMIZATION_LEVEL": "0"
        }
      }
    , "opt":
      { "cflags": ["-Os"]
      , "defines": ["NDEBUG"]
      , "xcode_settings":
        { "GCC_OPTIMIZATION_LEVEL": "s"
        }
      }
    }
  }
}
# -*- mode: python; tab-width: 2 -*-
