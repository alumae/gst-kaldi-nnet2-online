# WHAT IT IS


GStreamer plugin that wraps Kaldi's SingleUtteranceNnet2Decoder. It requires iVector-adapted
DNN acoustic models. The iVectors are adapted to the current audio stream automatically.


# CHANGELOG

2015-03-01: one can now optionally use the threaded online decoder. Citing 
Daniel Povey who developed the code in Kaldi: "This should make it possible to 
decode in real-time with larger models and graphs than before, because 
the decoding and the nnet evaluation are in separate threads and can be done in parallel."
Use `make CPPFLAGS=-DTHREADED_DECODER` to compile it. Note that the endpointing
and partial results might not work as expected with the threaded decoder, 
working on the fix.

2015-01-09: Added language model rescoring functionality. In order to use it,
you have to specify two properties: `lm-fst` and `big-lm-const-arpa`. The `lm-fst`
property gives the location of the *original* LM (the one that was used fpr 
compiling the HCLG.fst used during decodong). The `big-lm-const-arpa` property
gives the location of the big LM used that is used to rescore the final lattices.
The big LM must be in the 'ConstArpaLm' format, use the Kaldi's 
`utils/build_const_arpa_lm.sh` script to produce it from the ARPA format.

2014-11-11: the plugin saves the adaptation state between silence-segmented utterances and between
multiple decoding sessions of the same plugin instance. 
That is, if you start decoding a new stream, the adaptation state of the
previous stream is used (unless it's the first stream, in which case a global mean is used).
Use the `adaptation-state` plugin property to get, and set the adaptation state. Use an empty string
with the set method to reset the adaptation state to default. This functionality requires
Kaldi revision 4582 or later.


# HOW TO COMPILE IT

The following works on Linux (I'm using Debian 'testing').

Compile Kaldi trunk, using the shared configuration:
In Kaldi's 'src' directory:

    ./configure --shared
    make depend
    make

Install gstreamer-1.0:

    sudo apt-get install gstreamer1.0-plugins-bad  gstreamer1.0-plugins-base gstreamer1.0-plugins-good  gstreamer1.0-pulseaudio  gstreamer1.0-plugins-ugly  gstreamer1.0-tools libgstreamer1.0-dev

Now we can compile this plugin. Change to `src` of this project:

    cd src

Compile, specifying Kaldi's root directory:

    make depend
    KALDI_ROOT=/path/of/kaldi-trunk make

This should result in 'libgstkaldionline2.so'.

Test if GStreamer can access the plugin:

    GST_PLUGIN_PATH=. gst-inspect-1.0 kaldinnet2onlinedecoder


# HOW TO USE IT

Command-line usage is demonstrated in `demo/`.

Usage through GStreamer's Python bindings is demonstrated `demo/gui-demo.py`
and in the project
https://github.com/alumae/kaldi-gstreamer-server, in file kaldigstserver/decoder2.py.



