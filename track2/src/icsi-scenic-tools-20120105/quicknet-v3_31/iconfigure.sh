#!/bin/sh
#
# $Header: /u/drspeech/repos/quicknet2/iconfigure.sh,v 1.11 2006/02/04 01:19:05 davidj Exp $
# 
# Some example configure command lines for quicknet.
# This assumes that this is being called from the source directory
# and builds the tree in a subdirectory using the name of the configuration
# with a "H-" prefix.
# 
# E.g.
#            cd quicknet-v3_03
#	     sh iconfigure.sh drspeech-i586-linux-generic

confname="$1"


### Some variables we use in the config command lines

# This finds the version number from the configure.in file in case
# we need it as a suffix
if [ -f  configure.in ]; then
    qnver=`fgrep AC_INIT configure.in | awk -F, ' {print $2 }' | sed -e s'/ //'`
    configure="./configure"
elif [ -f ../configure.in ]; then
    qnver=`fgrep AC_INIT ../configure.in | awk -F, ' {print $2 }' | sed -e s'/ //'`
    configure="../configure"
else 
    qnver="unknown"
fi

#
testdata="../../quicknet_testdata"


# acml at ICSI
acmlver=2.7.0
acml=acml${acmlver}
acmldir=/usr/local/lib/${acml}

# The "drspeech" directory at ICSI
drspeech="/u/drspeech"
# The version of atlas we are using in drspeech
drspeech_atlasver="3.7.11"
drspeech_atlas="atlas${drspeech_atlasver}"
# Where the testdata is in the drspeech dir
drspeech_testdata="${drspeech}/share/quicknet_testdata"

case "${confname}" in

### Some examples of generic builds on different architectures.

i586-linux-acml)
  config="--with-blas=acml --with-testdata=${testdata}
    CPPFLAGS=\"-march=athlon64  -I${acmldir}/gnu32/include\"
    LDFLAGS=\"-L${acmldir}/gnu32/lib -Wl,-rpath=${acmldir}/gnu32/lib\"
  "
  ;;


### Some examples for installing in the /u/drspeech directory at ICSI
### using ICSIs atlas install.  Note that we use suffices to distinguish
### between versions.  We have to specifiy the include/lib dirs to
### find the rtst library.

drspeech-i586-linux-generic)  
  config="--prefix /u/drspeech
    --exec-prefix /u/drspeech/i586-linux
    --with-testdata=${drspeech_testdata}
    --program-suffix=-${qnver}-generic
    CPPFLAGS=\"-m32 -I${drspeech}/i586-linux/include\"
    LDFLAGS=\"-m32 -Wl,-rpath=${drspeech}/i586-linux/lib -L${drspeech}/i586-linux/lib\"
  "
  ;;

drspeech-i586-linux-atlas-HAMMER32SSE2)
  config="--prefix /u/drspeech
    --exec-prefix /u/drspeech/i586-linux
    --with-blas=atlas
    --with-testdata=${drspeech_testdata}
    --program-suffix=-${qnver}-HAMMER32SSE2
    CPPFLAGS=\"-march=athlon64 -m32
        -I${drspeech}/i586-linux/lib/${drspeech_atlas}/HAMMER32SSE2/include
	-I${drspeech}/i586-linux/include\"
    LDFLAGS=\"-m32 -L${drspeech}/i586-linux/lib/${drspeech_atlas}/HAMMER32SSE2/lib
        -Wl,-rpath=${drspeech}/i586-linux/lib -L${drspeech}/i586-linux/lib\"
  "
  ;;

drspeech-i586-linux-atlas-P4SSE2)
  config="--prefix /u/drspeech
    --exec-prefix /u/drspeech/i586-linux
    --with-blas=atlas
    --with-testdata=${drspeech_testdata}
    --program-suffix=-${qnver}-P4SSE2
    CPPFLAGS=\"-march=pentium4 -m32
        -I${drspeech}/i586-linux/lib/${drspeech_atlas}/P4SSE2/include
	-I${drspeech}/i586-linux/include\"
    LDFLAGS=\"-m32 -L${drspeech}/i586-linux/lib/${drspeech_atlas}/P4SSE2/lib
        -Wl,-rpath=${drspeech}/i586-linux/lib -L${drspeech}/i586-linux/lib\"
  "
  ;;

drspeech-i586-linux-atlas-PIIISSE1)
  config="--prefix /u/drspeech
    --exec-prefix /u/drspeech/i586-linux
    --with-blas=atlas
    --with-testdata=${drspeech_testdata}
    --program-suffix=-${qnver}-PIIISSE1
    CPPFLAGS=\"-march=pentium3 -m32
        -I${drspeech}/i586-linux/lib/${drspeech_atlas}/PIIISSE1/include
	-I${drspeech}/i586-linux/include\"
    LDFLAGS=\"-m32 -L${drspeech}/i586-linux/lib/${drspeech_atlas}/PIIISSE1/lib
        -Wl,-rpath=${drspeech}/i586-linux/lib -L${drspeech}/i586-linux/lib\"
  "
  ;;

drspeech-sun4-sunos5-generic)
  config="--prefix /u/drspeech
    --exec-prefix /u/drspeech/sun4-sunos5
    --with-testdata=${drspeech_testdata}
    --program-suffix=-${qnver}-generic
    CC=/usr/local/lang/gcc-3.4.3/bin/gcc 
    CXX=/usr/local/lang/gcc-3.4.3/bin/g++ 
    CPPFLAGS=\"-I${drspeech}/sun4-sunos5/include\"
    LDFLAGS=\"-Wl,-R${drspeech}/sun4-sunos5/lib -L${drspeech}/sun4-sunos5/lib\"
  "
  ;;

drspeech-sun4-sunos5-atlas-SunUSIII)
  config="--prefix /u/drspeech
    --exec-prefix /u/drspeech/sun4-sunos5
    --with-blas=atlas
    --with-testdata=${drspeech_testdata}
    --program-suffix=-${qnver}-SunUSIII
    CC=/usr/local/lang/gcc-3.4.3/bin/gcc
    CXX=/usr/local/lang/gcc-3.4.3/bin/g++ 
    CPPFLAGS=\"-mcpu=ultrasparc3
        -I${drspeech}/sun4-sunos5/lib/${drspeech_atlas}/SunUSIII/include
	-I${drspeech}/sun4-sunos5/include\"
    LDFLAGS=\"-L${drspeech}/sun4-sunos5/lib/${drspeech_atlas}/SunUSIII/lib
        -Wl,-R${drspeech}/sun4-sunos5/lib -L${drspeech}/sun4-sunos5/lib\"
  "
  ;;

*)
  echo "ERROR - unknown config: ${confname}"
  exit 1
esac

echo "### configure ${config}"
echo "###"
myconfigure="iconfigure-${confname}.sh"
if [ -f ${myconfigure} ]; then
    mv ${myconfigure} ${myconfigure}.old
fi
echo '#!/bin/sh' >${myconfigure}
echo ${configure} ${config} >>${myconfigure}
chmod +x ${myconfigure}
exec sh ${myconfigure}

