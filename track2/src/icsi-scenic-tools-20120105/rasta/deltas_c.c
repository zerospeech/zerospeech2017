/*
 * deltas_c.c - C-only version of deltas.C
 *
 * Calculate deltas on the fly
 * for sequential output from rasta to pfile
 * 1997jun18 dpwe@icsi.berkeley.edu
 */
static char rcsid[] = "$Header: /n/abbott/da/drspeech/src/rasta/RCS/deltas_c.c,v 1.7 2001/03/16 14:09:39 dpwe Exp $";

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rasta.h"

/*
 struct fvec {
   float *values;
   int length;
 };

 struct fmat {
   float **values;
   int nrows;
   int ncols;
 };
*/

#ifndef MIN
#define MIN(a,b)	((a)<(b))?(a):(b)
#define MAX(a,b)	((a)>(b))?(a):(b)
#endif /* !MIN */

#ifdef DONT_USE_PHP

/* static */ struct fvec * MultCols(struct fmat* pstate, struct fvec *pfilt, 
			      int offset, struct fvec* prslt)
{   /* return the inner product of <filt> with each *column* of <state>;
       the <filt> is offset to be applied starting from the <offset>th 
       row of <state>*/
    int i, c;
    float f = 0;

    if(prslt) {
	assert(prslt->length == pstate->ncols);
    } else {
	prslt = alloc_fvec(pstate->ncols);
    }
    for(c = 0; c < pstate->ncols; ++c) {
	f = 0;
	/* By doing this sum in reverse, I can make the results 
	   exactly match the output of calc_deltas_v0_03 as 
	   currently compiled (forward sum didn't) */
	for(i = pfilt->length-1; i >= 0; --i) {
	    f += pstate->values[offset+i][c] * pfilt->values[i];
	}
	prslt->values[c] = f;
    }
    return prslt;
}

#else /* !DONT_USE_PHP - use phipac matrix mult to do delta calc */

extern void php_mul_vfmf_vf(const size_t rows, const size_t cols,
		     const float* vec, const float* mat, float* res);

#define MultCols(fmat,fvec,offs,prslt) \
     assert(offs==0);	\
     assert(fmat->nrows == fvec->length /* rows of a column vector */); \
     assert(fmat->ncols == (*(prslt)).length); \
     php_mul_vfmf_vf(fmat->nrows, fmat->ncols, fvec->values, fmat->values[0], (*(prslt)).values);

#endif /* USE_PHP */

static struct fvec * MakeDeltaFilt(int len, struct fvec *fv)
{   /* build a filter for delta calculation */
    int i, mp = (len-1)/2;
    double sumsq = 0;

    if(fv) {
	assert(len == fv->length);
    } else {
	fv = alloc_fvec(len);
    }
    fv->values[mp] = 0;
    for(i = 1; i <= mp; ++i) {
	fv->values[mp+i] = i;
	fv->values[mp-i] = -i;
	sumsq += 2*i*i;
    }
    /* .. should divide each pt by sumsq, but actually norm is not 
       to normalize */
    return fv;
}

static struct fvec * ConvFRs(struct fvec *f1, struct fvec *f2, 
			     struct fvec *fv)
{   /* convolve two floatVecs to produce a third */
    int i, j, convlen;
    int oplen = f1->length + f2->length - 1;
    float f, *pf1, *pf2;

    if(fv) {
	assert(fv->length == oplen);
    } else {
	fv = alloc_fvec(oplen);
    }
    for(i = 0; i < fv->length; ++i) {
	if(i < f1->length) {
	    pf1 = f1->values + (f1->length - 1 - i);
	    pf2 = f2->values;
	    convlen = MIN(f2->length, i+1);
	} else {
	    pf1 = f1->values;
	    pf2 = f2->values + i + 1 - f1->length;
	    convlen = MIN(f1->length, f2->length - (i + 1 - f1->length));
	}
	f = 0;
	for(j = 0; j < convlen; ++j) {
	    f += *pf1++ * *pf2++;
	}
	fv->values[i] = f;
    }
    return fv;
}

static struct fvec *MakeDoubleDeltaFilt(int len, struct fvec *fv)
{   /* build a filter for double-delta calculation - convol'n of 2 dfilts */
    int i, dlen = (len+1)/2;
    struct fvec *dfilt = MakeDeltaFilt(dlen, NULL);
    struct fvec *ddfilt = ConvFRs(dfilt, dfilt, fv);
    /* apparently, have to flip sign?? */
    for (i = 0; i < ddfilt->length; ++i) {
	ddfilt->values[i] *= -1.0;
    }
    free_fvec(dfilt);
    return ddfilt;
}

static struct fvec *MakeStrutDoubleDeltaFilt(int len, struct fvec *fv)
{   /* STRUT conventionally uses a weird double-delta filter. 
       Fake it for now. */
    if(fv) {
	assert(len == fv->length);
    } else {
	fv = alloc_fvec(len);
    }
    if (len != 7) {
	fprintf(stderr, "MakeStrutDoubleDeltaFilt: Only know 7 pt dd filt, not %d\n", len);
	abort();
    }
    /* filter from strut header */
    fv->values[0] = 2;
    fv->values[1] = 1;
    fv->values[2] = -2;
    fv->values[3] = -2;
    fv->values[4] = -2;
    fv->values[5] = 1;
    fv->values[6] = 2;
    return fv;
}

void DeltaCalc_reset(DeltaCalc *dc) {
    /* return to initial state */
    dc->rownum = 0;
    dc->unpadding = 0;
    dc->retvec->length = dc->nOutputFtrs;
}

static void dcAllocState(DeltaCalc *dc, int feas)
{   /* Perform the remainder of the initialization that depends on 
       the number of features in each vector.  We separate this 
       out from the main initialization so that it can in fact 
       be deferred until the first frame is submitted, if the 
       programmer doesn't want to guess in advance the width of 
       his feature vectors. */
    int winlen = dc->dfilt->length;
    dc->nfeas = feas;
    dc->nOutputFtrs = (dc->degree+1)*dc->nfeas; 
    dc->state = alloc_fmat(winlen, feas);
    dc->retvec = alloc_fvec(dc->nOutputFtrs);
}

DeltaCalc* alloc_DeltaCalc(int feas, int winlen, int dgree, int strut_mode)
{   /* Initialize the object that will "filter" feature vectors 
       to return them augmented by their deltas. 
       <feas> is the size of the vectors we will be passed; <winlen> 
       is the window length over which deltas are to be calculated, 
       and <dgree> is 1 for plain deltas, or 2 for deltas and 
       double deltas. 
       If <feas> == 0, defer actually doing the alloc until we 
       see the first frame, and get the number of features from it.  
       strut_mode uses a different double-delta filter, for compatibility 
       with STRUT's version of RASTA. */
    DeltaCalc *dc = (DeltaCalc *)malloc(sizeof(DeltaCalc));
    assert(dc);
    dc->dfilt = alloc_fvec(winlen);
    dc->ddfilt = alloc_fvec(winlen + (strut_mode?2:0));
    dc->rownum = 0;
    dc->unpadding = 0;
    dc->firstpad = (winlen+1)/2;
    dc->minrows = winlen - dc->firstpad;
    dc->degree  = (dgree==0)?0:((dgree==2)?2:1);
    dc->middle  = dc->firstpad - 1;
    dc->dfoffset = 0;
    if(dc->degree > 0) {
	/* setup the values in the dfilt and ddfilt filters. */
	MakeDeltaFilt(winlen, dc->dfilt);
	if (strut_mode && dc->degree == 2) {
	    int i;
	    struct fvec *new_dwin = alloc_fvec(winlen+2);
	    /* build the special-case STRUT ddelta window */
	    MakeStrutDoubleDeltaFilt(winlen+2, dc->ddfilt);
	    /* also fix up everything else for winlen+2 */
	    new_dwin->values[0] = 0;
	    new_dwin->values[winlen+1] = 0;
	    for (i=0; i<winlen; ++i) {
		new_dwin->values[1+i] = dc->dfilt->values[i];
	    }
	    free_fvec(dc->dfilt);
	    dc->dfilt = new_dwin;
	    dc->firstpad++;
	    dc->minrows++;
	    dc->middle++;
	} else {
	    MakeDoubleDeltaFilt(winlen, dc->ddfilt);
	}
    }
    dc->nfeas   = 0;
    dc->nOutputFtrs = 0;/* until we see the number of feas */
    dc->state  = NULL;	/* until we see the number of feas */
    dc->retvec = NULL;  /* until we see the number of feas */
    if(feas > 0) {
	/* we can set up the feature-size dependent stuff now! */
	dcAllocState(dc, feas);
    }
    return dc;
}

void free_DeltaCalc(DeltaCalc *dc) 
{   /* free the DeltaCalc object */
    if(dc->dfilt)	free_fvec(dc->dfilt);
    if(dc->ddfilt)	free_fvec(dc->ddfilt);
    if(dc->state)	free_fmat(dc->state);
    if(dc->retvec)	free_fvec(dc->retvec);
    free(dc);
}

static struct fvec * CopyFvec(const struct fvec *src, struct fvec *dst)
{   /* Make dst have the same values as src;
       maybe make dst smaller; error if it's too small */
    int i;
    if(dst) {
	assert(dst->length >= src->length);
	dst->length = src->length;
    } else {
	dst = alloc_fvec(src->length);
    }
    for(i = 0; i < src->length; ++i) {
	dst->values[i] = src->values[i];
    }
    return dst;
}

static void CopyFloats(const float *src, float *dst, int n) {
    memcpy(dst, src, n*sizeof(float));
}

struct fvec *DeltaCalc_appendDeltas(DeltaCalc *dc, const struct fvec *infeas)
{   /* Take a row of features, and append it to our state.  When 
       we have enough state, return a nonempty floatRef with 
       fully-padded feature rows for output. 
       When caller reaches end of input, they repeatedly call DeltaCalc 
       with a zero-len <infeas> to allow it to flush its state; DeltaCalc
       will eventually give a zero-len response, meaning all done.
       */
    if(dc->retvec == NULL) {
	/* we haven't yet allocated all the state - must be a deferred 
	   feature-width thing.  Assume we can now get the width from 
	   the infeas.  This code should execute at most just the first 
	   time appendDeltas is called, and not even then if the 
	   feature width was specified to the init fn. */
	dcAllocState(dc, infeas->length);
    }
    if(dc->degree == 0) {
	if(infeas) {
	    CopyFvec(infeas, dc->retvec);
	} else {
	    dc->retvec->length = 0;
	}
    } else {
	if (dc->rownum < dc->state->nrows) {
	    /* copy it in */
	    int r, copies = 1;
	    if(dc->rownum == 0) {
		/* duplicate the first feature over a few rows */
		copies = dc->firstpad;
	    }
	    for(r = 0; r < copies; ++r) {
		/* although it's crazy to unpad without having filled 
		   the state, better do something other than crash */
		if(infeas && infeas->length > 0) {
		    CopyFloats(infeas->values, 
			       dc->state->values[dc->rownum], 
			       dc->state->ncols);
		} else {
		    /* zero-len input means: we're doing end-of-run pad */
		    ++dc->unpadding;
		}
		++dc->rownum;
	    }
	} else {
	    /* we're into shifting it all back */
	    int r;
	    for (r = 0; r < dc->state->nrows-1; ++r) {
		CopyFloats(dc->state->values[r+1], dc->state->values[r], 
			   dc->state->ncols);
	    }
	    /* add new row at bottom of state */
	    if(infeas && infeas->length > 0) {
		CopyFloats(infeas->values, 
			   dc->state->values[dc->state->nrows-1], 
			   dc->state->ncols);
	    } else {
		/* zero-len input means: we're doing end-of-run pad */
		++dc->unpadding;
	    }
	    ++dc->rownum;
	}
	/* rownum has now increased */
	if (dc->rownum >= dc->state->nrows \
	    && dc->unpadding <= (dc->state->nrows - dc->firstpad)) {
	    /* state is full - enough memory to calc deltas */
	    struct fvec tmpfv;
	    dc->retvec->length = dc->nOutputFtrs;
	    if(dc->degree > 1) {
		tmpfv.length = dc->nfeas;
		tmpfv.values = dc->retvec->values + 2*dc->nfeas;
		MultCols(dc->state, dc->ddfilt, 0, &tmpfv);
	    }
	    /* calculate deltas */
	    tmpfv.length = dc->nfeas;
	    tmpfv.values = dc->retvec->values + dc->nfeas;
	    MultCols(dc->state, dc->dfilt, 0, &tmpfv);
	    /* copy in the actual features */
	    CopyFloats(dc->state->values[dc->middle], dc->retvec->values, 
		       dc->state->ncols);
	} else {
	    dc->retvec->length = 0;
	}
	    
    }
    return dc->retvec;
}


#ifdef MAIN

#include <stdio.h>

static void fvec_Print(const struct fvec *fv) {
    /* print an fvec to stderr */
    int i;
    for (i = 0; i < fv->length; ++i) {
	fprintf(stderr, "%.2f ", fv->values[i]);
    }
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[]) {
    int nfeas = 1;
    int winlen = 9;
    int degree = 2;
    DeltaCalc *dc = alloc_DeltaCalc(nfeas, winlen, degree);
    struct fvec *fv = alloc_fvec(nfeas);
    int x = 0, i, l=5;
    struct fvec *rv;

    fv->values[0] = 0;

    for (i = 0; i < l; ++i) {
	rv = DeltaCalc_appendDeltas(dc, fv);
	fprintf(stderr, "row %d: ", x++);
	fvec_Print(rv);
    }
    fv->values[0] = 1;
	rv = DeltaCalc_appendDeltas(dc, fv);
	fprintf(stderr, "row %d: ", x++);
	fvec_Print(rv);
    fv->values[0] = 0;
    for (i = 0; i < l; ++i) {
	rv = DeltaCalc_appendDeltas(dc, fv);
	fprintf(stderr, "row %d: ", x++);
	fvec_Print(rv);
    }
    /* empty the cache */
    fv->length = 0;
    for (i = 0; i < 9; ++i) {
	rv = DeltaCalc_appendDeltas(dc, fv);
	fprintf(stderr, "row %d: ", x++);
	fvec_Print(rv);
    }
}

#endif /* MAIN */
