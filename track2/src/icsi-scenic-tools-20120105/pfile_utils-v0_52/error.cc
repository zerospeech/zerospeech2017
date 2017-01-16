/*
    $Header: /u/drspeech/repos/pfile_utils/error.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
    Simple fatal error function.
    Jeff Bilmes <bilmes@cs.berkeley.edu>
    $Header: /u/drspeech/repos/pfile_utils/error.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
*/


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


#include "error.h"

void
error(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  /* print out remainder of message */
  (void) vfprintf(stderr, format, ap);
  va_end(ap);
  (void) fprintf(stderr, "\n");
  (void) exit(EXIT_FAILURE);
}
