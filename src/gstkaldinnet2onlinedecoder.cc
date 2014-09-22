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
  PROP_LAST
};

#define DEFAULT_MODEL           "final.mdl"
#define DEFAULT_FST             "HCLG.fst"
#define DEFAULT_WORD_SYMS       "words.txt"

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

  filter->endpoint_config->Register(filter->simple_options);
  filter->feature_config->Register(filter->simple_options);
  filter->nnet2_decoding_config->Register(filter->simple_options);

  // will be set later
  filter->feature_info = NULL;
  filter->sample_rate = 0;
  filter->decoding = false;

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
                               G_MINFLOAT,
                               G_MAXFLOAT, tmp_float,
                               (GParamFlags) G_PARAM_READWRITE));
        break;
      case SimpleOptions::kDouble:
        filter->simple_options->GetOption(name, &tmp_double);
        g_object_class_install_property(
            G_OBJECT_CLASS(klass),
            PROP_LAST + i,
            g_param_spec_double(name.c_str(), option_info.doc.c_str(),
                                option_info.doc.c_str(),
                                G_MINDOUBLE,
                                G_MAXDOUBLE, tmp_double,
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
    default:
      if (prop_id >= PROP_LAST) {
        const gchar* name = g_param_spec_get_name(pspec);
        SimpleOptions::OptionType option_type;
        if (filter->simple_options->GetOptionType(std::string(name),
                                                  &option_type)) {
          switch (option_type) {
            case SimpleOptions::kBool:
              filter->simple_options->SetOption(name,
                                                g_value_get_boolean(value));
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

static void gst_kaldinnet2onlinedecoder_final_result(
    Gstkaldinnet2onlinedecoder * filter, const CompactLattice &clat,
    int64 *tot_num_frames, double *tot_like) {
  if (clat.NumStates() == 0) {
    KALDI_WARN<< "Empty lattice.";
    return;
  }
  CompactLattice best_path_clat;
  CompactLatticeShortestPath(clat, &best_path_clat);

  Lattice best_path_lat;
  ConvertLattice(best_path_clat, &best_path_lat);

  double likelihood;
  LatticeWeight weight;
  int32 num_frames;
  std::vector<int32> alignment;
  std::vector<int32> words;
  GetLinearSymbolSequence(best_path_lat, &alignment, &words, &weight);
  num_frames = alignment.size();
  likelihood = -(weight.Value1() + weight.Value2());
  *tot_num_frames += num_frames;
  *tot_like += likelihood;
  GST_DEBUG_OBJECT(filter, "Likelihood per frame for is %f over %d frames",
      (likelihood / num_frames), num_frames);

  std::stringstream sentence;
  for (size_t i = 0; i < words.size(); i++) {
    std::string s = filter->word_syms->Find(words[i]);
    if (s == "")
    GST_ERROR_OBJECT(filter, "Word-id %d not in symbol table.", words[i]);
    if (i > 0) {
      sentence << " ";
    }
    sentence << s;
  }
  GST_DEBUG_OBJECT(filter, "Final: %s", sentence.str().c_str());

  guint hyp_length = sentence.str().length();
  if (hyp_length > 0) {
    GstBuffer *buffer = gst_buffer_new_and_alloc(hyp_length + 1);
    gst_buffer_fill(buffer, 0, sentence.str().c_str(), hyp_length);
    gst_buffer_memset(buffer, hyp_length, '\n', 1);
    gst_pad_push(filter->srcpad, buffer);

    /* Emit a signal for applications. */
    g_signal_emit(filter, gst_kaldinnet2onlinedecoder_signals[FINAL_RESULT_SIGNAL], 0, sentence.str().c_str());
  }
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

static void gst_kaldinnet2onlinedecoder_loop(
    Gstkaldinnet2onlinedecoder * filter) {

  GST_DEBUG_OBJECT(filter, "Starting decoding loop..");
  BaseFloat chunk_length_secs = 0.05;
  BaseFloat traceback_period_secs = 1.0;

  int32 chunk_length = int32(filter->sample_rate * chunk_length_secs);

  bool more_data = true;
  while (more_data) {
    OnlineIvectorExtractorAdaptationState adaptation_state(
        filter->feature_info->ivector_extractor_info);

    OnlineNnet2FeaturePipeline feature_pipeline(*(filter->feature_info));
    feature_pipeline.SetAdaptationState(adaptation_state);

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
      if (filter->do_endpointing
          && decoder.EndpointDetected(*(filter->endpoint_config))) {
        GST_DEBUG_OBJECT(filter, "Endpoint detected!");
        break;
      }
      num_seconds_decoded += chunk_length_secs;
      if (num_seconds_decoded - last_traceback > traceback_period_secs) {
        Lattice lat;
        decoder.GetBestPath(false, &lat);
        gst_kaldinnet2onlinedecoder_partial_result(filter, lat);
        last_traceback += traceback_period_secs;
      }
    }
    GST_DEBUG_OBJECT(filter, "Getting lattice..");
    CompactLattice clat;
    bool end_of_utterance = true;
    decoder.GetLattice(end_of_utterance, &clat);
    GST_DEBUG_OBJECT(filter, "Lattice done");
    double tot_like = 0.0;
    int64 num_frames = 0;
    gst_kaldinnet2onlinedecoder_final_result(filter, clat, &num_frames,
                                             &tot_like);
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

