/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      nl_audspec(): nonlinear processing; intended to  *
 *                                    put signal in a good domain for    *
 *                                    RASTA processing                   *
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
 *	This routine computes a nonlinearity on an fvec array (floats).
 *	Currently defined are log(x), log(1+ jah * x), and x.
 *
 *	The first time that this program is called, we do
 *	the usual allocation.
 */
struct fvec *nl_audspec( const struct param *pptr, struct fvec *audspec)
{

	int i, lastfilt;
	char *funcname;

	static struct fvec *nl_audptr = NULL; 
		/* array for nonlinear auditory spectrum */

	funcname = "nl_audspec";

	if(nl_audptr == (struct fvec *)NULL)
	{
		nl_audptr = alloc_fvec( pptr->nfilts );
	}

	lastfilt = pptr->nfilts - pptr->first_good;

	fvec_check( funcname, audspec, (lastfilt - 1) );
                /* bounds-checking for array reference */

	for(i=pptr->first_good; i<lastfilt; i++)
	{
		if(pptr->jrasta)
		{
			nl_audptr->values[i] 
				= log(1.0 + (pptr->jah)*(audspec->values[i]));
		}
		else if(pptr->lrasta)
		{
			if(audspec->values[i] < TINY)
			{
				audspec->values[i] = TINY;
			}
			nl_audptr->values[i] 
				= log((audspec->values[i]));
		}
		else /* Allow for doing no nonlinearity here */
		{
			nl_audptr->values[i] 
				= (audspec->values[i]);
		}
	}

	return( nl_audptr );
}

