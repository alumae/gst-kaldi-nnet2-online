all:

KALDI_ROOT?=/home/tanel/tools/kaldi-online

include $(KALDI_ROOT)/src/kaldi.mk
ifneq ($(KALDI_FLAVOR), dynamic)
$(error Kaldi must compiled with dynamic libraries support. Run configure with --shared flag. )
endif


CXXFLAGS+=-I$(KALDI_ROOT)/src

EXTRA_CXXFLAGS += $(shell pkg-config --cflags gstreamer-1.0)
EXTRA_CXXFLAGS += $(shell pkg-config --cflags glib-2.0)

EXTRA_LDLIBS = -pthread -lgstbase-1.0 -lgstcontroller-1.0 -lgstreamer-1.0 -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lrt -lglib-2.0

#Kaldi shared libraries required by the GStreamer plugin
EXTRA_LDLIBS += -lkaldi-online2 -lkaldi-lat -lkaldi-decoder -lkaldi-feat -lkaldi-transform \
 -lkaldi-gmm -lkaldi-hmm \
 -lkaldi-tree -lkaldi-matrix  -lkaldi-util -lkaldi-base -lkaldi-thread


OBJFILES = gstkaldinnet2onlinedecoder.o simple-options-gst.o gst-audio-source.o kaldimarshal.o

LIBNAME=gstkaldionline2

LIBFILE = lib$(LIBNAME).so
BINFILES= $(LIBFILE)

all: $(LIBFILE)


# MKL libs required when linked via shared library
ifdef MKLROOT
EXTRA_LDLIBS+=-lmkl_p4n -lmkl_def
endif

$(LIBFILE): $(OBJFILES)
	$(CXX) -shared -DPIC -o $(LIBFILE) -Wl,-soname=$(LIBFILE) -Wl,--no-as-needed \
	  -L$(KALDILIBDIR) -Wl,-rpath=$(KALDILIBDIR) $(EXTRA_LDLIBS) $(LDLIBS) $(LDFLAGS) \
	  $(OBJFILES)
 
kaldimarshal.h: kaldimarshal.list
	glib-genmarshal --header --prefix=kaldi_marshal kaldimarshal.list > kaldimarshal.h.tmp
	mv kaldimarshal.h.tmp kaldimarshal.h

kaldimarshal.cc: kaldimarshal.list kaldimarshal.h
	echo "#include \"glib-object.h\"" >> kaldimarshal.c.tmp
	echo "#include \"kaldimarshal.h\"" >> kaldimarshal.c.tmp
	glib-genmarshal --body --prefix=kaldi_marshal kaldimarshal.list >> kaldimarshal.c.tmp
	mv kaldimarshal.c.tmp kaldimarshal.cc
 
clean: 
	-rm -f *.o *.a $(TESTFILES) $(BINFILES) kaldimarshal.h kaldimarshal.cc
 
#
depend:  kaldimarshal.h kaldimarshal.cc 
	-$(CXX) -M $(CXXFLAGS) *.cc > .depend.mk  

-include .depend.mk


