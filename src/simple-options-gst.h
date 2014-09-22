// simple-options-gst.h

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

#ifndef KALDI_SRC_SIMPLE_OPTIONS_GST_H_
#define KALDI_SRC_SIMPLE_OPTIONS_GST_H_

#include <string>

#include "util/simple-options.h"

namespace kaldi {

// This class is the same as Kaldi's SimpleOptions except that
// it transforms all '.' characters to '-' in options names,
// in order to avoid GStreamer doing it itself
class SimpleOptionsGst : public SimpleOptions {
  void Register(const std::string &name, bool *ptr, const std::string &doc);
  void Register(const std::string &name, int32 *ptr, const std::string &doc);
  void Register(const std::string &name, uint32 *ptr, const std::string &doc);
  void Register(const std::string &name, float *ptr, const std::string &doc);
  void Register(const std::string &name, double *ptr, const std::string &doc);
  void Register(const std::string &name, std::string *ptr,
                  const std::string &doc);

 private:
  std::string TransformName(const std::string &name);
};
}
#endif  // KALDI_SRC_SIMPLE_OPTIONS_GST_H_
