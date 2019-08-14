#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: transcribe-audio.sh <audio>"
    echo "e.g.: transcribe-audio.sh dr_strangelove.mp3"
    exit 1;
fi

! GST_PLUGIN_PATH=../src gst-inspect-1.0 kaldinnet2onlinedecoder > /dev/null 2>&1 && echo "Compile the plugin in ../src first" && exit 1;

if [ ! -f models/HCLG.fst ]; then
    echo "Run ./prepare-models.sh first to download models"
    exit 1;
fi

audio=$1

GST_PLUGIN_PATH=../src gst-launch-1.0 --gst-debug="" -q filesrc location=$audio ! decodebin ! audioconvert ! audioresample ! \
kaldinnet2onlinedecoder \
  use-threaded-decoder=true \
  model=models/final.mdl \
  fst=models/HCLG.fst \
  word-syms=models/words.txt \
  phone-syms=models/phones.txt \
  word-boundary-file=models/word_boundary.int \
  num-nbest=3 \
  num-phone-alignment=3 \
  do-phone-alignment=true \
  feature-type=mfcc \
  mfcc-config=conf/mfcc.conf \
  ivector-extraction-config=conf/ivector_extractor.fixed.conf \
  max-active=7000 \
  beam=11.0 \
  lattice-beam=5.0 \
  do-endpointing=true \
  endpoint-silence-phones="1:2:3:4:5:6:7:8:9:10" \
  chunk-length-in-secs=0.2 \
! filesink location=/dev/stdout buffer-mode=2
