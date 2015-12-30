{ "target_defaults":
  { "include_dirs":
    [ "libneon-0.29.6/src"
    , "/usr/local/opt/openssl098/include"
    ]
  , "direct_dependent_settings":
    { "include_dirs": ["include"]
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

, "targets": []

, "conditions":
  [ [ "<(BUNDLED_NEON) != 0"
    , { "targets":
        [ { "target_name": "libneon"
          , "type": "static_library"
          , "sources":
            [ "neon-0.29.6/src/ne_request.c"
            , "neon-0.29.6/src/ne_session.c"
            , "neon-0.29.6/src/ne_basic.c"
            , "neon-0.29.6/src/ne_string.c"
            , "neon-0.29.6/src/ne_uri.c"
            , "neon-0.29.6/src/ne_dates.c"
            , "neon-0.29.6/src/ne_alloc.c"
            , "neon-0.29.6/src/ne_md5.c"
            , "neon-0.29.6/src/ne_utils.c"
            , "neon-0.29.6/src/ne_socket.c"
            , "neon-0.29.6/src/ne_auth.c"
            , "neon-0.29.6/src/ne_redirect.c"
            , "neon-0.29.6/src/ne_compress.c"
            , "neon-0.29.6/src/ne_i18n.c"
            , "neon-0.29.6/src/ne_pkcs11.c"
            , "neon-0.29.6/src/ne_socks.c"
            , "neon-0.29.6/src/ne_ntlm.c"
            , "neon-0.29.6/src/ne_207.c"
            , "neon-0.29.6/src/ne_xml.c"
            , "neon-0.29.6/src/ne_props.c"
            , "neon-0.29.6/src/ne_locks.c"
            , "neon-0.29.6/src/ne_xmlreq.c"
            , "neon-0.29.6/src/ne_openssl.c"
            ]
          , "dependencies":
            [ "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
            ]
          , "link_settings":
            { "libraries": ["-lgssapi_krb5", "-lssl", "-lcrypto"]
            }
          }
        ]
      }
    , { "targets":
        [ { "target_name": "libneon"
          , "type": "static_library"
          , "link_settings":
            { "libraries": ["-lneon"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
