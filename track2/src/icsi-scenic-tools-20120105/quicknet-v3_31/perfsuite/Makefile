# $Header: /u/drspeech/repos/quicknet2/perfsuite/Makefile.in,v 1.27 2006/01/05 20:51:20 davidj Exp $
# Makefile.in for quicknet - a 3 layer MLP library for SPERT
# davidj - Mon Jun  5 11:18:46 1995


# Standard things we need

.SUFFIXES: 
.SUFFIXES: .c .cc .o .lo .s .dis

SHELL=/bin/sh

# Details of our host and target

build_alias = 
host_alias = 

srcdir = .
top_srcdir = ..
builddir = .
top_builddir = ..


# Installation stuff

prefix = /usr/local
exec_prefix = ${prefix}
libdir = $(exec_prefix)/lib
includedir = $(exec_prefix)/include

# Programs we use

SHELL = /bin/bash
CVS = cvs
CC = gcc
CXX = g++
LD = $(CXX)
AR = @AR@
RANLIB = ranlib
LIBTOOL = $(SHELL) $(top_builddir)/libtool
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
RUN =
RUNFLAGS = 

# Tool arguments

CPPFLAGS = 
CFLAGS = -g -O2
CXXFLAGS = -g -O2
LDFLAGS = 
LIBS = -lpthread -lm 
INCS = 

cflags = -I$(top_builddir) -I$(top_srcdir) $(CFLAGS) $(CPPFLAGS)
cxxflags = -I$(top_builddir) -I$(top_srcdir) $(CXXFLAGS) $(CPPFLAGS)
ldflags = $(LDFLAGS)
libs = $(LIBS)
compile.c = $(CC) $(cflags)
compile.cc = $(CXX) $(cxxflags)
run = $(RUN) $(RUNFLAGS)

################################################################
# Implicit rules
################################################################

.c.o:
	$(compile.c) -c $<

.cc.o:
	$(compile.cc) -c $<

################################################################
# The default target
################################################################

default: all


################################################################
# Variables we accumulate into
################################################################

all_srcs =
all_objs =
all_hdrs =
all_libs =
all_progs =
garbage =

################################################################
# A program for measuring the performance of an MLP
################################################################

libfile = ../libquicknet3.a 

MLP3_perf.o: $(srcdir)/MLP3_perf.cc
	$(compile.cc) -c  $(srcdir)/MLP3_perf.cc -o MLP3_perf.o

MLP3_perf : MLP3_perf.o $(libfile)
	$(LD) $(ldflags) -o MLP3_perf MLP3_perf.o $(libfile) $(libs)

# PERFFLAGS = -i 1 -o 1
PERFFLAGS =
NETTYPE = x
BUNCH = 256
REPEAT = 1000
THREADS = 0

all_srcs += MLP3_perf.cc
all_objs += MLP3_perf.o
all_progs += MLP3_perf

perfprog = ./MLP3_perf
perfflags = $(PERFFLAGS)

perfn:
	@$(run) $(perfprog) $(perfflags)  153 4000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 8000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 16000 56 512 $(THREADS)

perfit:
	@$(run) $(perfprog) $(perfflags)  116 400 48 16 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  116 1000 48 16 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  116 1000 48 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  116 4000 48 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  116 8000 48 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 1000 56 16 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 1000 56 64 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 1000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 4000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 8000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 1000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 4000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 8000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 16000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 32000 56 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  153 4000 2 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 8000 2 512 $(THREADS)
	@$(run) $(perfprog) $(perfflags)  351 4000 1000 512 $(THREADS)

perfit16:
	@for i in 0 2 4 8 16; do \
		$(MAKE) THREADS=$$i perfit; \
	done

perfit8:
	@for i in 0 2 4 8; do \
		$(MAKE) THREADS=$$i perfit; \
	done

perfit4:
	@for i in 0 2 3 4; do \
		$(MAKE) THREADS=$$i perfit; \
	done

perfit2:
	@for i in 0 2; do \
		$(MAKE) THREADS=$$i perfit; \
	done

perf: $(perfprog) Makefile
	@$(run) $(perfprog) $(perfflags)  153 200 56 16 0 500
	@$(run) $(perfprog) $(perfflags)  -f 153 200 56 16 0 500

	@$(run) $(perfprog) $(perfflags)  153 4000 56 512 0 20
	@$(run) $(perfprog) $(perfflags)  -f 153 4000 56 512 0 20

	@$(run) $(perfprog) $(perfflags)  153 4000 56 512 2 50
	@$(run) $(perfprog) $(perfflags)  -f 153 4000 56 512 2 50

perf2: $(perfprog) Makefile
	@$(run) $(perfprog) $(perfflags) 32 32 32 $(BUNCH) $(THREADS) $(REPEAT) 
	@$(run) $(perfprog) $(perfflags) 64 64 64 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) 128 128 128 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) 256 256 256 $(BUNCH) $(THREADS) $(REPEAT)
#	@$(run) $(perfprog) $(perfflags) 512 512 512 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) 153 200 56 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) 153 400 56 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) 342 1000 56 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) 342 2000 56 $(BUNCH) $(THREADS) $(REPEAT)
	@echo
	@$(run) $(perfprog) $(perfflags) -f 32 32 32 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 64 64 64 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 128 128 128 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 256 256 256 $(BUNCH) $(THREADS) $(REPEAT)
#	@$(run) $(perfprog) $(perfflags) -f 512 512 512 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 153 200 56 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 153 400 56 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 342 1000 56 $(BUNCH) $(THREADS) $(REPEAT)
	@$(run) $(perfprog) $(perfflags) -f 342 2000 56 $(BUNCH) $(THREADS) $(REPEAT)


################################################################
# Cleanup etc
################################################################

garbage += $(all_objs) $(all_libs) $(all_progs) TAGS

all: $(all_progs) $(all_libs)

clean:
	rm -f $(garbage)

################################################################
# Configuration stuff
################################################################

config_srcs = Makefile.in
config_outfiles =

Makefile: Makefile.in ../config.status
	cp -p Makefile Makefile.old
	( cd ..; ./config.status )
	@if cmp -s Makefile Makefile.old 2>/dev/null; then \
		rm -f Makefile.old; \
		true; \
	else \
		rm -f Makefile.old; \
		echo "### Makefile changed - rerun make ###"; \
		exit 1; \
	fi
     
################################################################
# Building a distribution
################################################################

dist_files = $(all_srcs) $(all_hdrs) \
	$(config_srcs) $(config_outfiles) # ChangeLog

# The name of the distribution
distname = quicknet-perfsuite-$(VERSION)

# The file name of the compressed tar fiel
distfile = $(distname).tar.gz

# The directory in which the distribution is built
distdir = $$r/$(distname)

# Build a distribution
dist: $(distfile)

# Build the file that contains the distribution
$(distfile): disttree
	rm -f $(distfile); \
	tar cvf - $(distname) | gzip >$(distfile)

# Build a directory tree that can be tared and gzipped to form the distribution
disttree: Makefile $(dist_files)
	r=`pwd`; export r; \
	rm -fr $(distdir); \
	mkdir $(distdir); \
	chmod a=rwx $(distdir); \
	for i in $(dist_files); do \
		file=`basename $$i`; \
		cp -p $(srcdir)/$$file $(distdir)/$$file || exit 1; \
	done
