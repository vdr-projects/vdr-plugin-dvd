#
# Makefile for a Video Disk Recorder plugin
# Adapted to the new VDR makefile environment by Stefan Hofmann
#
# $Id$

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN = dvd

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell pkg-config --variable=$(1) vdr || pkg-config --variable=$(1) ../../../vdr.pc))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
PLGCFG = $(call PKGCFG,plgcfg)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags)

# Avoid plugin-specific compiler warnings
override CXXFLAGS += -Wno-unused-result
override CXXFLAGS += -Wno-unused-but-set-variable
override CXXFLAGS += -Wno-write-strings

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

### The version number of VDR's plugin API:
APIVERSION = $(call PKGCFG,apiversion)
### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

INCLUDES += -I$(call PKGCFG,incdir)

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'
DEFINES += -D__STDC_LIMIT_MACROS

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

ifdef RESUMEDIR
  DEFINES += -DRESUMEDIR=\"$(RESUMEDIR)\"
endif

### The object files (add further files here):

OBJS = $(PLUGIN).o dvddev.o player-dvd.o control-dvd.o dvdspu.o     \
	           ca52.o i18n.o setup-dvd.o 

### The main target:

all: $(SOFILE) i18n

### Implicit rules:

%.o: %.c
	@echo CC $@
	$(Q)$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	@echo MO $@
	$(Q)msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	@echo GT $@
	$(Q)xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<marco@lordzodiac.de>' -o $@ `ls $^`

%.po: $(I18Npot)
	@echo PO $@
	$(Q)msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	@echo IN $@
	$(Q)install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): $(OBJS) retain-sym
	@echo LD $@
	$(Q)$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install-lib: $(SOFILE)
	@echo IN $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)
	$(Q)install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install: install-lib install-i18n

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

retain-sym:
	echo "VDRPluginCreator" > retain-sym

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~

