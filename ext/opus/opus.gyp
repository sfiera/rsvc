{ "target_defaults":
  { "include_dirs":
    [ "opus-1.1.1"
    , "opus-1.1.1/include"
    , "opus-1.1.1/celt"
    , "opus-1.1.1/silk"
    , "opus-1.1.1/silk/float"
    , "opus-1.1.1/silk/fixed"
    ]
  , "direct_dependent_settings":
    { "include_dirs": ["libopus-1.1.1/include"]
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

, "conditions":
  [ [ "<(BUNDLED_OPUS) != 0"
    , { "targets":
        [ { "target_name": "libopus"
          , "type": "static_library"
          , "sources":
            [ "opus-1.1.1/celt/bands.c"
            , "opus-1.1.1/celt/celt.c"
            , "opus-1.1.1/celt/celt_encoder.c"
            , "opus-1.1.1/celt/celt_decoder.c"
            , "opus-1.1.1/celt/cwrs.c"
            , "opus-1.1.1/celt/entcode.c"
            , "opus-1.1.1/celt/entdec.c"
            , "opus-1.1.1/celt/entenc.c"
            , "opus-1.1.1/celt/kiss_fft.c"
            , "opus-1.1.1/celt/laplace.c"
            , "opus-1.1.1/celt/mathops.c"
            , "opus-1.1.1/celt/mdct.c"
            , "opus-1.1.1/celt/modes.c"
            , "opus-1.1.1/celt/pitch.c"
            , "opus-1.1.1/celt/celt_lpc.c"
            , "opus-1.1.1/celt/quant_bands.c"
            , "opus-1.1.1/celt/rate.c"
            , "opus-1.1.1/celt/vq.c"

            , "opus-1.1.1/silk/CNG.c"
            , "opus-1.1.1/silk/code_signs.c"
            , "opus-1.1.1/silk/init_decoder.c"
            , "opus-1.1.1/silk/decode_core.c"
            , "opus-1.1.1/silk/decode_frame.c"
            , "opus-1.1.1/silk/decode_parameters.c"
            , "opus-1.1.1/silk/decode_indices.c"
            , "opus-1.1.1/silk/decode_pulses.c"
            , "opus-1.1.1/silk/decoder_set_fs.c"
            , "opus-1.1.1/silk/dec_API.c"
            , "opus-1.1.1/silk/enc_API.c"
            , "opus-1.1.1/silk/encode_indices.c"
            , "opus-1.1.1/silk/encode_pulses.c"
            , "opus-1.1.1/silk/gain_quant.c"
            , "opus-1.1.1/silk/interpolate.c"
            , "opus-1.1.1/silk/LP_variable_cutoff.c"
            , "opus-1.1.1/silk/NLSF_decode.c"
            , "opus-1.1.1/silk/NSQ.c"
            , "opus-1.1.1/silk/NSQ_del_dec.c"
            , "opus-1.1.1/silk/PLC.c"
            , "opus-1.1.1/silk/shell_coder.c"
            , "opus-1.1.1/silk/tables_gain.c"
            , "opus-1.1.1/silk/tables_LTP.c"
            , "opus-1.1.1/silk/tables_NLSF_CB_NB_MB.c"
            , "opus-1.1.1/silk/tables_NLSF_CB_WB.c"
            , "opus-1.1.1/silk/tables_other.c"
            , "opus-1.1.1/silk/tables_pitch_lag.c"
            , "opus-1.1.1/silk/tables_pulses_per_block.c"
            , "opus-1.1.1/silk/VAD.c"
            , "opus-1.1.1/silk/control_audio_bandwidth.c"
            , "opus-1.1.1/silk/quant_LTP_gains.c"
            , "opus-1.1.1/silk/VQ_WMat_EC.c"
            , "opus-1.1.1/silk/HP_variable_cutoff.c"
            , "opus-1.1.1/silk/NLSF_encode.c"
            , "opus-1.1.1/silk/NLSF_VQ.c"
            , "opus-1.1.1/silk/NLSF_unpack.c"
            , "opus-1.1.1/silk/NLSF_del_dec_quant.c"
            , "opus-1.1.1/silk/process_NLSFs.c"
            , "opus-1.1.1/silk/stereo_LR_to_MS.c"
            , "opus-1.1.1/silk/stereo_MS_to_LR.c"
            , "opus-1.1.1/silk/check_control_input.c"
            , "opus-1.1.1/silk/control_SNR.c"
            , "opus-1.1.1/silk/init_encoder.c"
            , "opus-1.1.1/silk/control_codec.c"
            , "opus-1.1.1/silk/A2NLSF.c"
            , "opus-1.1.1/silk/ana_filt_bank_1.c"
            , "opus-1.1.1/silk/biquad_alt.c"
            , "opus-1.1.1/silk/bwexpander_32.c"
            , "opus-1.1.1/silk/bwexpander.c"
            , "opus-1.1.1/silk/debug.c"
            , "opus-1.1.1/silk/decode_pitch.c"
            , "opus-1.1.1/silk/inner_prod_aligned.c"
            , "opus-1.1.1/silk/lin2log.c"
            , "opus-1.1.1/silk/log2lin.c"
            , "opus-1.1.1/silk/LPC_analysis_filter.c"
            , "opus-1.1.1/silk/LPC_inv_pred_gain.c"
            , "opus-1.1.1/silk/table_LSF_cos.c"
            , "opus-1.1.1/silk/NLSF2A.c"
            , "opus-1.1.1/silk/NLSF_stabilize.c"
            , "opus-1.1.1/silk/NLSF_VQ_weights_laroia.c"
            , "opus-1.1.1/silk/pitch_est_tables.c"
            , "opus-1.1.1/silk/resampler.c"
            , "opus-1.1.1/silk/resampler_down2_3.c"
            , "opus-1.1.1/silk/resampler_down2.c"
            , "opus-1.1.1/silk/resampler_private_AR2.c"
            , "opus-1.1.1/silk/resampler_private_down_FIR.c"
            , "opus-1.1.1/silk/resampler_private_IIR_FIR.c"
            , "opus-1.1.1/silk/resampler_private_up2_HQ.c"
            , "opus-1.1.1/silk/resampler_rom.c"
            , "opus-1.1.1/silk/sigm_Q15.c"
            , "opus-1.1.1/silk/sort.c"
            , "opus-1.1.1/silk/sum_sqr_shift.c"
            , "opus-1.1.1/silk/stereo_decode_pred.c"
            , "opus-1.1.1/silk/stereo_encode_pred.c"
            , "opus-1.1.1/silk/stereo_find_predictor.c"
            , "opus-1.1.1/silk/stereo_quant_pred.c"

            , "opus-1.1.1/silk/float/apply_sine_window_FLP.c"
            , "opus-1.1.1/silk/float/corrMatrix_FLP.c"
            , "opus-1.1.1/silk/float/encode_frame_FLP.c"
            , "opus-1.1.1/silk/float/find_LPC_FLP.c"
            , "opus-1.1.1/silk/float/find_LTP_FLP.c"
            , "opus-1.1.1/silk/float/find_pitch_lags_FLP.c"
            , "opus-1.1.1/silk/float/find_pred_coefs_FLP.c"
            , "opus-1.1.1/silk/float/LPC_analysis_filter_FLP.c"
            , "opus-1.1.1/silk/float/LTP_analysis_filter_FLP.c"
            , "opus-1.1.1/silk/float/LTP_scale_ctrl_FLP.c"
            , "opus-1.1.1/silk/float/noise_shape_analysis_FLP.c"
            , "opus-1.1.1/silk/float/prefilter_FLP.c"
            , "opus-1.1.1/silk/float/process_gains_FLP.c"
            , "opus-1.1.1/silk/float/regularize_correlations_FLP.c"
            , "opus-1.1.1/silk/float/residual_energy_FLP.c"
            , "opus-1.1.1/silk/float/solve_LS_FLP.c"
            , "opus-1.1.1/silk/float/warped_autocorrelation_FLP.c"
            , "opus-1.1.1/silk/float/wrappers_FLP.c"
            , "opus-1.1.1/silk/float/autocorrelation_FLP.c"
            , "opus-1.1.1/silk/float/burg_modified_FLP.c"
            , "opus-1.1.1/silk/float/bwexpander_FLP.c"
            , "opus-1.1.1/silk/float/energy_FLP.c"
            , "opus-1.1.1/silk/float/inner_product_FLP.c"
            , "opus-1.1.1/silk/float/k2a_FLP.c"
            , "opus-1.1.1/silk/float/levinsondurbin_FLP.c"
            , "opus-1.1.1/silk/float/LPC_inv_pred_gain_FLP.c"
            , "opus-1.1.1/silk/float/pitch_analysis_core_FLP.c"
            , "opus-1.1.1/silk/float/scale_copy_vector_FLP.c"
            , "opus-1.1.1/silk/float/scale_vector_FLP.c"
            , "opus-1.1.1/silk/float/schur_FLP.c"
            , "opus-1.1.1/silk/float/sort_FLP.c"

            , "opus-1.1.1/src/opus.c"
            , "opus-1.1.1/src/opus_decoder.c"
            , "opus-1.1.1/src/opus_encoder.c"
            , "opus-1.1.1/src/opus_multistream.c"
            , "opus-1.1.1/src/opus_multistream_encoder.c"
            , "opus-1.1.1/src/opus_multistream_decoder.c"
            , "opus-1.1.1/src/repacketizer.c"
            , "opus-1.1.1/src/analysis.c"
            , "opus-1.1.1/src/mlp.c"
            , "opus-1.1.1/src/mlp_data.c"
            ]
          , "dependencies":
            [ "<(DEPTH)/ext/ogg/ogg.gyp:libogg"
            ]
          , "defines":
            [ "HAVE_CONFIG_H"
            , "OPUS_WILL_BE_SLOW"  # Silence warning in -O0 mode.
            ]
          }
        ]
      }

    , { "targets":
        [ { "target_name": "libopus"
          , "type": "static_library"
          , "direct_dependent_settings":
            { "include_dirs": ["<@(OPUS_INCLUDE_DIRS)"]
            , "defines": ["<@(OPUS_DEFINES)"]
            , "cflags": ["<@(OPUS_CFLAGS)"]
            }
          , "link_settings":
            { "library_dirs": ["<@(OPUS_LIBRARY_DIRS)"]
            , "libraries": ["<@(OPUS_LIBRARIES)"]
            }
          }
        ]
      }
    ]
  ]
}
# -*- mode: python; tab-width: 2 -*-
