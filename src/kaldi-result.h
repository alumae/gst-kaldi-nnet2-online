// kaldi-result.h

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


#ifndef __KALDI_RESULT_H__
#define __KALDI_RESULT_H__

#include <glib-object.h>

namespace kaldi {

#define KALDI_TYPE_RESULT                  (kaldi_result_get_type ())
#define KALDI_RESULT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), KALDI_TYPE_RESULT, KaldiResult))
#define KALDI_IS_RESULT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KALDI_TYPE_RESULT))
#define KALDI_RESULT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), KALDI_TYPE_RESULT, KaldiResultClass))
#define KALDI_IS_RESULT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), KALDI_TYPE_RESULT))
#define KALDI_RESULT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), KALDI_TYPE_RESULT, KaldiResultClass))

typedef struct _KaldiResult        KaldiResult;
typedef struct _KaldiResultClass   KaldiResultClass;


struct _KaldiResult
{
    /* Parent instance structure */
    GObject parent_instance;

    /* Transcriptions for all results, separated by newlines */
    gchar *texts;
};

struct _KaldiResultClass
{
    /* Parent class structure */
    GObjectClass parent_class;
};

/* used by KALDI_TYPE_RESULT */
GType kaldi_result_get_type (void);

} // namespace kaldi

#endif /* __KALDI_RESULT_H__ */
