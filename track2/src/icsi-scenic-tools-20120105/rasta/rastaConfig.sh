#!/bin/sh
#
# rastaConfig.sh.in (becomes rastaConfig.sh through configure)
#
# This file freezes some of the configuration results of rasta
# so that they can be reused by subsequent library clients. 
# This is copying what the tcl install does.
#
# 1997aug09 dpwe@icsi.berkeley.edu after dpwelibConfig.sh
# $Header: /u/drspeech/src/rasta/RCS/rastaConfig.sh.in,v 1.2 1999/01/14 00:26:15 dpwe Exp $

# Version number
RASTA_VERSION='v2_6'

# String to pass to linker to pick up the rasta library from its
# installed directory, along with any other required libraries.
RASTA_BUILD_LIB_SPEC='-L/home/juan/software/icsi-scenic-tools-20120105/rasta -lrasta '
RASTA_INSTALL_LIB_SPEC='-L/usr/local/lib -lrasta '
RASTA_LIB_SPEC=${RASTA_BUILD_LIB_SPEC}

# Location of the installed include headers directory
RASTA_BUILD_INC_SPEC='-I/home/juan/software/icsi-scenic-tools-20120105/rasta '
RASTA_INSTALL_INC_SPEC='-I/usr/local/include '
RASTA_INC_SPEC=${RASTA_BUILD_INC_SPEC}

# Where to find, e.g., the rasta testsuite
RASTA_SRC_DIR='/home/juan/software/icsi-scenic-tools-20120105/rasta'

# Other compilation flags
RASTA_CC='gcc'
RASTA_CFLAGS='-O'
RASTA_LD_FLAGS=''
