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
LDFLAGS=-L$(DENTATADIR)/lib -ldentata -llua

###
# For X11:
#LDFLAGS:=-L/usr/X11R6/lib -lXext -lX11 $(LDFLAGS)
# For SVGAlib:
LDFLAGS:=-lvga $(LDFLAGS)

VPATH=src
CLIOBJS=main.o local.o localdat.o physics.o event.o decor.o audiounix.o \
        evsk.o network.o
CLIDATA=
CLIPROG=fobwart
CLIHEADERS=fobwart.h

SERVOBJS=fobserv.o network.o
SERVDATA=
SERVPROG=fobserv
SERVHEADERS=fobnet.h

.phony: default

default: $(CLIPROG) $(SERVPROG)

$(CLIPROG): $(CLIOBJS) $(CLIDATA)
	$(CC) $(CFLAGS) -o $@ $(CLIOBJS) $(LDFLAGS)

$(SERVPROG): $(SERVOBJS) $(SERVDATA)
	$(CC) $(CFLAGS) -o $@ $(SERVOBJS) $(LDFLAGS)

$(CLIOBJS): $(CLIHEADERS)
$(SERVOBJS): $(SERVHEADERS)

clean:
	rm -f $(CLIOBJS) $(SERVOBJS) *~ src/*~

distclean: clean
	rm -f $(CLIPROG) $(SERVPROG)

# EOF Makefile
