/*
 * invcep.c
 *
 * Convert from cepstral coefficients back to a spectral representation.
 * For back-converting recognized frames to a spectral approximation.
 *
 * 1997apr06 dpwe@icsi.berkeley.edu
 * $Header: /n/abbott/da/drspeech/src/rasta/RCS/invcep.c,v 1.4 2001/03/16 14:10:40 dpwe Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
/* rasta includes */
#include "rasta.h"

int _cep2spec(float *incep, int ceplen, float *outspec, int specpts, 
	      double lift)
{   /* Invert a cepstrum into a (linear) spectrum (without fvecs!) */
static float *costab = NULL;
static int   costablen = 0;
static float *tmpcep = NULL;
static int   tmpceplen = 0;
    int i, j, ix;
    double tmp, ilift;

    double pionk = M_PI/(specpts-1);

    if(costablen != 2*(specpts-1)) {
	/* need to (re)allocate static structures */
	if (costab != NULL) 	free(costab);
	costablen = 2*(specpts-1);
	costab = (float *)malloc(sizeof(float)*costablen);
	for(i=0; i < costablen; ++i) {
	    costab[i] = cos(pionk*i);
	}
    }
    if(ceplen > tmpceplen) {
	/* need to (re)allocate static structures */
	if (tmpcep != NULL) 	free(tmpcep);
	tmpceplen = ceplen;
	tmpcep = (float *)malloc(sizeof(float)*tmpceplen);
    }

    /* undo the liftering */
    tmpcep[0] = incep[0];
    for (i = 1; i<ceplen; ++i) {
	tmpcep[i] = pow((double)i, -lift) * incep[i];
    }

#ifndef FIXC0
    /* scale the c0 value now rather than on transform (???) */
    tmpcep[0] *= 0.5;
#endif /* !FIXC0 */

    for (j = 0; j < specpts; ++j) {
	/* initialize this element to zero */
	tmp = 0;
	ix = 0;
	for (i = 0; i < ceplen; ++i) {
	    /* add in contributions from each cepstal term */
	    /* tmp += cepco->values[i] * cos(pionk*i*j); */
	    tmp += tmpcep[i] * costab[ix];
	    ix += j;	if(ix >= costablen)	ix -= costablen;
	}
	/* map from log spectrum back into linear */
	/* supposed to be power, so square them ? guess not */
	outspec[j] = exp( 2.0 * tmp );
    }
	
    return(specpts);
}

int _inv_postaud(float *input, float *output, int len, double srate)
{   /* invert the post-audspec compression and weighting, no fvecs */
	int i, lastfilt;
	float step_barks;
        float f_bark_mid, f_hz_mid ;
	float ftmp, fsq, nyqbar;
	double tmp;
	char funcname[] = "inv_postaud";
static float *inveql = NULL;
        int nfilts = len;
	int first_good = 1;

        /* compute filter step in Barks */
        tmp = srate / 1200.;
	nyqbar = 6. * log((srate /1200.) + sqrt(tmp * tmp + 1.));
        step_barks = nyqbar / (float)(nfilts - 1);
	lastfilt = nfilts - first_good;
	if(inveql == NULL) { /* If first time */
	    inveql = (float *)malloc(nfilts*sizeof(float));
	    for(i=first_good; i<lastfilt; i++) {
		f_bark_mid = i * step_barks;
		/* get center frequency of the j-th filter in Hz */
		f_hz_mid = 300. * (exp((double)f_bark_mid / 6.)
				- exp(-(double)f_bark_mid / 6.));
		/* calculate the LOG 40 db equal-loudness curve */
		fsq = (f_hz_mid * f_hz_mid) ;
		ftmp = fsq + 1.6e5;
		inveql[i] = (ftmp / fsq) * (ftmp / fsq)
		    * ((fsq + (float)9.61e6)
		       /(fsq + (float)1.44e6));
	    }
	}
	for(i = first_good; i<lastfilt; i++) {
		/* invert compression */
		tmp = pow((double)input[i], 1.0/COMPRESS_EXP);
		/* remove equal-loudness curve */
		output[i] = tmp * inveql[i];
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

	for(i=first_good; i > 0; i--) {
		output[i-1] = 0;
	}
	for(i=lastfilt; i < nfilts; i++) {
		output[i] = 0;
	}
	return(nfilts);
}

/* don't define these fns for non-rasta use */
#ifndef JUST_SIMPLE_INVCEP

struct fvec *cep2spec(struct fvec *incep, int nfpts, double lift)
{   /* Invert a cepstrum into a (linear) spectrum */
    char *funcname = "cep2spec";
static struct fvec *output = NULL;

    if (output == NULL || output->length != nfpts) {
	if (output != NULL) {free_fvec(output);}
	output = alloc_fvec(nfpts);
    }
    _cep2spec(incep->values, incep->length, output->values, output->length, 
	      lift);
    return output;
}

struct fvec *tolog(struct fvec *vec)
{   /* take the log of an fvec */
static struct fvec *rslt = NULL;
    int i, npts = vec->length;
    if(rslt == NULL || rslt->length != npts) {
	if(rslt != NULL)	free_fvec(rslt);
	rslt = alloc_fvec(npts);
    }
    for (i = 0; i < npts; ++i) {
	rslt->values[i] = log(vec->values[i]);
    }
    return rslt;
}

struct fvec *toexp(struct fvec *vec)
{   /* inverse of tolog() */
static struct fvec *rslt = NULL;
    int i, npts = vec->length;
    if(rslt == NULL || rslt->length != npts) {
	if(rslt != NULL)	free_fvec(rslt);
	rslt = alloc_fvec(npts);
    }
    for (i = 0; i < npts; ++i) {
	rslt->values[i] = exp(vec->values[i]);
    }
    return rslt;
}

struct fvec *spec2cep(struct fvec *spec, int ncpts, double lift)
{   /* Construct a cepstrum directly from a spectrum */
    char *funcname = "spec2cep";
static struct fvec *output  = NULL;
static struct fvec *logspec = NULL;
static struct fvec *costab  = NULL;
    int i, j, ix;
    double tmp;

/*    int ncpts = pptr->order + 1; */
/*    int nfpts = pptr->nfilts;  */
    int nfpts = spec->length;
    double pionk = M_PI/(nfpts-1);

/*    assert(nfpts == spec->length); */

    if(output == NULL || output->length != ncpts) {  
	/* need to (re)allocate static structures */
	if (output != NULL) 	free_fvec(output);
	if (logspec != NULL) 	free_fvec(logspec);
	if (costab != NULL)	free_fvec(costab);
	output = alloc_fvec(ncpts);
	logspec = alloc_fvec(nfpts);
	costab = alloc_fvec(2*(nfpts-1));
	for(i=0; i < costab->length; ++i) {
	    costab->values[i] = cos(pionk*i);
	}
    }

    /* could maybe use the same IDFT matrix as band_to_auto? */
    for(i = 0; i < logspec->length; ++i) {
	/* spec is power, so halve to get mag ? noo */
	logspec->values[i] = 0.5 * log(spec->values[i]);
    }
    for (j = 0; j < output->length; ++j) {
	tmp = logspec->values[0] 
	      + logspec->values[logspec->length-1]*pow(-1,j);
	ix = j;
	for (i = 1; i < logspec->length-1; ++i) {
	    /* calculate inner product against cos bases */
	    /* tmp += 2 * logspec->values[i] * cos(pionk*i*j); */
	    tmp += 2*logspec->values[i] * costab->values[ix];
	    ix += j;	if(ix >= costab->length)	ix -= costab->length;
	}
	output->values[j] = tmp/(nfpts-1);
    }
#ifdef FIXC0
    /* fix the C0 coef which is too big (or compensate in inversion) */
    output->values[0] *= 0.5;
#endif /* FIXC0 */

#ifndef NO_LIFT
    /* do the liftering */
    for (i = 1; i < output->length; ++i) {
	output->values[i] *= pow((double)i, lift);
    }
#endif /* NO_LIFT */
    return(output);
}

struct fvec *inv_postaud(const struct param *pptr, struct fvec *input)
{   /* invert the post-audspec compression and weighting */
	int i, lastfilt;
	float step_barks;
        float f_bark_mid, f_hz_mid ;
	float ftmp, fsq;
	double tmp;
	char funcname[] = "inv_postaud";
static float inveql[MAXFILTS];
static struct fvec *output = NULL;

        /* compute filter step in Barks */
        step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);
	lastfilt = pptr->nfilts - pptr->first_good;
	if(output == (struct fvec *)NULL) { /* If first time */
	    output = alloc_fvec( pptr->nfilts );
	    
	    for(i=pptr->first_good; i<lastfilt; i++) {
		f_bark_mid = i * step_barks;
		/* get center frequency of the j-th filter in Hz */
		f_hz_mid = 300. * (exp((double)f_bark_mid / 6.)
				- exp(-(double)f_bark_mid / 6.));
		/* calculate the LOG 40 db equal-loudness curve */
		fsq = (f_hz_mid * f_hz_mid) ;
		ftmp = fsq + 1.6e5;
		inveql[i] = (ftmp / fsq) * (ftmp / fsq)
		    * ((fsq + (float)9.61e6)
		       /(fsq + (float)1.44e6));
	    }
	}
        fvec_check(funcname, input, lastfilt - 1);

	for(i=pptr->first_good; i<lastfilt; i++) {
		/* invert compression */
		tmp = pow((double)input->values[i], 1.0/COMPRESS_EXP);
		/* remove equal-loudness curve */
		output->values[i] = tmp * inveql[i];
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

	for(i=pptr->first_good; i > 0; i--) {
		output->values[i-1] = 0;
	}
	for(i=lastfilt; i < pptr->nfilts; i++) {
		output->values[i] = 0;
	}
	return(output);
}

void lpc_to_spec(struct fvec * lpcptr, struct fvec * specptr )
{   /* convert lpc coefficients directly to a spectrum */
    int order = lpcptr->length;
    int nfpts = specptr->length ;
    /* assume the lpc params are gain*[1 -a1 -a2 -a3] etc */
    int i, j;
    double re, im, sc, cf;
    /* looking at this compared to the mapped output of lpccep, 
       nfpts is the right answer, if nfpts = nfilts */
    double pionk = M_PI/(nfpts - 1);	/* same as for cep2spec */

    sc = lpcptr->values[0];
    for(i = 0; i < specptr->length; ++i) {
	re = 1.0; im = 0;
	/* figure complex val of 1 - sum(ai.z^-i) where z = expijpi/n */
	for(j = 1; j < lpcptr->length; ++j) {
	    cf = lpcptr->values[j]/sc;
	    re += cf * cos(i*j*pionk);
	    im -= cf * sin(i*j*pionk);
	}
	/* specptr->values[i] = 1 / hypot(re, im); */
	/* Spec is power - so take square */
	specptr->values[i] = 1 / (sc* (re*re + im*im));
    }
}

/* modified version of lpccep.c:lpccep() returns lpc coefficients 
   of going straight to cepstrum */

struct fvec *spec2lpc(const struct param *pptr, struct fvec *in)
{   /* Find an LPC fit to a spectrum */
    static struct fvec *autoptr = NULL; /* array for autocor */
    static struct fvec *lpcptr; /* array for predictors */
    static struct fvec *outptrOLD; /* last time's output */
    float lpcgain;
    int i;
    char *funcname = "spec2lpc";
	
    if(autoptr == (struct fvec *)NULL) { /* Check for 1st time */
	/* Allocate space */
	autoptr = alloc_fvec( pptr->order + 1 );
	lpcptr = alloc_fvec( pptr->order + 1 );
	outptrOLD = alloc_fvec( pptr->order + 1 );
	for(i=0; i<outptrOLD->length; i++)
	    outptrOLD->values[i] = TINY;		
    }
    
    band_to_auto(pptr, in, autoptr);
    /* Do IDFT by multiplying cosine matrix times power values,
       getting autocorrelations */

    auto_to_lpc( pptr, autoptr, lpcptr, &lpcgain );
    /* do Durbin recursion to get predictor coefficients */

    if(lpcgain<=0) { /* in case of calculation inaccuracy */
	for(i=0; i<outptrOLD->length; i++)
	    lpcptr->values[i] = outptrOLD->values[i];
	fprintf(stderr,"Warning: inaccuracy of calculation -> using last frame\n");
    } else {
	if( pptr->gainflag == TRUE) {
	    norm_fvec( lpcptr, lpcgain );  /* divide by the gain */
	}
	/* Put in model gain */
    }
    for(i=0; i<lpcptr->length; i++)
	outptrOLD->values[i] = lpcptr->values[i];
    
    return( lpcptr );
}

struct fvec *lpc2cep(const struct param *pptr, struct fvec *lpcptr) 
{   /* Convert the LPC params to cepstra */
static struct fvec *outptr = NULL;
    if(outptr == NULL) {
	outptr = alloc_fvec(pptr->nout);
    }
    lpc_to_cep( pptr, lpcptr, outptr, pptr->nout);
    return outptr;
}

struct fvec *lpc2spec(const struct param *pptr, struct fvec *lpcptr)
{   /* Convert the LPC params to a spectrum */
    int nfilts = pptr->nfilts;
static struct fvec *outptr = NULL;
    if(outptr == NULL) {
	outptr = alloc_fvec(nfilts);
    }
    /* convert lpc coefs to spectral vals */
    lpc_to_spec(lpcptr, outptr);
    return(outptr);
}	

/* Covers to allow otcl access to inner rasta fns */
float my_auto_to_lpc(const struct param *pptr, struct fvec *autoptr, 
		     struct fvec *lpcptr)
{  /* do Levinson/Durbin recursion to get LPC coefs from autocorrelation vec */
    float f;
    auto_to_lpc(pptr, autoptr, lpcptr, &f);
    return f;
}

#endif /* !JUST_SIMPLE_INVCEP */
