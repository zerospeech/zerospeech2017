#!/bin/sh
#
# $Header: /u/drspeech/repos/quicknet2/qndo.sh.in,v 1.4 2005/09/03 03:07:07 davidj Exp $
#
# A script to run the correct version of the quicknet programs at ICSI

PROGNAME="qndo"
PATH=/bin:/usr/bin:/usr/sbin

cmd=`basename $0`
dir=`dirname $0`
os=`uname -s`
case $os in
SunOS)
	suffix="generic"
	;;
Linux)
	flags=`grep '^flags.*:' /proc/cpuinfo | sed -e 's/.*://'`
	for i in $flags; do
		if [ $i = "sse" ]; then
			sse=true
		fi
		if [ $i = "sse2" ]; then
			sse2=true
		fi
		if [ $i = "3dnowext" ]; then
			amd=true
		fi
	done
	if [ -n "$amd" -a -n "$sse2" ]; then
		suffix="HAMMER32SSE2"
	elif [ -z "$amd" -a -n "$sse2" ]; then
		suffix="P4SSE2"
	elif [ -n "$sse" -a -z "$sse2" ]; then
		suffix="PIIISSE1"
	else
		suffix="generic"
	fi
	;;
*)
	echo "$PROGNAME ($cms): ERROR - unknown OS/arch"
	exit 1
	;;
esac
exec ${dir}/${cmd}-${suffix} "$@"
#echo "os=$os cmd=$cmd dir=$dir sse=$sse sse2=$sse2 amd=$amd suffix=$suffix"
#echo "flags=$flags"
