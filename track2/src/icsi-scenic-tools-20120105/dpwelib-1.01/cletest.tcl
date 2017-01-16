#!/usr/local/bin/tclsh
#
# cletest.tcl
#
# Script to test the cle (command line entity) functions
# using the cletest binary
#
# 1997aug02 dpwe@icsi.berkeley.edu
# $Header: /u/drspeech/repos/dpwelib/cletest.tcl,v 1.1 1997/08/03 04:22:00 dpwe Exp $

if {[llength $argv] > 0} {
    set cmddir [lindex $argv 0]
    set datadir [file dirname $argv0]
} else {
    puts stderr "$argv0: no directory given to find cletest; assuming \".\""
    set cmddir  "."
}
set cletest "$cmddir/cletest"

proc ResultCode code {
    switch $code {
        0 {return TCL_OK}
        1 {return TCL_ERROR}
        2 {return TCL_RETURN}
        3 {return TCL_BREAK}
        4 {return TCL_CONTINUE}
    }
    return "Invalid result code $code"
}

proc OutputTestError {id command expectCode expectResult resultCode result} {
    puts stderr "======== Test $id failed ========"
    puts stderr $command
    puts stderr "==== Result was: [ResultCode $resultCode]:"
    puts stderr $result
    puts stderr "==== Expected : [ResultCode $expectCode]:"
    puts stderr $expectResult
    puts stderr "===="
}

# Test Procedure used by all tests
# id is the test identifier
# code is the test scenario
# optional -dontclean argument will stop the test classes being cleaned out
# alternatively -cleanup {script} will execute script before cleaning out

proc Test {id command expectCode expectResult args} {
    # debug
    puts "------------------------- Test $id:"

    set resultCode [catch {uplevel $command} result]

    if {($resultCode != $expectCode) ||
        ([string compare $result $expectResult] != 0)} {
        OutputTestError $id $command $expectCode $expectResult $resultCode \
                $result
    }

   if {[llength $args] == 0 || [lindex $args 0] != "-dontclean"} {
      if { [llength $args] > 0 && [lindex $args 0] == "-cleanup" } {
	  # post-completion cleanup specified
	  if { [llength $args] != 2 }  {
	      puts stderr "Test: -cleanup specified without cleanup script"
          } else {
	      set cscrpt [lindex $args 1]
 	      if {[catch {uplevel $cscrpt} rslt]} {
	          puts stderr "**Cleanup string \"$cscrpt\" returned \"$rslt\""
	      }
	  }
      }
   }
}

global results

Test 1.1 {
    # Default output
    exec $cletest
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

set usage "cleUsage: command line flags for $cletest:
 -A|--algo?rithm {-|*F|mpeg2|mp2|mr|*T|mpeg?1|mp1|}	algorithm type (no)
 -F?ixbr <float>	fixed bitrate (192000)
 -K|-nokbd 	ignore the keyboard interrupt
 -D|--de?bug <int>	debug level
  <string>	input sound file name
  <string>	output compressed file name ()"

Test 1.2 {
    # Usage message
    set rc [catch {exec $cletest --} rslt]
    lappend rc $rslt
    set rc
} 0 "1 \{cleExtractArgs: '--' not recognized
$usage\}"

Test 1.3 {
    # Normal args
    exec $cletest -A mr -F 337.3 -K -D 1 foo bar
} 0 {algoType=3 fixedBR=337.3 nokbd=1 debug=1
inpath=foo
outpath=bar}

Test 1.4 {
    # Alternate forms
    exec $cletest -nokbd --debug 1 --algo mp2 -Fixbr 337.3 foo
} 0 {algoType=2 fixedBR=337.3 nokbd=1 debug=1
inpath=foo
outpath=}

Test 1.5 {
    # Unrecognized arg amdist many other args
    set rc [catch {exec $cletest -nokbd -- -A mpeg -D 1} rslt]
    lappend rc $rslt
    set rc
} 0 "1 \{cleExtractArgs: '--' not recognized
$usage\}"

Test 2.1 {
    # Varying length
    exec $cletest --algorithm mpeg2
} 0 {algoType=2 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 2.2 {
    # Varying length
    exec $cletest --algor mpeg2
} 0 {algoType=2 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 2.3 {
    # Varying length
    exec $cletest --algo mpeg2
} 0 {algoType=2 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 2.4 {
    # Varying length
    exec $cletest --alg mpeg2
} 1 "cleExtractArgs: '--alg' not recognized
$usage"

Test 2.5 {
    # Varying length
    exec $cletest --a mpeg2
} 1 "cleExtractArgs: '--a' not recognized
$usage"

Test 3.1 {
    # Varying token length
    exec $cletest --algo mpeg1 foo
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=foo
outpath=}

Test 3.2 {
    # Varying token length
    exec $cletest --algo mpeg foo
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=foo
outpath=}

Test 3.3 {
    # Varying token length
    exec $cletest --algo mpe foo
    # --algo accepts null, so mpe gets pushed to file arg
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=mpe
outpath=foo}

Test 3.4 {
    # minus as a token
    exec $cletest --algo - - bar
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=-
outpath=bar}

Test 4.1 {
    # Boolean shorthands to symbol
    exec $cletest --algo 0
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.2 {
    # Boolean shorthands to symbol
    exec $cletest --algo no
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.3 {
    # Boolean shorthands to symbol
    exec $cletest --algo n
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.4 {
    # Boolean shorthands to symbol
    exec $cletest --algo false
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.5 {
    # Boolean shorthands to symbol
    exec $cletest --algo f
} 0 {algoType=0 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.6 {
    # Boolean shorthands to symbol
    exec $cletest --algo 1
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.7 {
    # Boolean shorthands to symbol
    exec $cletest --algo y
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.8 {
    # Boolean shorthands to symbol
    exec $cletest --algo yes
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.9 {
    # Boolean shorthands to symbol
    exec $cletest --algo t
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 4.10 {
    # Boolean shorthands to symbol
    exec $cletest --algo true
} 0 {algoType=1 fixedBR=192000.0 nokbd=0 debug=-99
inpath=(NULL)
outpath=}

Test 5.1 {
    # Repeated bool args
    exec $cletest -K -K
} 0 {algoType=0 fixedBR=192000.0 nokbd=2 debug=-99
inpath=(NULL)
outpath=}

Test 6.1 {
    # agglomerating single-char args with vals
    exec $cletest -K -D 1 -F 10 foo
} 0 {algoType=0 fixedBR=10.0 nokbd=1 debug=1
inpath=foo
outpath=}

Test 6.2 {
    # agglomerating single-char args with vals
    exec $cletest -K -D23 -F 10 foo
} 0 {algoType=0 fixedBR=10.0 nokbd=1 debug=23
inpath=foo
outpath=}

Test 6.3 {
    # agglomerating single-char args with other args
    exec $cletest -KD 345 -F 10 foo
} 0 {algoType=0 fixedBR=10.0 nokbd=1 debug=345
inpath=foo
outpath=}

Test 6.4 {
    # agglomerating single-char args with other args and vals
    exec $cletest -KD6789 -F 10 foo
} 0 {algoType=0 fixedBR=10.0 nokbd=1 debug=6789
inpath=foo
outpath=}

Test 6.5 {
    # Doesn't work for -F because it's not a simple 1chr but an abbrev
    exec $cletest -KD6789 -F10 foo
} 1 "cleExtractArgs: '-F10' not recognized
$usage"

Test 6.6 {
    # .. but can strip real 1-chr bool flags off longer flags
    exec $cletest -KFix 10 foo
} 0 {algoType=0 fixedBR=10.0 nokbd=1 debug=-99
inpath=foo
outpath=}
