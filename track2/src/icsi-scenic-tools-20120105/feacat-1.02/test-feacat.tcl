#!/usr/local/bin/tclsh
#
# test-feacat.tcl
#
# Perform some tests of feacat
#
# 1998nov03 dpwe@icsi.berkeley.edu  after test-syl.tcl
# $Header: /u/drspeech/repos/feacat/test-feacat.tcl,v 1.12 2010/10/29 03:41:53 davidj Exp $

# Do the tests right here, in the src directory

proc Exec {cmd {docatch 0}} {
    # exec the command, but echo it too
    puts $cmd
    if {$docatch} {
	eval "catch \{exec $cmd\} rslt"
    } else {
	set rslt [eval "exec $cmd"]
    }
    if {$rslt != ""} {puts "-> $rslt"}
    return $rslt
}

set datadir [file dirname [info script]]/testdata

# Things to test:
#  v- ability to read each input file type
#  v- ability to write each output file type
#  v- access to indexed and unindexed input 
#  v	- and output files
#  v- access to gzipped files for input and output
#  v- range specification: simple ranges, 
#	- complex ranges, 
#	- from file
#  v- padding, both positive and negative, even for nonindexed files
#  v- repeatutts
#  v- deslen/skip
#  v- per-utterance frame ranges
#  v- HTK format output
#  v- per-utterance output filename lists

if {[llength $argv] > 0} {
    set prog [lindex $argv 0]
} else {
    set prog "./feacat"
}

if {[llength $argv] > 1} {
    set teststodo [lrange $argv 1 e]
} else {
    set teststodo "feacat labcat linecat"
}
# Check the basic ability to filter a pfile
set fwidth 10
set pwidth 54
set junk ""

if {[lsearch $teststodo "feacat"] > -1} {

# Test reading and writing of pre files and pfiles
Exec "$prog -ipf pre -width $fwidth -opf pfile -o test.pf test-ref.pre"
lappend junk test.pf
Exec "$prog -ipf pf -opf pre -o test.pre test.pf"
lappend junk test.pre
Exec "diff test-ref.pre test.pre"

# Test reading and writing of rapbin and lna
Exec "$prog -ipf lna -width $pwidth -opf rapbin -o test.rap test-ref.lna"
lappend junk test.rap
Exec "$prog -ipf rapbin -opf lna -o test.lna test.rap"
lappend junk test.lna
Exec "diff test-ref.lna test.lna"

# Test reading and writing of gzipped files
Exec "gzip --force test.lna"
Exec "$prog -ipf lna -width $pwidth -opf rapbin -o test.rap2 test.lna.gz"
Exec "diff test.rap test.rap2"
lappend junk test.rap2
Exec "$prog -ipf rapbin -opf lna -o test.lna.gz test.rap"
Exec "gzip --force --uncompress test.lna.gz"
Exec "diff test-ref.lna test.lna"

# Test ability to read & write a pipe
Exec "rm -f test.pre"
Exec "$prog -ipf pre -width $fwidth -opf onl -o - test-ref.pre \
	| $prog -ipf onl -width $fwidth -opf pre -o test.pre -"
Exec "diff test-ref.pre test.pre"

# Check verbose reportting with indexed and nonindexed input
# Assume pf already written
Exec "$prog -ix 0 -ipf pre -width $fwidth -o /dev/null test-ref.pre -verbose |& sed -e s^$prog^feacat^ > test.verbose"
lappend junk test.verbose
Exec "diff test-ref.verbose test.verbose"

Exec "$prog -ix 1 -ipf pre -width $fwidth -o /dev/null test-ref.pre -verbose |& sed -e s^$prog^feacat^  | grep -v contains > test.verbose"
Exec "grep -v Warning test-ref.verbose | grep -v contains > test-ref-cmp.verbose"
lappend junk test-ref-cmp.verbose
Exec "diff test-ref-cmp.verbose test.verbose"

# Check it's the same for the prev-written pfile
Exec "$prog -ix 0 -ipf pf -verbose -o /dev/null test.pf |& sed -e s^$prog^feacat^ |& sed -e s/test.pf/test-ref.pre/  | grep -v contains > test.verbose"
## pfile knows seg count even with -ix 0, so EOF *not* unexpected
Exec "grep -v \"end of file\" test-ref.verbose  | grep -v contains > test-ref-cmp.verbose"
Exec "diff test-ref-cmp.verbose test.verbose"
# yes, but we no longer look at it
#Exec "test-ref.verbose > test-ref-cmp.verbose"
#Exec "diff test-ref-cmp.verbose test.verbose"

Exec "$prog -ix 1 -ipf pf -verbose -o /dev/null test.pf |& sed -e s^$prog^feacat^ |& sed -e s/test.pf/test-ref.pre/ | grep -v contains > test.verbose"
Exec "grep -v Warning test-ref.verbose | grep -v contains > test-ref-cmp.verbose"
Exec "diff test-ref-cmp.verbose test.verbose"

# Test repeatutts and sentrange against one another
Exec "$prog -ipf pf -opf pf -o test2.pf -repeatutts 3 -sr 0:2 test.pf"
lappend junk test2.pf
Exec "$prog -ipf pf -opf pre -o test1.pre -sr 3:5 test2.pf"
lappend junk test1.pre
Exec "$prog -ipf pf -opf pre -o test2.pre -sr 1,1,1 test.pf"
lappend junk test2.pre
Exec "diff test1.pre test2.pre"

# Test of trimming (negative padding), per-utterance ranging, 
# and writing to nonindexed files
Exec "$prog -ipf pf -opf pf -o test1.pf -sr 0:1 -pr 5:^5 test.pf"
lappend junk test1.pf
Exec "$prog -ipf pf -opf pre -o - -sr 0:1 test.pf | $prog -ipf pre -width 10 -opf pf -pad -5 -o test2.pf"
Exec "diff test1.pf test2.pf"

# Test (positive) padding
Exec "$prog -ipf pf -opf pf -o test1.pf -sr 0 -pr 1,1,1:2:41,41,41 test.pf"
Exec "$prog -ipf pf -opf pre -o - -sr 0 -pr 1:41 test.pf | $prog -ipf pre -width 10 -opf pf -pad 2 -pr 0:2:40 -o test2.pf"
Exec "diff test1.pf test2.pf"

# Test concat of multiple files
Exec "$prog -ipf pf -opf pre -o test1.pre test.pf"
Exec "$prog -ipf pf -opf pf -o test1.pf test.pf test.pf"
Exec "cat test1.pre test1.pre | $prog -ipf pre -width 10 -o test2.pf"
Exec "diff test1.pf test2.pf"

# Test sentence range striding multiple files
Exec "$prog -ipf pf -opf pf -o test1.pf test.pf test.pf test.pf -sr 0:3:"
Exec "$prog -ipf pf -opf pf -o test2.pf test.pf -sr 0,3,6,9,2,5,8,1,4,7"
Exec "diff test1.pf test2.pf"

# Same for online input
# This one actually returns nonzero because it's an error to have an 
# undetermined utterance count (unindexed, online input) when using 
# an explicit utterance range.  However, it should write the correct 
# output file, so we'll ignore the error (docatch=1)
Exec "cat test1.pre test1.pre test1.pre | $prog -ipf pre -width 10 -o test1.pf -sr 0:3:" 1
Exec "diff test1.pf test2.pf"

# Test switching between input files
Exec "$prog -ipf pf -opf pf -o test1.pf test.pf test.pf -sr 0,10,1,11"
Exec "$prog -ipf pf -opf pf -o test2.pf test.pf -sr 0,0,1,1"
Exec "diff test1.pf test2.pf"

# .. and without indexing?
Exec "$prog -ix 0 -ipf pf -opf pf -o test1.pf test.pf test2.pf -sr 0,10,1,12"
Exec "diff test1.pf test2.pf"
# This wouldn't work except for a pilfe, because for other file types 
# feacat has to read all the way to the end of the first file 
# to find out how long it is.

# Test handling of infinite range end vs. explicit
# When input is not indexed, have to guess the final value, and get EOF
# by running off end, not an error.  But if we run off end chasing 
# an utterance explicitly requested by the user, it *is* an error
# Test reading and writing of pre files and pfiles
# sr 0: not an error
Exec "$prog -sr 0: -ipf pre -width $fwidth -opf pfile -o test.pf test-ref.pre"
# sr 0:100 *is*
set rslt [Exec "$prog -sr 0:100 -ipf pre -width $fwidth -opf pfile -o test.pf test-ref.pre" 1]
if {![string match "*: ERROR: requested utterance index 10 exceeds total utterance count of 10" $rslt]} {
    error "No error when requesting nonexistent utterance"
}

# Test reading deslen file
set f [open "test.list" w]
puts $f "10 20 "
puts $f " 30 	40"
puts $f "\n50\n # Comment\n 60 "
puts -nonewline $f "70"
close $f
lappend junk test.list
# Make desired output
set f [open "test-ref.rslt" w]
puts $f "test.pf\n0 10\n1 20\n2 30\n3 40\n4 50\n5 60\n6 70\n7 sentences, 280 frames, 10 features."
close $f
lappend junk test-ref.rslt
# Ok, test
Exec "$prog -ipf pre -width $fwidth -sr 0:6 -deslenfile test.list -o test.pf test-ref.pre"
Exec "$prog -q -v test.pf |& grep -v utt > test.verbose"
Exec "cmp test.verbose test-ref.rslt"

# Test skipfile
set f [open "test2.list" w]
puts -nonewline $f "1 2 3 4 5 6 7 8 9 10"
close $f
lappend junk test2.list
Exec "$prog -skipfile test2.list -o test2.pf test.pf"
Exec "$prog -sr 4 -o test3.pf test2.pf"
Exec "$prog -ipf pre -width $fwidth -sr 4 -pr 5:49 -o test2.pf test-ref.pre"
Exec "cmp test3.pf test2.pf"
# Test skipfile & deslenfile together
Exec "$prog -ipf pre -width $fwidth -sr 0:6 -deslenfile test.list -skipfile test2.list -o test.pf test-ref.pre"
Exec "$prog -sr 4 -o test3.pf test.pf"
Exec "$prog -ipf pre -width $fwidth -sr 4 -pr 5:54 -o test2.pf test-ref.pre"
Exec "cmp test3.pf test2.pf"

# Test recently-added HTK output format
Exec "$prog -ipf pre -width $fwidth -opf htk -o test.htk test-ref.pre"
lappend junk test.htk
# - try without indexing first
Exec "$prog -ipf htk -opf pre -ix 0 -o test.pre test.htk"
Exec "diff test-ref.pre test.pre"
# - and now with indexing
Exec "$prog -ipf htk -opf pre -ix 1 -o test.pre test.htk"
Exec "diff test-ref.pre test.pre"

# Test per-utterance output list files
Exec "echo test1.htk test2.htk test3.htk > test.list"
lappend junk test1.htk
lappend junk test2.htk
lappend junk test3.htk
Exec "$prog -ipf pre -width $fwidth -opf htk -ol test.list -sr 0:2 test-ref.pre"
Exec "$prog -ipf pre -width $fwidth -opf htk -o test.htk -sr 0 test-ref.pre"
Exec "cmp test.htk test1.htk"
Exec "$prog -ipf pre -width $fwidth -opf htk -o test.htk -sr 1 test-ref.pre"
Exec "cmp test.htk test2.htk"
Exec "$prog -ipf pre -width $fwidth -opf htk -o test.htk -sr 2 test-ref.pre"
Exec "cmp test.htk test3.htk"

# Test transforms (-tr)
# First, check softmax against a previous version. 
# May get small variations due to numeric differences...
Exec "$prog -ipf pre -width $fwidth -opf pfile -tr softmax -opf lna -o test.lna test-ref.pre"
Exec "cmp -l test.lna test-ref-softmax.lna"
# Now, check that softmax matches exponentiation followed by normalization
Exec "$prog -ipf pre -width $fwidth -tr exp -opf pf -o test.pf test-ref.pre"
Exec "$prog -ipf pf -width $fwidth -tr norm -opf lna -o test.lna test.pf"
Exec "cmp -l test.lna test-ref-softmax.lna"

# Test 'pasting' of files (merging feature vectors 'side by side' 
# instead of end-to-end)
# First, break our test pf into three bits
Exec "$prog -fr 0:2 -o test1.pf test.pf"
Exec "$prog -fr 3:6 -o test2.pf test.pf"
Exec "$prog -fr 7:[expr $fwidth - 1] -o test3.pf test.pf"
# Now, paste them back together...
Exec "$prog -o test4.pf -usedoubleslash test1.pf//test2.pf//test3.pf"
lappend junk test4.pf
# Should match original
Exec "cmp test4.pf test.pf"

}

if {[lsearch $teststodo "labcat"] > -1} {

# Quick little test of labcat
regsub {feacat([^/]*)$} $prog {labcat\1} lprog
Exec "$lprog -ipf pf -opf pf -o test0.pflab -repeatutts 3 -sr 0:2 test-ref.pflab"
lappend junk test0.pflab
Exec "$lprog -ipf pf -opf pf -o test1.pflab -sr 3:5 test0.pflab"
lappend junk test1.pflab
Exec "$lprog -ipf pf -opf pf -o test2.pflab -sr 1,1,1 test-ref.pflab"
lappend junk test2.pflab
Exec "diff test1.pflab test2.pflab"

}

if {[lsearch $teststodo "linecat"] > -1} {

# Test of linecat.  It's a separate program, so we should be more suspicious
regsub {feacat([^/]*)$} $prog {linecat\1} iprog
regsub {feacat([^/]*)$} $prog {Range\1} rangeprog
Exec "$rangeprog 0:9 > tmp.list"
lappend junk tmp.list 
Exec "$iprog -r 0:2:8 -o tmp3.list < tmp.list"
Exec "$rangeprog 0:2:8 > tmp2.list"
lappend junk tmp2.list
lappend junk tmp3.list
Exec "diff tmp2.list tmp3.list"
# Test switching between multiple files
Exec "$rangeprog 100:109 > tmp2.list"
Exec "$rangeprog 2,1,101,3,103 > tmp3.list"
Exec "$iprog -r 2,1,11,3,13 -o tmp4.list tmp.list tmp2.list"
lappend junk tmp4.list
Exec "diff tmp3.list tmp4.list"
# Test index base
Exec "$rangeprog 2:6 > tmp2.list"
Exec "$iprog -ixb 2 -r 4:8 tmp.list > tmp3.list"
Exec "diff tmp2.list tmp3.list"
# Test with and without seekable
Exec "$rangeprog 100:109 > tmp2.list"
Exec "$iprog -r 2,1,11,3,13 -o tmp3.list -seek 0 tmp.list tmp2.list"
Exec "$iprog -r 2,1,11,3,13 -o tmp4.list -seek 1 tmp.list tmp2.list"
Exec "diff tmp3.list tmp4.list"
# There was a bug with it dumping core when reading off the end 
# of an infinite range (2000-08-02)
Exec "$iprog -r all/15 -o tmp3.list tmp.list tmp2.list"
Exec "$iprog -r 0:14,16:19 -o tmp4.list tmp.list tmp2.list"
Exec "diff tmp3.list tmp4.list"
}

puts " *** Tests completed successfully! *** "

# Cleanup
Exec "rm $junk"

