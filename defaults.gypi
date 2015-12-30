{ "make_global_settings":
  [ ["CC","/usr/bin/clang"]
  , ["LINK","/usr/bin/clang"]
  ]
, "target_defaults":
  { "default_configuration": "opt"
  , "configurations":
    { "dbg":
      { "cflags": ["-g"]
      }
    , "dev": { }
    , "opt":
      { "cflags": ["-Os"]
      , "defines": ["NDEBUG"]
      }
    }
  }
}
# -*- mode: python; tab-width: 2 -*-
