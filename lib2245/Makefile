# Basic stuff
LIBS2245_NAME ?= S2245ImageReProcess

# Destination directories
DESTDIR ?=
LIBDIR ?= $(DESTDIR)/usr/local/lib

# Tools
CC ?= $(CROSS_COMPILE)gcc
CPPCHECK ?= cppcheck
MKDIR ?= mkdir
INSTALL ?= install
RM ?= rm

# Flags
CFLAGS += -Wall -Wextra -g -Os -std=c99 -D_FORTIFY_SOURCE=2 -fPIC --no-strict-overflow # -Wconversion
#LDFLAGS +=
#CPPFLAGS +=
CFLAGS += -funit-at-a-time

ifneq (,$(findstring mingw,$(CC)))
SUFFIX=dll
else
SUFFIX=so
endif

# Build stuff
SOURCES = libS2245ImageReProcess.c

# And now the rules!
.PHONY: clean all install cppcheck

all: lib$(LIBS2245_NAME).$(SUFFIX)

cppcheck:
	$(CPPCHECK) -q -v --std=c99 --enable=all -I/usr/include $(CPPFLAGS) $(SOURCES)

install:
	$(MKDIR) -p $(LIBDIR)
	$(INSTALL) -o root -m 755 lib$(LIBS2245_NAME).$(SUFFIX) $(LIBDIR)
clean:
	$(RM) -f lib$(LIBS6145_NAME).$(SUFFIX) *.o

lib$(LIBS2245_NAME).$(SUFFIX):  $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) -g -shared -o $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
