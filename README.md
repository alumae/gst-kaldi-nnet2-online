# WHAT IT IS


GStreamer plugin that wraps Kaldi's SingleUtteranceNnet2Decoder. It requires iVector-adapted
DNN acoustic models. The iVectors are adapted to the current audio stream automatically.


# CHANGELOG
2016-06-23: Got rid of the GStreamer property warnings at startup, thanks to @MathieuDuponchelle.

2015-11-05: Minor changes for compatibility with Ubuntu 12.04 (and its older version
of Jansson). Also use `ReadDecodeGraph` to read fst file, supporting the use of
const (mapped) fsts.

2015-06-03: Full results can now inlude word alignment information for the
best hypothesis. In order to activate this, set the `word-boundary-file`
property to point to the `word_boundary.int` file typically found in the
`data/lang/phones/` subdirectory of a trained system.

2015-06-02: Added functionality to output structured recognition results,
that optionally include n-best results, phone alignment information and
possibly other things (word alignments, sausages, lattices, sentence and
word confidence scores) in the future.
The structured results are pushed using the signal `full-final-result`, and
are formatted using a JSON encoding. See below for a full description.
Also removed the individual signals that were used for pushing out phone alignments.


2015-04-30: Added functionality to change models (FST, acoustic model,
big language model) after initial initialization. Added functionality to 
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

The following works on Linux (I'm using Debian 'testing'; Ubuntu 12.04 also works).

Compile Kaldi trunk, using the shared configuration:
In Kaldi's 'src' directory:

    ./configure --shared
    make depend
    make

Install gstreamer-1.0:

    sudo apt-get install gstreamer1.0-plugins-bad  gstreamer1.0-plugins-base gstreamer1.0-plugins-good  gstreamer1.0-pulseaudio  gstreamer1.0-plugins-ugly  gstreamer1.0-tools libgstreamer1.0-dev

On Ubuntu 12.04, you'll need to use a backport ppa to get version 1.0 of gstreamer, before doing the sudo apt-get install above:

    sudo add-apt-repository ppa:gstreamer-developers/ppa
    sudo apt-get update

Install the Jansson library development package (version 2.7 or newer), used for encoding results as JSON:

    sudo apt-get install libjansson-dev

Now we can compile this plugin. Change to `src` of this project:

    cd src

Compile, specifying Kaldi's root directory:

    make depend
    KALDI_ROOT=/path/of/kaldi-trunk make

This should result in 'libgstkaldionline2.so'.

Test if GStreamer can access the plugin:

    GST_PLUGIN_PATH=. gst-inspect-1.0 kaldinnet2onlinedecoder

The output should list all plugin properties with their default values:

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


# STRUCTURED RESULTS

Below is a sample of JSON-encoded full recognition results, pushed out 
using the `full-final-result` signal. This sample was generated using
`do-phone-alignment=true` and `num-nbest=10` (although due to pruning it 
includes only two n-best hypotheses). Note that the words in the file specified
by `word-syms` and the phones in the file specified in `phone-syms` must 
be encoded using UTF-8, otherwise the output won't be valid JSON.

    {
      "status": 0,
      "result": {
        "hypotheses": [
          {
            "transcript": "we're not ready for the next epidemic",
            "likelihood": 128.174,
            "phone-alignment": [
              {
                "start": 0,
                "phone": "SIL",
                "length": 0.71
              },
              {
                "start": 0.71,
                "phone": "W_B",
                "length": 0.18
              },
              {
                "start": 0.89,
                "phone": "ER_E",
                "length": 0.06
              },
              {
                "start": 0.95,
                "phone": "N_B",
                "length": 0.06
              },
              {
                "start": 1.01,
                "phone": "AA_I",
                "length": 0.19
              },
              {
                "start": 1.2,
                "phone": "T_E",
                "length": 0.11
              },
              {
                "start": 1.31,
                "phone": "R_B",
                "length": 0.07
              },
              {
                "start": 1.38,
                "phone": "EH_I",
                "length": 0.1
              },
              {
                "start": 1.48,
                "phone": "D_I",
                "length": 0.05
              },
              {
                "start": 1.53,
                "phone": "IY_E",
                "length": 0.22
              },
              {
                "start": 1.75,
                "phone": "SIL",
                "length": 0.46
              },
              {
                "start": 2.21,
                "phone": "F_B",
                "length": 0.1
              },
              {
                "start": 2.31,
                "phone": "ER_E",
                "length": 0.05
              },
              {
                "start": 2.36,
                "phone": "DH_B",
                "length": 0.05
              },
              {
                "start": 2.41,
                "phone": "AH_E",
                "length": 0.05
              },
              {
                "start": 2.46,
                "phone": "N_B",
                "length": 0.06
              },
              {
                "start": 2.52,
                "phone": "EH_I",
                "length": 0.11
              },
              {
                "start": 2.63,
                "phone": "K_I",
                "length": 0.08
              },
              {
                "start": 2.71,
                "phone": "S_I",
                "length": 0.05
              },
              {
                "start": 2.76,
                "phone": "T_E",
                "length": 0.07
              },
              {
                "start": 2.83,
                "phone": "EH_B",
                "length": 0.08
              },
              {
                "start": 2.91,
                "phone": "P_I",
                "length": 0.09
              },
              {
                "start": 3,
                "phone": "AH_I",
                "length": 0.04
              },
              {
                "start": 3.04,
                "phone": "D_I",
                "length": 0.08
              },
              {
                "start": 3.12,
                "phone": "EH_I",
                "length": 0.1
              },
              {
                "start": 3.22,
                "phone": "M_I",
                "length": 0.08
              },
              {
                "start": 3.3,
                "phone": "IH_I",
                "length": 0.08
              },
              {
                "start": 3.38,
                "phone": "K_E",
                "length": 0.18
              },
              {
                "start": 3.56,
                "phone": "SIL",
                "length": 0.13
              }
            ],
            "word-alignment": [
              {
                "word": "we're",
                "start": 0.71,
                "length": 0.24
              },
              {
                "word": "not",
                "start": 0.95,
                "length": 0.36
              },
              {
                "word": "ready",
                "start": 1.31,
                "length": 0.44
              },
              {
                "word": "for",
                "start": 2.21,
                "length": 0.15
              },
              {
                "word": "the",
                "start": 2.36,
                "length": 0.1
              },
              {
                "word": "next",
                "start": 2.46,
                "length": 0.37
              },
              {
                "word": "epidemic",
                "start": 2.83,
                "length": 0.73
              }
            ]
          },
          {
            "transcript": "were not ready for the next epidemic",
            "likelihood": 125.323
          }
        ]
      },
      "segment-start": 58.25,
      "segment-length": 3.69,
      "total-length": 61.94
    }

# CITING

If you use this software for research, you can cite the following paper 
(available here: http://ebooks.iospress.nl/volumearticle/37996):

    @inproceedigs{alumae2014,
      author={Tanel Alum\"{a}e},
      title="Full-duplex Speech-to-text System for {Estonian}",
      booktitle="Baltic HLT 2014",
      year=2014,
      address="Kaunas, Lithuania"
    }

Of course, you should also acknowledge Kaldi, which does all the hard work.
