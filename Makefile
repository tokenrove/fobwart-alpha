# 
# Makefile
# Copyright 2001 Julian E. C. Squires (tek@wiw.org)
# This program comes with ABSOLUTELY NO WARRANTY.
# $Id$
# 
#

DENTATADIR=/usr/local

DEFINES=-DTHIRTYTWOBIT
CPPFLAGS=$(DEFINES) -I$(DENTATADIR)/include
CFLAGS=-Wall -pedantic -O6 -g
LDFLAGS=-L$(DENTATADIR)/lib -ldentata -llua -ldb2

###
# For X11:
#CLILDFLAGS:=-L/usr/X11R6/lib -lXext -lX11 $(LDFLAGS)
# For SVGAlib:
CLILDFLAGS:=-lvga $(LDFLAGS)

VPATH=src
CLIOBJS=main.o local.o localdat.o physics.o event.o decor.o audiounix.o \
        evsk.o network.o netcommon.o datacommon.o
CLIDATA=
CLIPROG=fobwart
CLIHEADERS=fobwart.h

SERVOBJS=fobserv.o netcommon.o datacommon.o evsk.o
SERVDATA=
SERVPROG=fobserv
SERVHEADERS=fobnet.h

UTILS=sprcomp tmapcomp foblogindb

.phony: default

default: $(CLIPROG) $(SERVPROG) $(UTILS)

$(CLIPROG): $(CLIOBJS) $(CLIDATA)
	$(CC) $(CFLAGS) -o $@ $(CLIOBJS) $(CLILDFLAGS)

$(SERVPROG): $(SERVOBJS) $(SERVDATA)
	$(CC) $(CFLAGS) -o $@ $(SERVOBJS) $(LDFLAGS)

$(UTILS): %: %.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(CLIOBJS): $(CLIHEADERS)
$(SERVOBJS): $(SERVHEADERS)

clean:
	rm -f $(CLIOBJS) $(SERVOBJS) *~ src/*~

distclean: clean
	rm -f $(CLIPROG) $(SERVPROG)

# EOF Makefile
