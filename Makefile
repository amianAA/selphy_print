### Makefile for selphy_print and family
#
#  SPDX-License-Identifier: GPL-3.0+
#

# Windows stuff needs to end in .exe
ifneq (,$(findstring mingw,$(CC)))
MINGW_LIBS=/usr/x86_64-w64-mingw32/sys-root/mingw/bin
EXEC_SUFFIX=.exe
LIB_SUFFIX=dll
else
EXEC_SUFFIX=
LIB_SUFFIX=so
endif

# Set to disable looking for gutenprint for the backend name..
#NO_GUTENPRINT = 1

# pkg-config extra stuff
#PKG_CONFIG_EXTRA = --with-path=/usr/local/lib/pkgconfig  # only works with pkgconf, not pkg-config!

# Base executable name
EXEC_NAME ?= dyesub_backend$(EXEC_SUFFIX)

# More stuff..
CPUS ?= $(shell nproc)
REVISION ?= -$(shell if [ -n "${BUILD_NUMBER}" ] ; then echo -n "b${BUILD_NUMBER}"- ; fi ; if [ -d .git ] ; then echo -n "g" ; git rev-parse --short HEAD; else echo -n "NONE" ; fi)

# Destination directories (rely on CUPS & Gutenprint to tell us where)
GP_PREFIX ?= $(shell pkg-config $(PKG_CONFIG_EXTRA) --variable=prefix gutenprint)
PREFIX ?=
CUPS_BACKEND_DIR ?= $(PREFIX)$(shell cups-config --serverbin)/backend
CUPS_DATA_DIR ?= $(PREFIX)$(shell cups-config --datadir)
LIB_DIR ?= $(PREFIX)/usr/local/lib/

ifeq ($(GP_PREFIX),)
GP_PREFIX=/usr/local
endif

ifneq (,$(findstring mingw,$(CC)))
BACKEND_DATA_DIR ?= backend_data
else
BACKEND_DATA_DIR ?= $(GP_PREFIX)/share/gutenprint/backend_data
endif

# Figure out what the backend name needs to be
ifeq ($(NO_GUTENPRINT),)
GUTENPRINT_INCLUDE := $(shell pkg-config $(PKG_CONFIG_EXTRA) --variable=includedir gutenprint)/gutenprint
ifneq (/gutenprint,$(GUTENPRINT_INCLUDE))
GUTENPRINT_MAJOR := $(shell grep 'define STP_MAJOR' $(GUTENPRINT_INCLUDE)/gutenprint-version.h | tr -d '()\t' | cut -c33- )
GUTENPRINT_MINOR := $(shell grep 'define STP_MINOR' $(GUTENPRINT_INCLUDE)/gutenprint-version.h | tr -d '()\t' | cut -c33- )
BACKEND_NAME ?= gutenprint$(GUTENPRINT_MAJOR)$(GUTENPRINT_MINOR)+usb
endif
endif

# For Gutenprint 5.2, use old URI scheme
ifneq ($(GUTENPRINT_MINOR),3)
OLD_URI := -DOLD_URI=1
endif

# Fallthrough
BACKEND_NAME ?= gutenprint5X+usb

# Libraries:
LIBS6145_NAME = lib6145/libS6145ImageReProcess.$(LIB_SUFFIX)
LIBS6145_SOURCES = lib6145/libS6145ImageReProcess.c
LIBS2245_NAME = lib2245/libS2245ImageReProcess.$(LIB_SUFFIX)
LIBS2245_SOURCES = lib2245/libS2245ImageReProcess.c
LIB70X_NAME ?= lib70x/libMitsuD70ImageReProcess.$(LIB_SUFFIX)
LIB70X_SOURCES = lib70x/libMitsuD70ImageReProcess.c

LIBRARIES = $(LIBS6145_NAME) $(LIB70X_NAME) $(LIBS2245_NAME)

# Tools
CC ?= $(CROSS_COMPILE)gcc
LD ?= $(CROSS_COMPILE)ld
CPPCHECK ?= cppcheck
MKDIR ?= mkdir
INSTALL ?= install
LN ?= ln
RM ?= rm
CP ?= cp
TAR ?= tar
ZIP ?= zip

# Try and pretty things up a bit.
Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif

# Flags
CFLAGS += -Wall -Wextra -Wformat-security -funit-at-a-time -g -Og -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -std=c99 -D_POSIX_C_SOURCE=200809L # -Wconversion
LDFLAGS += $(shell pkg-config $(PKG_CONFIG_EXTRA) --libs libusb-1.0)
CPPFLAGS += $(shell pkg-config $(PKG_CONFIG_EXTRA) --cflags libusb-1.0)
# CPPFLAGS += -DLIBUSB_PRE_1_0_10
CPPFLAGS += -DURI_PREFIX=\"$(BACKEND_NAME)\" $(OLD_URI) -DCORRTABLE_PATH=\"$(BACKEND_DATA_DIR)\"
LIBLDFLAGS = -g -shared

# List of backends
BACKENDS = canonselphy canonselphyneo dnpds40 hiti kodak605 kodak1400 kodak6800 magicard mitsu70x mitsu9550 mitsud90 mitsup95d shinkos1245 shinkos2145 shinkos6145 shinkos6245 sonyupd sonyupdneo

# List of data files
DATAFILES = $(wildcard hiti_data/*bin) $(wildcard lib70x/data/*raw) \
	$(wildcard lib70x/data/*cpc) $(wildcard lib70x/data/*lut) \
	$(wildcard lib70x/data/*dat) $(wildcard lib70x/data/*csv)

# For the s6145, mitsu70x, mitsud90, and mitsu9550 backends
ifneq (,$(findstring mingw,$(CC)))
CPPFLAGS += -DUSE_LTDL
LDFLAGS += -lltdl
else
CPPFLAGS += -DUSE_DLOPEN
LDFLAGS += -ldl
endif

# Testing verbosity?
STP_VERBOSE=0

# Linking..
LDFLAGS += $(CFLAGS) $(CPPFLAGS)

# Build stuff
DEPS += backend_common.h
SOURCES = backend_common.c backend_sinfonia.c backend_mitsu.c $(addsuffix .c,$(addprefix backend_,$(BACKENDS)))

# Dependencies for sinfonia backends..
SINFONIA_BACKENDS = sinfonia kodak605 kodak6800 shinkos1245 shinkos2145 shinkos6145 shinkos6245
SINFONIA_BACKENDS_O = $(addsuffix .o,$(addprefix backend_,$(SINFONIA_BACKENDS)))
MITSU_BACKENDS = mitsu mitsu70x mitsu9550 mitsud90
MITSU_BACKENDS_O = $(addsuffix .o,$(addprefix backend_,$(MITSU_BACKENDS)))

# Datafiles
DATAFILES_TGT = $(addprefix datafiles/,$(notdir $(DATAFILES)))
DATAFILES_TMP = datafiles

# And now the rules!
.PHONY: config clean all install cppcheck
all: config $(EXEC_NAME) $(BACKENDS) libraries $(DATAFILES_TMP) $(DATAFILES_TGT)

config:
	@echo
	@echo "     Configuration Summary"
	@echo
ifeq ($(NO_GUTENPRINT),)
	@echo "   GUTENPRINT VER:    $(GUTENPRINT_MAJOR).$(GUTENPRINT_MINOR)"
endif
	@echo "   CUPS BACKEND DIR:  $(CUPS_BACKEND_DIR)"
	@echo "   BACKEND NAME:      $(BACKEND_NAME)"
	@echo "   CUPS DATA DIR:     $(CUPS_DATA_DIR)"
	@echo "   BACKEND DATA DIR:  $(BACKEND_DATA_DIR)"
	@echo "   LIBDIR:            $(LIB_DIR)"
	@echo

libraries: $(LIBRARIES)
#	$(MAKE) -C lib70x $@

%.o: %.c $(DEPS)
	@$(E) "      CC  " $@
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(EXEC_NAME): $(SOURCES:.c=.o) $(DEPS)
	@$(E) "    CCLD  " $@
	$(Q)$(CC) -o $@ $(SOURCES:.c=.o) $(LDFLAGS)

$(BACKENDS): $(EXEC_NAME)
	@$(E) "      LN  " $@
	$(Q)$(LN) -sf $(EXEC_NAME) $@

# Metrics and sanity-checks
sloccount:
	sloccount *.[ch] lib*/*.[ch] *.pl

cppcheck:
	$(CPPCHECK) -q -v --std=c99 --enable=all --suppress=variableScope --suppress=selfAssignment --suppress=unusedStructMember -I. -I/usr/include $(CPPFLAGS) $(SOURCES) $(LIB70X_SOURCES) $(LIBS6145_SOURCES)

# Test-related stuff
$(DATAFILES_TMP)/%: hiti_data/%
	@$(E) "      LN  " $@
	$(Q)$(LN) -sf ../$< $@

$(DATAFILES_TMP)/%: lib70x/data/%
	@$(E) "      LN  " $@
	$(Q)$(LN) -sf ../$< $@

$(DATAFILES_TMP):
	@$(E) "   MKDIR  " $@
	$(Q)$(MKDIR) -p datafiles

test: all
	LD_LIBRARY_PATH=lib70x:lib6145:$(LD_LIBRARY_PATH) STP_VERBOSE=$(STP_VERBOSE) STP_PARALLEL=$(CPUS) CORRTABLE_PATH=$(DATAFILES_TMP) ./regression.pl regression.csv

test_%: all
	LD_LIBRARY_PATH=lib70x:lib6145:$(LD_LIBRARY_PATH) STP_VERBOSE=$(STP_VERBOSE) STP_PARALLEL=$(CPUS) CORRTABLE_PATH=$(DATAFILES_TMP) ./regression.pl regression.csv $(subst test_,,$@)

testgp: all
	LD_LIBRARY_PATH=lib70x:lib6145:$(LD_LIBRARY_PATH) STP_VERBOSE=$(STP_VERBOSE) STP_PARALLEL=$(CPUS) CORRTABLE_PATH=$(DATAFILES_TMP) ./regression-gp.pl regression-gp.csv

testgp_%: all
	LD_LIBRARY_PATH=lib70x:lib6145:$(LD_LIBRARY_PATH) STP_VERBOSE=$(STP_VERBOSE) STP_PARALLEL=$(CPUS) CORRTABLE_PATH=$(DATAFILES_TMP) ./regression-gp.pl regression-gp.csv $(subst testgp_,,$@)

# Install and cleanup

install: all
	$(MKDIR) -p $(CUPS_BACKEND_DIR)
	$(INSTALL) -o root -m 700 $(EXEC_NAME) $(CUPS_BACKEND_DIR)/$(BACKEND_NAME)
	$(INSTALL) -o root -m 755 $(LIBRARIES) $(LIB_DIR)
	$(MKDIR) -p $(CUPS_DATA_DIR)/usb
	$(INSTALL) -o root -m 644 blacklist $(CUPS_DATA_DIR)/usb/net.sf.gimp-print.usb-quirks
	$(MKDIR) -p $(BACKEND_DATA_DIR)
	$(INSTALL) -o root -m 644 $(DATAFILES_TMP)/* $(BACKEND_DATA_DIR)

clean:
	@$(E) "   CLEAN  " all
	$(Q)$(RM) $(EXEC_NAME) $(BACKENDS) $(LIBRARIES) $(SOURCES:.c=.o) $(LIBS6145_SOURCES:.c=.o) $(LIB70X_SOURCES:.c=.o)
	$(Q)$(RM) -Rf $(DATAFILES_TMP)

release:
	$(RM) -Rf selphy_print$(REVISION)
	$(MKDIR) -p selphy_print$(REVISION)
	$(CP) -a COPYING README selphy_print$(REVISION)
ifeq (,$(findstring mingw,$(CC)))
	$(CP) -a *.c *.h Makefile blacklist lib6145 lib70x hiti_data selphy_print$(REVISION)
	$(TAR) -czvf selphy_print-src$(REVISION).tar.gz selphy_print$(REVISION)
else
	$(CP) -a $(EXEC_NAME) $(LIBRARIES) selphy_print$(REVISION)
	$(CP) -a $(MINGW_LIBS)/libltdl*.dll $(MINGW_LIBS)/libusb*.dll selphy_print$(REVISION)
	$(MKDIR) -p selphy_print$(REVISION)/$(BACKEND_DATA_DIR)
	$(CP) -a hiti_data/* lib70x/data/* selphy_print$(REVISION)/$(BACKEND_DATA_DIR)
	$(CP) -a lib70x/README selphy_print$(REVISION)/README-lib70x
	$(CP) -a lib6145/README selphy_print$(REVISION)/README-lib6145
	$(ZIP) -r selphy_print-mingw$(REVISION).zip selphy_print$(REVISION)
endif
	$(RM) -Rf selphy_print$(REVISION)

# Backend-specific joy:
$(SINFONIA_BACKENDS_O): backend_sinfonia.h
$(MITSU_BACKENDS_O): backend_mitsu.h

# Library joy:
%.$(LIB_SUFFIX): CFLAGS += -fPIC --no-strict-overflow

$(LIB70X_NAME):  $(LIB70X_SOURCES:.c=.o)
	@$(E) "    CCLD  " $@
	$(Q)$(CC) $(LIBLDFLAGS) -o $@ $^

$(LIBS6145_NAME):  $(LIBS6145_SOURCES:.c=.o)
	@$(E) "    CCLD  " $@
	$(Q)$(CC) $(LIBLDFLAGS) -o $@ $^

$(LIBS2245_NAME):  $(LIBS2245_SOURCES:.c=.o)
	@$(E) "    CCLD  " $@
	$(Q)$(CC) $(LIBLDFLAGS) -o $@ $^
