#! /bin/sh
# -*- Tcl -*-   The next line restarts using tclsh \
exec tclsh $0 ${1+"$@"}

#!/usr/local/bin/tclsh
#
# rasta.tcl
#
# Map the command-line args to original "rasta" into those used by feacalc.
# Mainly, this allows the rasta/testsuite tests to be performed by 
# feacalc.  Might also serve as a translation reference, although when 
# acting as rasta, feacalc gives essentially identical answers only 
# a little slower (so you'd use rasta rather than this script).
#
# 1997aug26 dpwe@icsi.berkeley.edu
# $Header: /u/drspeech/repos/feacalc/rasta.tcl,v 1.9 2007/08/30 20:30:51 janin Exp $

# We'll build up this string, then "eval exec" it
set machtype [exec $env(SPEECH_DIR)/share/bin/speech_arch]
set cmd "[file dirname [info script]]/H-$machtype/feacalc -delta 0 -window 20 -step 10 -domain cep"

# default file format
set ffmt "PCM/R\${srate}"
set order "8"
set srate "8000"
set opfmt "raw"
set opswap ""
set listfile ""
set postcmd ""

set i 0
while {$i < [llength $argv]} {
    set arg [lindex $argv $i]
    set next [lindex $argv [expr $i+1]]
    incr i
    switch -- $arg {
	-P	{append cmd " -domain lin -compress yes"; set opfmt "ascii"; set order "no"}
	-R	{append cmd " -domain lin -compress no"; set opfmt "ascii"; set order "no"}
	-S	{append cmd " -samplerate $next"; set srate $next; incr i}
	-s	{append cmd " -step $next"; incr i}
	-w	{append cmd " -window $next"; incr i}
	-y	{append cmd " -pad"}
	-k	{set postcmd " -"; set opfmt "online"; append ffmt "Abb"}
	-O	{set postcmd " -"}
	-U	{set opswap "swapped"}
	-e	{set ffmt "ESPS"}
	-z	{set ffmt "NIST"}
	-T	{append ffmt "El"}
	-o	{append cmd " -out $next"; incr i}
	-i	{append cmd " $next"; incr i}
	-L	{append cmd " -rasta log"}
	-J	{append cmd " -rasta jah"}
	-m	{set order $next; incr i}
	-n	{append cmd " -cepsord $next"; incr i}
	-q	{append cmd " -deltaorder $next"; incr i}
	-Q	{append cmd " -deltawin $next"; incr i}
	-F	{append cmd " -hpf"}
	-H 	{append cmd " -history $next"; incr i}
	-h	{append cmd " -readhist"}
	-f	{append cmd " -mapfile $next"; incr i}
	-X	{set opfmt "HTK"}
	-I	{set listfile $next; incr i}
	-M	{append cmd " -dith"}
	-c	{append cmd " -numfilts $next"; incr i}
	default {puts stderr "unsupported option '$arg' in '$argv'"}
    } 
}
#	-K	{append cmd " -strutmode"}

# Add on the default formats 
append cmd " -plp $order -ipformat $ffmt -opformat $opswap$opfmt"

# bit that comes last
append cmd $postcmd

puts stderr "$cmd"
fconfigure stdin -translation binary -buffering none
fconfigure stdout -translation binary -buffering none

if {$listfile != ""} {
    # List of input/output file name pairs - read it
    set lf [open $listfile "r"]
    while {![eof $lf]} {
	set pair [gets $lf]
	set infile [lindex $pair 0]
	set oufile [lindex $pair 1]
	if {$infile != "" && $oufile != ""} {
	    set fcmd "$cmd $infile -out $oufile"
	    catch {eval "exec $fcmd >@ stdout"} rslt
	    puts stderr $rslt
	}
    }
    close $lf
} else {
    catch {eval "exec $cmd >@ stdout"} rslt
    puts stderr $rslt
}
