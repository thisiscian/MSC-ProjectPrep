DEFINES		= IW_DEBUG _ISOC9X_SOURCE
FLAGS		= -g -O0

############ Architecture specific settings ##########################
ifndef ARCH
ARCH		= linux
endif
ifeq ($(ARCH),alpha)
FLAGS		= -O4 -g3
#FLAGS		= -O4 -inline speed -arch ev56
GTK_CONFIG      = /vol/ai/bin/gtk-config
DEFINES		+= HAVE_WORDEXP
endif

ifeq ($(ARCH),sol2)
USE_GCC		= USE_GCC
DEFINES		+= HAVE_WORDEXP HAVE_MEMALIGN
LDLIBS		= -L/usr/lib -R/usr/lib -R/vol/netpbm/lib -R/vol/local/lib
endif

ifeq ($(ARCH),macosx)
INCLUDE		+= -I/sw/include
USE_GCC		= USE_GCC
endif

ifeq ($(ARCH),linux)
FLAGS		= -g -O3 -ffast-math
#FLAGS		= -O3 -march=pentiumpro -fomit-frame-pointer -ffast-math
USE_GCC		= USE_GCC
DEFINES		+= HAVE_WORDEXP HAVE_MEMALIGN
endif

############ Features and destination ################################
#WITH_GTK1	= 1
WITH_GPB	= 1
#WITH_DACS	= 1
#WITH_FFMPEG	= /usr
WITH_JPEG	= /vol/netpbm
WITH_PNG	= /vol/netpbm
#WITH_ZLIB	= /vol/local
WITH_GRABBER	= /vol/ai
#WITH_ARAVIS	= /usr
#WITH_FIRE	= /usr
#WITH_FIRE2	= 1
#WITH_ICUBE	= /usr
#WITH_LIBV4L	= /usr
#WITH_MVIMPACT	= /usr
#WITH_UEYE	= /usr
#WITH_UNICAP	= /usr

PACKET		= iceWing

#PREFIX		= /home/floemker/c/uni/install
PREFIX		= /usr/local
MANDIR		= $(PREFIX)/man
BINDIR		= $(PREFIX)/bin
INCLUDEDIR_B	= $(PREFIX)/include
DATADIR_B	= $(PREFIX)/share
LIBDIR_B	= $(PREFIX)/lib

INCLUDEDIR	= $(INCLUDEDIR_B)/$(PACKET)
PINCLUDEDIR	= $(INCLUDEDIR_B)/iwPlugins
DATADIR		= $(DATADIR_B)/$(PACKET)
LIBDIR		= $(LIBDIR_B)/$(PACKET)
LOGDIR		= $(DATADIR_B)/log

LOGFILE		= $(LOGDIR)/$(PACKET).log

LOGHEADER	= "\# $(LOGFILE)\n\#\
log-file for package '$(PACKET)'\n\#\n\#\
Provides a graphical plugin shell for e.g. image processing\n\#\
(tracking of skin colored regions, ...)\n\#\n\#\
Frank Loemker, 19.04.2009\n\#"

INSTALL_LIB	= install -m 444 $(1) $(2) ; \
		echo "install -m 444 $(1) $(2)" >> $(LOGFILE)
INSTALL_BIN	= install -m 555 $(1) $(2) ; \
		echo "install -m 555 $(1) $(2)" >> $(LOGFILE)
MAKE_DIR	= @if [ ! -d $(1) ] ; then \
			mkdir -m 2775 $(1) ; \
			echo "mkdir $(1) 2>/dev/null" >> $(LOGFILE) ; \
		fi
MAKE_DIR_NL	= @if [ ! -d $(1) ] ; then \
			mkdir -m 2775 $(1) ; \
		fi

PKG_PATH = "$($(1))/lib/pkgconfig:$($(1))/lib64/pkgconfig:$($(1))/lib32/pkgconfig"
define PKG_FIND
	$(call PKG_PATH $(1))
	DEFINES	+= $(1)
	INCLUDE	+= $(shell PKG_CONFIG_PATH=$(PKG_PATH) pkg-config --cflags $(2))
	LDLIBS	+= $(shell export PKG_CONFIG_PATH=$(PKG_PATH); \
			pkg-config --libs-only-L $(2) | sed 's/-L/-Wl,-rpath,/'; \
			pkg-config --libs $(2))
endef

############ WITH_GTK1 ###############################################

ifdef WITH_GTK1
GTK_CONFIG	?= gtk-config
GTK_CFLAGS	:= $(shell $(GTK_CONFIG) --cflags gthread)
GTK_LDLIBS	= `$(GTK_CONFIG) --libs gthread`
else
GTK_CONFIG	?= pkg-config
GTK_CFLAGS	:= $(shell $(GTK_CONFIG) --cflags gtk+-2.0 gthread-2.0) -DGTK_ENABLE_BROKEN
GTK_LDLIBS	= `$(GTK_CONFIG) --libs gtk+-2.0 gthread-2.0`
endif

######################################################################

INCLUDE		+= -I. -I./plugins

MAIN_SRCS = icewing.c mainloop.c grab.c grab_avgui.c output.c output_sfb.c image.c \
	plugin.c plugin_comm.c plugin_gui.c plugin_gui_plug.c plugin_cxx.cpp \
	region.c regcalc.c
GUI_SRCS = Gimage.c Gmovie.c Gpreview.c Goptions.c Gdialog.c Ginfo.c Grender.c \
	GrenderImg.c GrenderText.c text.c Gpolygon.c Gsession.c Gtools.c \
	Gpreferences.c Gcolor.c Ggui.c Glist.c Gdata.c
GUI_AVI_SRCS = avilib.c
TOOLS_SRCS = fileset.c tools.c img_video.c opencv.cpp
AV_SRCS = AVOptions.c
BACKPRO_SRCS = backpro.c
GSIMPLE_SRCS = gauss_simple.c gsimple_image.c skin.c
POLYNOM_SRCS = polynom.c
IMGCLASS_SRCS = imgclass.c
SKINCLASS_SRCS = skinclass.c sclas_image.c
RECORD_SRCS = record.c
TRACKING_SRCS = handtrack.c trajectory.c track_kalman.c track_simple.c track_tools.c \
	kalman.c matrix.c

SRCS = $(patsubst %,main/%,$(MAIN_SRCS)) \
	$(patsubst %,gui/%,$(GUI_SRCS)) \
	$(patsubst %,gui/avi/%,$(GUI_AVI_SRCS)) \
	$(patsubst %,tools/%,$(TOOLS_SRCS)) \
	$(patsubst %,avvideo/%,$(AV_SRCS)) \
	\
	$(patsubst %,plugins/backpro/%,$(BACKPRO_SRCS)) \
	$(patsubst %,plugins/gsimple/%,$(GSIMPLE_SRCS)) \
	$(patsubst %,plugins/polynom/%,$(POLYNOM_SRCS)) \
	$(patsubst %,plugins/imgclass/%,$(IMGCLASS_SRCS)) \
	$(patsubst %,plugins/skinclass/%,$(SKINCLASS_SRCS)) \
	$(patsubst %,plugins/record/%,$(RECORD_SRCS)) \
	$(patsubst %,plugins/tracking/%,$(TRACKING_SRCS)) # memtest.c

############ WITH_FFMPEG #############################################

ifdef WITH_FFMPEG

DEFINES		+= WITH_FFMPEG
DEFINES		+= `if [ -r $(WITH_FFMPEG)/include/libavformat/avformat.h ]; then echo FFMPEG_LIBINC; fi`
INCLUDE		+= -I$(WITH_FFMPEG)/include
LDLIBS		+= -L$(WITH_FFMPEG)/lib -lavformat -lavcodec -lswscale -lavutil -lz -lm

endif

############ WITH_JPEG ###############################################

ifdef WITH_JPEG

GUI_AVI_SRCS	+= jpegutils.c

DEFINES		+= WITH_JPEG
LDLIBS		+= -L$(WITH_JPEG)/lib -ljpeg
INCLUDE		+= -I$(WITH_JPEG)/include

endif

############ WITH_PNG ################################################

ifdef WITH_PNG

DEFINES		+= WITH_PNG
LDLIBS		+= -L$(WITH_PNG)/lib -lpng
INCLUDE		+= -I$(WITH_PNG)/include
ifdef WITH_ZLIB
LDLIBS		+= -L$(WITH_ZLIB)/lib -lz
INCLUDE		+= -I$(WITH_ZLIB)/include
else
LDLIBS		+= -lz
endif

endif

############ WITH_GPB ################################################

ifdef WITH_GPB

ifdef WITH_GTK1
GDK_PIXBUF	= gdk-pixbuf-config
CFLAGS		+= `$(GDK_PIXBUF) --cflags`
LDLIBS		+= `$(GDK_PIXBUF) --libs`
endif

DEFINES		+= WITH_GPB

endif

############ WITH_GRABBER ############################################

ifdef WITH_GRABBER

ifeq ($(ARCH),alpha)
LDLIBS		+= -lmme -L$(WITH_GRABBER)/lib -lAVVideo
INCLUDE		+= -I$(WITH_GRABBER)/include
else
INCLUDE		+= -Iavvideo
AV_SRCS		+= AVImg.c AVCapture.c AVColor.c drv_v4l2.c drv_v4l2_old.c \
		drv_aravis.c drv_firewire.c drv_icube.c drv_unicap.c \
		drv_mvimpact.c drv_ueye.c

############ WITH_ARAVIS ###############################################
ifdef WITH_ARAVIS

$(eval $(call PKG_FIND,WITH_ARAVIS,aravis-0.2))

endif ################################################################

############ WITH_FIRE ###############################################
ifdef WITH_FIRE2
ifndef WITH_FIRE
WITH_FIRE=$(WITH_FIRE2)
endif
endif

ifdef WITH_FIRE

INCLUDE		+= -I$(WITH_FIRE)/include

ifdef WITH_FIRE2
DEFINES		+= WITH_FIRE2
LDLIBS		+= -L$(WITH_FIRE)/lib -ldc1394 -lraw1394
else
DEFINES		+= WITH_FIRE
LDLIBS		+= -L$(WITH_FIRE)/lib -ldc1394_control -lraw1394
#LDLIBS		+= $(WITH_FIRE)/lib/libdc1394_control.a $(WITH_FIRE)/lib/libraw1394.a
endif

endif ################################################################

############ WITH_ICUBE ##############################################
ifdef WITH_ICUBE

DEFINES		+= WITH_ICUBE
INCLUDE		+= -I$(WITH_ICUBE)/include
LDLIBS		+= -Wl,-rpath,$(WITH_ICUBE)/lib -L$(WITH_ICUBE)/lib -lNETUSBCAM

endif ################################################################

############ WITH_LIBV4L #############################################
ifdef WITH_LIBV4L

$(eval $(call PKG_FIND,WITH_LIBV4L,libv4lconvert))

endif ################################################################

############ WITH_MVIMPACT ###########################################
ifdef WITH_MVIMPACT

DEFINES		+= WITH_MVIMPACT
INCLUDE		+= -I$(WITH_MVIMPACT)/include
LDLIBS		+= -Wl,-rpath,$(WITH_MVIMPACT)/lib -L$(WITH_MVIMPACT)/lib -lmvDeviceManager -lmvPropHandling

endif ################################################################

############ WITH_UEYE ###############################################
ifdef WITH_UEYE

DEFINES		+= WITH_UEYE
INCLUDE		+= -I$(WITH_UEYE)/include
LDLIBS		+= -Wl,-rpath,$(WITH_UEYE)/lib -L$(WITH_UEYE)/lib -lueye_api

endif ################################################################

############ WITH_UNICAP #############################################
ifdef WITH_UNICAP

$(eval $(call PKG_FIND,WITH_UNICAP,libunicap))

endif ################################################################

endif

else

INCLUDE		+= -Ilibs
SRCS		+= libs/AVImg.c

endif

############ WITH_DACS ###############################################

ifndef WITH_DACS

INCLUDE		+= -Ilibs/dacs
SRCS		+= libs/dacs/dacs.c

endif

######################################################################

DEFINES		+= DATADIR='"$(DATADIR)"' LIBDIR='"$(LIBDIR)"' 

CFLAGS		+= $(FLAGS) $(GTK_CFLAGS) $(INCLUDE)
CXXFLAGS	+= $(FLAGS) $(GTK_CFLAGS) $(INCLUDE)
LDFLAGS		+= $(FLAGS) $(GTK_LDFLAGS)
LDLIBS		+= $(GTK_LDLIBS) -L/usr/X11R6/lib -lX11 -ldl

OBJS += $(patsubst %c,%o,$(filter %.c,$(SRCS))) \
	$(patsubst %cc,%o,$(filter %.cc,$(SRCS))) \
	$(patsubst %C,%o,$(filter %.C,$(SRCS))) \
	$(patsubst %cpp,%o,$(filter %.cpp,$(SRCS)))

BINS = icewing
INCL_GUI = gui/Gcolor.h gui/Gdata.h gui/Gdialog.h gui/Ggui.h gui/Gimage.h \
	gui/Goptions.h gui/Gpolygon.h gui/Gpreview.h gui/Grender.h \
	gui/Gsession.h
INCL_MAIN = main/grab.h main/grab_prop.h main/image.h main/output.h main/plugin.h \
	main/region.h main/plugin_comm.h main/plugin_cxx.h main/sfb_iw.h
INCL_TOOL = tools/tools.h tools/opencv.h
INCL_DACS = libs/dacs/dacs.h
INCL_ICEMM = icemm/PluginData.hpp

DOXYGEN_FILES = docs/icewing.dox $(INCL_GUI) $(INCL_TOOL) \
	$(filter-out main/sfb_iw.h, $(INCL_MAIN)) $(INCL_ICEMM)

DATA = $(shell find data/* -prune -type f)
DATA_TMPL = $(wildcard data/tmpl/*)
MAN1 = $(patsubst %,%.1,$(BINS))

all: config-gen
	@$(MAKE) $(BINS)

ifdef WITH_DACS
include $(D3)/share/mk/general.mk
endif

######################################################################

ifdef USE_GCC

CC	= gcc
CXX	= g++
LD	= g++
FLAGS	+= -Wall
LDLIBS	+= -lpthread -rdynamic

ifeq ($(ARCH),alpha)
LDFLAGS	+= -rpath /vol/gnu/lib
LDLIBS	:= $(subst -pthread,,$(LDLIBS))
CFLAGS	:= $(subst -std1 -warnprotos,,$(CFLAGS))
endif
ifeq ($(ARCH),sol2)
CFLAGS	:= $(subst -mt,,$(CFLAGS))
CXXFLAGS:= $(subst -mt,,$(CXXFLAGS))
endif

else

#CC	= insure
#CXX	= insure
#LD	= insure
CC	= cc
CXX	= cxx
LD	= cxx
FLAGS	+= -w0 -std1
CXXFLAGS+= -msg_disable undpreid -msg_disable declbutnotref

endif

ifeq ($(ARCH),alpha)
CXXFLAGS := $(subst -Dalpha,,$(CXXFLAGS))
CFLAGS := $(subst -Dalpha,,$(CFLAGS))
endif

######################################################################

icewing: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

one:
	$(CC) $(CFLAGS) -ifo -c $(filter %.c,$(SRCS))
	$(CXX) $(CXXFLAGS) -ifo -c $(filter %.C,$(SRCS)) $(filter %.cpp,$(SRCS))
	$(LD) $(LDFLAGS) *.o $(LDLIBS) -o icewing

.PHONY: config-gen
config-gen:
	@echo "Checking file config.h"
	@rm -f config.h.new
	@for def in $(DEFINES) ; do \
	  echo "#define $$def" | sed 's/=/	/' >>config.h.new; \
	done
	diff config.h.new config.h >/dev/null || mv config.h.new config.h
	@rm -f config.h.new

############ INSTALLING ##############################################

install: init_log install_bin install_inc install_man install_data

ifdef WITH_DACS
PLUG_CFLAGS	:= -I$(INCLUDEDIR) -I$(D3)/include $(GTK_CFLAGS)
else
PLUG_CFLAGS	:= -I$(INCLUDEDIR) -I$(INCLUDEDIR)/dacs $(GTK_CFLAGS)
endif
VERSION		:= $(shell grep 'VERSION[[:space:]]*=' main/icewing.c | sed 's/[^0-9]*"\([^"]*\)".*/\1/')

.PHONY: icewing-config icewing.pc utils/icewing-plugingen utils/icewing-docgen
icewing-config icewing.pc:
	sed -e "s#@PREFIX@#$(PREFIX)#" \
		-e "s#@cflags@#$(PLUG_CFLAGS)#" \
		-e "s#@libs@##" \
		-e "s#@LIBDIR@#$(LIBDIR)#" \
		-e "s#@DATADIR@#$(DATADIR)#" \
		-e "s#@INCLUDEDIR@#$(INCLUDEDIR)#" \
		-e "s#@PINCLUDEDIR@#$(PINCLUDEDIR)#" \
		-e "s#@VERSION@#$(VERSION)#" $@.in > $@

utils/icewing-plugingen:
	sed -e "s#@BINDIR@#$(BINDIR)#" $@.in > $@

utils/icewing-docgen:
	sed -e "s#@BINDIR@#$(BINDIR)#" $@.in > $@

init_log: $(LOGFILE)
	echo >> $(LOGFILE)
	echo `date` >> $(LOGFILE)
	echo >> $(LOGFILE)

install_bin: $(BINS) icewing-config utils/icewing-plugingen utils/icewing-docgen
	$(call MAKE_DIR,$(BINDIR))
	$(call INSTALL_BIN,$^,$(BINDIR));

install_inc: $(INCL_GUI) $(INCL_MAIN) $(INCL_TOOL) $(INCL_ICEMM)
	$(call MAKE_DIR,$(INCLUDEDIR_B))
	$(call MAKE_DIR,$(INCLUDEDIR))
	$(call MAKE_DIR,$(INCLUDEDIR)/gui)
	$(call INSTALL_LIB,$(INCL_GUI),$(INCLUDEDIR)/gui);
	$(call MAKE_DIR,$(INCLUDEDIR)/main)
	$(call INSTALL_LIB,$(INCL_MAIN),$(INCLUDEDIR)/main);
	$(call MAKE_DIR,$(INCLUDEDIR)/tools)
	$(call INSTALL_LIB,$(INCL_TOOL),$(INCLUDEDIR)/tools);
ifndef WITH_DACS
	$(call MAKE_DIR,$(INCLUDEDIR)/dacs)
	$(call INSTALL_LIB,$(INCL_DACS),$(INCLUDEDIR)/dacs);
endif
	$(call MAKE_DIR,$(INCLUDEDIR)/icemm)
	$(call INSTALL_LIB,$(INCL_ICEMM),$(INCLUDEDIR)/icemm);

install_man: $(MAN1) 
	$(call MAKE_DIR,$(MANDIR))
	$(call MAKE_DIR,$(MANDIR)/man1)
	$(call INSTALL_LIB,$(MAN1),$(MANDIR)/man1);

install_data: $(DATA) icewing.pc
	$(call MAKE_DIR,$(DATADIR))
	$(call MAKE_DIR,$(DATADIR)/tmpl)
ifneq ($(DATA),)
	$(call INSTALL_LIB,$(DATA),$(DATADIR))
endif
	$(call INSTALL_LIB,$(DATA_TMPL),$(DATADIR)/tmpl)
	$(call INSTALL_LIB,docs/icewing.pdf,$(DATADIR));
	$(call MAKE_DIR,$(LIBDIR_B))
	$(call MAKE_DIR,$(LIBDIR_B)/pkgconfig)
	$(call INSTALL_LIB,icewing.pc,$(LIBDIR_B)/pkgconfig)
	$(call MAKE_DIR,$(LIBDIR))
	$(call MAKE_DIR,$(PINCLUDEDIR))

$(LOGFILE):
	$(call MAKE_DIR_NL,$(DATADIR_B))
	$(call MAKE_DIR_NL,$(LOGDIR))
	touch $@
	chmod 664 $@
	echo -e $(LOGHEADER) >> $(LOGFILE)

############ MAINTAINANCE ############################################

FILES = COPYING README NEWS TODO HACKING Makefile icewing.1 \
	*.in CMakeLists.txt icewing-api-update \
	cmake/cmake.txt \
	utils/Makefile utils/*doxygen utils/*txt utils/*cpp utils/*in utils/scripts/*[hk] \
	libs/*.[hc] libs/*/*.[hc] \
	avvideo/*.[hc] gui/*.[hc] gui/avi/*.[hc] gui/images/*.xpm gui/images/*.png \
	tools/*[hcp] main/*.[hc] main/*.cpp main/*.xpm main/*.xbm \
	plugins/README plugins/Makefile plugins/*/Makefile plugins/*/*.[hc] \
	plugins/*/*.cpp data/tmpl/*[ecph] \
	icemm/*.[ch]pp \
	docs/icewing.dox
FDOCS_SRCS = Makefile CMakeLists.txt *.bib *.sty *.ist *.tex tex/*.tex images/*.png images/*.fig
FDOCS =	$(patsubst %,docs/%,$(FDOCS_SRCS))
DATE := $(shell date +"%B %e %Y"|sed "s/  / /g")

.PHONY: clean depend pack packdoc dist one doxygen

doxygen:
	sed "s#@DOXYGEN_FILES@#$(DOXYGEN_FILES)#" Doxyfile.in >Doxyfile
	doxygen

pack:
	tar --use-compress-program bzip2 -cvf icewing.tar.bz2 $(FILES) $(FDOCS)

dist: pack
	sed 's/n%s V%s ([A-Za-z0-9 ]*),/n%s V%s ($(DATE)),/' main/icewing.c >temp.1
	mv temp.1 main/icewing.c
	mkdir icewing-$(VERSION)
	(cd icewing-$(VERSION); tar --use-compress-program bzip2 -xvf ../icewing.tar.bz2)
	cp docs/icewing.pdf docs/quicktour.avi icewing-$(VERSION)/docs
	tar --use-compress-program bzip2 -cvf icewing-$(VERSION).tar.bz2 icewing-$(VERSION)
	rm -rf icewing-$(VERSION)

depend: config-gen
	gcc -MM $(filter-out -I/usr/include%,$(CFLAGS)) $(SRCS) 2>/dev/null | sed 's/[^:]*: \([^.]*\)/\1.o: \1/' >.depend

clean:
	echo "Deleting all OBJ-files..."
	rm -f $(OBJS)

-include .depend
