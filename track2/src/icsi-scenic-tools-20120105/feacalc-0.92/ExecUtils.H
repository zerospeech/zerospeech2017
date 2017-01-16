//
// ExecUtils.H
//
// A few routines to provide some of the functionality of Tcl's "exec" 
// for a C++ program (i.e. convert a string into the stdout of it 
// when executed).
//
// 1997aug14 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/ExecUtils.H,v 1.4 2012/01/05 20:30:15 dpwe Exp $

// Originally based on /u/drspeech/src/berpdemo/berpbackend/dbquery.C

#include <stdio.h>

int exuCreateCommandPipes(char *cmd, char *argv[], FILE **myin, FILE **myout);
    /* Execute the named command with the specified arguments, 
       and return files to its input and output.
       Return child pid on success, 0 if the command couldn't launch. */

int exuSearchExecPaths(char *cmd, char *paths, char *rslt, int rsltlen);
    /* Search through the WS or colon-separated list of paths
       for an executable file whose tail is cmd.  Return absolute 
       path in rslt, which is allocated at rsltlen.  Return 1 
       if executable found & path successfully returned, else 0. */

int exuParseToArgv(char *cmd, char *argv[], int maxargv);
    /* Take a single string with spaces and parse it into an argv array;
       Basically, break the string up on white space, but allow quoting.
       Fill in upt to maxargv elements in the pre-allocated argv array, 
       but return the actual argc we would have if argv were large enough.  
       Return 0 under error conditions. */

/* This is the one that calls all the above ones */
char* exuExecCmd(char *cmd);
    /* Take a command and run it as passed; return its stdout 
       as a single, newly-allocated string, with any trailing 
       newline stripped (but interior newlines kept).  Return NULL 
       on error. */

/* This is just a utility often used in conjunction */
int exuExpandPercents(const char *input, char *output, int outlen, 
		      int nsubs, const char *subchrs, char **substrs);
	/* Take a string containing %x type strings, and expand them.  
	   Recognize <nsubs> substitutions, defined by subchrs[i]; 
	   %<subchrs[i]> becomes <substrs[i]>. 
	   <output> is pre-allocated output string, <outlen> long; 
	   return 0 if that's not big enough. */
