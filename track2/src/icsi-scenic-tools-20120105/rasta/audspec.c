/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      audspec(): critical band analysis; takes in      *
 *                              pointer to power spectrum fvec and       *
 *                              return a pointer to crit band fvec       *
 *                                                                       *
 *                      get_trapezoidal_ranges(): computes low and high  *
 *                              edges of trapezoidal critical bands      *
 *                                                                       *
 *                      get_trapezoidal_cbweights(): get freq domain     *
 *                              weights to implement trapezoidal         *
 *                              critical bands                           *
 *                                                                       *
 *                      get_triangular_ranges(): computes low and high   *
 *                              edges of triangular critical bands       *
 *                                                                       *
 *                      get_triangular_cbweights(): get freq domain      *
 *                              weights to implement triangular critical *
 *                              bands                                    *
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
#include <limits.h>
#include "rasta.h"

/*
 *	This is the file that would get hacked and replaced if
 *	you want to change the preliminary auditory band
 * 	analysis. In original form, a power spectrum is
 *	integrated in sections to act like a band of filters with less
 *	than 1 bark spacing. This array is passed back to the
 * 	calling routine.
 *
 *	The first time that this program is called, we compute the
 * 	spectral weights for the equivalent filters.
 *	This has some magical numbers in it, but they are left
 *	in numerical form for the present; they are required for
 *	Hynek's implementation of the Zwicker critical bands.
 */

#ifndef MIN
#define MIN(a,b)	((a)<(b))?(a):(b)
#define MAX(a,b)	((a)>(b))?(a):(b)
#endif /* !MIN */

/* local function prototypes */
static	void get_trapezoidal_ranges(struct range *,
                                    struct range *,
                                    const struct param *,
                                    int,
                                    float, float );

static 	void get_trapezoidal_cbweights(struct range *,
                                       struct fvec *, 
                                       const struct param *,
                                       float);

static	void get_triangular_ranges(struct range *,
                                   struct range *,
                                   const struct param *,
                                   int,
                                   float, float);

static 	void get_triangular_cbweights(struct range *,
                                      struct fvec *, 
                                      const struct param *,
                                      float);

void get_mel_ranges(struct range *frange, struct range *cbrange, 
		    const struct param *pptr, int pspeclength, 
		    float f_warp);

void get_mel_cbweights(struct range *frange, struct fvec *cbweight, 
		       const struct param *pptr);

static int ourint(double x);

/* Allow separate rebuilding of frequency axis */
/* 2001-04-30 dpwe@ee.columbia.edu for feacalc VTN */
static struct fvec *audptr; /* array for auditory spectrum */
static struct range frange[MAXFILTS];  /* pspec filt band indices  */
static struct range cbrange[MAXFILTS]; /* cb indices for weighting */
static struct fvec *cbweightptr = NULL;

#define FRQAXIS_MEL  (1)
#define FRQAXIS_TRPZ (2)
#define FRQAXIS_TRNG (3)

int set_freq_axis(const struct param *pptr) {

  int lastfilt;
  float hz_in_fsamp;
  float f_warp = pptr->f_warp;
  int pspeclen = pptr->pspeclen;
  int axistype = ((pptr->useMel)?FRQAXIS_MEL:
		     (pptr->trapezoidal?FRQAXIS_TRPZ:FRQAXIS_TRNG));

  /* chop the spectrum as indicated by early nyquist */
  int epspeclen = 1+rint((pspeclen-1.0) * pptr->nyqhz/(pptr->sampfreq/2.));
  if (epspeclen > pspeclen) {
      epspeclen = pspeclen;		/* don't go beyond end */
  }

  /*  fprintf(stderr, "set_freq_axis; epspeclen=%d\n", epspeclen); */

  /* what is 1 Hz in fft sampling points */
  hz_in_fsamp = (float)(pspeclen - 1) / (float)(pptr->sampfreq / 2.);

  lastfilt = pptr->nfilts - pptr->first_good;

  if(cbweightptr != (struct fvec *)NULL) {
      free_fvec(cbweightptr);
      free_fvec(audptr);
  }

  /* Find the fft bin ranges for the critical band filters */
  if (axistype == FRQAXIS_MEL) {
      get_mel_ranges(frange, cbrange, pptr, epspeclen, f_warp);
  } else if (axistype == FRQAXIS_TRPZ) {
      get_trapezoidal_ranges (frange,
			      cbrange,
			      pptr, 
			      epspeclen,
			      hz_in_fsamp, f_warp);
  } else {
      /* assume FRQAXIS_TRNG */
      get_triangular_ranges (frange,
			     cbrange,
			     pptr, 
			     epspeclen,
			     hz_in_fsamp, f_warp);
  }
  
  cbweightptr = alloc_fvec(cbrange[lastfilt-1].end + 1);

  /* Now compute the weightings */
  if (axistype == FRQAXIS_MEL) {
      get_mel_cbweights(frange, cbweightptr, pptr);
  } else if (axistype == FRQAXIS_TRPZ) {
      get_trapezoidal_cbweights(frange,
				cbweightptr,
				pptr,
				hz_in_fsamp);
  } else {	/* FRQAXIS_TRNG */
      get_triangular_cbweights(frange,
			       cbweightptr,
			       pptr,
			       hz_in_fsamp);
  }
  audptr = alloc_fvec( pptr->nfilts );

  if (pptr->debug) {
      int i;

      for(i=pptr->first_good; i<lastfilt; i++) {
	  fprintf(stderr, "channel %d freq=%.0f..%.0f Hz (fft bins %d..%d)\n", 
		  i, frange[i].start/hz_in_fsamp, frange[i].end/hz_in_fsamp, 
		  frange[i].start, frange[i].end);
      }
  }

  return pptr->nfilts;
}


struct fvec *audspec( const struct param *pptr, struct fvec *pspec)
{
  int i, j, icb, icb_end;
  int lastfilt = pptr->nfilts - pptr->first_good;
  char *funcname;

  funcname = "audspec";

  if(cbweightptr == (struct fvec *)NULL) {
      set_freq_axis(pptr);
  }

  fvec_check( funcname, audptr, lastfilt - 1);
  /* bounds-checking for array reference */

  for(i=pptr->first_good; i<lastfilt; i++) {
      audptr->values[i] = 0.;

      fvec_check( funcname, pspec, frange[i].end );

      icb_end = cbrange[i].start + frange[i].end - frange[i].start;

      fvec_check( funcname, cbweightptr, icb_end);

      for(j=frange[i].start; j<=frange[i].end;j++) {
          icb = cbrange[i].start + j - frange[i].start;
          audptr->values[i] += pspec->values[j] 
	      * cbweightptr->values[icb];
      }

  }
  return( audptr );
}

/* Get the start and end indices for the critical bands using
   trapezoidal auditory filters */

void get_trapezoidal_ranges(struct range *frange,
                            struct range *cbrange, 
                            const struct param *pptr,
                            int pspeclength,
                            float hz_in_samp, 
			    float f_warp)
{
  int i, wtindex;
  char *funcname;
  float step_barks;
  float f_bark_mid, f_hz_mid, f_hz_low, f_hz_high, 
    f_pts_low, f_pts_high;

  funcname = "get_trapezoidal_ranges";

  /* compute filter step in Barks */
  step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);

  /* start the critical band weighting array index 
     where we ignore 1st and last bands,
     as these values are just copied
     from their neighbors later on. */
  wtindex = 0;
	
  /* Now store all the indices for the ranges of
     fft bins (powspec) that will be summed up
     to approximate critical band filters. 
     Similarly save the start points for the
     frequency band weightings that implement
     the filtering. */
  for(i=pptr->first_good; i<(pptr->nfilts - pptr->first_good); i++)
    {
      (cbrange+i)->start = wtindex;

      /*     get center frequency of the j-th filter 
             in Bark */
      f_bark_mid = i * step_barks;

      /*     get center frequency of the j-th filter 
             in Hz */
      f_hz_mid = 300. * (exp((double)f_bark_mid / 6.) 
                         - exp(-(double)f_bark_mid / 6.));

      /*     get low-cut frequency of j-th filter in Hz */
      f_hz_low = 300. * (exp((f_bark_mid - 2.5) / 6.) 
                         - exp(-(double)(f_bark_mid - 2.5) / 6.));

      /*     get high-cut frequency of j-th filter in Hz */
      f_hz_high = 300. * (exp((f_bark_mid + 1.3) / 6.) 
                          - exp(-(double)(f_bark_mid + 1.3) / 6.));

      f_pts_low = f_hz_low * hz_in_samp / f_warp;

      (frange+i)->start = ourint((double)f_pts_low);
      if((frange+i)->start < 0) {
          (frange+i)->start = 0;
      } else if((frange+i)->start > (pspeclength-1) ) {
          (frange+i)->start = pspeclength - 1;
      }

      f_pts_high = f_hz_high * hz_in_samp / f_warp;
      (frange+i)->end = ourint((double)f_pts_high) ;
      if((frange+i)->end < 0) {
          (frange+i)->end = 0;
      } else if((frange+i)->end > (pspeclength-1) ) {
          (frange+i)->end = pspeclength - 1;
      }

      /*      fprintf(stderr, "f_pts=%.2f..%.2f\n", f_pts_low, f_pts_high); */

      wtindex += ((frange+i)->end - (frange+i)->start);
      (cbrange+i)->end = wtindex;
      wtindex++;
    }
}

/* Get the freq domain weights for the equivalent critical band
   filters using trapezoidal auditory filters */

void get_trapezoidal_cbweights(struct range *frange,
                               struct fvec *cbweight, 
                               const struct param *pptr,
                               float hz_in_fsamp)
{
  int i, j, wtindex;
  float f_bark_mid, step_barks;
  double freq_hz, freq_bark, ftmp, logwt;
  char *funcname;

  wtindex = 0;

  funcname = "get_trapezoidal_cbweights";

  /* compute filter step in Barks */
  step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);

  for(i=pptr->first_good; i<(pptr->nfilts - pptr->first_good); i++)
    {
      /*     get center frequency of the j-th filter
             in Bark */
      f_bark_mid = i * step_barks;
      for(j=(frange+i)->start; j<=(frange+i)->end; j++)
        {
          /* get frequency of j-th spectral point in Hz */
          freq_hz = (float) j / hz_in_fsamp;

          /* get frequency of j-th spectral point in Bark */
          ftmp = freq_hz / 600.;
          freq_bark = 6. * log(ftmp + sqrt(ftmp * ftmp + 1.));

          /*     normalize by center frequency in barks: */
          freq_bark -= f_bark_mid;

          /*     compute weighting */
          if (freq_bark <= -.5) 
            {
              logwt = (double)(freq_bark + .5);
            }
          else if(freq_bark >= .5)
            {
              logwt = (-2.5)*(double)(freq_bark - .5);
            }
          else 
            {
              logwt = 0.0;
            }
          fvec_check( funcname, cbweight, wtindex );
          ftmp = cbweight->values[wtindex]
            = (float)pow(LOG_BASE, logwt);
          wtindex++;
        }
    }
}

/* Get the start and end indices for the critical bands using
   triangular auditory filters */

void get_triangular_ranges(struct range *frange,
                           struct range *cbrange, 
                           const struct param *pptr,
                           int pspeclength,
                           float hz_in_samp, 
			   float f_warp)
{
  int i, wtindex;
  char *funcname;
  float step_barks;
  float f_bark_mid, f_hz_mid, f_hz_low, f_hz_high, 
    f_pts_low, f_pts_high;

  funcname = "get_triangular_ranges";

  /* compute filter step in Barks */
  step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);

  /* start the critical band weighting array index 
     where we ignore 1st and last bands,
     as these values are just copied
     from their neighbors later on. */
  wtindex = 0;
	
  /* Now store all the indices for the ranges of
     fft bins (powspec) that will be summed up
     to approximate critical band filters. 
     Similarly save the start points for the
     frequency band weightings that implement
     the filtering. */
  for(i=pptr->first_good; i<(pptr->nfilts - pptr->first_good); i++)
    {
      (cbrange+i)->start = wtindex;

      /*     get center frequency of the j-th filter 
             in Bark */
      f_bark_mid = i * step_barks;

      /*     get center frequency of the j-th filter 
             in Hz */
      f_hz_mid = 300. * (exp((double)f_bark_mid / 6.) 
                         - exp(-(double)f_bark_mid / 6.));

      /*     get low-cut frequency of j-th filter in Hz */
      f_hz_low = 300. * (exp((f_bark_mid - 0.5) / 6.) 
                         - exp(-(double)(f_bark_mid - 0.5) / 6.));

      /*     get high-cut frequency of j-th filter in Hz */
      f_hz_high = 300. * (exp((f_bark_mid + 0.5) / 6.) 
                          - exp(-(double)(f_bark_mid + 0.5) / 6.));

      f_pts_low = f_hz_low * hz_in_samp / f_warp;

      (frange+i)->start = ourint((double)f_pts_low);
      if((frange+i)->start < 0)
        {
          (frange+i)->start = 0;
        }

      f_pts_high = f_hz_high * hz_in_samp / f_warp;
      (frange+i)->end = ourint((double)f_pts_high) ;
      if((frange+i)->end > (pspeclength-1) )
        {
          (frange+i)->end = pspeclength - 1;
        }
      wtindex += ((frange+i)->end - (frange+i)->start);
      (cbrange+i)->end = wtindex;
      wtindex++;
    }
}

/* Get the freq domain weights for the equivalent critical band
   filters using triangular auditory filters */

void get_triangular_cbweights(struct range *frange,
                              struct fvec *cbweight, 
                              const struct param *pptr,
                              float hz_in_fsamp)
{
  int i, j, wtindex;
  float f_bark_mid, step_barks;
  double freq_hz, freq_bark, ftmp;
  char *funcname;

  wtindex = 0;

  funcname = "get_triangular_cbweights";

  /* compute filter step in Barks */
  step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);

  for(i=pptr->first_good; i<(pptr->nfilts - pptr->first_good); i++)
    {
      /*     get center frequency of the j-th filter
             in Bark */
      f_bark_mid = i * step_barks;
      for(j=(frange+i)->start; j<=(frange+i)->end; j++)
        {
          /* get frequency of j-th spectral point in Hz */
          freq_hz = (float) j / hz_in_fsamp;

          /* get frequency of j-th spectral point in Bark */
          ftmp = freq_hz / 600.;
          freq_bark = 6. * log(ftmp + sqrt(ftmp * ftmp + 1.));

          /*     normalize by center frequency in barks: */
          freq_bark -= f_bark_mid;

          /*     compute weighting */
          if (freq_bark < -0.5)
            {
              cbweight->values[wtindex] = 0.0f;
            }
          else if (freq_bark < 0.0) 
            {
              cbweight->values[wtindex] = (float)(2 * freq_bark + 1.0);
            }
          else if (freq_bark < 0.5)
            {
              cbweight->values[wtindex] = (float)(-2 * freq_bark + 1.0);
            }
          else
            {
              cbweight->values[wtindex] = 0.0f;
            }

          fvec_check( funcname, cbweight, wtindex );
          
          wtindex++;
        }
    }
}

/*
void get_trapezoidal_ranges(struct range *frange,
                            struct range *cbrange, 
                            const struct param *pptr,
                            int pspeclength,
                            float hz_in_samp);
void get_trapezoidal_cbweights(struct range *frange,
                               struct fvec *cbweight, 
                               const struct param *pptr,
                               float hz_in_fsamp);
 */

#define freq2mel(f) (2595.0*log10(1.0+f/700.0))
#define mel2freq(m) (700.0*(pow(10.0,m/2595.0)-1.0))

void get_mel_ranges(struct range *frange, struct range *cbrange, 
		    const struct param *pptr, int pspeclength, 
		    float f_warp)
{   /* Setup the cbrange and cbweight structures for a mel-scaled 
       frequency division, using triangular filters (I guess) 
       ramping down to zero at the center frqs of adjacent bands. */
    double fmax = pptr->nyqhz;
    int nbins = pspeclength;
    int nchans = pptr->nfilts - 2*pptr->first_good;
    double hzPerBin = fmax/nbins;
    double melPerChan = freq2mel(fmax)/(nchans+1);
    double melMin = 0;
    double melCtr = melPerChan;
    double melTop;
    int binMin = rint(mel2freq(melMin)/f_warp/hzPerBin);
    int binCtr = rint(mel2freq(melCtr)/f_warp/hzPerBin);
    int binTop;
    int cbweightlen = 0;
    int i, ii;

    /* first pass sets up the ranges, so we know length of cbweight */
    for (i = 0; i < nchans; ++i) {
	ii = i + pptr->first_good;
	melTop = (i+2)*melPerChan;
	binTop = rint(mel2freq(melTop)/f_warp/hzPerBin);
	frange[ii].start = MAX(0, binMin + 1);
	frange[ii].end   = MIN(nbins-1, binTop-1);
	cbrange[ii].start = cbweightlen;
	cbweightlen += frange[ii].end - frange[ii].start;
	cbrange[ii].end = cbweightlen;
	++cbweightlen;
	binMin = binCtr;
	binCtr = binTop;
    }
}
    /* all that happens inbetween these two is allocate cbweight */
/*    *cbweightptr = alloc_fvec(cbweightlen); */

void get_mel_cbweights(struct range *frange, struct fvec *cbweightvec, 
		       const struct param *pptr)
{   /* fill in the cbweight structure for the weights */
    int nchans = pptr->nfilts - 2*pptr->first_good;
    int binMin, binCtr, binTop;
    int i, ii, j;
    int slopelen;
    double scale;
    float *cbweight = cbweightvec->values;

    /* cheat - we know first band center is bottom of 2nd band */
    binTop = frange[1 + pptr->first_good].start - 1;
    /* fill in all the weights */
    for (i = 0; i < nchans; ++i) {
	ii = i + pptr->first_good;
	binCtr = binTop;
	binTop = frange[ii].end + 1;
	binMin = frange[ii].start - 1;
	slopelen = binCtr - binMin;
	/* make all weight for a chan sum to 1 */
	scale = 2.0/(slopelen * (binTop - binMin));
	for (j = 1; j < slopelen; ++j) {
	    *cbweight++ = scale * j;
	}
	slopelen = binTop - binCtr;
	scale = 2.0/(slopelen * (binTop - binMin));
	for (j = 0; j < slopelen; ++j) {
	    *cbweight++ = scale * (slopelen-j);
	}
    }
}



/*
  The following code assumes:
    (1) that `double' has more bits of significance than `int'.
    (2) that conversions from `int' to `double' are made without error.
    (3) that if x and y are approximately equal double-precision values,
        that x - y will be computed without error.
  No rounding behavior is assumed, other than what is required by the
  C language (K&R or ANSI).

  Thanks to John Hauser for this implementation of the function.
*/

int
ourint(double x) {
  int result;
  double fraction;

  if (x > ((double)INT_MAX ) - 0.5) return INT_MAX;
  if (x < ((double)INT_MIN ) + 0.5) return INT_MIN;
  result = x;
  fraction = x - result;
  if (fraction < 0) {
    if (fraction == -0.5) return (result & ~0x01);
      if (fraction > -0.5) {
          return result;

      }
      else {
          return (result - 1);
      }
  }
  else {
    if (fraction == 0.5) return ((result + 1) & ~0x01);
      if (fraction < 0.5) {
        return result;
      }
      else {
        return (result + 1);
      }
  }
}
