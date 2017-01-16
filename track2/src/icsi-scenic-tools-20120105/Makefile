# $Header: /n/abbott/da/drspeech/src/SPRACHcore/RCS/Makefile.in,v 1.10 2001/12/04 16:47:47 dpwe Exp $
# 
# Root Makefile for icsi-scenic-tools distribution
#
# Makefile.  Generated from Makefile.in by configure.
#

# Standard things we need

.SUFFIXES: 
.SUFFIXES: .c .o

SHELL=/bin/sh

# Installation stuff.

prefix = /usr/local
exec_prefix = /usr/local
libdir = $(exec_prefix)/lib
bindir = $(exec_prefix)/bin
includedir = $(exec_prefix)/include
docdir = $(prefix)/doc
mandir = $(prefix)/man
man1dir = $(mandir)/man1
man2dir = $(mandir)/man2
man3dir = $(mandir)/man3
man4dir = $(mandir)/man4
man5dir = $(mandir)/man5
man6dir = $(mandir)/man6
man7dir = $(mandir)/man7
man8dir = $(mandir)/man8
man9dir = $(mandir)/man9

srcdir = .
subdirs =  quicknet-v3_31 dpwelib-1.01 rasta feacalc-0.92 feacat-1.02 pfile_utils-v0_52

# Programs we use.

CC = gcc
CXX = g++
LD = $(CC)
AR = /usr/bin/ar
RANLIB = /usr/bin/ranlib
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_LIB = $(INSTALL_DATA)

# Tool arguments.

CFLAGS = -O
CXXFLAGS = -O
LDFLAGS = 
LIBS = 


subdir_flags = \
	"SHELL=$(SHELL)" \
	"CC=$(CC)" \
	"CXX=$(CXX)" \
	"RANLIB=$(RANLIB)" \
	"INSTALL=$(INSTALL)" \
	"INSTALL_DATA=$(INSTALL_DATA)" \
	"INSTALL_PROGRAM=$(INSTALL_PROGRAM)" \
	"CFLAGS=$(CFLAGS)" \
	"CXXFLAGS=$(CXXFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" \
	"exec_prefix=$(exec_prefix)" \
	"prefix=$(prefix)"

################################################################
# Building the packages.
################################################################

default: all

all:
	for i in $(subdirs); do \
	( \
	    if [ -f $$i/Makefile ]; then \
		cd $$i; \
		$(MAKE) $(subdir_flags) $@; \
	    else \
		true; \
	    fi \
	) \
	done


################################################################
# Installing the packages.
################################################################

install:
	for i in $(subdirs); do \
	( \
	    if [ -f $$i/Makefile ]; then \
		cd $$i; \
		$(MAKE) $(subdir_flags) $@; \
	    else \
		true; \
	    fi \
	) \
	done




installdirs: $(srcdir)/mkinstalldirs
	$(srcdir)/mkinstalldirs $(bindir) $(libdir) $(includedir) \
				$(mandir) $(man1dir) $(man3dir) $(docdir)


################################################################
# Misc targets.
################################################################

clean:
	for i in $(subdirs); do \
	( \
		cd $$i; \
		$(MAKE) $(subdir_flags) $@; \
	) \
	done

# No need to make distclean invoke clean, since all the "clean" target 
# at this level does is "make clean" in all subtargets, and "make distclean" 
# in the subtargets will do that anyway.
distclean: 
	for i in $(subdirs); do \
	( \
		cd $$i; \
		$(MAKE) $(subdir_flags) $@; \
	) \
	done
	rm -f Makefile config.log config.status config.cache

$(srcdir)/configure: $(srcdir)/configure.in
	cd $(srcdir) && autoconf

################################################################
# Putting together a distribution.
################################################################

config_srcs = \
	$(srcdir)/configure.in \
	$(srcdir)/Makefile.in \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/install-sh \
	$(srcdir)/mkinstalldirs
config_outfiles = \
	$(srcdir)/configure
all_docs = \
	$(srcdir)/README

dist_files =  $(all_docs) \
	$(config_srcs) $(config_outfiles) \
	$(srcdir)/ChangeLog

# The name of the distribution
name = icsi-scenic-tools
version = 20120105
distname = $(name)-$(version)

# The file name of the compressed tar file
distfile = $(distname).tar.gz

# The directory in which the distribution is built
# Note that shell var $$r must be defined to use this macro
distdir = $$r/$(distname)

# Build a distribution
dist: $(distfile)

# Build the file that contains the distribution
$(distfile): disttree
	rm -f $(distfile); \
	tar cvf - $(distname) | gzip >$(distfile); \
	rm -fr $(distdir)

# Build a directory tree that can be tared and gzipped to form the distribution
disttree: Makefile $(dist_files)
	r=`pwd`; export r; \
	rm -fr $(distdir); \
	mkdir $(distdir); \
	chmod a=rwx $(distdir); \
	for i in $(dist_files); do \
		file=`basename $$i`; \
		cp -p $$i $(distdir)/$$file || exit 1; \
	done; \
	for i in $(subdirs); do \
	( \
		cd $$i; \
		$(MAKE) distdir="$(distdir)/$$i" disttree \
	) \
	done


