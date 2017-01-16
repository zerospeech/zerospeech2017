/***************************************************************\
*	resample.h  (after diffuse.c)				*
* A kind of integer ratio resampler				*
* that takes input a bit at a time - for resmp.c		*
* dpwe 23feb91							*
* 1997feb18 dpwe@icsi.berkeley.edu  included in SPRACHworks	*
* $Header: /u/drspeech/repos/dpwelib/resample.h,v 1.2 2011/03/08 22:17:06 davidj Exp $
\***************************************************************/

#ifndef _RESAMPLE_H_
#define _RESAMPLE_H_

/* #include <samplops.h> */
/* formerly in samplops.h... */
typedef float  SAMPL;
typedef double LONGSAMPL;
#define SFMT_SAMPL SFMT_FLOAT
#define SAMPLUNITY	1.0
#define SCALEDOWN(x)	(x)

typedef struct
    {
    SAMPL *kernCtr;	/* pointer to _middle_ term of buf: +/- khl is ok */
    int   kernhalflen;	/* number of symmetric terms : len = 2*khl + 1 */
    int   inPrd;
    int   outPrd;
    } RsmpDescType;	/* private data for resampler */

void FindGoodRatio PARG((FLOATARG ratio, int *p_o, int *p_i, FLOATARG acc));
    /* find an integer ratio *p_o/*p_i closest to 'ratio', with *p_i
       smaller than its entry value.  Returns immediately error
       is less than acc or 0.001  PARG((.1%)		*/

long MinResampleBufLen PARG((RsmpDescType *rdp,long holdSize, long feedSize));
    /* return a minimum size for the buffer given it is fed through
       the passed rdp, may not be emptied until holdSize full, and
       may be fed in chunks feedSize big */

void DereferenceResampleDetails PARG((RsmpDescType *rdp, int *pop, int *pip, 
				int *pkhlen));
    /* deref or supply default chrs for NULL resampling descriptor */

RsmpDescType *NewResampKernel PARG((int outP, int inP, int lobecount, 
				    FLOATARG gain, FLOATARG prop));
    /* Setup lookup tables for resampling a given ratio, num. lobes */
    /* gain = 1.0 attempts to maintain peak amplitude, 0 preserves energy */
    /* prop smaller than 1.0 sets the LPF at below nyquist, for guard */

RsmpDescType *MakeResampKernel PARG((int outP, int inP, long len, 
				     float *kern));
    /* generate a structure from the passed filter & values */

void FreeResampKernel PARG((RsmpDescType *rdp));      /* free kernel memory */

long ResampleIntoBuffer PARG((SAMPL *dstBuf, long dbSize, SAMPL *sceBuf, 
			      long sbSize, long ifStart, RsmpDescType *rdp, 
			      int chk));
    /* Perform resampling; superposes into existing buffer; 
       Returns argument to use as ifStart next time for seamlessness */

long oldResampleIntoBuffer PARG((SAMPL *dstBuf, long dbSize, SAMPL *sceBuf, 
				 long sbSize, long ifStart, 
				 RsmpDescType *rdp));

#endif /* _RESAMPLE_H_ */
