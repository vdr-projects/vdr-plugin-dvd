#
# Makefile for a Video Disk Recorder plugin
#
# $Id$

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
#
PLUGIN = dvd

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The C++ compiler and options:

CXX      ?= g++
CXXFLAGS ?= -fPIC -O3 -Wall -Woverloaded-virtual
LDFLAGS  ?= $(CXXFLAGS)

### The directory environment:

VDRDIR = ../../..
LIBDIR = ../../lib
TMPDIR = /tmp
NAVDIR = /usr/include/dvdnav

### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' $(VDRDIR)/config.h)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES += -I$(VDRDIR)/include -I$(NAVDIR)

DEFINES += -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

# to use xine videoout:
ifdef POLLTIMEOUTS_BEFORE_DEVICECLEAR
DEFINES += -DPOLLTIMEOUTS_BEFORE_DEVICECLEAR=$(POLLTIMEOUTS_BEFORE_DEVICECLEAR)
endif

LIBS = -ldvdnav -la52

ifdef DJBFFT
LIBS   += -L/usr/lib/djbfft/lib -ldjbfft
endif

ifdef DBG
CXXFLAGS += -g -ggdb -O0
LDFLAGS  += -g -ggdb -O0
else
CXXFLAGS += -O3
LDFLAGS  += -O3 -Wl,--retain-symbols-file,retain-sym
endif

### The object files (add further files here):

OBJS = $(PLUGIN).o dvddev.o player-dvd.o control-dvd.o dvdspu.o     \
	           ca52.o i18n.o setup-dvd.o 

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

# Dependencies:

MAKEDEP = g++ -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Targets:

all: libvdr-$(PLUGIN).so

libvdr-$(PLUGIN).so: $(OBJS) retain-sym
	$(CXX) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(APIVERSION)

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) --exclude SCCS $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution archive created as $(PACKAGE).tgz


retain-sym:
	echo "VDRPluginCreator" > retain-sym

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* retain-sym *~
