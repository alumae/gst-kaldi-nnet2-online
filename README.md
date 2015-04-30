# WHAT IT IS


GStreamer plugin that wraps Kaldi's SingleUtteranceNnet2Decoder. It requires iVector-adapted
DNN acoustic models. The iVectors are adapted to the current audio stream automatically.


# CHANGELOG

2015-04-30: Added functionality to change models (FST, acoustic model,
big language model) after initial initializetion. Added functionality to 
ouput phone aligment information (see properties `do-phone-alignment`
and `phone-syms`). Both additions by Ricard Marxer (@rikrd).

2015-04-28: Endpointing and interim recognition results now work correctly
when using the threaded decoder. Note that endpointing does not work exactly
the same as with the unthreaded decoder, and there might be some differences
between individual decoder runs, but for all practical purposes it should be OK.
Also, introduced a new property `traceback-period-in-secs`
that specifies how often intermediate results are sent to the client (default is 0.5).
*NB:* this update requires Kaldi revision 5036 or later.

2015-03-05: Threaded decoder can now be selected at configuration time, using the
`use-threaded-decoder` property. *NB:* this property should be set before other 
properties. Endpointing and partial results might still not work as expected with the threaded decoder.

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
compiling the HCLG.fst used during decoding). The `big-lm-const-arpa` property
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

First, this prints a lot of warnings like:

    (gst-inspect-1.0:10810): GLib-GObject-WARNING **: Attempt to add property Gstkaldinnet2onlinedecoder::endpoint-silence-phones after class was initialised

    (gst-inspect-1.0:10810): GLib-GObject-WARNING **: Attempt to add property Gstkaldinnet2onlinedecoder::endpoint-rule1-must-contain-nonsilence after class was initialised

    (gst-inspect-1.0:10810): GLib-GObject-WARNING **: Attempt to add property Gstkaldinnet2onlinedecoder::endpoint-rule1-min-trailing-silence after class was initialised

This is because the properties of the plugin are initialized dynamically from Kaldi components
and the Kaldi components are created after plugin initialization. It doesn't seem
to harm any functinality.

The second part of the `gst-inspect-1.0` output should list all plugin properties with their default values:

    Factory Details:
      Rank                     none (0)
      Long-name                KaldiNNet2OnlineDecoder
      Klass                    Speech/Audio
      Description              Convert speech to text
    [...]
      name                : The name of the object
                            flags: readable, writable
                            String. Default: "kaldinnet2onlinedecoder0"
      parent              : The parent of the object
                            flags: readable, writable
                            Object of type "GstObject"
      silent              : Silence the decoder
                            flags: readable, writable
                            Boolean. Default: false
      model               : Filename of the acoustic model
                            flags: readable, writable
                            String. Default: "final.mdl"
    [...]
      max-nnet-batch-size : Maximum batch size we use in neural-network decodable object, in cases where we are not constrained by currently available frames (this will rarely make a difference)
                            flags: readable, writable
                            Integer. Range: -2147483648 - 2147483647 Default: 256 

    Element Signals:
      "partial-result" :  void user_function (GstElement* object,
                                              gchararray arg0,
                                              gpointer user_data);
      "final-result" :  void user_function (GstElement* object,
                                            gchararray arg0,
                                            gpointer user_data);



# HOW TO USE IT

Command-line usage is demonstrated in `demo/`.

Usage through GStreamer's Python bindings is demonstrated `demo/gui-demo.py`
and in the project
https://github.com/alumae/kaldi-gstreamer-server, in file kaldigstserver/decoder2.py.



# CITING

If you use this software for research, you can cite the following paper 
(available here: http://ebooks.iospress.nl/volumearticle/37996):

    @inproceedigs{alumae2014,
      author={Tanel Alum\"{a}e},
      title="Full-duplex Speech-to-text System for {Estonian}",
      booktitle="Baltic HLT 2014",
      year=2014,
      address="Kaunas, Lihtuania"
    }

Of course, you should also acknowledge Kaldi, which does all the hard work.
