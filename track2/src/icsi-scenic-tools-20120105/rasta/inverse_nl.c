/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      inverse_nl(): nonlinear processing; intended as  *
 *                              a kind of inverse for pre-rasta          *
 *                              nonlinearities                           *
 *                                                                       *
 ************************************************************************/

/***********************************************************************

 (C) 1995 US West and International Computer Science Institute,
     All rights reserved
 U.S. Patent Numbers 5,450,522 and 5,537,647

 Embodiments of this technology are covered by U.S. Patent
 Nos. 5,450,522 and 5,537,647.  A limited license for internal
 research and development use only is hereby granted.  All other
 rights, including all commercial exploitation, are retained unless a
 license has been granted by either US West or International Computer
 Science Institute.

***********************************************************************/

#include <stdio.h>
#include <math.h>
#include "rasta.h"

/*
 *	This routine computes a nonlinear function of
 *	an fvec array (floats).
 *	Currently this is  just the exp, which is the inverse
 *	of the log compression used in log rasta.
 *
 *	The first time that this program is called, we do
 *	the usual allocation.
 */
struct fvec *inverse_nonlin( const struct param *pptr, struct fvec *in)
{

  int i, lastfilt;
  char *funcname;

  static struct fvec *outptr = NULL; 
  /* array for nonlinear auditory spectrum */

  funcname = "inverse_nonlin";

  if(outptr == (struct fvec *)NULL)
    {
      outptr = alloc_fvec( pptr->nfilts );
    }

  lastfilt = pptr->nfilts - pptr->first_good;

  /* We only check on the incoming vector, as the output
     is allocated here so we know how long it is. */
  fvec_check(funcname, in, lastfilt - 1);

  for(i=pptr->first_good; i<lastfilt; i++)
    {
      if(pptr->lrasta == TRUE || pptr->jrasta == TRUE)
        {
          if (in->values[i] < LOG_MAXFLOAT)
            {
              outptr->values[i] = exp((in->values[i]));
            }
          else
            {
              if (pptr->jrasta == TRUE)
                {
                  fprintf(stderr,
                          "Warning (%s): saturating inverse nonlinearity to prevent overflow.\n",
                      funcname);
                  fprintf(stderr, "You may want to scale down your input.\n\n");
                }
              else
                {
                  fprintf(stderr,
                          "Warning (%s): saturating inverse nonlinearity to prevent overflow.\n",
                      funcname);
                  fprintf(stderr,
                          "You should run rasta with the -M option.\n\n");
                }
              outptr->values[i] = MAXFLOAT;
            }
        }
      else
        {
          outptr->values[i] = in->values[i];
        }
    }

  return( outptr );
}

