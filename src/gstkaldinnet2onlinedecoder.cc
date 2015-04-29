/*
 * GStreamer
 * Copyright 2014 Tanel Alumae <tanel.alumae@phon.ioc.ee>
 * Copyright 2014 Johns Hopkins University (author: Daniel Povey)
 *
 * 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
 * WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
 * MERCHANTABLITY OR NON-INFRINGEMENT.
 * See the Apache 2 License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * SECTION:element-kaldinnet2onlinedecoder
 *
 * Converts speech to text using Kaldi's SingleUtteranceNnet2Decoder.
 *
 * <title>Example launch line</title>
 * |[
 * GST_PLUGIN_PATH=. gst-launch-1.0 --gst-debug="kaldinnet2onlinedecoder:5" -q \
 * filesrc location=123_456.wav ! decodebin ! audioconvert ! audioresample ! \
 * kaldinnet2onlinedecoder model=nnet2_online_ivector_online/final.mdl fst=tri3b/graph/HCLG.fst word-syms=tri3b/graph/words.txt \
 * feature-type=mfcc mfcc-config=nnet2_online_ivector_online/conf/mfcc.conf \
 * ivector-extraction-config=ivector_extractor.conf max-active=7000 beam=11.0 lattice-beam=5.0 \
 * do-endpointing=true endpoint-silence-phones="1:2:3:4:5" ! filesink location=tmp.txt
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  define VERSION "1.0"
#endif

#include <gst/gst.h>

#include "./kaldimarshal.h"
#include "./gstkaldinnet2onlinedecoder.h"

#include "fstext/fstext-lib.h"
#include "lat/confidence.h"
#include <fst/script/project.h>

#include <fstream>
#include <iostream>


namespace kaldi {

GST_DEBUG_CATEGORY_STATIC(gst_kaldinnet2onlinedecoder_debug);
#define GST_CAT_DEFAULT gst_kaldinnet2onlinedecoder_debug

/* Filter signals and args */
enum {
  PARTIAL_RESULT_SIGNAL,
  FINAL_RESULT_SIGNAL,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SILENT,
  PROP_MODEL,
  PROP_FST,
  PROP_WORD_SYMS,
  PROP_DO_ENDPOINTING,
  PROP_ADAPTATION_STATE,
  PROP_INVERSE_SCALE,
  PROP_LMWT_SCALE,
  PROP_CHUNK_LENGTH_IN_SECS,
  PROP_TRACEBACK_PERIOD_IN_SECS,
  PROP_LM_FST,
  PROP_BIG_LM_CONST_ARPA,
  PROP_USE_THREADED_DECODER,
  PROP_NUM_TRANSCRIPTIONS,
  PROP_LAST
};

#define DEFAULT_MODEL           "final.mdl"
#define DEFAULT_FST             "HCLG.fst"
#define DEFAULT_WORD_SYMS       "words.txt"
#define DEFAULT_LMWT_SCALE	1.0
#define DEFAULT_CHUNK_LENGTH_IN_SECS  0.05
#define DEFAULT_TRACEBACK_PERIOD_IN_SECS  0.5
#define DEAFULT_USE_THREADED_DECODER false
#define DEFAULT_NUM_TRANSCRIPTIONS 1

/* the capabilities of the inputs and outputs.
 *
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "audio/x-raw, "
        "format = (string) S16LE, "
        "channels = (int) 1, "
        "rate = (int) [ 1, MAX ]"));

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("text/x-raw, format= { utf8 }"));

static guint gst_kaldinnet2onlinedecoder_signals[LAST_SIGNAL];

#define gst_kaldinnet2onlinedecoder_parent_class parent_class
G_DEFINE_TYPE(Gstkaldinnet2onlinedecoder, gst_kaldinnet2onlinedecoder,
              GST_TYPE_ELEMENT);

static void gst_kaldinnet2onlinedecoder_set_property(GObject * object,
                                                     guint prop_id,
                                                     const GValue * value,
                                                     GParamSpec * pspec);

static void gst_kaldinnet2onlinedecoder_get_property(GObject * object,
                                                     guint prop_id,
                                                     GValue * value,
                                                     GParamSpec * pspec);

static gboolean gst_kaldinnet2onlinedecoder_sink_event(GstPad * pad,
                                                       GstObject * parent,
                                                       GstEvent * event);

static GstFlowReturn gst_kaldinnet2onlinedecoder_chain(GstPad * pad,
                                                       GstObject * parent,
                                                       GstBuffer * buf);

static GstStateChangeReturn gst_kaldinnet2onlinedecoder_change_state(
    GstElement *element, GstStateChange transition);

static gboolean gst_kaldinnet2onlinedecoder_query(GstPad *pad, GstObject * parent, GstQuery * query);

static void gst_kaldinnet2onlinedecoder_finalize(GObject * object);

/* GObject vmethod implementations */

/* initialize the kaldinnet2onlinedecoder's class */
static void gst_kaldinnet2onlinedecoder_class_init(
    Gstkaldinnet2onlinedecoderClass * klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_kaldinnet2onlinedecoder_set_property;
  gobject_class->get_property = gst_kaldinnet2onlinedecoder_get_property;
  gobject_class->finalize = gst_kaldinnet2onlinedecoder_finalize;

  gstelement_class->change_state = gst_kaldinnet2onlinedecoder_change_state;

  g_object_class_install_property(
      gobject_class, PROP_SILENT,
      g_param_spec_boolean("silent", "Silent", "Silence the decoder",
      FALSE,
                           (GParamFlags) G_PARAM_READWRITE));
  g_object_class_install_property(
      gobject_class,
      PROP_MODEL,
      g_param_spec_string("model", "Acoustic model",
                          "Filename of the acoustic model",
                          DEFAULT_MODEL,
                          (GParamFlags) G_PARAM_READWRITE));
  g_object_class_install_property(
      gobject_class, PROP_FST,
      g_param_spec_string("fst", "Decoding FST", "Filename of the HCLG FST",
      DEFAULT_FST,
                          (GParamFlags) G_PARAM_READWRITE));
  g_object_class_install_property(
      gobject_class,
      PROP_WORD_SYMS,
      g_param_spec_string("word-syms", "Word symbols",
                          "Name of word symbols file (typically words.txt)",
                          DEFAULT_WORD_SYMS,
                          (GParamFlags) G_PARAM_READWRITE));
  g_object_class_install_property(
      gobject_class,
      PROP_DO_ENDPOINTING,
      g_param_spec_boolean(
          "do-endpointing", "If true, apply endpoint detection",
          "If true, apply endpoint detection, and split the audio at endpoints",
          FALSE,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_ADAPTATION_STATE,
      g_param_spec_string("adaptation-state", "Adaptation state",
                          "Current adaptation state, in stringified form, set to empty string to reset",
                          "",
                          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_INVERSE_SCALE,
      g_param_spec_boolean(
          "inverse-scale", "If true, inverse acoustic scale in lattice",
          "If true, inverse the acoustic scaling of the output lattice",
          FALSE,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_LMWT_SCALE,
      g_param_spec_float(
          "lmwt-scale", "LM weight for scaling output lattice",
          "LM scaling for the output lattice, usually in conjunction with inverse-scaling=true",
          G_MINFLOAT,
          G_MAXFLOAT,
          DEFAULT_LMWT_SCALE,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_CHUNK_LENGTH_IN_SECS,
      g_param_spec_float(
          "chunk-length-in-secs", "Length of a audio chunk that is processed at a time",
          "Smaller values decrease latency, bigger values (e.g. 0.2) improve speed if multithreaded BLAS/MKL is used",
          0.05,
          G_MAXFLOAT,
          DEFAULT_CHUNK_LENGTH_IN_SECS,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_TRACEBACK_PERIOD_IN_SECS,
      g_param_spec_float(
          "traceback-period-in-secs", "Time period after which new interim recognition result is sent",
          "Time period after which new interim recognition result is sent",
          0.05,
          G_MAXFLOAT,
          DEFAULT_TRACEBACK_PERIOD_IN_SECS,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_LM_FST,
      g_param_spec_string(
          "lm-fst",
          "Language language model FST (G.fst), only needed when rescoring with the constant ARPA LM",
          "Old LM as FST (G.fst)", "", (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_BIG_LM_CONST_ARPA,
      g_param_spec_string(
          "big-lm-const-arpa",
          "Big language model in constant ARPA format (typically G.carpa), to be used for rescoring final lattices. Also requires 'lm-fst' property",
          "Big language model in constant ARPA format (typically G.carpa), to be used for rescoring final lattices. Also requires 'lm-fst' property",
          "", (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_USE_THREADED_DECODER,
      g_param_spec_boolean(
          "use-threaded-decoder",
          "Use a decoder that does feature calculation and decoding in separate threads (NB! must be set before other properties)",
          "Whether to use a threaded decoder (NB! must be set before other properties)",
          DEAFULT_USE_THREADED_DECODER,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(
      gobject_class,
      PROP_NUM_TRANSCRIPTIONS,
      g_param_spec_int(
          "num-transcriptions",
          "How many transcriptions to return per utterance (note this is the maximum, less is possible)",
          "How many transcriptions to return per utterance",
          0,
          1000,
          DEFAULT_NUM_TRANSCRIPTIONS,
          (GParamFlags) G_PARAM_READWRITE));

  gst_kaldinnet2onlinedecoder_signals[PARTIAL_RESULT_SIGNAL] = g_signal_new(
      "partial-result", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(Gstkaldinnet2onlinedecoderClass, partial_result),
      NULL,
      NULL, kaldi_marshal_VOID__STRING, G_TYPE_NONE, 1,
      G_TYPE_STRING);

  gst_kaldinnet2onlinedecoder_signals[FINAL_RESULT_SIGNAL] = g_signal_new(
      "final-result", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(Gstkaldinnet2onlinedecoderClass, final_result),
      NULL,
      NULL, kaldi_marshal_VOID__STRING, G_TYPE_NONE, 1,
      G_TYPE_STRING);

  gst_element_class_set_details_simple(
      gstelement_class, "KaldiNNet2OnlineDecoder", "Speech/Audio",
      "Convert speech to text", "Tanel Alumae <tanel.alumae@phon.ioc.ee>");


  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&sink_template));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_kaldinnet2onlinedecoder_init(
    Gstkaldinnet2onlinedecoder * filter) {
  bool tmp_bool;
  int32 tmp_int;
  uint32 tmp_uint;
  float tmp_float;
  double tmp_double;
  std::string tmp_string;

  filter->sinkpad = NULL;

  filter->sinkpad = gst_pad_new_from_static_template(&sink_template, "sink");
  gst_pad_set_event_function(
      filter->sinkpad,
      GST_DEBUG_FUNCPTR(gst_kaldinnet2onlinedecoder_sink_event));
  gst_pad_set_chain_function(
      filter->sinkpad, GST_DEBUG_FUNCPTR(gst_kaldinnet2onlinedecoder_chain));
  gst_pad_set_query_function(
      filter->sinkpad, GST_DEBUG_FUNCPTR(gst_kaldinnet2onlinedecoder_query));
  gst_pad_use_fixed_caps(filter->sinkpad);
  gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template(&src_template, "src");
  gst_pad_use_fixed_caps(filter->srcpad);
  gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

  filter->silent = FALSE;
  filter->model_rspecifier = g_strdup(DEFAULT_MODEL);
  filter->fst_rspecifier = g_strdup(DEFAULT_FST);
  filter->word_syms_filename = g_strdup(DEFAULT_WORD_SYMS);

  filter->simple_options = new SimpleOptionsGst();

  filter->endpoint_config = new OnlineEndpointConfig();
  filter->feature_config = new OnlineNnet2FeaturePipelineConfig();
  filter->nnet2_decoding_config = new OnlineNnet2DecodingConfig();
  filter->nnet2_decoding_threaded_config = new OnlineNnet2DecodingThreadedConfig();

  filter->endpoint_config->Register(filter->simple_options);
  filter->feature_config->Register(filter->simple_options);

  // since the properties of the decoders overlap, they need to be set in the correct order
  // we'll redo this if the use-threaded-decoder property is changed
  if (DEAFULT_USE_THREADED_DECODER) {
    filter->nnet2_decoding_config->Register(filter->simple_options);
    filter->nnet2_decoding_threaded_config->Register(filter->simple_options);
  } else {
    filter->nnet2_decoding_threaded_config->Register(filter->simple_options);
    filter->nnet2_decoding_config->Register(filter->simple_options);
  }

  // will be set later
  filter->feature_info = NULL;
  filter->sample_rate = 0;
  filter->decoding = false;
  filter->lmwt_scale = DEFAULT_LMWT_SCALE;
  filter->inverse_scale = FALSE;
  filter->chunk_length_in_secs = DEFAULT_CHUNK_LENGTH_IN_SECS;

  filter->lm_fst_name = g_strdup("");
  filter->big_lm_const_arpa_name = g_strdup("");

  filter->use_threaded_decoder = false;

  // init properties from various Kaldi Opts
  GstElementClass * klass = GST_ELEMENT_GET_CLASS(filter);

  std::vector<std::pair<std::string, SimpleOptions::OptionInfo> > option_info_list;
  option_info_list = filter->simple_options->GetOptionInfoList();
  int32 i = 0;
  for (vector<std::pair<std::string, SimpleOptions::OptionInfo> >::iterator dx =
      option_info_list.begin(); dx != option_info_list.end(); dx++) {
    std::pair<std::string, SimpleOptions::OptionInfo> result = (*dx);
    SimpleOptions::OptionInfo option_info = result.second;
    std::string name = result.first;
    switch (option_info.type) {
      case SimpleOptions::kBool:
        filter->simple_options->GetOption(name, &tmp_bool);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_boolean(name.c_str(), option_info.doc.c_str(),
                                 option_info.doc.c_str(), tmp_bool,
                                 (GParamFlags) G_PARAM_READWRITE));
        break;
      case SimpleOptions::kInt32:
        filter->simple_options->GetOption(name, &tmp_int);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_int(name.c_str(), option_info.doc.c_str(),
                             option_info.doc.c_str(),
                             G_MININT,
                             G_MAXINT, tmp_int,
                             (GParamFlags) G_PARAM_READWRITE));
        break;
      case SimpleOptions::kUint32:
        filter->simple_options->GetOption(name, &tmp_uint);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_uint(name.c_str(), option_info.doc.c_str(),
                              option_info.doc.c_str(), 0,
                              G_MAXUINT,
                              tmp_uint, (GParamFlags) G_PARAM_READWRITE));
        break;
      case SimpleOptions::kFloat:
        filter->simple_options->GetOption(name, &tmp_float);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_float(name.c_str(), option_info.doc.c_str(),
                               option_info.doc.c_str(),
                               -std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::infinity(), tmp_float,
                               (GParamFlags) G_PARAM_READWRITE));
        break;
      case SimpleOptions::kDouble:
        filter->simple_options->GetOption(name, &tmp_double);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_double(name.c_str(), option_info.doc.c_str(),
                                option_info.doc.c_str(),
                                -std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity(), tmp_double,
                                (GParamFlags) G_PARAM_READWRITE));
        break;
      case SimpleOptions::kString:
        filter->simple_options->GetOption(name, &tmp_string);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_string(name.c_str(), option_info.doc.c_str(),
                                option_info.doc.c_str(), tmp_string.c_str(),
                                (GParamFlags) G_PARAM_READWRITE));
        break;
    }
    i += 1;
  }
}

static void gst_kaldinnet2onlinedecoder_set_property(GObject * object,
                                                     guint prop_id,
                                                     const GValue * value,
                                                     GParamSpec * pspec) {


  Gstkaldinnet2onlinedecoder *filter = GST_KALDINNET2ONLINEDECODER(object);
  GST_DEBUG_OBJECT(filter, "Setting property %s", g_param_spec_get_name(pspec));

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean(value);
      break;
    case PROP_MODEL:
      g_free(filter->model_rspecifier);
      filter->model_rspecifier = g_value_dup_string(value);
      break;
    case PROP_FST:
      g_free(filter->fst_rspecifier);
      filter->fst_rspecifier = g_value_dup_string(value);
      break;
    case PROP_WORD_SYMS:
      g_free(filter->word_syms_filename);
      filter->word_syms_filename = g_value_dup_string(value);
      break;
    case PROP_DO_ENDPOINTING:
      filter->do_endpointing = g_value_get_boolean(value);
      break;
    case PROP_INVERSE_SCALE:
      filter->inverse_scale = g_value_get_boolean(value);
      break;
    case PROP_LMWT_SCALE:
      filter->lmwt_scale = g_value_get_float(value);
      break;
    case PROP_CHUNK_LENGTH_IN_SECS:
      filter->chunk_length_in_secs = g_value_get_float(value);
      break;
    case PROP_TRACEBACK_PERIOD_IN_SECS:
      filter->traceback_period_in_secs = g_value_get_float(value);
      break;
    case PROP_LM_FST:
      g_free(filter->lm_fst_name);
      filter->lm_fst_name = g_value_dup_string(value);
      break;
    case PROP_BIG_LM_CONST_ARPA:
      g_free(filter->big_lm_const_arpa_name);
      filter->big_lm_const_arpa_name = g_value_dup_string(value);
      break;
    case PROP_USE_THREADED_DECODER:
      filter->use_threaded_decoder = g_value_get_boolean(value);
      if (filter->use_threaded_decoder) {
        filter->nnet2_decoding_threaded_config->Register(filter->simple_options);
      } else {
        filter->nnet2_decoding_config->Register(filter->simple_options);
      }
      break;
    case PROP_ADAPTATION_STATE:
      {
        if (G_VALUE_HOLDS_STRING(value)) {
          gchar * adaptation_state_string = g_value_dup_string(value);
          if (strlen(adaptation_state_string) > 0) {
            std::istringstream str(adaptation_state_string);
            try {
              filter->adaptation_state->Read(str, false);
            } catch (std::runtime_error& e) {
              GST_WARNING_OBJECT(filter, "Failed to read adaptation state from given string, resetting instead");
              delete filter->adaptation_state;
              filter->adaptation_state = new OnlineIvectorExtractorAdaptationState(
                  filter->feature_info->ivector_extractor_info);
            }
          } else {
            GST_DEBUG_OBJECT(filter, "Resetting adaptation state");
            delete filter->adaptation_state;
            filter->adaptation_state = new OnlineIvectorExtractorAdaptationState(
                filter->feature_info->ivector_extractor_info);
          }
          delete adaptation_state_string;
        } else {
          GST_DEBUG_OBJECT(filter, "Resetting adaptation state");
          delete filter->adaptation_state;
          filter->adaptation_state = new OnlineIvectorExtractorAdaptationState(
              filter->feature_info->ivector_extractor_info);
        }
      }
      break;
    case PROP_NUM_TRANSCRIPTIONS:
      filter->num_transcriptions = g_value_get_int(value);
      break;
    default:
      if (prop_id >= PROP_LAST) {
        const gchar* name = g_param_spec_get_name(pspec);
        SimpleOptions::OptionType option_type;
        if (filter->simple_options->GetOptionType(std::string(name),
                                                  &option_type)) {
          switch (option_type) {
            case SimpleOptions::kBool:
              filter->simple_options->SetOption(name,
                                                (bool)g_value_get_boolean(value));
              break;
            case SimpleOptions::kInt32:
              filter->simple_options->SetOption(name, g_value_get_int(value));
              break;
            case SimpleOptions::kUint32:
              filter->simple_options->SetOption(name, g_value_get_uint(value));
              break;
            case SimpleOptions::kFloat:
              filter->simple_options->SetOption(name, g_value_get_float(value));
              break;
            case SimpleOptions::kDouble:
              filter->simple_options->SetOption(name,
                                                g_value_get_double(value));
              break;
            case SimpleOptions::kString:
              filter->simple_options->SetOption(name,
                                                g_value_dup_string(value));
              break;
          }
          break;
        }
      }
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_kaldinnet2onlinedecoder_get_property(GObject * object,
                                                     guint prop_id,
                                                     GValue * value,
                                                     GParamSpec * pspec) {
  bool tmp_bool;
  int32 tmp_int;
  uint32 tmp_uint;
  float tmp_float;
  double tmp_double;
  std::string tmp_string;
  std::ostringstream string_stream;

  Gstkaldinnet2onlinedecoder *filter = GST_KALDINNET2ONLINEDECODER(object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean(value, filter->silent);
      break;
    case PROP_MODEL:
      g_value_set_string(value, filter->model_rspecifier);
      break;
    case PROP_FST:
      g_value_set_string(value, filter->fst_rspecifier);
      break;
    case PROP_WORD_SYMS:
      g_value_set_string(value, filter->word_syms_filename);
      break;
    case PROP_DO_ENDPOINTING:
      g_value_set_boolean(value, filter->do_endpointing);
      break;
    case PROP_INVERSE_SCALE:
      g_value_set_boolean(value, filter->inverse_scale);
      break;
    case PROP_LMWT_SCALE:
      g_value_set_float(value, filter->lmwt_scale);
      break;
    case PROP_CHUNK_LENGTH_IN_SECS:
      g_value_set_float(value, filter->chunk_length_in_secs);
      break;
    case PROP_TRACEBACK_PERIOD_IN_SECS:
      g_value_set_float(value, filter->traceback_period_in_secs);
      break;
    case PROP_LM_FST:
      g_value_set_string(value, filter->lm_fst_name);
      break;
    case PROP_BIG_LM_CONST_ARPA:
      g_value_set_string(value, filter->big_lm_const_arpa_name);
      break;
    case PROP_USE_THREADED_DECODER:
      g_value_set_boolean(value, filter->use_threaded_decoder);
      break;
    case PROP_NUM_TRANSCRIPTIONS:
      g_value_set_int(value, filter->num_transcriptions);
      break;

    case PROP_ADAPTATION_STATE:
      string_stream.clear();
      if (filter->adaptation_state) {
          filter->adaptation_state->Write(string_stream, false);
          g_value_set_string(value, string_stream.str().c_str());
      } else {
          g_value_set_string(value, "");
      }
      break;
    default:
      if (prop_id >= PROP_LAST) {
        const gchar* name = g_param_spec_get_name(pspec);
        SimpleOptions::OptionType option_type;
        if (filter->simple_options->GetOptionType(std::string(name),
                                                  &option_type)) {
          switch (option_type) {
            case SimpleOptions::kBool:
              filter->simple_options->GetOption(name, &tmp_bool);
              g_value_set_boolean(value, tmp_bool);
              break;
            case SimpleOptions::kInt32:
              filter->simple_options->GetOption(name, &tmp_int);
              g_value_set_int(value, tmp_int);
              break;
            case SimpleOptions::kUint32:
              filter->simple_options->GetOption(name, &tmp_uint);
              g_value_set_uint(value, tmp_uint);
              break;
            case SimpleOptions::kFloat:
              filter->simple_options->GetOption(name, &tmp_float);
              g_value_set_float(value, tmp_float);
              break;
            case SimpleOptions::kDouble:
              filter->simple_options->GetOption(name, &tmp_double);
              g_value_set_double(value, tmp_double);
              break;
            case SimpleOptions::kString:
              filter->simple_options->GetOption(name, &tmp_string);
              g_value_set_string(value, tmp_string.c_str());
              break;
          }
          break;
        }
      }
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_kaldinnet2onlinedecoder_scale_lattice(
    Gstkaldinnet2onlinedecoder * filter, CompactLattice &clat) {
  if (filter->inverse_scale) {
    BaseFloat inv_acoustic_scale = 1.0;
    if (filter->use_threaded_decoder) {
      inv_acoustic_scale = 1.0 / filter->
          nnet2_decoding_threaded_config->acoustic_scale;
    } else {
      inv_acoustic_scale = 1.0 / filter->nnet2_decoding_config->
          decodable_opts.acoustic_scale;
    }

    fst::ScaleLattice(fst::AcousticLatticeScale(inv_acoustic_scale), &clat);
  }

  fst::ScaleLattice(fst::LatticeScale(filter->lmwt_scale, 1.0), &clat);
}


static void gst_kaldinnet2onlinedecoder_final_result(
    Gstkaldinnet2onlinedecoder * filter, CompactLattice &clat,
    int64 *tot_num_frames, double *tot_like, guint *num_words) {

  if (clat.NumStates() == 0) {
    KALDI_WARN<< "Empty lattice.";
    return;
  }

  gst_kaldinnet2onlinedecoder_scale_lattice(filter, clat);
  Lattice lat;
  ConvertLattice(clat, &lat);

  std::vector<Lattice> nbest_lats; // one lattice per path
  {
    Lattice nbest_lat; // one lattice with all best paths, temporary
    fst::ShortestPath(lat, &nbest_lat, filter->num_transcriptions);
    fst::ConvertNbestToVector(nbest_lat, &nbest_lats);
  }
  std::stringstream sentence;
  for (int i = 0; i < nbest_lats.size(); i++) {
    double likelihood;
    LatticeWeight weight;
    int32 num_frames;
    std::vector<int32> alignment;
    std::vector<int32> words;
    GetLinearSymbolSequence(nbest_lats[i], &alignment, &words, &weight);
    num_frames = alignment.size();
    likelihood = -(weight.Value1() + weight.Value2());
    *tot_num_frames += num_frames;
    *tot_like += likelihood;
    GST_DEBUG_OBJECT(filter, "Likelihood per frame for is %f over %d frames",
        (likelihood / num_frames), num_frames);

    for (size_t j = 0; j < words.size(); j++) {
      std::string s = filter->word_syms->Find(words[j]);
      if (s == "")
        GST_ERROR_OBJECT(filter, "Word-id %d not in symbol table.", words[j]);
      if (j > 0) {
        sentence << " ";
      }
      sentence << s;
    }
    sentence << "\n";
  }

  GST_DEBUG_OBJECT(filter, "Final: %s", sentence.str().c_str());

  guint hyp_length = sentence.str().length();
  *num_words = hyp_length;
  if (hyp_length > 0) {
    GstBuffer *buffer = gst_buffer_new_and_alloc(hyp_length);
    gst_buffer_fill(buffer, 0, sentence.str().c_str(), hyp_length);
    gst_pad_push(filter->srcpad, buffer);
  }

  /* Emit a signal for applications. */
  g_signal_emit(filter, gst_kaldinnet2onlinedecoder_signals[FINAL_RESULT_SIGNAL], 0, sentence.str().c_str());
}

static void gst_kaldinnet2onlinedecoder_partial_result(
    Gstkaldinnet2onlinedecoder * filter, const Lattice lat) {
  LatticeWeight weight;
  std::vector<int32> alignment;
  std::vector<int32> words;
  GetLinearSymbolSequence(lat, &alignment, &words, &weight);

  std::stringstream sentence;
  for (size_t i = 0; i < words.size(); i++) {
    std::string s = filter->word_syms->Find(words[i]);
    if (s == "")
      GST_ERROR_OBJECT(filter, "Word-id %d  not in symbol table.", words[i]);
    if (i > 0) {
      sentence << " ";
    }
    sentence << s;
  }
  GST_DEBUG_OBJECT(filter, "Partial: %s", sentence.str().c_str());
  if (sentence.str().length() > 0) {
    /* Emit a signal for applications. */
    g_signal_emit(filter,
                  gst_kaldinnet2onlinedecoder_signals[PARTIAL_RESULT_SIGNAL], 0,
                  sentence.str().c_str());
  }
}

static bool gst_kaldinnet2onlinedecoder_rescore_big_lm(
    Gstkaldinnet2onlinedecoder * filter, CompactLattice &clat, CompactLattice &result_lat) {

  Lattice tmp_lattice;
  ConvertLattice(clat, &tmp_lattice);
  // Before composing with the LM FST, we scale the lattice weights
  // by the inverse of "lm_scale".  We'll later scale by "lm_scale".
  // We do it this way so we can determinize and it will give the
  // right effect (taking the "best path" through the LM) regardless
  // of the sign of lm_scale.
  fst::ScaleLattice(fst::GraphLatticeScale(-1.0), &tmp_lattice);
  ArcSort(&tmp_lattice, fst::OLabelCompare<LatticeArc>());

  Lattice composed_lat;
  // Could just do, more simply: Compose(lat, lm_fst, &composed_lat);
  // and not have lm_compose_cache at all.
  // The command below is faster, though; it's constant not
  // logarithmic in vocab size.

  TableCompose(tmp_lattice, *(filter->lm_fst), &composed_lat, filter->lm_compose_cache);

  Invert(&composed_lat); // make it so word labels are on the input.
  CompactLattice determinized_lat;
  DeterminizeLattice(composed_lat, &determinized_lat);
  fst::ScaleLattice(fst::GraphLatticeScale(-1.0), &determinized_lat);
  if (determinized_lat.Start() == fst::kNoStateId) {
    GST_INFO_OBJECT(filter, "Empty lattice (incompatible LM?)");
    return false;
  } else {
    fst::ScaleLattice(fst::GraphLatticeScale(1.0), &determinized_lat);
    ArcSort(&determinized_lat, fst::OLabelCompare<CompactLatticeArc>());

    // Wraps the ConstArpaLm format language model into FST. We re-create it
    // for each lattice to prevent memory usage increasing with time.
    ConstArpaLmDeterministicFst const_arpa_fst(*(filter->big_lm_const_arpa));

    // Composes lattice with language model.
    CompactLattice composed_clat;
    ComposeCompactLatticeDeterministic(determinized_lat,
                                       &const_arpa_fst, &composed_clat);

    // Determinizes the composed lattice.
    Lattice composed_lat;
    ConvertLattice(composed_clat, &composed_lat);
    Invert(&composed_lat);
    DeterminizeLattice(composed_lat, &result_lat);
    fst::ScaleLattice(fst::GraphLatticeScale(1.0), &result_lat);
    if (result_lat.Start() == fst::kNoStateId) {
      GST_INFO_OBJECT(filter, "Empty lattice (incompatible LM?)");
      return false;
    }
  }
  return true;
}

static void gst_kaldinnet2onlinedecoder_threaded_decode_segment(Gstkaldinnet2onlinedecoder * filter,
                                                      bool &more_data,
                                                      int32 chunk_length,
                                                      BaseFloat traceback_period_secs,
                                                      Vector<BaseFloat> *remaining_wave_part) {
    SingleUtteranceNnet2DecoderThreaded decoder(*(filter->nnet2_decoding_threaded_config),
                                        *(filter->trans_model), *(filter->nnet),
                                        *(filter->decode_fst),
                                        *(filter->feature_info),
                                        *(filter->adaptation_state));

    Vector<BaseFloat> wave_part = Vector<BaseFloat>(chunk_length);
    GST_DEBUG_OBJECT(filter, "Reading audio in %d sample chunks...",
                     wave_part.Dim());
    BaseFloat last_traceback = 0.0;
    BaseFloat num_seconds_decoded = 0.0;
    if (remaining_wave_part->Dim() > 0) {
      GST_DEBUG_OBJECT(filter, "Submitting remaining wave of size %d", remaining_wave_part->Dim());
      decoder.AcceptWaveform(filter->sample_rate, *remaining_wave_part);
      while (decoder.NumFramesReceivedApprox() - decoder.NumFramesDecoded() > 100) {
        Sleep(0.1);
      }
    }
    while (true) {
      more_data = filter->audio_source->Read(&wave_part);
      GST_DEBUG_OBJECT(filter, "Submitting wave of size: %d", wave_part.Dim());
      decoder.AcceptWaveform(filter->sample_rate, wave_part);
      if (!more_data) {
        decoder.InputFinished();
        break;
      }

      if (filter->do_endpointing) {
        GST_DEBUG_OBJECT(filter, "Before the sleep check: Frames received: ~ %d, frames decoded: %d, pieces pending: %d",
                         decoder.NumFramesReceivedApprox(),
                         decoder.NumFramesDecoded(),
                         decoder.NumWaveformPiecesPending());

        // Wait until there are less than one second of frames left to decode
        // Depends of the frame shift, but one second is also selected arbitrarily
        while (decoder.NumFramesReceivedApprox() - decoder.NumFramesDecoded() > 100) {
          Sleep(0.1);
        }

        GST_DEBUG_OBJECT(filter, "After the sleep check: Frames received: ~ %d, frames decoded: %d, pieces pending: %d",
                         decoder.NumFramesReceivedApprox(),
                         decoder.NumFramesDecoded(),
                         decoder.NumWaveformPiecesPending());

        if ((decoder.NumFramesDecoded() > 0)
            && decoder.EndpointDetected(*(filter->endpoint_config))) {
          decoder.TerminateDecoding();
          GST_DEBUG_OBJECT(filter, "Endpoint detected!");
          break;
        }
      }
      num_seconds_decoded += filter->chunk_length_in_secs;
      if ((num_seconds_decoded - last_traceback > traceback_period_secs)
          && (decoder.NumFramesDecoded() > 0)) {
        Lattice lat;
        decoder.GetBestPath(false, &lat, NULL);
        gst_kaldinnet2onlinedecoder_partial_result(filter, lat);
        last_traceback += traceback_period_secs;
      }
    }

    decoder.Wait();
    decoder.GetRemainingWaveform(remaining_wave_part);
    GST_DEBUG_OBJECT(filter, "Remaining waveform size: %d", remaining_wave_part->Dim());

    if (num_seconds_decoded > 0.1) {
      GST_DEBUG_OBJECT(filter, "Getting lattice..");
      decoder.FinalizeDecoding();
      CompactLattice clat;
      bool end_of_utterance = true;
      decoder.GetLattice(end_of_utterance, &clat, NULL);
      GST_DEBUG_OBJECT(filter, "Lattice done");
      if ((filter->lm_fst != NULL) && (filter->big_lm_const_arpa != NULL)) {
        GST_DEBUG_OBJECT(filter, "Rescoring lattice with a big LM");
        CompactLattice rescored_lat;
        if (gst_kaldinnet2onlinedecoder_rescore_big_lm(filter, clat, rescored_lat)) {
          clat = rescored_lat;
        }
      }

      double tot_like = 0.0;
      int64 num_frames = 0;

      guint num_words = 0;
      gst_kaldinnet2onlinedecoder_final_result(filter, clat, &num_frames,
                                               &tot_like, &num_words);
      if (num_words > 0) {
        // Only update adaptation state if the utterance was not empty
        decoder.GetAdaptationState(filter->adaptation_state);
      }
    } else {
      GST_DEBUG_OBJECT(filter, "Less than 0.1 seconds decoded, discarding");
    }

}

static void gst_kaldinnet2onlinedecoder_unthreaded_decode_segment(Gstkaldinnet2onlinedecoder * filter,
                                                        bool &more_data,
                                                        int32 chunk_length,
                                                        BaseFloat traceback_period_secs) {

  OnlineNnet2FeaturePipeline feature_pipeline(*(filter->feature_info));
  feature_pipeline.SetAdaptationState(*(filter->adaptation_state));
  SingleUtteranceNnet2Decoder decoder(*(filter->nnet2_decoding_config),
                                      *(filter->trans_model), *(filter->nnet),
                                      *(filter->decode_fst),
                                      &feature_pipeline);

  Vector<BaseFloat> wave_part = Vector<BaseFloat>(chunk_length);
  GST_DEBUG_OBJECT(filter, "Reading audio in %d sample chunks...",
                   wave_part.Dim());
  BaseFloat last_traceback = 0.0;
  BaseFloat num_seconds_decoded = 0.0;
  while (true) {
    more_data = filter->audio_source->Read(&wave_part);
    feature_pipeline.AcceptWaveform(filter->sample_rate, wave_part);
    if (!more_data) {
      feature_pipeline.InputFinished();
    }
    decoder.AdvanceDecoding();
    if (!more_data) {
      break;
    }
    GST_DEBUG_OBJECT(filter, "%d frames decoded", decoder.NumFramesDecoded());
    if (filter->do_endpointing
        && (decoder.NumFramesDecoded() > 0)
        && decoder.EndpointDetected(*(filter->endpoint_config))) {
      GST_DEBUG_OBJECT(filter, "Endpoint detected!");
      break;
    }
    num_seconds_decoded += filter->chunk_length_in_secs;
    if ((num_seconds_decoded - last_traceback > traceback_period_secs)
        && (decoder.NumFramesDecoded() > 0)) {
      Lattice lat;
      decoder.GetBestPath(false, &lat);
      gst_kaldinnet2onlinedecoder_partial_result(filter, lat);
      last_traceback += traceback_period_secs;
    }
  }
  if (num_seconds_decoded > 0.1) {
    GST_DEBUG_OBJECT(filter, "Getting lattice..");
    decoder.FinalizeDecoding();
    CompactLattice clat;
    bool end_of_utterance = true;
    decoder.GetLattice(end_of_utterance, &clat);
    GST_DEBUG_OBJECT(filter, "Lattice done");
    if ((filter->lm_fst != NULL) && (filter->big_lm_const_arpa != NULL)) {
      GST_DEBUG_OBJECT(filter, "Rescoring lattice with a big LM");
      CompactLattice rescored_lat;
      if (gst_kaldinnet2onlinedecoder_rescore_big_lm(filter, clat, rescored_lat)) {
        clat = rescored_lat;
      }
    }

    double tot_like = 0.0;
    int64 num_frames = 0;

    guint num_words = 0;
    gst_kaldinnet2onlinedecoder_final_result(filter, clat, &num_frames,
                                             &tot_like, &num_words);
    if (num_words > 0) {
      // Only update adaptation state if the utterance was not empty
      feature_pipeline.GetAdaptationState(filter->adaptation_state);
    }
  } else {
    GST_DEBUG_OBJECT(filter, "Less than 0.1 seconds decoded, discarding");
  }
}

static void gst_kaldinnet2onlinedecoder_loop(
    Gstkaldinnet2onlinedecoder * filter) {

  GST_DEBUG_OBJECT(filter, "Starting decoding loop..");
  BaseFloat traceback_period_secs = filter->traceback_period_in_secs;

  int32 chunk_length = int32(filter->sample_rate * filter->chunk_length_in_secs);

  bool more_data = true;
  Vector<BaseFloat> remaining_wave_part;
  while (more_data) {
    if (filter->use_threaded_decoder) {
      gst_kaldinnet2onlinedecoder_threaded_decode_segment(filter, more_data, chunk_length, traceback_period_secs, &remaining_wave_part);
    } else {
      gst_kaldinnet2onlinedecoder_unthreaded_decode_segment(filter, more_data, chunk_length, traceback_period_secs);
    }
  }

  GST_DEBUG_OBJECT(filter, "Finished decoding loop");
  GST_DEBUG_OBJECT(filter, "Pushing EOS event");
  gst_pad_push_event(filter->srcpad, gst_event_new_eos());

  GST_DEBUG_OBJECT(filter, "Pausing decoding task");
  gst_pad_pause_task(filter->srcpad);
  delete filter->audio_source;
  filter->audio_source = new GstBufferSource();
  filter->decoding = false;
}

/* GstElement vmethod implementations */

static gboolean
gst_kaldinnet2onlinedecoder_query (GstPad *pad, GstObject * parent, GstQuery * query) {
  gboolean ret;
  Gstkaldinnet2onlinedecoder *filter;

  filter = GST_KALDINNET2ONLINEDECODER(parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS: {
      if (filter->feature_info == NULL) {
        filter->feature_info = new OnlineNnet2FeaturePipelineInfo(*(filter->feature_config));
        filter->sample_rate = (int) filter->feature_info->mfcc_opts.frame_opts.samp_freq;
      }
      GstCaps *new_caps = gst_caps_new_simple ("audio/x-raw",
            "format", G_TYPE_STRING, "S16LE",
            "rate", G_TYPE_INT, filter->sample_rate,
            "channels", G_TYPE_INT, 1, NULL);

      GST_DEBUG_OBJECT (filter, "Setting caps query result: %" GST_PTR_FORMAT, new_caps);
      gst_query_set_caps_result (query, new_caps);
      gst_caps_unref (new_caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }
  return ret;
}



/* this function handles sink events */
static gboolean gst_kaldinnet2onlinedecoder_sink_event(GstPad * pad,
                                                       GstObject * parent,
                                                       GstEvent * event) {
  gboolean ret;
  Gstkaldinnet2onlinedecoder *filter;

  filter = GST_KALDINNET2ONLINEDECODER(parent);

  GST_DEBUG_OBJECT(filter, "Handling %s event", GST_EVENT_TYPE_NAME(event));

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEGMENT: {
      GST_DEBUG_OBJECT(filter, "Starting decoding task");
      filter->decoding = true;
      gst_pad_start_task(filter->srcpad,
                         (GstTaskFunction) gst_kaldinnet2onlinedecoder_loop,
                         filter, NULL);

      GST_DEBUG_OBJECT(filter, "Started decoding task");
      ret = TRUE;
      break;
    }
    case GST_EVENT_CAPS: {
      ret = TRUE;
      break;
    }
    case GST_EVENT_EOS: {
      /* end-of-stream, we should close down all stream leftovers here */
      GST_DEBUG_OBJECT(filter, "EOS received");
      if (filter->decoding) {
        filter->audio_source->SetEnded(true);
      } else {
        GST_DEBUG_OBJECT(filter, "EOS received while not decoding, pushing EOS out");
        gst_pad_push_event(filter->srcpad, gst_event_new_eos());
      }
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_event_default(pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_kaldinnet2onlinedecoder_chain(GstPad * pad,
                                                       GstObject * parent,
                                                       GstBuffer * buf) {
  Gstkaldinnet2onlinedecoder *filter = GST_KALDINNET2ONLINEDECODER(parent);

  if (G_UNLIKELY(!filter->audio_source))
    goto not_negotiated;
  if (!filter->silent) {
    GST_DEBUG_OBJECT(filter, "Pushing buffer of length %zu", gst_buffer_get_size(buf));
    filter->audio_source->PushBuffer(buf);
  }
  gst_buffer_unref(buf);
  return GST_FLOW_OK;

  /* special cases */
  not_negotiated: {
    GST_ELEMENT_ERROR(filter, CORE, NEGOTIATION, (NULL),
                      ("decoder wasn't allocated before chain function"));

    gst_buffer_unref(buf);
    return GST_FLOW_NOT_NEGOTIATED;
  }
}


static bool
gst_kaldinnet2onlinedecoder_allocate(
    Gstkaldinnet2onlinedecoder * filter) {
  if (!filter->decode_fst) {
    GST_INFO_OBJECT(filter, "Loading Kaldi models and feature extractor");

    filter->audio_source = new GstBufferSource();

    if (filter->feature_info == NULL) {
      filter->feature_info = new OnlineNnet2FeaturePipelineInfo(*(filter->feature_config));
      filter->sample_rate = (int) filter->feature_info->mfcc_opts.frame_opts.samp_freq;
    }

    filter->sample_rate = (int) filter->feature_info->mfcc_opts.frame_opts.samp_freq;

    filter->trans_model = new TransitionModel();
    filter->nnet = new nnet2::AmNnet();
    {
      bool binary;
      Input ki(filter->model_rspecifier, &binary);
      filter->trans_model->Read(ki.Stream(), binary);
      filter->nnet->Read(ki.Stream(), binary);
    }

    filter->decode_fst = fst::ReadFstKaldi(filter->fst_rspecifier);

    if (!(filter->word_syms = fst::SymbolTable::ReadText(
        filter->word_syms_filename))) {
      GST_ERROR_OBJECT(filter, "Could not read symbol table from file %s",
                       filter->word_syms_filename);
      return false;
    }

    filter->adaptation_state = new OnlineIvectorExtractorAdaptationState(
        filter->feature_info->ivector_extractor_info);

    if ((strlen(filter->lm_fst_name) > 0) &&
        (strlen(filter->big_lm_const_arpa_name) > 0)) {
      GST_DEBUG_OBJECT(filter, "Loading models for LM rescoring with a big LM");
      fst::script::MutableFstClass *fst =
          fst::script::MutableFstClass::Read(filter->lm_fst_name, true);
      fst::script::Project(fst, fst::PROJECT_OUTPUT);

      const fst::Fst<fst::StdArc> *tmp_fst = fst->GetFst<fst::StdArc>();

      fst::VectorFst<fst::StdArc> *std_lm_fst = new fst::VectorFst<fst::StdArc>(*tmp_fst);

      if (std_lm_fst->Properties(fst::kILabelSorted, true) == 0) {
        // Make sure LM is sorted on ilabel.
        fst::ILabelCompare<fst::StdArc> ilabel_comp;
        fst::ArcSort(std_lm_fst, ilabel_comp);
      }

      // mapped_fst is the LM fst interpreted using the LatticeWeight semiring,
      // with all the cost on the first member of the pair (since it's a graph
      // weight).
      int32 num_states_cache = 50000;
      fst::CacheOptions cache_opts(true, num_states_cache);
      fst::StdToLatticeMapper<BaseFloat> mapper;
      filter->lm_fst = new fst::MapFst<fst::StdArc, LatticeArc,
          fst::StdToLatticeMapper<BaseFloat> >(*std_lm_fst, mapper, cache_opts);
      delete std_lm_fst;
      delete fst;
      // FIXME: maybe?
      //delete tmp_fst;

      // The next fifteen or so lines are a kind of optimization and
      // can be ignored if you just want to understand what is going on.
      // Change the options for TableCompose to match the input
      // (because it's the arcs of the LM FST we want to do lookup
      // on).
      fst::TableComposeOptions compose_opts(fst::TableMatcherOptions(),
                                            true, fst::SEQUENCE_FILTER,
                                            fst::MATCH_INPUT);

      // The following is an optimization for the TableCompose
      // composition: it stores certain tables that enable fast
      // lookup of arcs during composition.
      filter->lm_compose_cache = new fst::TableComposeCache<fst::Fst<LatticeArc> >(compose_opts);

      GST_DEBUG_OBJECT(filter, "Loading big LM in constant ARPA format");
      filter->big_lm_const_arpa = new ConstArpaLm();
      ReadKaldiObject(filter->big_lm_const_arpa_name, filter->big_lm_const_arpa);

    }

  }
  return true;
}

static bool gst_kaldinnet2onlinedecoder_deallocate(
    Gstkaldinnet2onlinedecoder * filter) {
  /* We won't deallocate the decoder once it's already allocated, since model loading could take a lot of time */
  GST_INFO_OBJECT(filter, "Refusing to unload Kaldi models");
  return true;
}

static GstStateChangeReturn gst_kaldinnet2onlinedecoder_change_state(
    GstElement *element, GstStateChange transition) {

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  Gstkaldinnet2onlinedecoder *filter = GST_KALDINNET2ONLINEDECODER(element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_kaldinnet2onlinedecoder_allocate(filter))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_kaldinnet2onlinedecoder_deallocate(filter);
      break;
    default:
      break;
  }

  return ret;
}

static void gst_kaldinnet2onlinedecoder_finalize(GObject * object) {
  Gstkaldinnet2onlinedecoder *filter = GST_KALDINNET2ONLINEDECODER(object);

  g_free(filter->model_rspecifier);
  g_free(filter->fst_rspecifier);
  g_free(filter->word_syms_filename);
  delete filter->endpoint_config;
  delete filter->feature_config;
  delete filter->nnet2_decoding_config;
  delete filter->simple_options;
  if (filter->feature_info) {
    delete filter->feature_info;
  }
  if (filter->trans_model) {
    delete filter->trans_model;
  }
  if (filter->nnet) {
    delete filter->nnet;
  }
  if (filter->decode_fst) {
    delete filter->decode_fst;
  }
  if (filter->word_syms) {
    delete filter->word_syms;
  }
  if (filter->adaptation_state) {
    delete filter->adaptation_state;
  }
  g_free(filter->lm_fst_name);
  g_free(filter->big_lm_const_arpa_name);
  if (filter->lm_fst) {
    delete filter->lm_fst;
  }
  if (filter->big_lm_const_arpa) {
    delete filter->big_lm_const_arpa;
  }
  if (filter->lm_compose_cache) {
    delete filter->lm_compose_cache;
  }


  G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean kaldinnet2onlinedecoder_init(
    GstPlugin * kaldinnet2onlinedecoder) {
  /* debug category for fltering log messages
   *
   * exchange the string 'Template kaldinnet2onlinedecoder' with your description
   */
  GST_DEBUG_CATEGORY_INIT(gst_kaldinnet2onlinedecoder_debug,
                          "kaldinnet2onlinedecoder", 0,
                          "Template kaldinnet2onlinedecoder");

  return gst_element_register(kaldinnet2onlinedecoder,
                              "kaldinnet2onlinedecoder", GST_RANK_NONE,
                              GST_TYPE_KALDINNET2ONLINEDECODER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "Kaldi"
#endif

/* gstreamer looks for this structure to register kaldinnet2onlinedecoders
 *
 * exchange the string 'Template kaldinnet2onlinedecoder' with your kaldinnet2onlinedecoder description
 *
 * License is specified as "unknown" because gstreamer doesn't recognize "Apache" as
 * a license and blacklists the module :S
 */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, kaldinnet2onlinedecoder,
                  "kaldinnet2onlinedecoder",
                  kaldinnet2onlinedecoder_init, VERSION, "unknown", "GStreamer",
                  "http://gstreamer.net/")


}

