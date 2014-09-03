#ifndef SIMPLE_OPTIONS_GST_H_
#define SIMPLE_OPTIONS_GST_H_

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
#endif  // SIMPLE_OPTIONS_GST_H_
