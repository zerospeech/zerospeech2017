/************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      rasta_filt(): does filtering on the feature      *
 *                                    trajectory of each critical band   *
 *                                                                       *
 *                      get_delta(): returns the coefficients for        *
 *                                   delta computation                   *
 *                                                                       *
 *                      get_rasta_fir(): returns steptime-adaptive       *
 *                                       rasta delta coefficients        *
 *                                                                       *
 *                      get_integ(): returns the coefficients for        *
 *                                   lossy integration                   *
 *                                                                       *
 *                      filt(): does a filter step on the single point;  *
 *                                    maintains internal history         *
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rasta.h"

#include <assert.h>

static float filt(struct fhistory *, int, float, int, struct fvec *, struct fvec *);

/* This routine computes the temporally filtered form of
   the trajectories for each of the parameters included in
   the input vector. To do this, it internally holds state
   from the histories of each parameter.
	
   The initial, default form uses a "standard" rasta
   filter, which is a bandpass filter with a delta
   calculation followed by a single-pole integrator.
   This form of the routine has no dynamic changing of
   the filters, and it calls simple routines to set
   up the integrators and differentiators to be the
   same for each filter. The calls to these routines,
   however, could be replaced with other routines
   which will put in differing filters if needed. */

struct fvec
*rasta_filt(struct fhistory *hptr,
            const struct param *pptr,
            struct fvec *nl_audspec)
{

  int i, lastfilt;
  char *funcname;

  int init;                               /* flag which says to not
                                             filter, just accumulate
                                             history */
  static int i_call = 0;

  static struct fvec *outptr = NULL;      /* array for filtered
                                             nonlinear auditory
                                             spectrum */

  static struct fvec *fir_filt[MAXFILTS]; /* array for numerator of
                                             impulse responses */

  static struct fvec *iir_filt[MAXFILTS]; /* array for denominator of
                                             impulse responses */

  funcname = "rasta_filt";

  if (hptr->eos == TRUE)
    {
      i_call = 0;
      return (struct fvec*) NULL;
    }
  else
    {
      i_call++;
      if (i_call > MAXHIST) 
        {
          i_call = MAXHIST; /* Saturate i_call since
                               we only care if it is greater
                               than the filter history length;
                               this way it will not wrap around if
                               we call this program a zillion
                               times  */
        }

      lastfilt = pptr->nfilts - pptr->first_good;

      if (outptr == (struct fvec *) NULL)
        /* first time, allocate output fvec and initialize filter
           responses */
        {
          outptr = alloc_fvec( pptr->nfilts );

          for(i = pptr->first_good; i < lastfilt; i++)
            { 
		/* fir_filt[i] = get_delta( FIR_COEF_NUM ); */
		fir_filt[i] = get_rasta_fir( pptr );
		iir_filt[i] = get_integ( pptr );
            }
        }

      fvec_check( funcname, nl_audspec, lastfilt - 1 );

      /* If no RASTA filtering, just copy array and return */

      if ((pptr->lrasta == FALSE) && (pptr->jrasta == FALSE))
        {
          fvec_copy(funcname, nl_audspec, outptr );
          return( outptr );
        }

      /* Now for all bands except first and last few (which are
         left out here but just copied in later on) */

      for(i = pptr->first_good; i < lastfilt; i++)
        {
          /* If we have enough history of the input, do
             the filter computation. */
          if ((i_call > (fir_filt[i]->length - 1)) ||
              (hptr->normInit == FALSE))
            {
              init = FALSE;
            }
          else
            {
              init = TRUE;
            }

          /* Now call the routine to do the actual filtering. */
          outptr->values[i] = filt(hptr,
                                   init,
                                   nl_audspec->values[i],
                                   i,
                                   fir_filt[i],
                                   iir_filt[i]);

          /* In case of partial RASTA, we use a mixing
             parameter */
          if(pptr->rfrac != 1.0)
            {
              outptr->values[i] = pptr->rfrac * outptr->values[i] +
                (1-pptr->rfrac) * nl_audspec->values[i];
            }
        }

      return outptr;
    }
}

/* Calculate modified FIR portion of rasta filter to respect
   requested fcupper (frequency of the zero) */
struct fvec *get_rasta_fir( const struct param *pptr )
{
    int length = 5;	/* This is intrinsic to the definition here */
    struct fvec *temp = alloc_fvec(length);
    float w = 2*M_PI * pptr->fcupper*(pptr->stepsize/1000);
    float denom = 0;
    float *array = temp->values;
    int i;

    /* check that rasta.h still does what we think */
    assert(length == FIR_COEF_NUM);

    /* The rasta filter has 4 zeros:
       - one at dc (0Hz)
       - one at fnyq (half the frame rate)
       - a complex pair on the unit circle
       This last pair is defined as being at 0.5804pi in 'classic' 
       rasta, but you may want to move it around to hit a particular
       frequency for other frame rates.  Here, we place it by 
       frq. 

       Thus, the rasta FIR polynomial is (z+1)(z-1)(2z^2 - 4cos(w)z + 2)
        == 2.z^4 - 4cos(w).z^3 + (2-2).z^2 + 4cos(w)z -2 
	== [2 1 0 -1 -2] when 4cos(w)==-1 => w = 0.5804 pi . */
    array[0] = 2;
    array[1] = -4 * cos(w);
    array[2] = 0;
    array[3] = 4 * cos(w);
    array[4] = -2;

    /* Normalize the total mag to 1.0 */
    for(i=0; i<length; i++) {
	denom += (array[i] * array[i]);
    }
    /* the original rasta filter forgot to sqrt this denom, so we 
       add a factor of sqrt(10) to make the classic values match */
    denom = sqrt(10*denom);
    for(i=0; i<length; i++) {
	array[i] /= denom;
    }

    /* Maybe display */
    if (pptr->debug) {
	static int flag = 0;
	if (!flag) {
	    fprintf(stderr, "rasta fir = %f %f %f %f %f\n", 
		    temp->values[0], temp->values[1], temp->values[2], 
		    temp->values[3], temp->values[4]);
	    flag = 1;
	}
    }

    return temp;
}   

/* Get impulse response for delta calculation */
struct fvec *get_delta( int length )
{
	int i;
	float denom;
	float *array;
	struct fvec *temp;
	int midpoint = (length-1)/2;

	if(length%2 != 1)
	{
		fprintf(stderr,"delta length must be odd\n");
		exit(-1);
	}

	temp = alloc_fvec( length );
	array = temp->values;

	denom = 0.0;
	for(i=0; i<length; i++)
	{
		array[i] = -(float)(i - midpoint);
		denom += (array[i] * array[i]);
	}
	for(i=0; i<length; i++)
	{
	    /* this should of course be array[i] /= sqrt(denom); 
	       but then we'd lose compatibility with historical rasta */
		array[i] /= denom;
	}

	return (temp );
}

/* Put single pole into iir coefficient array */
struct fvec *get_integ( const struct param *pptr )
{
	float *array;
	struct fvec *temp;

	temp = alloc_fvec( IIR_COEF_NUM );
	array = temp->values;

	if (pptr->polepos == POLE_NOTSET) {
	  /* calculate the polepos from the lower cutoff frq & the framerate */
	    array[0] = 1 - sin(2*M_PI*pptr->fclower*(pptr->stepsize/1000));
	} else {
	    array[0] = pptr->polepos;
	}

    if (pptr->debug) {
	static int flag = 0;
	if (!flag) {
	    fprintf(stderr, "rasta iir polepos = %f\n", 
		    temp->values[0]);
	    flag = 1;
	}
    }
	return (temp );
}

/* This routine implements a bandpass ``RASTA'' style filter,
   which ordinarily re-integrates a delta-style FIR differentiator
   with a single pole leaky integrator. However,
   as the fvecs for numerator and denominator are passed,
   these can be any linear-time invariant filter with
   maximum length of MAXHIST. Also, the filters can
   be different for every call, either changing over
   time or being different for each nfilt.  (Note: while
   this potential is in this routine, the calling routines
   and the command line reader do not currently handle this
   case). The passed variable
   nfilt is there so that we can maintain separate
   histories for the different channels which we are
   filtering. Because of this internal history, the routine
   is not generally callable from functions in other files.

   The routine returns the filter output value.

   The variable ``init'' is used to say whether we should filter
   or just build up the history. */

static float
filt(struct fhistory *hptr,
     int init,
     float inval,
     int nfilt,
     struct fvec* numer,
     struct fvec* denom)
{
  int j;
  float sum;


  if(nfilt >= MAXFILTS)
    {
      fprintf(stderr,"filter %d is past %d\n",
              nfilt, MAXFILTS );
      exit(-1);
    }
	
  if(numer->length >= MAXHIST)
    {
      fprintf(stderr,"filter %d needs more history than %d\n",
              numer->length, MAXHIST );
      exit(-1);
    }
	
  if(denom->length >= MAXHIST)
    {
      fprintf(stderr,"filter %d needs more history than %d\n",
              denom->length, MAXHIST );
      exit(-1);
    }
	
  /* The current initialization scheme is just to
     ignore the input until there is enough history
     in this routine to do the filtering. */

  hptr->filtIN[nfilt][0] = inval;
  sum = 0.0;

  if(init == FALSE)
    {
      /* Here we do the FIR filtering. In standard
         RASTA processing, this comes from a delta
         calculation. */
      for(j=0; j<numer->length; j++)
        {
          sum += numer->values[j] * hptr->filtIN[nfilt][j];
        }

      /* Here we would insert any nonlinear processing
         that we want between numerator and
         denominator filtering. For instance,
         for the standard case of delta coefficient
         calculation followed by integration,
         we can insert something like thresholding
         or median smoothing here. */

      /* Now we do the IIR filtering (denominator) */
      for(j=0; j<denom->length; j++)
        {
          sum += denom->values[j] * hptr->filtOUT[nfilt][j];
        }
    }

  /* Now shift the data into the histories. This could
     be done with a circular buffer, but this
     is probably easier to understand. */
  for(j=numer->length-1; j>0; j--)
    {
      hptr->filtIN[nfilt][j] = hptr->filtIN[nfilt][j-1];
    }
  for(j=denom->length-1; j>0; j--)
    {
      hptr->filtOUT[nfilt][j] = hptr->filtOUT[nfilt][j-1];
    }

  hptr->filtOUT[nfilt][0] = sum;

  return sum;
}
