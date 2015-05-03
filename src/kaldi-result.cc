// kaldi-result.cc

// Copyright 2015 Amit Beka (amit.beka@gmail.com)

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

#include "./kaldi-result.h"

namespace kaldi {

G_DEFINE_TYPE(KaldiResult, kaldi_result, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_TEXTS,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
kaldi_result_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec   *pspec)
{
  KaldiResult *self = KALDI_RESULT(object);

  switch (property_id)
  {
    case PROP_TEXTS:
      g_free(self->texts);
      self->texts = g_value_dup_string(value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kaldi_result_get_property (GObject *object, guint property_id,
    GValue *value, GParamSpec *pspec)
{
  KaldiResult *self = KALDI_RESULT(object);

  switch (property_id)
  {
    case PROP_TEXTS:
      g_value_set_string(value, self->texts);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kaldi_result_init (KaldiResult *self) {
  self->texts = NULL;
}

static void
kaldi_result_finalize (GObject *gobject) {
  KaldiResult *self = KALDI_RESULT(gobject);
  g_free(self->texts);
  G_OBJECT_CLASS(kaldi_result_parent_class)->finalize(gobject);
}


static void
kaldi_result_class_init (KaldiResultClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize = kaldi_result_finalize;
  gobject_class->set_property = kaldi_result_set_property;
  gobject_class->get_property = kaldi_result_get_property;

  obj_properties[PROP_TEXTS] =
    g_param_spec_string ("texts",
        "transcription texts",
        "transcriptions texts, separated with newlines",
        "",
        G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, obj_properties);
}

} // namespace kaldi
