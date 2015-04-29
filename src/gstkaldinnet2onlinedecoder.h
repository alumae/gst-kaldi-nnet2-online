// gstkaldinnet2onlinedecoder.h

// Copyright 2014 Tanel Alumäe

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

#ifndef KALDI_SRC_GSTKALDINNET2ONLINEDECODER_H_
#define KALDI_SRC_GSTKALDINNET2ONLINEDECODER_H_

#include <gst/gst.h>

#include "./simple-options-gst.h"
#include "./gst-audio-source.h"

#include "online2/online-nnet2-decoding-threaded.h"
#include "online2/online-nnet2-decoding.h"

#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "lm/const-arpa-lm.h"

namespace kaldi {

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_KALDINNET2ONLINEDECODER \
  (gst_kaldinnet2onlinedecoder_get_type())
#define GST_KALDINNET2ONLINEDECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KALDINNET2ONLINEDECODER,Gstkaldinnet2onlinedecoder))
#define GST_KALDINNET2ONLINEDECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KALDINNET2ONLINEDECODER,Gstkaldinnet2onlinedecoderClass))
#define GST_IS_KALDINNET2ONLINEDECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KALDINNET2ONLINEDECODER))
#define GST_IS_KALDINNET2ONLINEDECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KALDINNET2ONLINEDECODER))

typedef struct _Gstkaldinnet2onlinedecoder Gstkaldinnet2onlinedecoder;
typedef struct _Gstkaldinnet2onlinedecoderClass Gstkaldinnet2onlinedecoderClass;

struct _Gstkaldinnet2onlinedecoder {
  GstElement element;

  GstPad *sinkpad, *srcpad;

  GstCaps *sink_caps;

  gboolean silent;
  gboolean do_endpointing;
  gboolean inverse_scale;
  float lmwt_scale;
  GstBufferSource *audio_source;

  gchar* model_rspecifier;
  gchar* fst_rspecifier;
  gchar* word_syms_filename;

  SimpleOptionsGst *simple_options;
  OnlineEndpointConfig *endpoint_config;
  OnlineNnet2FeaturePipelineConfig *feature_config;
  OnlineNnet2DecodingThreadedConfig *nnet2_decoding_threaded_config;
  OnlineNnet2DecodingConfig *nnet2_decoding_config;

  OnlineNnet2FeaturePipelineInfo *feature_info;
  TransitionModel *trans_model;
  nnet2::AmNnet *nnet;
  fst::Fst<fst::StdArc> *decode_fst;
  fst::SymbolTable *word_syms;
  int sample_rate;
  int num_transcriptions;
  gboolean decoding;
  float chunk_length_in_secs;
  float traceback_period_in_secs;
  bool use_threaded_decoder;
  OnlineIvectorExtractorAdaptationState *adaptation_state;

  // The following are needed for optional LM rescoring with a "big" LM
  gchar* lm_fst_name;
  gchar* big_lm_const_arpa_name;
  fst::MapFst<fst::StdArc, LatticeArc, fst::StdToLatticeMapper<BaseFloat> > *lm_fst;
  fst::TableComposeCache<fst::Fst<LatticeArc> > *lm_compose_cache;
  ConstArpaLm *big_lm_const_arpa;
};

struct _Gstkaldinnet2onlinedecoderClass {
  GstElementClass parent_class;
  void (*partial_result)(GstElement *element, const gchar *result_str);
  void (*final_result)(GstElement *element, const gchar *result_str);
};

GType gst_kaldinnet2onlinedecoder_get_type(void);

G_END_DECLS
}
#endif  // KALDI_SRC_GSTKALDINNET2ONLINEDECODER_H_
