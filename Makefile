# 
# Makefile
# Copyright 2001 Julian E. C. Squires (tek@wiw.org)
# This program comes with ABSOLUTELY NO WARRANTY.
# $Id$
# 
#

DENTATADIR=/usr/local

DEFINES=-DTHIRTYTWOBIT
CPPFLAGS=$(DEFINES) -I$(DENTATADIR)/include -Iinclude
CFLAGS=-Wall -pedantic -O6 -g
LDFLAGS=-L$(DENTATADIR)/lib -ldentata -llua -ldb3
SPRCOMP=./sprcomp
TMAPCOMP=./tmapcomp
LUAC=luac

###
# For X11:
#CLILDFLAGS:=-L/usr/X11R6/lib -lXext -lX11 $(LDFLAGS)
# For SVGAlib:
CLILDFLAGS:=-lvga $(LDFLAGS)

VPATH=src datagen include

COMMONDATA=

CLIPROG=fobwart
SERVPROG=fobserv
UTILS=sprcomp tmapcomp edlogin edobject edroom

.phony: default

all: $(UTILS) $(CLIPROG) $(SERVPROG)

default: $(UTILS) $(CLIPROG) $(SERVPROG)

include Makefile.client Makefile.server Makefile.utils

data/%.spr: %.spd $(SPRCOMP)
	$(SPRCOMP) $<

data/%.map: %.mpd $(TMAPCOMP)
	$(TMAPCOMP) $<

data/%.luc: %.lua
	$(LUAC) -o $@ $<

clean:
	rm -f *.o *~ src/*~ include/*~ datagen/*~

distclean: clean
	rm -f $(CLIPROG) $(SERVPROG) $(UTILS)

# EOF Makefile
