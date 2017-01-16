# $Header: /u/drspeech/src/rasta/RCS/Makefile.in,v 1.22 1999/03/30 00:04:32 dpwe Exp $
#
# Makefile.  Generated from Makefile.in by configure.
#
# Makefile.in for RASTA - a speech recognition feature extraction program.


# Standard things we need

.SUFFIXES: 
.SUFFIXES: .c .o

SHELL=/bin/sh


# Installation stuff.

prefix = /usr/local
exec_prefix = /usr/local
libdir = $(exec_prefix)/lib
bindir = $(exec_prefix)/bin
shbindir = $(prefix)/bin
includedir = $(exec_prefix)/include
mandir = $(prefix)/man
man1dir = $(mandir)/man1
docdir = $(prefix)/doc
srcdir = .
top_srcdir = .

transform = s,x,x,

# Programs we use.

CC = gcc
LD = $(CC)
AR = /usr/bin/ar
RANLIB = /usr/bin/ranlib
INSTALL = /usr/bin/install -c
INSTALL_PROG = $(INSTALL)
INSTALL_DATA = ${INSTALL} -m 644
MKINSTALLDIRS =		$(srcdir)/mkinstalldirs

# Tool arguments.

CFLAGS = -O
AC_FLAGS = -DHAVE_CONFIG_H
LDFLAGS = 
LIBS = 
INCS = 

cflags = -I. -I$(srcdir) $(INCS) $(CFLAGS) $(AC_FLAGS) -I$(prefix)/include 
ldflags = $(LDFLAGS)
libs = -L$(exec_prefix)/lib $(LIBS)
compile.c = $(CC) $(cflags)

################################################################
# Implicit rules
################################################################

.c.o:
	$(compile.c) -c $<


################################################################
# The default target
################################################################

default: all


################################################################
# Variables we accumulate into.
################################################################

all_srcs =
all_objs =
all_hdrs =
all_inst_hdrs =
all_progs =
all_libs =
all_docs =
all_man1pages =
garbage =
distgarbage =
rcs_files =

######################################################################
# The rasta program itself.
######################################################################

LIBRASTA	=	librasta.a

rasta_srcs = 	$(srcdir)/rasta.c $(srcdir)/init.c $(srcdir)/anal.c $(srcdir)/powspec.c $(srcdir)/noise_est.c $(srcdir)/mapping.c \
		$(srcdir)/audspec.c $(srcdir)/nl_audspec.c $(srcdir)/rasta_filt.c $(srcdir)/inverse_nl.c \
		$(srcdir)/post_audspec.c $(srcdir)/deltas_c.c $(srcdir)/mul_vfmf_vf.c \
		$(srcdir)/lpccep.c $(srcdir)/fft.c $(srcdir)/io.c $(srcdir)/fvecsubs.c $(srcdir)/svecsubs.c $(srcdir)/debug.c \
		$(srcdir)/invcep.c

rasta_hdrs =  	$(srcdir)/rasta.h

rasta_inst_hdrs =  	$(srcdir)/rasta.h

rasta_main_obj	=	rasta.o

rasta_objs =	init.o anal.o powspec.o noise_est.o mapping.o \
		audspec.o nl_audspec.o rasta_filt.o inverse_nl.o \
		post_audspec.o deltas_c.o mul_vfmf_vf.o \
		lpccep.o fft.o fvecsubs.o svecsubs.o io.o debug.o \
		invcep.o


rasta.o: $(srcdir)/rasta.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/rasta.c

init.o: $(srcdir)/init.c $(srcdir)/rasta.h config.h
	$(compile.c) -DRASTA_VERSION=\"${VERSION}\" -c $(srcdir)/init.c

debug.o: $(srcdir)/debug.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/debug.c

io.o: $(srcdir)/io.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/io.c

anal.o: $(srcdir)/anal.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/anal.c

powspec.o: $(srcdir)/powspec.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/powspec.c

audspec.o: $(srcdir)/audspec.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/audspec.c

noise_est.o: $(srcdir)/noise_est.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/noise_est.c

mapping.o: $(srcdir)/mapping.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/mapping.c

nl_audspec.o: $(srcdir)/nl_audspec.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/nl_audspec.c

rasta_filt.o: $(srcdir)/rasta_filt.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/rasta_filt.c

inverse_nl.o: $(srcdir)/inverse_nl.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/inverse_nl.c

post_audspec.o: $(srcdir)/post_audspec.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/post_audspec.c

lpccep.o: $(srcdir)/lpccep.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/lpccep.c

fft.o: $(srcdir)/fft.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/fft.c

fvecsubs.o: $(srcdir)/fvecsubs.c config.h $(srcdir)/rasta.h
	$(compile.c) -c $(srcdir)/fvecsubs.c

svecsubs.o: $(srcdir)/svecsubs.c config.h $(srcdir)/rasta.h
	$(compile.c) -c $(srcdir)/svecsubs.c

deltas_c.o: $(srcdir)/deltas_c.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/deltas_c.c

# invcep isn't actually used by rasta, but by some librasta clients
invcep.o: $(srcdir)/invcep.c $(srcdir)/rasta.h config.h
	$(compile.c) -c $(srcdir)/invcep.c

mul_vfmf_vf.o: $(srcdir)/mul_vfmf_vf.c config.h
	$(compile.c) -c $(srcdir)/mul_vfmf_vf.c

$(LIBRASTA): $(rasta_objs)
	-rm -f $(LIBRASTA)
	$(AR) rcv $(LIBRASTA) $(rasta_objs)
	$(RANLIB) $(LIBRASTA)


rasta: $(rasta_main_obj) $(LIBRASTA)
	$(LD) $(LDFLAGS) $(rasta_main_obj) $(LIBRASTA) -o $@ $(LIBS) -lm


all_srcs += $(rasta_srcs)
all_objs += $(rasta_objs)
all_objs += $(rasta_main_obj)
all_hdrs += $(rasta_hdrs)
all_inst_hdrs += $(rasta_inst_hdrs)
all_progs += rasta
all_libs += $(LIBRASTA)
all_man1pages += $(srcdir)/rasta.1

#################################################################
## The lsqsolve utility
#################################################################

lsq_srcs = $(srcdir)/lsqsolve.c $(srcdir)/mul_mdmd_md.c

lsq_objs = lsqsolve.o mul_mdmd_md.o

lsqsolve.o: $(srcdir)/lsqsolve.c
	$(compile.c) -c $(srcdir)/lsqsolve.c

mul_mdmd_md.o: $(srcdir)/mul_mdmd_md.c
	$(compile.c) -c $(srcdir)/mul_mdmd_md.c

lsqsolve: $(lsq_objs)
	$(LD) $(LDFLAGS) $(lsq_objs) -o $@ $(LIBS) -lm

all_srcs += $(lsq_srcs)
all_objs += $(lsq_objs)
all_progs += lsqsolve
all_man1pages += $(srcdir)/lsqsolve.1

#################################################################
## The create_mapping script
#################################################################

all_script_srcs += create_mapping.in

all_scripts += create_mapping

#################################################################
## Config. and doc files.
#################################################################

all_docs += INSTALL README NEWS TODO $(all_man1pages)

config_srcs = \
	configure.in \
	acconfig.h \
	aclocal.m4 \
	Makefile.in \
	rastaConfig.sh.in \
	install-sh \
	mkinstalldirs \
	config.guess \
	config.sub 
config_outfiles = \
	configure \
	config.h.in

all_config_scripts += rastaConfig-install.sh

distgarbage += config.cache config.log config.status

$(srcdir)/configure: $(srcdir)/configure.in
	cd $(srcdir) && autoconf
	./config.status --recheck

Makefile: $(srcdir)/Makefile.in $(srcdir)/configure
	$(SHELL) config.status

rastaConfig-install.sh: rastaConfig.sh
	sed -e 's/{RASTA_BUILD_/{RASTA_INSTALL_/' rastaConfig.sh \
		> rastaConfig-install.sh || rm -f rastaConfig-install.sh

#################################################################
## Run testsuite (if present)
#################################################################
TESTSUITE	=	$(srcdir)/testsuite

test:	$(TESTSUITE) rasta 
	r=`pwd`; export r; \
	cd $(TESTSUITE); make RASTA=$$r/rasta main

#################################################################
## Installation.
#################################################################

install: Makefile $(all_progs) $(all_libs) $(all_inst_hdrs) $(all_scripts) $(all_config_scripts) $(all_man1pages)
	@$(MKINSTALLDIRS) $(bindir) $(shbindir) $(libdir) $(includedir) $(man1dir)
	for i in $(all_progs); do \
		$(INSTALL_PROG) $$i $(bindir)/`echo $$i|sed '$(transform)'`; \
	done
	for i in $(all_scripts); do \
		$(INSTALL_PROG) $$i $(shbindir)/`echo $$i|sed '$(transform)'`; \
	done
	for i in $(all_config_scripts); do \
		$(INSTALL_PROG) $$i $(libdir)/`echo $$i|sed -e '$(transform)' -e 's/-install//'`; \
	done
	for i in $(all_libs); do \
		$(INSTALL_DATA) $$i $(libdir)/`echo $$i|sed '$(transform)'`; \
	done
	for i in $(all_inst_hdrs); do \
		file=`basename $$i`; \
		$(INSTALL_DATA) $(srcdir)/$$file $(includedir)/`echo $$file|sed '$(transform)'`; \
	done
	for i in $(all_man1pages); do \
		$(INSTALL_DATA) $$i \
			$(man1dir)/`echo $$i|sed 's@^.*/@@' |sed '$(transform)'`; \
	done

# Have to strip off $(srcdir) from all_man1pages


#################################################################
## Misc useful targets.
#################################################################

garbage += $(all_objs) $(all_progs) $(all_scripts) $(all_libs) \
	   $(all_config_scripts)
garbage += *.o a.out core
distgarbage += $(srcdir)/TAGS Makefile config.h rastaConfig.sh

all: $(all_progs)

objs: $(all_objs)

progs: $(all_progs)

tags: TAGS

$(srcdir)/TAGS:	$(all_srcs) $(all_hdrs) Makefile
	cd $(srcdir) && etags --output=TAGS $(all_srcs) $(all_hdrs)

clean:
	rm -f $(garbage)

distclean: 
	rm -f $(garbage) $(distgarbage)

#################################################################
## Package distribution.
#################################################################

dist_files = $(all_srcs) $(all_hdrs) $(all_docs) \
	$(config_srcs) $(config_outfiles) \
	$(all_script_srcs)

# The name of the distribution
distname = rasta-$(VERSION)

# The file name of the compressed tar file
distfile = $(distname).tar.gz

# What we do to check it is okay to do a distribution
# DISTCHECK = ensure-ci

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

# Build a directory tree that can be tared and gzipped to form the distribution
disttree: Makefile $(DISTCHECK) $(dist_files)
	rm -fr $(distdir); \
	mkdir $(distdir); \
	chmod a=rwx $(distdir); \
	for i in $(dist_files); do \
		file=`basename $$i`; \
		cp -p $(srcdir)/$$file $(distdir)/$$file || exit 1; \
	done

#################################################################
## Version and revision control.
#################################################################

rcs_files += $(all_srcs) $(all_hdrs) $(all_docs) $(config_srcs) \
	$(all_script_srcs)


# Ensure all files are checked in.
ensure-ci: Makefile
	@cd $(srcdir) && \
	if [ `rlog -L -l -R $(rcs_files) | wc -w` -ne 0 ]; \
        then \
                echo "ERROR: The following files are not checked in:"; \
                rlog -L -l -R $(rcs_files); \
                exit 1; \
        fi

# Execute an RCS command on all files.

RCSCMD =  rlog

rcs: Makefile
	@cd $(srcdir) && \
	$(RCSCMD) $(rcs_files)

# Version stamp.

VERSION = v2_6
VERSIONCHECK = ensure-ci

version: $(VERSIONCHECK)
	@cd $(srcdir) && \
	rcs "-n$(VERSION):" $(rcs_files)

version-force: $(VERSIONCHECK)
	@cd $(srcdir) && \
	rcs "-N$(VERSION):" $(rcs_files)

#### BOTTOM OF MAKEFILE/MAKEFILE.IN
# DO NOT DELETE THIS LINE -- make depend depends on it.
