/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2014 Tanel Alumae <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_KALDINNET2ONLINEDECODER_H__
#define __GST_KALDINNET2ONLINEDECODER_H__


#include <gst/gst.h>

#include "simple-options-gst.h"
#include "gst-audio-source.h"

#include "online2/online-nnet2-decoding.h"
#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"

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

typedef struct _Gstkaldinnet2onlinedecoder      Gstkaldinnet2onlinedecoder;
typedef struct _Gstkaldinnet2onlinedecoderClass Gstkaldinnet2onlinedecoderClass;

struct _Gstkaldinnet2onlinedecoder
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean silent;
  gboolean do_endpointing;
  GstBufferSource *audio_source;

  gchar* model_rspecifier;
  gchar* fst_rspecifier;
  gchar* word_syms_filename;

  SimpleOptionsGst *simple_options;
  OnlineEndpointConfig *endpoint_config;
  OnlineNnet2FeaturePipelineConfig *feature_config;
  OnlineNnet2DecodingConfig *nnet2_decoding_config;

  OnlineNnet2FeaturePipelineInfo *feature_info;
  TransitionModel *trans_model;
  nnet2::AmNnet *nnet;
  fst::Fst<fst::StdArc> *decode_fst;
  fst::SymbolTable *word_syms;
};

struct _Gstkaldinnet2onlinedecoderClass 
{
  GstElementClass parent_class;
  void (*partial_result)(GstElement *element, const gchar *result_str);
  void (*final_result)(GstElement *element, const gchar *result_str);
};

GType gst_kaldinnet2onlinedecoder_get_type (void);

G_END_DECLS
}
#endif /* __GST_KALDINNET2ONLINEDECODER_H__ */
