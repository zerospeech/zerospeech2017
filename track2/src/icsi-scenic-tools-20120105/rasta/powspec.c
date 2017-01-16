/************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                         powspec(): compute some kind of power         *
 *                                    spectrum                           *
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
#include <assert.h>
#include "rasta.h"

/*
 *	This is the file that would get hacked and replaced if
 *	you want to change the basic pre-PLP spectral
 * 	analysis. In original form, it calls the usual
 *	ugly but efficient FFT routine that was translated
 *	from Fortran, and computes a simple power spectrum.
 *	A pointer to the fvec structure (float vector plus length) holding
 *	this computed array is returned to the calling program.
 *
 *      This routine is friendly to being called with different 
 *      frame lengths at different times (because ....?). 
 *      It checks the size of the currently-held output fvec, 
 *      and reallocates it if the required size changes. (dpwe 1997sep09)
 */
struct fvec *powspec( const struct param *pptr, struct fvec *fptr, float *totalE)
{
	int i, fftlength, log2length;
	char *funcname;
	static struct fvec *pspecptr = NULL;
	float noise;
	int reqpts;

	funcname = "powspec";

        /* Round up */
	log2length = ceil(log((double)(fptr->length))/log(2.0));
	fftlength = two_to_the((double)log2length);
	/* Allow space for pspec from bin 0 through bin N/2 */
	reqpts = ( (fftlength / 2) + 1);
	/* allocate the output space on the first call, or 
	   if the frame size is different from last time */
	if (pspecptr == NULL || pspecptr->length != reqpts) {
	    /* free any existing (wrong-size) structure */
	    if (pspecptr)	free_fvec(pspecptr);
	    pspecptr = alloc_fvec ( reqpts );
	    assert(reqpts == pptr->pspeclen);
	}
	/* Not currently checking array bounds for fft;
		since we pass the input length, we know that
		we are not reading from outside of the array.
		The power spectral routine should not write past
		the fftlength/2 + 1 . */
		
	fft_pow( fptr->values, pspecptr->values, 
			(long)fptr->length, (long) log2length );

	/* We may need to scale these values to line up to 
	   pseudo-downsampling - do it *before* adding noise! */
	if (pptr->nyqhz != pptr->sampfreq/2.) {
	    double nyqscale = pow(pptr->nyqhz/(pptr->sampfreq/2.), 2.0);
	    for(i=0; i<pspecptr->length; i++) {
		pspecptr->values[i] *= nyqscale;
	    }
	}
	    
	/* Calculate total power */
	if (totalE != NULL) {
	    double E = 0;
	    for(i=0; i<pspecptr->length; i++) {
		E += pspecptr->values[i];
	    }
	    *totalE = E;
	}

	if(pptr->smallmask == TRUE)
	{
		noise = (float)fptr->length;
			/* adding the length of the data vector
			to each power spectral value is roughly
			like adding a random least significant
	        	 bit to the data, (differing only
			because of window and powspeclength not
			being exactly the same as data length)
			but is less computaton
			than calling random() many times. */
		for(i=0; i<pspecptr->length; i++)
		{
			pspecptr->values[i] += noise;
		}
	}


	return( pspecptr );
}
