Demos
=====

Preparation
-----------

First, compile the plugin.

Next, change to the demo directory  and run `./prepare-models.sh` to download models from  http://kaldi-asr.org.
Some model configuration files are also patched so that the paths in the 
configuration files would be correct.

Command-line demo
-----------------

Run:

    ./transcribe-audio.sh dr_strangelove.mp3
    
Many messages like

    gst-launch-1.0:9156): GLib-GObject-WARNING **: Attempt to add property Gstkaldinnet2onlinedecoder::endpoint-silence-phones after class was initialised
    
are printed to the console. They can be ignored. They are caused by the fact
that the plugin uses a bit unorthodox method to define the plugin
properties.

After the warnings, you should get something like:

    LOG (ComputeDerivedVars():ivector-extractor.cc:180) Computing derived variables for iVector extractor
    LOG (ComputeDerivedVars():ivector-extractor.cc:201) Done.
    huh i hello this is hello dimitri listen i i can't hear too well do you support you could turn the music down just a little
    ha ha that's much better yet not yet
    fine i can hear you now dimitri
    [...]
    
    
GUI demo
--------

Run:

    GST_PLUGIN_PATH=../src ./gui-demo.py
    
Note that the GUI demo runs very slowlly when using the threaded decoder.
(that's why we don't use it in the GUI demo).
This probably has something to do with Python, GIL, etc. I have no idea how
to fix it.

