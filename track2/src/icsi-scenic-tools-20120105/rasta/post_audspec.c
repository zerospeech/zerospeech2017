/************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      post_audspec(): auditory processing that is post *
 *                                    RASTA processing, typically        *
 *                                    approximations to loudness         *
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
 *	This is the file that would get hacked and replaced if
 *	you want to change the preliminary post-auditory band
 * 	analysis. In original form, a critical band -like
 *	analysis is augmented with an equal loudness curve
 *	and cube root compression.

 *	The first time that this program is called, 
 *	in addition to the usual allocations, we compute the
 * 	spectral weights for the equal loudness curve.
 */
struct fvec *post_audspec( const struct param *pptr, struct fvec *audspec)
{

	int i, lastfilt;
	char *funcname;
	float step_barks;
        float f_bark_mid, f_hz_mid ;
	float ftmp, fsq;
	static float eql[MAXFILTS];

	static struct fvec *post_audptr = NULL; /* array for post-auditory 
						spectrum */

        /* compute filter step in Barks */
        step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);

	lastfilt = pptr->nfilts - pptr->first_good;

	funcname = "post_audspec";

	if(post_audptr == (struct fvec *)NULL) /* If first time */
	{
		post_audptr = alloc_fvec( pptr->nfilts );

		for(i=pptr->first_good; i<lastfilt; i++)
		{
			f_bark_mid = i * step_barks;

			/*     get center frequency of the j-th filter
					in Hz */
			f_hz_mid = 300. * (exp((double)f_bark_mid / 6.)
				- exp(-(double)f_bark_mid / 6.));

		/*     calculate the LOG 40 db equal-loudness curve */
		/*     at center frequency */

			fsq = (f_hz_mid * f_hz_mid) ;
			ftmp = fsq + 1.6e5;
			eql[i] = (fsq / ftmp) * (fsq / ftmp)
				* ((fsq + (float)1.44e6)
				/(fsq + (float)9.61e6));
			
		}
	}

        fvec_check( funcname, audspec, (lastfilt - 1) );

	for(i=pptr->first_good; i<lastfilt; i++)
	{
		/* Apply equal-loudness curve */
		post_audptr->values[i] = audspec->values[i] * eql[i];

		/* Apply equal-loudness compression */
		post_audptr->values[i] = 
			pow( (double)post_audptr->values[i], COMPRESS_EXP);
	}

	/* Since the first critical band has center frequency at 0 Hz and
	bandwidth 1 Bark (which is about 100 Hz there) 
	we would need negative frequencies to integrate.
	In short the first filter is JUNK. Since all-pole model always
	starts its spectrum perpendicular to the y-axis (its slope is
	0 at the beginning) and the same is true at the Nyquist frequency,
	we start the auditory spectrum (and also end it) with this slope
	to get around this junky frequency bands. This is not to say
	that this operation is justified by anything but it seems
	to do no harm. - H.H. 

	8-8-93 Morgan note: in this version, as per request from H.H.,
	the number of critical band filters is a command-line option.
	Therefore, if the spacing is less than one bark, more than
	one filter on each end can be junk. Now the number of
	copied filter band outputs depends on the number
	of filters used. */

	for(i=pptr->first_good; i > 0; i--)
	{
		post_audptr->values[i-1] = post_audptr->values[i];
	}
	for(i=lastfilt; i < pptr->nfilts; i++)
	{
		post_audptr->values[i] = post_audptr->values[i-1];
	}
		
	return( post_audptr );
}
