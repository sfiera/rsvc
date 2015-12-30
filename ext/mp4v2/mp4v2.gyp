{ "conditions":
  [ [ "<(BUNDLED_MP4V2) != 0"

    , { "targets":
        [ { "target_name": "libmp4v2"
          , "type": "static_library"
          , "sources":
            [ "mp4v2-2.0.0/src/3gp.cpp"
            , "mp4v2-2.0.0/src/atom_ac3.cpp"
            , "mp4v2-2.0.0/src/atom_amr.cpp"
            , "mp4v2-2.0.0/src/atom_avc1.cpp"
            , "mp4v2-2.0.0/src/atom_avcC.cpp"
            , "mp4v2-2.0.0/src/atom_chpl.cpp"
            , "mp4v2-2.0.0/src/atom_colr.cpp"
            , "mp4v2-2.0.0/src/atom_d263.cpp"
            , "mp4v2-2.0.0/src/atom_dac3.cpp"
            , "mp4v2-2.0.0/src/atom_damr.cpp"
            , "mp4v2-2.0.0/src/atom_dref.cpp"
            , "mp4v2-2.0.0/src/atom_elst.cpp"
            , "mp4v2-2.0.0/src/atom_enca.cpp"
            , "mp4v2-2.0.0/src/atom_encv.cpp"
            , "mp4v2-2.0.0/src/atom_free.cpp"
            , "mp4v2-2.0.0/src/atom_ftyp.cpp"
            , "mp4v2-2.0.0/src/atom_ftab.cpp"
            , "mp4v2-2.0.0/src/atom_gmin.cpp"
            , "mp4v2-2.0.0/src/atom_hdlr.cpp"
            , "mp4v2-2.0.0/src/atom_hinf.cpp"
            , "mp4v2-2.0.0/src/atom_hnti.cpp"
            , "mp4v2-2.0.0/src/atom_href.cpp"
            , "mp4v2-2.0.0/src/atom_mdat.cpp"
            , "mp4v2-2.0.0/src/atom_mdhd.cpp"
            , "mp4v2-2.0.0/src/atom_meta.cpp"
            , "mp4v2-2.0.0/src/atom_mp4s.cpp"
            , "mp4v2-2.0.0/src/atom_mp4v.cpp"
            , "mp4v2-2.0.0/src/atom_mvhd.cpp"
            , "mp4v2-2.0.0/src/atom_nmhd.cpp"
            , "mp4v2-2.0.0/src/atom_ohdr.cpp"
            , "mp4v2-2.0.0/src/atom_pasp.cpp"
            , "mp4v2-2.0.0/src/atom_root.cpp"
            , "mp4v2-2.0.0/src/atom_rtp.cpp"
            , "mp4v2-2.0.0/src/atom_s263.cpp"
            , "mp4v2-2.0.0/src/atom_sdp.cpp"
            , "mp4v2-2.0.0/src/atom_sdtp.cpp"
            , "mp4v2-2.0.0/src/atom_smi.cpp"
            , "mp4v2-2.0.0/src/atom_sound.cpp"
            , "mp4v2-2.0.0/src/atom_standard.cpp"
            , "mp4v2-2.0.0/src/atom_stbl.cpp"
            , "mp4v2-2.0.0/src/atom_stdp.cpp"
            , "mp4v2-2.0.0/src/atom_stsc.cpp"
            , "mp4v2-2.0.0/src/atom_stsd.cpp"
            , "mp4v2-2.0.0/src/atom_stsz.cpp"
            , "mp4v2-2.0.0/src/atom_stz2.cpp"
            , "mp4v2-2.0.0/src/atom_text.cpp"
            , "mp4v2-2.0.0/src/atom_tfhd.cpp"
            , "mp4v2-2.0.0/src/atom_tkhd.cpp"
            , "mp4v2-2.0.0/src/atom_treftype.cpp"
            , "mp4v2-2.0.0/src/atom_trun.cpp"
            , "mp4v2-2.0.0/src/atom_tx3g.cpp"
            , "mp4v2-2.0.0/src/atom_udta.cpp"
            , "mp4v2-2.0.0/src/atom_url.cpp"
            , "mp4v2-2.0.0/src/atom_urn.cpp"
            , "mp4v2-2.0.0/src/atom_uuid.cpp"
            , "mp4v2-2.0.0/src/atom_video.cpp"
            , "mp4v2-2.0.0/src/atom_vmhd.cpp"
            , "mp4v2-2.0.0/src/cmeta.cpp"
            , "mp4v2-2.0.0/src/descriptors.cpp"
            , "mp4v2-2.0.0/src/exception.cpp"
            , "mp4v2-2.0.0/src/isma.cpp"
            , "mp4v2-2.0.0/src/log.cpp"
            , "mp4v2-2.0.0/src/mp4.cpp"
            , "mp4v2-2.0.0/src/mp4atom.cpp"
            , "mp4v2-2.0.0/src/mp4container.cpp"
            , "mp4v2-2.0.0/src/mp4descriptor.cpp"
            , "mp4v2-2.0.0/src/mp4file.cpp"
            , "mp4v2-2.0.0/src/mp4file_io.cpp"
            , "mp4v2-2.0.0/src/mp4info.cpp"
            , "mp4v2-2.0.0/src/mp4property.cpp"
            , "mp4v2-2.0.0/src/mp4track.cpp"
            , "mp4v2-2.0.0/src/mp4util.cpp"
            , "mp4v2-2.0.0/src/ocidescriptors.cpp"
            , "mp4v2-2.0.0/src/odcommands.cpp"
            , "mp4v2-2.0.0/src/qosqualifiers.cpp"
            , "mp4v2-2.0.0/src/rtphint.cpp"
            , "mp4v2-2.0.0/src/text.cpp"
            , "mp4v2-2.0.0/src/bmff/typebmff.cpp"
            , "mp4v2-2.0.0/src/itmf/CoverArtBox.cpp"
            , "mp4v2-2.0.0/src/itmf/Tags.cpp"
            , "mp4v2-2.0.0/src/itmf/generic.cpp"
            , "mp4v2-2.0.0/src/itmf/type.cpp"
            , "mp4v2-2.0.0/src/qtff/ColorParameterBox.cpp"
            , "mp4v2-2.0.0/src/qtff/PictureAspectRatioBox.cpp"
            , "mp4v2-2.0.0/src/qtff/coding.cpp"
            , "mp4v2-2.0.0/libplatform/io/File.cpp"
            , "mp4v2-2.0.0/libplatform/io/FileSystem.cpp"
            , "mp4v2-2.0.0/libplatform/prog/option.cpp"
            , "mp4v2-2.0.0/libplatform/sys/error.cpp"
            , "mp4v2-2.0.0/libplatform/time/time.cpp"

            , "mp4v2-2.0.0/libplatform/io/File_posix.cpp"
            , "mp4v2-2.0.0/libplatform/io/FileSystem_posix.cpp"
            , "mp4v2-2.0.0/libplatform/number/random_posix.cpp"
            , "mp4v2-2.0.0/libplatform/process/process_posix.cpp"
            , "mp4v2-2.0.0/libplatform/time/time_posix.cpp"

            , "mp4v2-2.0.0/libutil/Database.cpp"
            , "mp4v2-2.0.0/libutil/Timecode.cpp"
            , "mp4v2-2.0.0/libutil/TrackModifier.cpp"
            , "mp4v2-2.0.0/libutil/Utility.cpp"
            , "mp4v2-2.0.0/libutil/crc.cpp"
            , "mp4v2-2.0.0/libutil/other.cpp"
            ]
          , "include_dirs":
            [ "mp4v2-2.0.0/include"
            , "mp4v2-2.0.0"
            ]
          , "direct_dependent_settings":
            { "include_dirs": ["mp4v2-2.0.0/include"]
            }
          , "conditions":
            [ [ "OS == 'mac'"
              , { "include_dirs": ["darwin"] }
              ]
            ]
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libmp4v2"
          , "type": "static_library"
          , "link_settings":
            { "libraries": ["-lmp4v2"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
