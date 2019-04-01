#!/usr/bin/env python
#
# Copyright (c) 2013 Tanel Alumae
# Copyright (c) 2008 Carnegie Mellon University.
#
# Inspired by the CMU Sphinx's Pocketsphinx Gstreamer plugin demo (which has BSD license)
#
# Licence: BSD

import sys
import os
import gi
gi.require_version('Gst', '1.0')
from gi.repository import GObject, Gst, Gtk, Gdk
GObject.threads_init()
Gdk.threads_init()

Gst.init(None)

class DemoApp(object):
    """GStreamer/Kaldi Demo Application"""
    def __init__(self):
        """Initialize a DemoApp object"""
        self.init_gui()
        self.init_gst()

    def init_gui(self):
        """Initialize the GUI components"""
        self.window = Gtk.Window()
        self.window.connect("destroy", self.quit)
        self.window.set_default_size(400,200)
        self.window.set_border_width(10)
        vbox = Gtk.VBox()        
        self.text = Gtk.TextView()
        self.textbuf = self.text.get_buffer()
        self.text.set_wrap_mode(Gtk.WrapMode.WORD)
        vbox.pack_start(self.text, True, True, 1)
        self.button = Gtk.Button("Speak")
        self.button.connect('clicked', self.button_clicked)
        vbox.pack_start(self.button, False, False, 5)
        self.window.add(vbox)
        self.window.show_all()

    def quit(self, window):
        Gtk.main_quit()

    def init_gst(self):
        """Initialize the speech components"""
        self.pulsesrc = Gst.ElementFactory.make("pulsesrc", "pulsesrc")
        if self.pulsesrc == None:
            print >> sys.stderr, "Error loading pulsesrc GST plugin. You probably need the gstreamer1.0-pulseaudio package"
            sys.exit()	
        self.audioconvert = Gst.ElementFactory.make("audioconvert", "audioconvert")
        self.audioresample = Gst.ElementFactory.make("audioresample", "audioresample")    
        self.asr = Gst.ElementFactory.make("kaldinnet2onlinedecoder", "asr")
        self.fakesink = Gst.ElementFactory.make("fakesink", "fakesink")
        
        if self.asr:
          model_file = "models/final.mdl"
          if not os.path.isfile(model_file):
              print >> sys.stderr, "Models not downloaded? Run prepare-models.sh first!"
              sys.exit(1)
          self.asr.set_property("fst", "models/HCLG.fst")
          self.asr.set_property("model", model_file)
          self.asr.set_property("word-syms", "models/words.txt")
          self.asr.set_property("feature-type", "mfcc")
          self.asr.set_property("mfcc-config", "conf/mfcc.conf")
          self.asr.set_property("ivector-extraction-config", "conf/ivector_extractor.fixed.conf")
          self.asr.set_property("max-active", 7000)
          self.asr.set_property("beam", 10.0)
          self.asr.set_property("lattice-beam", 6.0)
          self.asr.set_property("do-endpointing", True)
          self.asr.set_property("endpoint-silence-phones", "1:2:3:4:5:6:7:8:9:10")
          self.asr.set_property("use-threaded-decoder", False)
          self.asr.set_property("chunk-length-in-secs", 0.2)
        else:
          print >> sys.stderr, "Couldn't create the kaldinnet2onlinedecoder element. "
          if os.environ.has_key("GST_PLUGIN_PATH"):
            print >> sys.stderr, "Have you compiled the Kaldi GStreamer plugin?"
          else:
            print >> sys.stderr, "You probably need to set the GST_PLUGIN_PATH envoronment variable"
            print >> sys.stderr, "Try running: GST_PLUGIN_PATH=../src %s" % sys.argv[0]
          sys.exit();
        
        # initially silence the decoder
        self.asr.set_property("silent", True)
        
        self.pipeline = Gst.Pipeline()
        for element in [self.pulsesrc, self.audioconvert, self.audioresample, self.asr, self.fakesink]:
            self.pipeline.add(element)         
        self.pulsesrc.link(self.audioconvert)
        self.audioconvert.link(self.audioresample)
        self.audioresample.link(self.asr)
        self.asr.link(self.fakesink)    
  
        self.asr.connect('partial-result', self._on_partial_result)
        self.asr.connect('final-result', self._on_final_result)        
        self.pipeline.set_state(Gst.State.PLAYING)


    def _on_partial_result(self, asr, hyp):
        """Delete any previous selection, insert text and select it."""
        Gdk.threads_enter()
        # All this stuff appears as one single action
        self.textbuf.begin_user_action()
        self.textbuf.delete_selection(True, self.text.get_editable())
        self.textbuf.insert_at_cursor(hyp)
        ins = self.textbuf.get_insert()
        iter = self.textbuf.get_iter_at_mark(ins)
        iter.backward_chars(len(hyp))
        self.textbuf.move_mark(ins, iter)
        self.textbuf.end_user_action()    
        Gdk.threads_leave()
                
    def _on_final_result(self, asr, hyp):
        Gdk.threads_enter()
        """Insert the final result."""
        # All this stuff appears as one single action
        self.textbuf.begin_user_action()
        self.textbuf.delete_selection(True, self.text.get_editable())
        self.textbuf.insert_at_cursor(hyp)
        if (len(hyp) > 0):
            self.textbuf.insert_at_cursor(" ")
        self.textbuf.end_user_action()
        Gdk.threads_leave()


    def button_clicked(self, button):
        """Handle button presses."""
        if button.get_label() == "Speak":
            button.set_label("Stop")
            self.asr.set_property("silent", False)
        else:
            button.set_label("Speak")
            self.asr.set_property("silent", True)
            

if __name__ == '__main__':
  app = DemoApp()
  Gdk.threads_enter()
  Gtk.main()
  Gdk.threads_leave()

