/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_utils.h,v 1.1 2012/01/05 20:32:02 dpwe Exp $
  
    This program normalizes the features in a pfile to be
    Gaussian distributed with zero mean and unit variance. 
    The features are scaled to be within +/- k standard deviations.
    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_VALUES_H
#  include <values.h> */
#endif

#include <float.h> /* for DBL_MIN */

#include <math.h>
#ifdef HAVE_SYS_IEEEFP_H
#  include <sys/ieeefp.h>
#else
#  ifdef HAVE_IEEEFP_H
#    include <ieeefp.h>
#  endif
#endif

#include <assert.h>
