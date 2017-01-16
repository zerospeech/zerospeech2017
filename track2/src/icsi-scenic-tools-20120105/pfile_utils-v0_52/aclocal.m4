dnl dpwe''s aclocal.m4     -*- sh -*-
dnl
dnl A place for my own autoconf macros
dnl 
dnl 1998jan23 Dan Ellis dpwe@icsi.berkeley.edu
dnl $Header: /u/drspeech/repos/pfile_utils/aclocal.m4,v 1.3 2005/06/06 22:22:53 davidj Exp $
dnl
dnl Clients:
dnl  dpwetcl dpweutils_tcl farray_otcl gdtcl libdat otcl pfif_otcl
dnl  sound_otcl tclsh-readline aprl feacat

#--------------------------------------------------------------------
# ACDPWE_INIT_PREFIX
#	Make sure prefix and exec_prefix are not set to NONE
#	- anticipates standard end-of-config code so we can use 
#	prefix & exec_prefix earlier
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_INIT_PREFIX, [
  if test "x$prefix" = "xNONE" ; then
dnl    prefix=$ac_default_prefix
    prefix=$ICSIBUILDPATH
  fi
  if test "x$exec_prefix" = "xNONE" ; then
    exec_prefix=$prefix
  fi
])

#--------------------------------------------------------------------
# ACDPWE_PROG_CC_CXX
#	Setup CC and CXX as:
#	   (a) gcc and g++ (by AC macros) *if* --enable-gcc specified
#	   (b) entry values of $CC and $CXX
#	   (c) "cc" and "CC"
#	Specify the first argument as "0" to omit the CXX checks
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PROG_CC_CXX, [
  # Check the input argument; if $1="0", omit C++ checks.  If it's anything
  # else, or omitted, do C++ checks too.
  dpwe_doCxx=$1
  if test "x$dpwe_doCxx" = "x0" ; then
    dpwe_doCxx=""
  else
    dpwe_doCxx="1"
  fi
  AC_ARG_ENABLE(gcc, [  --enable-gcc            allow use of gcc if available],
    [dpwe_ok=$enableval], [dpwe_ok=yes])
  if test "$dpwe_ok" = "yes"; then
      CC=
      CXX=
      CFLAGS=
      CXXFLAGS=
      AC_PROG_CC
      test -n "$dpwe_doCxx" && \
        AC_PROG_CXX
  else
      CC=${CC-cc}
      test -n "$dpwe_doCxx" && \
        CXX=${CXX-CC}
  fi
  AC_MSG_CHECKING([default C compiler])
  AC_ARG_WITH(cc, [  --with-cc=CC         set C compiler flags to CC],
      [CC="$with_cc"])
  AC_MSG_RESULT([$CC])
  AC_MSG_CHECKING([default compiler flags])
  if test -z "$CFLAGS" ; then
      CFLAGS="-O"
  fi
  AC_ARG_WITH(cflags, [  --with-cflags=FLAGS     set compiler flags to FLAGS],
      [CFLAGS="$with_cflags"])
  AC_MSG_RESULT([$CFLAGS])

  AC_SUBST(CC)
  AC_SUBST(CFLAGS)
  if test -n "$dpwe_doCxx" ; then 
    AC_MSG_CHECKING([default C++ compiler])
    AC_ARG_WITH(cc, [  --with-cxx=CXX         set C++ compiler flags to CXX],
        [CXX="$with_cxx"])
    AC_MSG_RESULT([$CXX])
    AC_MSG_CHECKING([default C++ compiler flags])
    if test -z "$CXXFLAGS" ; then
	CXXFLAGS="-O"
    fi
    AC_ARG_WITH(cxxflags, [  --with-cxxflags=FLAGS   set C++ compiler flags to FLAGS],
	[CXXFLAGS="$with_cxxflags"])
    AC_MSG_RESULT([$CXXFLAGS])
    AC_SUBST(CXX)
    AC_SUBST(CXXFLAGS)
  fi
])

#--------------------------------------------------------------------
# ACDPWE_CXX_BOOL:
#       If the C++ compiler knows the bool type, define HAVE_BOOL.
#       Else don't.  This one uses the previously-defined $CXX
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_CXX_BOOL, [
AC_MSG_CHECKING([if C++ compiler supports "bool"])
cat > conftest.cc <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
int main() {
bool b=0;
return 0;
}
EOF
ac_try="$CXX -o conftest conftest.cc >/dev/null 2>conftest.out"
AC_TRY_EVAL(ac_try)
ac_err=`grep -v '^ *+' conftest.out`
if test -z "$ac_err"; then
  AC_DEFINE(HAVE_BOOL)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no);
  echo "$ac_err" >&AC_FD_CC
fi
rm -f conftest*])


#--------------------------------------------------------------------
# ACDPWE_LIB_MATH: (after tcl's config.in)
#	On a few very rare systems, all of the libm.a stuff is
#	already in libc.a.  Set compiler flags accordingly.
#	Also, Linux requires the "ieee" library for math to work
#	right (and it must appear before "-lm").
#	Requires line of form "LIBS=@MATH_LIBS@ -lc" in Makefile.in.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_LIB_MATH, [
  AC_CHECK_FUNC(sin, MATH_LIBS="", MATH_LIBS="-lm")
  AC_CHECK_LIB(ieee, main, [MATH_LIBS="-lieee $MATH_LIBS"])
  AC_SUBST(MATH_LIBS)
])

#--------------------------------------------------------------------
# ACDPWE_PATH_FULLSRCDIR
#	Setup variable $fullSrcDir to be an absolute path to the 
#	same directory as $srcdir (but don't expand ugly automounter 
#	paths if $srcdir is already absolute).
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PATH_FULLSRCDIR, [
  if test "`echo $srcdir | sed -e 's/\(.\).*/\1/'`" = "/" ; then
    # srcdir is absolute - use as is
    fullSrcDir=$srcdir
  else
    # srcdir is a relative path - have to resolve it
    fullSrcDir=`cd $srcdir; pwd`
  fi
])

#--------------------------------------------------------------------
# ACDPWE_PKG_TCL
#	Assume dependency on tcl; attempt to read the tclConfig.sh
#	definitions, offering --with-tcl.
#	Sets up TCL_BIN_DIR, and whatever else is defined in tclConfig.sh.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PKG_TCL, [ 
  # Ensure exec_prefix is set...
  AC_REQUIRE([ACDPWE_INIT_PREFIX])
  # Default Tcl version to look for
  TCL_VERSION_GUESS=7.6
  # Try to guess - look for a peer first
  TCL_DFLT_DIR=$srcdir/../tcl$TCL_VERSION_GUESS/unix
  if test ! -d $TCL_DFLT_DIR ; then
    # Look elswhere: check for itcl on the off-chance
    if test -d $exec_prefix/lib/itcl ; then 
      TCL_DFLT_DIR=$exec_prefix/lib/itcl
    else
      TCL_DFLT_DIR=$exec_prefix/lib
    fi
  fi
  ACDPWE_CONFIG_PKG(tcl, TCL, $TCL_DFLT_DIR, $TCL_DFLT_DIR)
  if test -z "$TCL_LIB_RUNTIME_DIR" ; then
    # The "lib runtime dir" is where libtcl sits.  It's usually specified 
    #  as -L... in the first term of TCL_LIB_SPEC - try to pull it out...
    TCL_LIB_RUNTIME_DIR=`echo "$TCL_LIB_SPEC" | sed -e "s@ .*@@" -e "s@-L@@"`
  fi
  if test -z "$TCL_INC_SPEC" ; then
    # This is (one place) to find the tcl.h that you'll need to link against.
    # Actually, it gets installed in $prefix/include/itcl, but there's 
    # nothing pointing there, unfortunately
    TCL_INC_SPEC="-I$TCL_SRC_DIR/generic"
  fi
])

#--------------------------------------------------------------------
# ACDPWE_PKG_TK(ACTION_IF_FOUND, ACTION_IF_NOT_FOUND)
#	Assume dependency on Tk; attempt to read the tkConfig.sh
#	definitions, offering --with-tk.
#	Sets up TK_BIN_DIR, and whatever else is defined in tkConfig.sh.
#	ACTION_ arguments are optional.  If ACTION_IF_NOT_FOUND is 
#	specified, it's OK not to find Tk.  But if it isn't specified, 
#	not finding Tk is an error.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PKG_TK, [ 
  # Find the tk configuration
  AC_REQUIRE([ACDPWE_PKG_TCL])
  # Guess Tk dir from Tcl dir; also, substitute 4.2 for 7.6 if present
  TK_DFLT_DIR=`echo $TCL_BIN_DIR | sed -e "s@/tcl@/tk@" | sed -e "s/7\.6/4\.2/"`
  ACDPWE_CONFIG_PKG(tk, TK, $TK_DFLT_DIR, $TK_DFLT_DIR, , $1, $2)
  if test -n "$TK_LIB_SPEC" ; then
    # looks like tkConfig.sh was read, so can go ahead with fix-ups
    if test -z "$TK_LIB_RUNTIME_DIR" ; then
      # The "lib runtime dir" is where libtcl sits.  It's usually specified 
      #  as -L... in the first term of TK_LIB_SPEC - try to pull it out...
      TK_LIB_RUNTIME_DIR=`echo "$TK_LIB_SPEC" | sed -e "s@ .*@@" -e "s@-L@@"`
    fi
    if test -z "$TK_INC_SPEC" ; then
      # This is (one place) to find the tk.h that you'll need to link against.
      # Actually, it gets installed in $prefix/include/itcl, but there's 
      # nothing pointing there, unfortunately
      if test -z "$TK_SRC_DIR" ; then
        # Looks like we even have to gues the Tk src dir for std tkConfig.sh
        TK_SRC_DIR=`echo $TK_BUILD_LIB_SPEC | sed -e "s/-L//" | sed -e "s@/unix.*@@"`
      fi
      TK_INC_SPEC="-I$TK_SRC_DIR/generic"
    fi
  fi
])

#--------------------------------------------------------------------
# ACDPWE_PKG_TCL_CHECK_SHLIB
#	Check that Tcl was configured for shared libraries, and set 
#	all the SHLIB* variables from the ones read from the tclConfig.sh 
#	file.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PKG_TCL_CHECK_SHLIB, [
  # Check that Tcl was configured & built for shared libraries
  AC_REQUIRE([ACDPWE_PKG_TCL])
  if test -z "$TCL_SHLIB_LD" ; then
    AC_MSG_ERROR(Tcl was not configured to use dynamic shared libraries)
  else
    # CFLAGS=$TCL_CFLAGS
    SHLIB_CFLAGS=$TCL_SHLIB_CFLAGS
    SHLIB_LD=$TCL_SHLIB_LD
    SHLIB_LD_LIBS=$TCL_SHLIB_LD_LIBS
    SHLIB_SUFFIX=$TCL_SHLIB_SUFFIX
    SHLIB_VERSION=$TCL_SHLIB_VERSION
    DL_LIBS=$TCL_DL_LIBS
    LD_FLAGS=$TCL_LD_FLAGS
    LD_SEARCH_FLAGS=$TCL_LD_SEARCH_FLAGS
    LIB_RUNTIME_DIR=$TCL_LIB_RUNTIME_DIR
  fi
])

#--------------------------------------------------------------------
# ACDPWE_PKG_OTCL([ACTION_IF_FOUND [, ACTION_IF_NOT_FOUND]])
#	Locate & load the config file written by otcl/tclcpp, our 
#	Tcl-to-C++ interface package.
#	Sets up a large number of OTCL_ variables, including
#	OTCL_BIN_DIR, OTCL_INC_SPEC, OTCL_LIB_SPEC, OTCL_CDL 
#	and OTCL_DEFS
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PKG_OTCL, [
  # Guess at a particular version number (if no path given)
  OTCL_VERSION_GUESS=0.5.1
  # Try to guess - look for a peer first
  OTCL_DFLT_DIR=$srcdir/../tclcpp-$OTCL_VERSION_GUESS
  if test -d $OTCL_DFLT_DIR ; then
    # found it - resolve name
    OTCL_DFLT_DIR=`cd $OTCL_DFLT_DIR; pwd`
  else
    # did not find it - use best guess
    OTCL_DFLT_DIR=$exec_prefix/lib
  fi
  ACDPWE_CONFIG_PKG(otcl, OTCL, $OTCL_DFLT_DIR, $OTCL_DFLT_DIR, , $1, $2)
])

#--------------------------------------------------------------------
# ACDPWE_SYSTEM
#	Cut down variant of system-name guessing, copied from 
#	Tcl's dynamic loading config.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_SYSTEM, [
  AC_MSG_CHECKING([system version])
  if test -f /usr/lib/NextStep/software_version; then
      system=NEXTSTEP-`awk '/3/,/3/' /usr/lib/NextStep/software_version`
  else
      system=`uname -s`-`uname -r`
      if test "$?" -ne 0 ; then
	  AC_MSG_RESULT([unknown (can not find uname command)])
	  system=unknown
      else
	  # Special check for weird MP-RAS system (uname returns weird
	  # results, and the version is kept in special file).

	  if test -r /etc/.relid -a "X`uname -n`" = "X`uname -s`" ; then
	      system=MP-RAS-`awk '{print $3}' /etc/.relid'`
	  fi
	  if test "`uname -s`" = "AIX" ; then
	      system=AIX-`uname -v`.`uname -r`
	  fi
	  AC_MSG_RESULT($system)
      fi
  fi
])

#--------------------------------------------------------------------
# ACDPWE_SYS_SHLIB_CC_CXX
#	Figure out all the system-dependent stuff to configure the 
#	SHLIB* flags for compiling shared libraries under both C and 
#	C++ (e.g. for Tcl dynamic loadable objects).  
#	After tcl's configure.in
#	Re-uses the dpwe_doCxx flag set up in ACDPWE_PROG_CC_CXX
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_SYS_SHLIB_CC_CXX, [
  
  AC_REQUIRE([ACDPWE_PROG_CC_CXX])
  # Assume dpwe_doCxx has already been set up.

  #--------------------------------------------------------------------
  #	The statements below define a collection of symbols related to
  #	dynamic loading and shared libraries:
  #
  #	LD_FLAGS -	Flags to pass to the compiler when linking object
  #			files into an executable application binary such
  #			as tclsh.
  #	LD_SEARCH_FLAGS-Flags to pass to ld, such as "-R /usr/local/tcl/lib",
  #			that tell the run-time dynamic linker where to look
  #			for shared libraries such as libtcl.so.  Depends on
  #			the variable LIB_RUNTIME_DIR in the Makefile.
  #	SHLIB_CFLAGS -	Flags to pass to cc when compiling the components
  #			of a shared library (may request position-independent
  #			code, among other things).
  #	SHLIB_LD -	Base command to use for combining object files
  #			into a shared library.
  #	SHLIB_LD_LIBS -	Dependent libraries for the linker to scan when
  #			creating shared libraries.  This symbol typically
  #			goes at the end of the "ld" commands that build
  #			shared libraries. The value of the symbol is
  #			"${LIBS}" if all of the dependent libraries should
  #			be specified when creating a shared library.  If
  #			dependent libraries should not be specified (as on
  #			SunOS 4.x, where they cause the link to fail, or in
  #			general if Tcl and Tk aren't themselves shared
  #			libraries), then this symbol has an empty string
  #			as its value.
  #	SHLIB_SUFFIX -	Suffix to use for the names of dynamically loadable
  #			extensions.  An empty string means we don't know how
  #			to use shared libraries on this platform.
  # Also: if doing CXX, SHLIB_CXXFLAGS SHLIB_LDXX SHLIB_LDXX_LIBS SHLD_RANLIB
  #--------------------------------------------------------------------

  # Step 1: set the variable "system" to hold the name and version number
  # for the system.  This can usually be done via the "uname" command, but
  # there are a few systems, like Next, where this doesn't work.

  ACDPWE_SYSTEM

  # Step 2: check for existence of -ldl library.  This is needed because
  # Linux can use either -ldl or -ldld for dynamic loading.

  AC_CHECK_LIB(dl, dlopen, have_dl=yes, have_dl=no)

  # Step 3: set configuration options based on system name and version.

  fullSrcDir=`cd $srcdir; pwd`
  AIX=no
  SHARED_LIB_SUFFIX=""
  UNSHARED_LIB_SUFFIX=""
  LIB_VERSIONS_OK=ok
  case $system in
      AIX-*)
	  SHLIB_CFLAGS=""
	  SHLIB_LD="$fullSrcDir/ldAix /bin/ld -bhalt:4 -bM:SRE -bE:lib.exp -H512 -T512"
	  SHLIB_LD_LIBS='${LIBS}'
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	  AIX=yes
	  ;;
      BSD/OS-2.1*)
	  SHLIB_CFLAGS=""
	  SHLIB_LD="ld -r"
	  SHLIB_LD_FLAGS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS=""
	  ;;
      HP-UX-*.08.*|HP-UX-*.09.*|HP-UX-*.10.*)
	  AC_CHECK_LIB(dld, shl_load, tcl_ok=yes, tcl_ok=no)
	  if test "$tcl_ok" = yes; then
	      SHLIB_CFLAGS="+z"
	      SHLIB_LD="ld -b"
	      SHLIB_LD_LIBS='${LIBS}'
	      SHLIB_SUFFIX=".sl"
	      LD_FLAGS="-Wl,-E"
	      LD_SEARCH_FLAGS='-Wl,+b,${LIB_RUNTIME_DIR}:.'
	  fi
	  ;;
      IRIX-4.*)
	  SHLIB_CFLAGS="-G 0"
	  SHLIB_SUFFIX="..o"
	  SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	  SHLIB_LD_LIBS='${LIBS}'
	  LD_FLAGS="-Wl,-D,08000000"
	  LD_SEARCH_FLAGS=""
	  ;;
      IRIX-5.*|IRIX-6.*)
	  SHLIB_CFLAGS=""
	  SHLIB_LD="ld -shared -rdata_shared -update_registry /usr/tmp/so_locations"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  SHLD_RANLIB='${RANLIB}'
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
          if test -n "$dpwe_doCxx" ; then
	    SHLIB_CXXFLAGS=""
	    SHLIB_LDXX="$(CXX) -shared -rdata_shared -update_registry /usr/tmp/so_locations"
	    SHLIB_LDXX_LIBS='${LIBS}'
          fi
	  ;;
      IRIX64-6.*)
	  SHLIB_CFLAGS=""
	  SHLIB_LD="ld -32 -shared -rdata_shared -rpath /usr/local/lib"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	  ;;
      Linux*)
	  SHLIB_CFLAGS="-fPIC"
	  SHLIB_LD_LIBS='${LIBS}'
	  SHLIB_SUFFIX=".so"
	  if test "$have_dl" = yes; then
	      SHLIB_LD="${CC} -shared"
	      LD_FLAGS="-rdynamic"
	      LD_SEARCH_FLAGS='-Xlinker -rpath -Xlinker ${LIB_RUNTIME_DIR}'
	  else
	      AC_CHECK_HEADER(dld.h, [
		  SHLIB_LD="ld -shared"
		  LD_FLAGS=""
		  LD_SEARCH_FLAGS=""])
	  fi
	  SHLD_RANLIB='${RANLIB}'
          if test -n "$dpwe_doCxx" ; then
	    SHLIB_CXXFLAGS="-fPIC"
	    SHLIB_LDXX="${CXX} -shared"
	    SHLIB_LDXX_LIBS=$SHLIB_LD_LIBS
	  fi
	  ;;
      MP-RAS-02*)
	  SHLIB_CFLAGS="-K PIC"
	  SHLIB_LD="cc -G"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS=""
	  ;;
      MP-RAS-*)
	  SHLIB_CFLAGS="-K PIC"
	  SHLIB_LD="cc -G"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS="-Wl,-Bexport"
	  LD_SEARCH_FLAGS=""
	  ;;
      NetBSD-*|FreeBSD-*)
	  # Not available on all versions:  check for include file.
	  AC_CHECK_HEADER(dlfcn.h, [
	      SHLIB_CFLAGS="-fpic"
	      SHLIB_LD="ld -Bshareable -x"
	      SHLIB_LD_LIBS=""
	      SHLIB_SUFFIX=".so"
	      LD_FLAGS=""
	      LD_SEARCH_FLAGS=""
	  ], [
	      SHLIB_CFLAGS=""
	      SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	      SHLIB_LD_LIBS='${LIBS}'
	      SHLIB_SUFFIX="..o"
	      LD_FLAGS=""
	      LD_SEARCH_FLAGS=""
	  ])
	  # FreeBSD doesn't handle version numbers with dots.  Also, have to
	  # append a dummy version number to .so file names.
	  ;;
      NEXTSTEP-*)
	  SHLIB_CFLAGS=""
	  SHLIB_LD="cc -nostdlib -r"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS=""
	  ;;
      OSF1-1.0|OSF1-1.1|OSF1-1.2)
	  # OSF/1 1.[012] from OSF, and derivatives, including Paragon OSF/1
	  SHLIB_CFLAGS=""
	  # Hack: make package name same as library name
	  SHLIB_LD='ld -R -export $@:'
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS=""
	  ;;
      OSF1-1.*)
	  # OSF/1 1.3 from OSF using ELF, and derivatives, including AD2
	  SHLIB_CFLAGS="-fpic"
	  SHLIB_LD="ld -shared"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS=""
	  ;;
      OSF1-V*)
	  # Digital OSF/1
	  SHLIB_CFLAGS=""
	#  SHLIB_LD='ld -shared -expect_unresolved "*"'
	# Problem with stray /mas/system/bin/ld on media lab DEC machines
	  SHLIB_LD='/usr/bin/ld -shared -expect_unresolved "*"'
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	  SHLD_RANLIB=":"
          if test -n "$dpwe_doCxx" ; then
	    SHLIB_CXXFLAGS=""
	  #  SHLIB_LDXX='ld -shared -expect_unresolved "*"'
	    SHLIB_LDXX='/usr/bin/ld -shared -expect_unresolved "*"'
	    SHLIB_LDXX_LIBS=""
	  fi
	  ;;
      SCO_SV-3.2*)
	  # Note, dlopen is available only on SCO 3.2.5 and greater.  However,
	  # this test works, since "uname -s" was non-standard in 3.2.4 and
	  # below.
	  SHLIB_CFLAGS="-Kpic -belf"
	  SHLIB_LD="ld -G"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS="-belf -Wl,-Bexport"
	  LD_SEARCH_FLAGS=""
	  ;;
       SINIX*5.4*)
	  SHLIB_CFLAGS="-K PIC"
	  SHLIB_LD="cc -G"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS=""
	  ;;
      SunOS-4*)
	  SHLIB_CFLAGS="-PIC"
	  SHLIB_LD="ld -assert pure-text"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	  # SunOS can't handle version numbers with dots in them in library
	  # specs, like -ltcl7.5, so use -ltcl75 instead.  Also, it
	  # requires an extra version number at the end of .so file names.
	  # So, the library has to have a name like libtcl75.so.1.0
	  SHLD_RANLIB=":"
          if test -n "$dpwe_doCxx" ; then
	    SHLIB_CXXFLAGS="-pic"
	    # SHLIB_LDXX="ld -assert pure-text"
	    # SHLIB_LDXX_LIBS=""
	    SHLIB_LDXX="ld"
	    SHLIB_LDXX_LIBS=""
	  fi
	  ;;
      SunOS-5*)
	  SHLIB_CFLAGS="-KPIC"
	  SHLIB_LD="/usr/ccs/bin/ld -G -z text"
	  SHLIB_LD_LIBS='${LIBS}'
	  SHLIB_SUFFIX=".so"
	  LD_FLAGS=""
	  LD_SEARCH_FLAGS='-R ${LIB_RUNTIME_DIR}'
	  SHLD_RANLIB='${RANLIB}'
          if test -n "$dpwe_doCxx" ; then
	    SHLIB_CXXFLAGS="-pic"
	    SHLIB_LDXX="$CXX -G -z text"
	    SHLIB_LDXX_LIBS='${LIBS}'
	  fi
	  ;;
      ULTRIX-4.*)
	  SHLIB_CFLAGS="-G 0"
	  SHLIB_SUFFIX="..o"
	  SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	  SHLIB_LD_LIBS='${LIBS}'
	  LD_FLAGS="-Wl,-D,08000000"
	  LD_SEARCH_FLAGS=""
	  ;;
      UNIX_SV*)
	  SHLIB_CFLAGS="-KPIC"
	  SHLIB_LD="cc -G"
	  SHLIB_LD_LIBS=""
	  SHLIB_SUFFIX=".so"
	  # Some UNIX_SV* systems (unixware 1.1.2 for example) have linkers
	  # that don't grok the -Bexport option.  Test that it does.
	  hold_ldflags=$LDFLAGS
	  AC_MSG_CHECKING(for ld accepts -Bexport flag)
	  LDFLAGS="${LDFLAGS} -Wl,-Bexport"
	  AC_TRY_LINK(, [int i;], found=yes, found=no)
	  LDFLAGS=$hold_ldflags
	  AC_MSG_RESULT($found)
	  if test $found = yes; then
	    LD_FLAGS="-Wl,-Bexport"
	  else
	    LD_FLAGS=""
	  fi
	  LD_SEARCH_FLAGS=""
	  ;;
  esac

  # If we're running gcc, then change the C flags for compiling shared
  # libraries to the right flags for gcc, instead of those for the
  # standard manufacturer compiler.
  if test "$CC" = "gcc" -o `$CC -v 2>&1 | grep -c gcc` != "0" ; then
    SHLIB_CFLAGS="-fPIC"
    SHLIB_LD="$CC --shared"
  fi

  # In case the SHLIB_CFLAGS are already specified as part of 
  # regular CFLAGS, strip them out???
  # Note: this wont work if SHLIB_CFLAGS="-Kpic -belf" 
  # and CFLAGS="-g -belf -Kpic".  Also, -G 0 vs -G0 etc.
  if test -n "$SHLIB_CFLAGS" ; then
    CFLAGS=`echo $CFLAGS | sed -e "s@$SHLIB_CFLAGS@@"`
  fi

  if test -n "$dpwe_doCxx" ; then
    if test "$CXX" = "g++" -o "$CXX" = "gcc" -o `$CXX -v 2>&1 | grep -c gcc` != "0" ; then
	SHLIB_CXXFLAGS="-fPIC"
	#SHLIB_LDXX=$SHLIB_LD
        SHLIB_LDXX="$CXX --shared"
	SHLD_RANLIB=":"
    fi
    # Try to remove it from CXXFLAGS
    if test -n "$SHLIB_CXXFLAGS" ; then
      CXXFLAGS=`echo $CXXFLAGS | sed -e "s@$SHLIB_CXXFLAGS@@"`
    fi
  fi

  if test -z "$SHLIB_LD" ; then
    AC_MSG_ERROR(Cannot figure out how to compile shared libraries)
  fi

  if test -n "$dpwe_doCxx" && test -z "$SHLIB_LDXX" ; then
    AC_MSG_ERROR(C++ shared library building not yet configured for this arch)
  fi

])

#--------------------------------------------------------------------
# ACDPWE_SYS_SHLIB_CXX_STATIC_INIT
#	Test whether shared/dynamic libraries containing C++ statically-
#	initialized data actually gets statically initialized (I'm 
#	unable to make it do so under g++ 2.7)
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_SYS_SHLIB_CXX_STATIC_INIT, [
  # Make sure that we've configured for compiling C++ shared libraries
  AC_REQUIRE([ACDPWE_SYS_SHLIB_CC_CXX])
  AC_MSG_CHECKING(C++ static initializers in shared libraries)
  # See if it's cached (since it's slow)
  AC_CACHE_VAL(acdpwe_cv_sys_shlib_init, acdpwe_cv_sys_shlib_init="")

  if test -z "$acdpwe_cv_sys_shlib_init" ; then
    # Not found in cache, so we have to test it.
    # Construct a test case
    tmproot="_acdpwe_tmp1"
    tmpsrc=${tmproot}.cc
    tmpobj=${tmproot}.o
    tmplib=${tmproot}$SHLIB_SUFFIX
    rm -f $tmpsrc
    cat > $tmpsrc << EOF
class A {
public:
  int i;
  A(void) { i = 1;}
};
static A a;
int test(void) {return a.i;}
EOF
    # Make it into a shared library
    $CXX $CXXFLAGS $SHLIB_CXXFLAGS -c $tmpsrc -o $tmpobj
    $SHLIB_LDXX -o $tmplib $tmpobj `eval echo $SHLIB_LDXX_LIBS`
    `eval echo $SHLD_RANLIB` $tmplib
    # Try running a program against it
    tmproot2="_acdpwe_tmp2"
    tmpsrc2=${tmproot2}.cc
    tmpobj2=${tmproot2}.o
    tmpprog=./${tmproot2}
    rm -f $tmpsrc2
    cat > $tmpsrc2 << EOF
extern int test(void);
int main(void) {
  int i = test();
  return i;
}
EOF
    # Build the executable
    $CXX $CXXFLAGS $SHLIB_CXXFLAGS -c $tmpsrc2 -o $tmpobj2
    LIB_RUNTIME_tmp=$LIB_RUNTIME_DIR
    LIB_RUNTIME_DIR=.
    $CXX $LDFLAGS $tmpobj2 $tmplib -o $tmpprog `eval echo $LD_SEARCH_FLAGS`
    if $tmpprog ; then
      # 'success' indicates zero exit code i.e. init was not called
      acdpwe_cv_sys_shlib_init="not called"
    else
      acdpwe_cv_sys_shlib_init="called OK"
    fi
    # Clean up
    LIB_RUNTIME_DIR=$LIB_RUNTIME_tmp
    rm -f $tmpsrc $tmpobj $tmplib $tmpsrc2 $tmpobj2 $tmpprog
  fi
  # Now either read from cache or newly calculated - set flag either way
  if test "$acdpwe_cv_sys_shlib_init" = "not called" ; then
    AC_DEFINE(SHLIB_NO_STATIC_INIT)
  else
    AC_DEFINE(SHLIB_STATIC_INIT)
  fi
  AC_MSG_RESULT($acdpwe_cv_sys_shlib_init)
])

#--------------------------------------------------------------------
# ACDPWE_CONFIG_PKG(pkgname, upcasename, nonedir, dfltdir, configfile, 
#		     ACTION_IF_FOUND, ACTION_IF_NOT_FOUND)
#	Locate & load a config file written by some other package.
#	'pkgname' is the tag for this package to be used in the 
#	"--with-pkg" configuration line.  
#	'upcasename' is the same thing in capitals, to serve as the 
#	prefix for defined variables.  
#	'nonedir' is the default to take as the directory if 
#	the user does not specify the --with.. flag (default: "none")
#	'dfltdir' is the default to take as the directory if
#	the user specifies the --with.. flag without an argument
#	(default: $exec_prefix/lib)
#	'configname' is the name of the config file to load 
#	(default: ${cachename}Config.sh)
#	ACTION_IF_FOUND will be executed if the file is successfully 
#	sourced;  
#	if ACTION_IF_NOT_FOUND is specified and nonempty, it will be 
#	executed if the config file is not found (and configuration 
#	will continue normally).   
#	If ACTION_IF_NOT_FOUND is empty or not specified, configuration 
#	will abort if the config file cannot be found.
#	  This macro will also use the cache file to obtain default 
#	values for the flag (overriding the passed in nonedir).  This 
#	allows subsidiary packages to re-use values specified in 
#	an earlier config within the same super-package.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_CONFIG_PKG, [
  AC_MSG_CHECKING($1 config dir)
  cachename=$1
  upcasename=$2
  dfltdir=$4
  if test -z "$dfltdir" ; then
    dfltdir="$exec_prefix/lib"
  fi
  configname=$5
  if test -z "$configname" ; then
    configname=$1Config.sh
  fi
  # Set up a flag to indicate whether an action-if-not was specified
  dpwe_naction='$7'
  dpwe_have_naction=${dpwe_naction:+yes}
  # Set a flag to see whether we took a default value or not
  dpwe_default=0
  AC_ARG_WITH($1, [  --with-$1=DIR	  use $1 package from DIR], 
	$2_BIN_DIR=$withval, $2_BIN_DIR=none)
  if test -z "$$2_BIN_DIR" -o "$$2_BIN_DIR" = "yes" ; then
    # User specified empty flag - try the default offered on the invocation
    $2_BIN_DIR=$dfltdir
    if test -d "$$2_BIN_DIR" ; then
      # found it - resolve name
      $2_BIN_DIR=`cd $$2_BIN_DIR; pwd`
#    else
#      # Specified default doesn't exist - maybe treat like they 
#      # didn't specify
#      $2_BIN_DIR=none
# no, should probably treat as an error if specified path is wrong
    fi
  fi
  if test "$$2_BIN_DIR" = "none" ; then
    dpwe_default=1
    # Did not specify check at all - try reverting to cache or none val
    AC_CACHE_VAL(acdpwe_cv_path_$1, acdpwe_cv_path_$1=none)
    # Value from cache takes precedence over passed-in "nonedir"
    if test "x$acdpwe_cv_path_$1" = "xnone" ; then
      nonedir=$3
    else
      nonedir=$acdpwe_cv_path_$1
    fi
    if test -z "$nonedir" ; then
      nonedir="none"
    fi
    $2_BIN_DIR=$nonedir
  fi
  if test "$$2_BIN_DIR" = "none" -o "$$2_BIN_DIR" = "no" ; then
    # User did not specify --with.. at all, and the default action 
    # was to ignore
    if test -z "$dpwe_have_naction" ; then
      # If no ACTION_IF_NOT given, this is an error
      AC_MSG_RESULT()
      AC_MSG_ERROR(--with-$1 not specified)
    else
      # Otherwise, continue unabated
      AC_MSG_RESULT($1 not specified)
      $7
    fi
  else
    # We have some stab at a directory
    dpwe_file=$$2_BIN_DIR/$configname
    # Report the path we are using
    AC_MSG_RESULT($$2_BIN_DIR)
    # Report which file we're looking at
    AC_MSG_CHECKING($dpwe_file)
    if test ! -r "$dpwe_file" ; then
      # Maybe in a subdir mirroring us relative to srcdir
      dpwe_build_dir=`pwd`
      dpwe_src_dir=`cd $srcdir; pwd`
      dpwe_branch=`echo $dpwe_build_dir | sed -e "s@${dpwe_src_dir}@@"`
      AC_MSG_RESULT([not found, guessing build subdir])
      dpwe_file=$$2_BIN_DIR$dpwe_branch/$configname
      AC_MSG_CHECKING($dpwe_file)
    fi
    # Record this path in the cache
    acdpwe_cv_path_$1=$$2_BIN_DIR
    if test ! -r "$dpwe_file" ; then
      if test -z "$dpwe_have_naction" -o $dpwe_default = 0; then
	# Not found and ACTION_IF_NOT_FOUND not specified, so throw error
        AC_MSG_RESULT()
	if test ! -d "$$2_BIN_DIR" ; then
	  AC_MSG_ERROR($1 directory $$2_BIN_DIR does not exist)
	else 
	  AC_MSG_ERROR(Cant read $configname in $$2_BIN_DIR;  specify with --with-$1=DIR)
	fi
      else
	# Do whatever we were given to do on input, but don't abort
	AC_MSG_RESULT(not found)
	$7
      fi
    else
      # We did find a config file, so read in the config information
      . $dpwe_file
      AC_MSG_RESULT(read)
      $6
    fi
  fi
])

#--------------------------------------------------------------------
# ACDPWE_CACHE_SAVE_PKG(tag, value)
#	Set a value in the config cache file so that subsequent 
#	packages that use the same cache file will find its value
#	automatically if they happen to call ACDPWE_CONFIG_PKG 
#	with this tag.
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_CACHE_SAVE_PKG, [
  # This seems a little verbose, but should work otherwise
  # I generate messages to wrap the (cached) message that can appear
  # This should be AC_MSG, but there isn't one.. sorry
  AC_MSG_CHECKING($1 path is saved in cache)
  AC_CACHE_VAL(acdpwe_cv_path_$1, acdpwe_cv_path_$1=$2)
  # Do it again in case it didn't do it before because it was already in cache
  acdpwe_cv_path_$1=$2
  AC_MSG_RESULT($2)
])

#--------------------------------------------------------------------
# ACDPWE_PKG_MATLAB(ACTION_IF_FOUND, ACTION_IF_NOT_FOUND)
#	Try to set up variables for the compilation and installation 
#	of MATLAB MEX files and other libraries
#--------------------------------------------------------------------

AC_DEFUN(ACDPWE_PKG_MATLAB, [ 
  AC_MSG_CHECKING(matlab directory)
  AC_ARG_WITH(matlab, [  --with-matlab=DIR       use matlab rooted at DIR],
      MATLAB_ROOT_DIR="$withval", MATLAB_ROOT_DIR=none)
  # if you want to default to best-guess matlab, 
  # use MATLAB_ROOT_DIR=/usr/local/matlab instead of none
  # Does it exist?
  if test ! -d $MATLAB_ROOT_DIR ; then
    # Did we have an action-if-not?
    dpwe_naction='$2'
    if test -z "$dpwe_naction" ; then
      AC_MSG_ERROR(Matlab root directory $MATLAB_ROOT_DIR does not exist - specify using --with-matlab)
    else
      # Do whatever we were given to do on input, but don't abort
      AC_MSG_RESULT(not found)
      $2
    fi
  else
    # Did find matlab root dir
    AC_MSG_RESULT($MATLAB_ROOT_DIR)

    # Stuff for mex compilation
  #  MEXDIR=/usr/local/lang/matlab/extern/include
  #  MEX_EXT=mexsol
    MEXDIR=$MATLAB_ROOT_DIR/extern/include
    MEX_CMEX=cmex
    MEX_CXX_FLAGS=-I$MEXDIR
    MEX_CXX_LIBS="-L/usr/lib -lc2"

    # Figure out how to do Mex based on system type
    ACDPWE_SYSTEM
    MEX_EXT=""
    case $system in
	IRIX-5.*|IRIX-6.*)
	    MEX_EXT=mexsgi
	    ;;
	OSF1-V*)
	    MEX_EXT=mexaxp
	    ;;
	SunOS-5*)
	    MEX_EXT=mexsol
	    # Appear to need this using Solaris CC (check not g++)
	    if test "$CXX" != "g++" -a "$CXX" != "gcc" -a `$CXX -v 2>&1 | grep -c gcc` = "0" ; then
	      MEX_CXX_FLAGS="$MEX_CXX_FLAGS -DNO_BUILT_IN_SUPPORT_FOR_BOOL"
	    fi
	    ;;
        Linux*)
	    MEX_EXT=mexlx
	    ;;
    esac
    if test -z "$MEX_EXT" ; then
      AC_MSG_WARN(Cannot figure out how to compile mex files on this machine)
    fi

    # Figure out how to link to -lmat
    MATLAB_INC_SPEC=-I$MATLAB_ROOT_DIR/extern/include
    case $system in
	IRIX-5.*|IRIX-6.*)
	    MATLAB_SYS_TYPE=sgi
	    ;;
	OSF1-V*)
	    MATLAB_SYS_TYPE=alpha
	    ;;
	SunOS-5*)
	    MATLAB_SYS_TYPE=sol2
	    ;;
	Linux*)
	    MATLAB_SYS_TYPE=lnx86
	    ;;
    esac
    if test -z "$MATLAB_SYS_TYPE" ; then
      AC_MSG_WARN(Cannot figure out how to link to Matlab libs on this machine)
    else
      MATLAB_LIB_SPEC="-L$MATLAB_ROOT_DIR/extern/lib/$MATLAB_SYS_TYPE -lmat"
      if test ! -f $MATLAB_LIB_SPEC/libmat.so ; then
        AC_MSG_WARN([Could not read Matlab lib $MATLAB_LIB_SPEC/libmat.so - this architecture probably not supported])
      fi
    fi
    # Once we've set up all the values, do the action_if_found
    $1
  fi
])

dnl PKG_CHECK_MODULES(GSTUFF, gtk+-2.0 >= 1.3 glib = 1.3.4, action-if, action-not)
dnl defines GSTUFF_LIBS, GSTUFF_CFLAGS, see pkg-config man page
dnl also defines GSTUFF_PKG_ERRORS on error
AC_DEFUN(PKG_CHECK_MODULES, [
  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(for $2)

        if $PKG_CONFIG --exists "$2" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING($1_CFLAGS)
            $1_CFLAGS=`$PKG_CONFIG --cflags "$2"`
            AC_MSG_RESULT($$1_CFLAGS)

            AC_MSG_CHECKING($1_LIBS)
            $1_LIBS=`$PKG_CONFIG --libs "$2"`
            AC_MSG_RESULT($$1_LIBS)
        else
            $1_CFLAGS=""
            $1_LIBS=""
            ## If we have a custom action on failure, don't print errors, but 
            ## do set a variable so people can do so.
            $1_PKG_ERRORS=`$PKG_CONFIG --errors-to-stdout --print-errors "$2"`
            ifelse([$4], ,echo $$1_PKG_ERRORS,)
        fi

        AC_SUBST($1_CFLAGS)
        AC_SUBST($1_LIBS)
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi

  if test $succeeded = yes; then
     ifelse([$3], , :, [$3])
  else
     ifelse([$4], , AC_MSG_ERROR([Library requirements ($2) not met; consider adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a nonstandard prefix so pkg-config can find them.]), [$4])
  fi
])


