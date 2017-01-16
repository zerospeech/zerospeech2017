/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      lpccep(): computes autoregressive cepstrum       *
 *                              from the auditory spectrum               *
 *                                                                       *
 *                      auto_to_lpc(): computes autoregressive coeffs    *
 *                              from the autocorrrelation                *
 *                                                                       *
 *                      lpc_to_cep(): computes cepstral coeffs           *
 *                              from the prediction vector               *
 *                                                                       *
 *                      band_to_auto(): computes autocorrelations from   *
 *                              auditory spectrum                        *   
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

/*
 *	This routine computes the lpc cepstrum from the
 *	cleaned up auditory spectrum.
 *
 *	The first time that this program is called, we do
 *	the usual allocation.
 */

struct fvec *lpccep( const struct param *pptr, struct fvec *in, int nout)
{
	static struct fvec *autoptr = NULL; /* array for autocor */
	static struct fvec *lpcptr; /* array for predictors */
	static struct fvec *outptr; /* array for cepstra */
	static struct fvec *outptrOLD; /* last array of cepstra */
	float lpcgain;
	int i;
	char *funcname;

	/* int nout = pptr->nout; */

	funcname = "lpccep";
	
	if(autoptr == (struct fvec *)NULL)  /* Check for 1st time */
	{
		/* Allocate space */
		autoptr = alloc_fvec( pptr->order + 1 );
		lpcptr = alloc_fvec( pptr->order + 1 );
		outptr = alloc_fvec( nout );
		outptrOLD = alloc_fvec( nout );
		for(i=0; i<outptr->length; i++)
			outptrOLD->values[i] = TINY;		
	}
	
	band_to_auto(pptr, in, autoptr);
	/* Do IDFT by multiplying cosine matrix times power values,
	   getting autocorrelations */

	auto_to_lpc( pptr, autoptr, lpcptr, &lpcgain );
	/* do Durbin recursion to get predictor coefficients */

	if(lpcgain<=0) /* in case of calculation inaccuracy */
	{
		for(i=0; i<outptr->length; i++)
			outptr->values[i] = outptrOLD->values[i];
		fprintf(stderr,"Warning: inaccuracy of calculation -> using last frame\n");
	}
	else
	{
		if( pptr->gainflag == TRUE)
		{
			norm_fvec( lpcptr, lpcgain );
		}
		/* Put in model gain */
		
		lpc_to_cep( pptr, lpcptr, outptr, nout);
		/* Recursion to get cepstral coefficients */
	}
	for(i=0; i<outptr->length; i++)
		outptrOLD->values[i] = outptr->values[i];
	
	return( outptr );
}


/* This routine computes the solution for an autoregressive
	model given the autocorrelation vector. 
	This routine uses essentially the
	same variables as were used in the original Fortran,
	as I don't want to mess with the magic therein. */
void auto_to_lpc( const struct param *pptr, struct fvec * autoptr, 
			struct fvec * lpcptr, float *lpcgain )
{
	float s;
	static float *alp = NULL;
	static float *rc;
	float alpmin, rcmct, aib, aip;
	float *a, *r;
	char *funcname;

	int idx, mct, mct2, ib, ip, i_1, i_2, mh;

	funcname = "auto_to_lpc";

	if(alp == (float *)NULL) /* If first time */
	{
		alp = (float *)malloc((pptr->order + 1) * sizeof(float));
		if(alp == (float *)NULL)
		{
			fprintf(stderr,"cant allocate alp\n");
			exit(-1);
		}
		rc = (float *)malloc((pptr->order ) * sizeof(float));
		if(rc == (float *)NULL)
		{
			fprintf(stderr,"cant allocate rc\n");
			exit(-1);
		}
	}

	fvec_check( funcname, lpcptr, pptr->order );
	fvec_check( funcname, autoptr, pptr->order );

	/* Move values and pointers over from nice calling
		names to Fortran names */
	a = lpcptr->values;
	r = autoptr->values;


	/*     solution for autoregressive model */

	a[0] = 1.;
    	alp[0] = r[0];
    	rc[0] = -(double)r[1] / r[0];
    	a[1] = rc[0];
    	alp[1] = r[0] + r[1] * rc[0];
	i_2 = pptr->order;
    	for (mct = 2; mct <= i_2; ++mct) 
	{
        	s = 0.;
        	mct2 = mct + 2;
        	alpmin = alp[mct - 1];
        	i_1 = mct;
        	for (ip = 1; ip <= i_1; ++ip) 
		{
            		idx = mct2 - ip;
            		s += r[idx - 1] * a[ip-1];
        	}
        	rcmct = -(double)s / alpmin;
        	mh = mct / 2 + 1;
        	i_1 = mh;
        	for (ip = 2; ip <= i_1; ++ip) 
		{
            		ib = mct2 - ip;
	    		aip = a[ip-1];
	    		aib = a[ib-1];
	    		a[ip-1] = aip + rcmct * aib;
	    		a[ib-1] = aib + rcmct * aip;
        	}
        	a[mct] = rcmct;
        	alp[mct] = alpmin - alpmin * rcmct * rcmct;
        	rc[mct-1] = rcmct;
    	}

    	*lpcgain = alp[pptr->order];
}

/* This routine computes the cepstral coefficients
	for an autoregressive model 
	given the prediction vector. 
	This routine uses essentially the
	same variables as were used in the original Fortran,
	as I don't want to mess with the magic therein. */

/* I believe this is the same algorithm as described on page 442 
   of Rabiner & Schafer (1978) "Digital processsing of speech signals"
   section 8.8.2 
   1999jun09 dpwe@icsi.berkeley.edu */

void lpc_to_cep( const struct param *pptr, struct fvec * lpcptr, 
			struct fvec * cepptr, int nout)
{
	int ii, j, l, jb;
	float sum;
	static float *c = NULL;
	float *a, *gexp;
	char *funcname;
	
	double d_1, d_2;

	funcname = "lpc_to_cep";

	if(c == (float *)NULL)
	{
		c = (float *)malloc((nout) * sizeof(float));
		if(c == (float *)NULL)
		{
			fprintf(stderr,"cant allocate c\n");
			exit(-1);
		}
	}


	fvec_check( funcname, lpcptr, pptr->order );
                /* bounds-checking for array reference */

	fvec_check( funcname, cepptr, nout - 1 );
                /* bounds-checking for array reference */

	/* Move values and pointers over from calling
		names to local array names */

	a = lpcptr->values;
	gexp = cepptr->values;

    /* Function Body */

    	c[0] = -log(a[0]);
    	c[1] = -(double)a[1] / a[0];

    	for (l = 2; l < nout; ++l) 
	{
        	if (l <= pptr->order ) 
		{
            		sum = l * a[l] / a[0];
        	} 
		else 
		{
            		sum = 0.;
        	}
        	for (j = 2; j <= l; ++j) 
		{
            		jb = l - j + 2;
            		if (j <= (pptr->order + 1) ) 
			{
                		sum += a[j-1] * c[jb - 1] * (jb - 1) / a[0];
            		}
        	}
        	c[l] = -(double)sum / l;
    	}

        gexp[0] = c[0];
    	for (ii = 2; ii <= nout; ++ii) 
	{
        	if (pptr->lift != 0.) 
		{
            		d_1 = (double) ((ii - 1));
            		d_2 = (double) (pptr->lift);
            		gexp[ii-1] = pow(d_1, d_2) * c[ii - 1];
        	} 
		else 
		{
            		gexp[ii-1] = c[ii - 1];
        	}
    	}
}


/* Do IDFT by multiplying cosine matrix times power values,
   getting autocorrelations */
void band_to_auto(const struct param *pptr, struct fvec *bandptr, 
				  struct fvec *autoptr)
{
	static double **wcosptr = (double **)NULL;
	double base_angle,tmp;
	int n_auto, n_freq;
	int i, j;
	char *funcname;
	

	funcname = "band_to_auto";
	n_auto = pptr->order+1;
	n_freq = pptr->nfilts;

	/* check dimension */
	if(bandptr->length != n_freq)
	{
		fprintf(stderr,"input veclength neq number of critical bands\n");
		exit(-1);
	}
	if(autoptr->length != n_auto)
	{
		fprintf(stderr,"output veclength neq autocorrelation length\n");
		exit(-1);
	}
	
	/* Builds up a matrix of cosines for IDFT, 1st time */
	if(wcosptr == (double **)NULL)
	{
		/* Allocate double matrix */
		wcosptr = (double **)malloc(n_auto * sizeof(double *));
		if(wcosptr == (double **)NULL)
		{
			fprintf(stderr,"Cannot malloc double ptr array (matrix of cosines)\n");
			exit(-1);
		}
		for(i=0; i<n_auto; i++)
		{
			wcosptr[i] = (double *)malloc(n_freq * sizeof(double));
			if(wcosptr[i] == (double *)NULL)
			{
				fprintf(stderr,"Cannot malloc double array (matrix of cosines)\n");
				exit(-1);
			}
		}

		/* Builds up a matrix of cosines for IDFT */
		base_angle =  M_PI / (double)(n_freq - 1);  /* M_PI is PI, defined in math.h */
		for(i=0; i<n_auto; i++)
		{
			wcosptr[i][0] = 1.0;
			for(j=1; j<(n_freq-1); j++)
			{
				wcosptr[i][j]
					= 2.0 * cos(base_angle * (double)i * (double)j);
			}
			/* No folding over from neg values for Nyquist freq */
			wcosptr[i][n_freq-1]
				= cos(base_angle * (double)i * (double)(n_freq-1));
		}
	}

	/* multiplying cosine matrix times power values */
	for(i=0; i<n_auto; i++)
	{
		tmp = wcosptr[i][0] * (double)bandptr->values[0];
		for(j=1; j<n_freq; j++)
			tmp += wcosptr[i][j] * (double)bandptr->values[j];
		autoptr->values[i] = (float)(tmp / (double)(2. * (n_freq-1)));  /* normalize */
	}
}
