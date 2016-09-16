// gstkaldinnet3onlinedecoder.h

// Copyright 2014 Tanel Alumäe
// Copyright 2015 University of Sheffield (author: Ricard Marxer <r.marxer@sheffield.ac.uk>)

// See ../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#ifndef KALDI_SRC_GSTKALDINNET3ONLINEDECODER_H_
#define KALDI_SRC_GSTKALDINNET3ONLINEDECODER_H_

#include <gst/gst.h>

#include "./simple-options-gst.h"
#include "./gst-audio-source.h"

#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/online-nnet3-decoding.h"

#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "lm/const-arpa-lm.h"
#include "lat/word-align-lattice.h"

namespace kaldi {

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_KALDINNET3ONLINEDECODER \
  (gst_kaldinnet3onlinedecoder_get_type())
#define GST_KALDINNET3ONLINEDECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KALDINNET3ONLINEDECODER,Gstkaldinnet3onlinedecoder))
#define GST_KALDINNET3ONLINEDECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KALDINNET3ONLINEDECODER,Gstkaldinnet3onlinedecoderClass))
#define GST_IS_KALDINNET3ONLINEDECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KALDINNET3ONLINEDECODER))
#define GST_IS_KALDINNET3ONLINEDECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KALDINNET3ONLINEDECODER))

typedef struct _Gstkaldinnet3onlinedecoder Gstkaldinnet3onlinedecoder;
typedef struct _Gstkaldinnet3onlinedecoderClass Gstkaldinnet3onlinedecoderClass;

struct _Gstkaldinnet3onlinedecoder {
  GstElement element;

  GstPad *sinkpad, *srcpad;

  GstCaps *sink_caps;

  gboolean silent;
  gboolean do_endpointing;
  gboolean inverse_scale;
  float lmwt_scale;
  GstBufferSource *audio_source;
  gboolean do_phone_alignment;

  gchar* model_rspecifier;
  gchar* fst_rspecifier;
  gchar* word_syms_filename;
  gchar* phone_syms_filename;
  gchar* word_boundary_info_filename;

  SimpleOptionsGst *simple_options;
  OnlineEndpointConfig *endpoint_config;
  OnlineNnet2FeaturePipelineConfig *feature_config;
  OnlineNnet3DecodingConfig *nnet3_decoding_config;
  OnlineSilenceWeightingConfig *silence_weighting_config;

  OnlineNnet2FeaturePipelineInfo *feature_info;
  TransitionModel *trans_model;
  nnet3::AmNnetSimple *nnet;
  fst::Fst<fst::StdArc> *decode_fst;
  fst::SymbolTable *word_syms;
  fst::SymbolTable *phone_syms;
  WordBoundaryInfo *word_boundary_info;
  int sample_rate;
  gboolean decoding;
  float chunk_length_in_secs;
  float traceback_period_in_secs;
  bool use_threaded_decoder;
  guint num_nbest;
  guint min_words_for_ivector;
  OnlineIvectorExtractorAdaptationState *adaptation_state;
  float segment_start_time;
  float total_time_decoded;

  // The following are needed for optional LM rescoring with a "big" LM
  gchar* lm_fst_name;
  gchar* big_lm_const_arpa_name;
  fst::MapFst<fst::StdArc, LatticeArc, fst::StdToLatticeMapper<BaseFloat> > *lm_fst;
  fst::TableComposeCache<fst::Fst<LatticeArc> > *lm_compose_cache;
  ConstArpaLm *big_lm_const_arpa;
};

struct _Gstkaldinnet3onlinedecoderClass {
  GstElementClass parent_class;
  void (*partial_result)(GstElement *element, const gchar *result_str);
  void (*final_result)(GstElement *element, const gchar *result_str);
  void (*full_final_result)(GstElement *element, const gchar *result_str);
};

GType gst_kaldinnet3onlinedecoder_get_type(void);

G_END_DECLS
}
#endif  // KALDI_SRC_GSTKALDINNET3ONLINEDECODER_H_
