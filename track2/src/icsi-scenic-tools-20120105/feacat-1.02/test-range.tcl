#!/bin/sh
# -*- Tcl -*-   The next line restarts using tclsh \
	exec tclsh $0 ${1+"$@"}

#!/usr/local/bin/tclsh
#
# test-range.tcl
#
# Script to test dpwe's Range.C object via the simple Range debug program
#
# 1998nov07 dpwe@icsi.berkeley.edu
# $Header: /u/drspeech/repos/feacat/test-range.tcl,v 1.5 2001/05/14 21:57:15 dpwe Exp $

if {[llength $argv] > 0} {
    set range [lindex $argv 0]
} else {
    puts stderr "$argv0: no path given; assuming \"./Range\""
    set range "./Range"
}

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
    # Simple list - comma-separated
    catch {exec $range -h 1,2,3,5,7,11} rslt
    set rslt
} 0 {1 2 3 5 7 11}

Test 1.2 {
    # Simple list - space separated
    catch {exec $range -h "1 2 3 5 7 11"} rslt
    set rslt
} 0 {1 2 3 5 7 11}

Test 2.1 {
    # Plain range
    catch {exec $range -h 1-10} rslt
    set rslt
} 0 {1 2 3 4 5 6 7 8 9 10}

Test 2.2 {
    # Check the internal representation
    catch {exec $range -q -d 1-10} rslt
    set rslt
} 0 {**Ranges '1-10' (npts=10):
  1:1:10
first=1 last=10 full=0 contains(0)=0}

Test 2.3 {
    # Colon instead of minus (matlab style)
    catch {exec $range -h 1:10} rslt
    set rslt
} 0 {1 2 3 4 5 6 7 8 9 10}

Test 2.4 {
    # With increment
    catch {exec $range -h 1:2:9} rslt
    set rslt
} 0 {1 3 5 7 9}

Test 2.5 {
    # .. missing the upper bound
    catch {exec $range -h 1:2:10} rslt
    set rslt
} 0 {1 3 5 7 9}

Test 2.6 {
    # Bipolar range
    catch {exec $range -h -5:5} rslt
    set rslt
} 0 {-5 -4 -3 -2 -1 0 1 2 3 4 5}

Test 2.7 {
    # Bipolar with negative step
    catch {exec $range -h 5:-2:-5} rslt
    set rslt
} 0 {5 3 1 -1 -3 -5}

Test 2.8 {
    # Bipolar range with dashes
    catch {exec $range -h -5-5} rslt
    set rslt
} 0 {-5 -4 -3 -2 -1 0 1 2 3 4 5}

Test 2.9 {
    # Dashes and negative step
    catch {exec $range -h 5--2--5} rslt
    set rslt
} 0 {5 3 1 -1 -3 -5}

Test 3.1 {
    # Default limits
    catch {exec $range -h -l 1 -u 10 :} rslt
    set rslt
} 0 {1 2 3 4 5 6 7 8 9 10}

Test 3.2 {
    # With increment
    catch {exec $range -h -l 1 -u 10 :2:} rslt
    set rslt
} 0 {1 3 5 7 9}

Test 3.3 {
    # Lower-sided
    catch {exec $range -h -l 1 -u 10 -1:} rslt
    set rslt
} 0 {-1 0 1 2 3 4 5 6 7 8 9 10}

Test 3.4 {
    # Lower-sided plus increment
    catch {exec $range -h -l 1 -u 10 3:2:} rslt
    set rslt
} 0 {3 5 7 9}

Test 3.5 {
    # Upper-sided
    catch {exec $range -h -l 1 -u 10 :9} rslt
    set rslt
} 0 {1 2 3 4 5 6 7 8 9}

Test 3.6 {
    # Upper-sided plus increment
    catch {exec $range -h -l 1 -u 10 :2:11} rslt
    set rslt
} 0 {1 3 5 7 9 11}

Test 3.7 {
    # Automatic selection of negative increment
    catch {exec $range -h -l 5 -u 1 :} rslt
    set rslt
} 0 {5 4 3 2 1}


Test 4.1 {
    # End-relative range
    catch {exec $range -h -u 10 1:^1} rslt
    set rslt
} 0 {1 2 3 4 5 6 7 8 9}

Test 4.2 {
    # Edge-relative range - both limits, expanding
    catch {exec $range -h -l 1 -u 10 ^3:^-2} rslt
    set rslt
} 0 {7 8 9 10 11 12}

Test 4.3 {
    # Edge-relative range - beginning only, with increment
    catch {exec $range -h -l 1 -u 10 ^4:2:13} rslt
    set rslt
} 0 {6 8 10 12}

Test 5.1 {
    # Simple exclusion
    catch {exec $range -h 1:9/5} rslt
    set rslt
} 0 {1 2 3 4 6 7 8 9}

Test 5.2 {
    # Multiple exclusion, arbitrary order
    catch {exec $range -h 1:9/5,3,7} rslt
    set rslt
} 0 {1 2 4 6 8 9}
    
Test 5.3 {
    # Range exclusion
    catch {exec $range -h 1:9/3-6} rslt
    set rslt
} 0 {1 2 7 8 9}
    
Test 5.4 {
    # Complex range exclusion plus list
    catch {exec $range -h 1:9/3:2:6,7,3} rslt
    set rslt
} 0 {1 2 4 6 8 9}

Test 5.5 {
    # Excessive exclusion
    catch {exec $range -h 1:9/3:2:29} rslt
    set rslt
} 0 {1 2 4 6 8}

Test 5.6 {
    # Complete exclusion
    catch {exec $range -h 1:4:9/-1:2:29} rslt
    set rslt
} 0 {}

Test 6.1 {
    # Multiple lists
    catch {exec $range -h 1:5,2:6} rslt
    set rslt
} 0 {1 2 3 4 5 2 3 4 5 6}

Test 6.2 {
    # Multiple lists, exclusions applied multiple times
    catch {exec $range -h 1:5,2:6/3,4} rslt
    set rslt
} 0 {1 2 5 2 5 6}

Test 6.3 {
    # Multiple ranges
    catch {exec $range -h "1:5;2:6"} rslt
    set rslt
} 0 {1 2 3 4 5 2 3 4 5 6}

Test 6.4 {
    # Multiple ranges, exclusions applies within range only
    catch {exec $range -h "1:5;2:6/3,4"} rslt
    set rslt
} 0 {1 2 3 4 5 2 5 6}

Test 7.1 {
    # Read spec from file
    set tmpfile "tmp.out"
    exec $range -h 1:10 > $tmpfile
    catch {exec $range -h @$tmpfile} rslt
    file delete $tmpfile
    set rslt
} 0 {1 2 3 4 5 6 7 8 9 10}

Test 7.2 {
    # Read spec from file, one number per line
    set tmpfile "tmp.out"
    exec $range 1:10 > $tmpfile
    catch {exec $range -h @$tmpfile} rslt
    file delete $tmpfile
    set rslt
} 0 {1 2 3 4 5 6 7 8 9 10}

Test 7.3 {
    # Read complex range spec from file
    set tmpfile "tmp.out"
    set f [open $tmpfile "w"]; puts $f "1:5,2-6/3-4"; close $f
    catch {exec $range -h @$tmpfile} rslt
    file delete $tmpfile
    set rslt
} 0 {1 2 5 2 5 6}

Test 7.4 {
    # Read complex range spec from file, each line is a sep range
    set tmpfile "tmp.out"
    set f [open $tmpfile "w"]; puts $f "1:5\n2-6/3-4"; close $f
    catch {exec $range -h @$tmpfile} rslt
    file delete $tmpfile
    set rslt
} 0 {1 2 3 4 5 2 5 6}

Test 7.5 {
    # Read a range from file, including a comment
    set tmpfile "tmp.out"
    set f [open $tmpfile "w"]; puts $f "  # Test file\n  \n1:5\n\n"; close $f
    catch {exec $range -h @$tmpfile} rslt
    file delete $tmpfile
    set rslt
} 0 {1 2 3 4 5}

Test 8.1 {
    # Bug in existing range (2000-01-18)!  Check it doesn't return
    set tmpfile "tmp.out"
    set tstfile [file join [file dirname [info script]] "testdata" "test-list.txt"]
    catch {exec $range @$tstfile > $tmpfile}
    catch {exec diff $tmpfile $tstfile} rslt
    file delete $tmpfile
    set rslt
} 0 {}


