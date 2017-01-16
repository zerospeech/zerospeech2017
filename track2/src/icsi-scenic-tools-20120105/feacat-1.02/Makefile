# $Header: /u/drspeech/repos/feacat/Makefile.in,v 1.13 2006/03/11 01:40:00 davidj Exp $
#
# Makefile.in for feacat
# 
# Universal utterance-delimited feature interconversion
#
# 1998jun09 dpwe@icsi.berkeley.edu
#

# Stuff from autoconf

# Default top-level directories in which to install architecture-
# specific files (exec_prefix) and machine-independent files such
# as scripts (prefix).  The values specified here may be overridden
# at configure-time with the --exec-prefix and --prefix options
# to the "configure" script.
prefix =		/usr/local
exec_prefix =		/usr/local

# Used to add prefixes and suffices to program names
transform = s,x,x,

# The following definition can be set to non-null for special systems
# like AFS with replication.  It allows the pathnames used for installation
# to be different than those used for actually reference files at
# run-time.  INSTALL_ROOT is prepended to $prefix and $exec_prefix
# when installing files.
INSTALL_ROOT =

# Directory in which to install the binary library:
#LIB_INSTALL_DIR =	$(INSTALL_ROOT)$(exec_prefix)/lib

# Where to install executables
BIN_INSTALL_DIR =	$(INSTALL_ROOT)$(exec_prefix)/bin

# Where to install man pages for executables
MAN1_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/man/man1
MAN3_INSTALL_DIR =	$(INSTALL_ROOT)$(prefix)/man/man3

# To change the compiler switches, for example to change from -O
# to -g, change the following line:
CFLAGS = -O
CXXFLAGS = -O

# Some versions of make, like SGI's, use the following variable to
# determine which shell to use for executing commands:
SHELL =		/bin/sh

#----------------------------------------------------------------
# The information below is modified by the configure script when
# Makefile is generated from Makefile.in.  You shouldn't normally
# modify any of this stuff by hand.
#----------------------------------------------------------------

AC_FLAGS =		-DPACKAGE_NAME=\"feacat\" -DPACKAGE_TARNAME=\"feacat\" -DPACKAGE_VERSION=\"1.02\" -DPACKAGE_STRING=\"feacat\ 1.02\" -DPACKAGE_BUGREPORT=\"quicknet-bugs@ICSI.Berkeley.EDU\" -DPACKAGE_URL=\"\" -DHAVE_BOOL=1 -DHAVE_FSEEKO=1
srcdir =		.



#----------------------------------------------------------------

# Programs we use.

RANLIB = ranlib
INSTALL = /usr/bin/install -c
INSTALL_PROG = $(INSTALL)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_LIB = $(INSTALL_DATA)
MKINSTALLDIRS =		$(srcdir)/mkinstalldirs

# To be redefined in make command line
DFLAGS=

CC = gcc
CC_SWITCHES = $(CFLAGS) $(DFLAGS) $(IFLAGS) $(AC_FLAGS)

CXX = g++
CXX_SWITCHES = $(CXXFLAGS) $(DFLAGS) $(IFLAGS) $(AC_FLAGS)

LD = $(CXX)

compile.c = $(CC) $(CC_SWITCHES)
compile.cxx = $(CXX) $(CXX_SWITCHES)

.c.o:
	$(compile.c) -c $<

.C.o:
	$(compile.cxx) -c $<

.cc.o:
	$(compile.cxx) -c $<

config_srcs	=	configure.in Makefile.in aclocal.m4 \
			config.sub config.guess
config_utils	=	install-sh mkinstalldirs
config_outfiles	=	configure

# ------------------------------------------------------------

# Special libraries for this package
QN_INC_SPEC 	= 	-g -O2 -I/home/juan/software/icsi-scenic-tools-20120105/quicknet-v3_31
QN_LIB_SPEC 	= 	-L/home/juan/software/icsi-scenic-tools-20120105/quicknet-v3_31 -lquicknet3 -lpthread -lm

DPWELIB_INC_SPEC = 	-I/home/juan/software/icsi-scenic-tools-20120105/dpwelib-1.01
DPWELIB_LIB_SPEC = 	-L/home/juan/software/icsi-scenic-tools-20120105/dpwelib-1.01 -ldpwe 

IFLAGS		=	$(QN_INC_SPEC) $(DPWELIB_INC_SPEC)

LIBS		=	$(QN_LIB_SPEC) $(DPWELIB_LIB_SPEC) 

all: feacat labcat linecat Range segpostsum

HDRS		=	$(srcdir)/Range.H
SRCS		=	$(srcdir)/feacat.C $(srcdir)/Range.C $(srcdir)/linecat.C $(srcdir)/segpostsum.C
DOCS		=	$(srcdir)/feacat.man $(srcdir)/labcat.man $(srcdir)/linecat.man $(srcdir)/Range.man $(srcdir)/segpostsum.man $(srcdir)/README $(srcdir)/INSTALL $(srcdir)/TODO $(srcdir)/COPYING $(srcdir)/NEWS
OTHERS		=	$(srcdir)/test-feacat.tcl $(srcdir)/test-range.tcl

FOBJS	 	= 	feacat.o Range.o

LOBJS	 	= 	labcat.o Range.o

feacat: $(FOBJS)
	$(LD) -o feacat $(FOBJS) $(LIBS)

labcat: $(LOBJS)
	$(LD) -o labcat $(LOBJS) $(LIBS)

segpostsum: segpostsum.o
	$(LD) -o segpostsum segpostsum.o $(LIBS)

segpostsum.o: $(srcdir)/segpostsum.C $(HDRS)
	$(compile.cxx) -c $(srcdir)/segpostsum.C -o $@

feacat.o: $(srcdir)/feacat.C $(HDRS)
	$(compile.cxx) -c $(srcdir)/feacat.C -o $@

labcat.o: $(srcdir)/feacat.C $(HDRS)
	$(compile.cxx) -DLABCAT -c $(srcdir)/feacat.C -o $@

Range.o: $(srcdir)/Range.C $(HDRS)
	$(compile.cxx) -c $(srcdir)/Range.C -o $@

Range: $(srcdir)/Range.C $(HDRS)
	$(compile.cxx) -DMAIN $(srcdir)/Range.C -o $@ $(LIBS)

linecat.o: $(srcdir)/linecat.C $(HDRS)
	$(compile.cxx) -c $(srcdir)/linecat.C -o $@

linecat: linecat.o Range.o
	$(LD) -o linecat linecat.o Range.o $(LIBS)



#### Utility targets #####

test-cats: feacat labcat linecat Range
	r=`pwd`; export r; cd $(srcdir)/testdata; $(srcdir)/test-feacat.tcl $$r/feacat

test-range: Range
	$(srcdir)/test-range.tcl

test: test-cats test-range

install: feacat labcat linecat Range segpostsum $(srcdir)/feacat.man $(srcdir)/labcat.man $(srcdir)/labcat.man $(srcdir)/Range.man $(srcdir)/segpostsum.man
	@$(MKINSTALLDIRS) $(BIN_INSTALL_DIR) $(MAN1_INSTALL_DIR)
	@echo "Installing programs"
	for i in feacat labcat linecat Range segpostsum; do \
		$(INSTALL_PROG)  \
			$$i $(BIN_INSTALL_DIR)/`echo $$i|sed '$(transform)'`; \
	done
	@echo "Installing man1 pages"
	for i in feacat labcat linecat segpostsum; do \
		$(INSTALL_DATA)  \
			$(srcdir)/$$i.man \
			$(MAN1_INSTALL_DIR)/`echo $$i|sed '$(transform)'`.1; \
	done
	@echo "Installing man3 page"
	$(INSTALL_DATA) $(srcdir)/Range.man $(MAN3_INSTALL_DIR)/Range.3

rmbak:
	-rm -f *~ *.bak core \#*

clean: rmbak
	-rm -f *.o feacat labcat linecat Range segpostsum

realclean: clean

distclean: clean
	rm -f Makefile config.status config.log config.cache

depend:
	makedepend -- $(CC_SWITCHES) -- $(SRCS)

$(srcdir)/configure: $(srcdir)/configure.in
	cd $(srcdir) && autoconf
	./config.status --recheck

Makefile: $(srcdir)/Makefile.in $(srcdir)/configure
	$(SHELL) config.status

#################################################################
## Package distribution.
#################################################################


# The name of the distribution
name = feacat
version = 1.02
distname = $(name)-$(version)

# The file name of the compressed tar file
distfile = $(distname).tar.gz

# Where to build the disttree
distroot = .

# The directory in which the distribution is built
distdir = $(distroot)/$(distname)

# Build a distribution
dist: $(distfile)

# Build the file that contains the distribution
$(distfile): disttree
	rm -f $(distfile); \
	(cd $(distroot); tar cvf - $(distname) ) | gzip >$(distfile)

dist_files =	$(HDRS) $(SRCS) $(DOCS) $(OTHERS) \
		$(config_srcs) $(config_utils) $(config_outfiles)

# Build a directory tree that can be tared and gzipped to form the distribution
disttree: $(dist_files)
	rm -fr $(distdir); \
	mkdir $(distdir); \
	chmod a=rwx $(distdir); \
	for i in $(dist_files) ; do \
		file=`echo $$i | sed -e "s^$(srcdir)/^^"`; \
		cp -p $(srcdir)/$$file $(distdir)/$$file || exit 1; \
	done;

# DO NOT DELETE THIS LINE -- make depend depends on it.
