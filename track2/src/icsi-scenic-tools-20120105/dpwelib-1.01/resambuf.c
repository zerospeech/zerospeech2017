/***************************************************************\
*	resambuf.c  (after resmp.c)				*
* Buffer that uses resample.c to incrementally resample blocks	*
* that takes input a bit at a time - for resmp.c		*
* dpwe 23feb91							*
* 1997feb18 dpwe@icsi.berkeley.edu  SPRACHworks release         *
* $Header: /u/drspeech/repos/dpwelib/resambuf.c,v 1.3 2011/03/09 01:35:03 davidj Exp $
\***************************************************************/

#include <dpwelib.h>

#include <resambuf.h>

/* implementations */

TrigRSBufType 
*NewTrigRSBuf(chainSize, emptSize, actLen, fill, 
	      chainFn, chainArg, emptFn, emptArg, rdp, desc)
    long chainSize;		/* block size for passing to chainfn */
    long emptSize;		/* block size for passing to emptfn */
    long actLen;		/* caller can figure out buf size req'd */
    long fill;			/* initially pad with zeros to this size */
    long (*chainFn)();		/* function to pass newly resampled points */
    char *chainArg;		/* first argment to chained fn */
    long (*emptFn)();		/* function to call to empty */
    int  emptArg;		/* first argment to empty fn */
    RsmpDescType *rdp;		/* descriptor for resampling to apply to ip */
    char	 *desc;		/* (string) description of object */
/* Instance a new resampling buffer that passes new pts to a chained func,
   and automatically empties itself with an empty func. */
/* <size> is tricky.  If the buffer is going to be emptied in blocks
       of EMBKSZ, and is fed raw data in blocks of FDBKSIZ, and is
       wired to a resampler of ratio outPrd:inPrd with an intermediate-
       frequency kernel extent (halflength) of KHLEN, then the biggest
       buffer required would be 
         size = EMBKSZ + (FDBKSIZ*inPrd + 2*KHLEN + outPrd)/outPrd
       give or take a point. */
    {
    TrigRSBufType *trbp;

/*  fprintf(stderr, "RBT: max = %ld, fill = %ld, khl %d, inpd %d, outpd %d\n", 
	    maxFeed,fill,khLen,inPd,outPd);	fflush(stderr);
 */
    trbp = (TrigRSBufType *)malloc(sizeof(TrigRSBufType));
    trbp->rsmpBufPtr = NewResampBuf(actLen, fill, rdp, desc);
    trbp->sentSize  = fill;
    trbp->chainSize = chainSize;
    trbp->chainFn   = chainFn;
    trbp->chainArg  = chainArg;
    trbp->emptSize = emptSize;
    trbp->emptFn   = emptFn;
    trbp->emptArg  = emptArg;
    if(desc)
	strncpy(trbp->desc, desc, DESCLEN);

    return trbp;
    }

void FreeTrigRSBuf(trBp)
    TrigRSBufType *trBp;
    {
    FreeResampBuf(trBp->rsmpBufPtr);
    free(trBp);
    }

long FeedTrigRSBuf(trBp, sptr, siz, flush)
    /* resample new data into buffer, pass to chain fn, empty with emptFn */
    TrigRSBufType *trBp;
    SAMPL	*sptr;
    long	siz;
    int 	flush;	/* 1 => don't hold back partially-valid samples */
    {
    long 	unsent_bot, unsent_top;
    long	new_top, num_to_send, num_to_drop, dropped;
    long	chainSize    = trBp->chainSize;
    long	(*chainFn)() = trBp->chainFn;
    char	*chainArg     = trBp->chainArg;
    long	emptSize     = trBp->emptSize;
    long	(*emptFn)()  = trBp->emptFn;
    int		emptArg      = trBp->emptArg;
    RsmpBufType *rsBp        = trBp->rsmpBufPtr;
    int 	toFlush, flushFlag;

    if(chainSize <= 0)		chainSize = 1;	/* else won't work */
    unsent_bot = trBp->sentSize;
	/* chainSize*(RBSize(rsBp,flush)/chainSize); /* sent up to here */
    AddToResampBuf(rsBp, sptr, siz, flush);
    unsent_top = RBSize(rsBp,flush);
    if(chainFn != NULL)	/* if we have a function to pass on to */
	{
	toFlush = flush;	flushFlag = 0;
	while( (unsent_top - unsent_bot) >= chainSize 
	      || (flushFlag = 1, toFlush--) )
	    {	/* send only blocks of integral numbers of chainSize */
	    num_to_send = chainSize*((unsent_top-unsent_bot)/chainSize);
	    if(flushFlag)    num_to_send = unsent_top-unsent_bot;
	                     /* NOT mult of size, mebe 0 */
	    unsent_bot += (*chainFn)(chainArg, 
				     RBBufPtr(rsBp, unsent_bot, num_to_send), 
				     num_to_send, flushFlag );
	    /* returns the number of entry points actually accepted */
	    }
	trBp->sentSize = unsent_bot;
	}
    if(emptFn != NULL)	/* we have an emptying function */
	{
	toFlush = flush;	flushFlag = 0;
	new_top = RBSize(rsBp,flush);
	while( new_top > emptSize
	      || (flushFlag = 1, toFlush--) )
	    {
	    num_to_send = emptSize*(new_top/emptSize);
	    if(flushFlag)
		num_to_send = new_top;
	    num_to_drop = (*emptFn)(emptArg, 
				    RBBufPtr(rsBp, 0L, num_to_send),
				    num_to_send, flushFlag);
	    DropFromTrigRSBuf(trBp, num_to_drop);
	    new_top = RBSize(rsBp,flush);
	    }
	}
    return siz;		/* caller wants to know how many we took */
    }

void DropFromTrigRSBuf(trBp, size)
    TrigRSBufType *trBp;
    long size;
    {
    long sentSize = trBp->sentSize;

    DropFromResampBuf(trBp->rsmpBufPtr, size);
    sentSize -= size;
    if(sentSize < 0)	sentSize = 0;
    trBp->sentSize = sentSize;
    }

long CopyAsEmptFun(dst, buf, siz, flag)
    SAMPL *dst;
    SAMPL *buf;
    long siz;
    int  flag;
    /* a buffer-emptying function for unfancy buffer ops */
    {
	/* CopySampls(buf, 1, dst, 1, siz, FWD); */
	memcpy(dst, buf, siz*sizeof(SAMPL));
    return siz;
    }

/********* Resamp Buf funs : new, free, add, drop *********/
/* Note to dpwe:
   Should maybe adjust function of 'flushing' to only return points within
   the ifIndex, i.e. current actual time passed */

/* private member access fns */

long RBSize(rsBp,flush)
    RsmpBufType *rsBp;
    int		flush;
    /* return data points in buffer; <flush> set gives a higher answer */
    {
    if(flush)
	/* return RBTotalSize(rsBp); */
	return (RBTotalSize(rsBp)+RBValidSize(rsBp) /*+1*/ )/2;  /* semi-valid? */
        /* 1999jun29: it turns out that I want to round down when counting 
	   valid samples.  This means that downsampling a 9-sample signal
	   by a factor of 2 gives 4 output frames, not 5. */
    else
	return RBValidSize(rsBp);
    }

long RBValidSize(rsBp)
    RsmpBufType *rsBp;
    /* return number of strictly valid data points in buffer */
    {
    return rsBp->firstInvalid;
    }

long RBTotalSize(rsBp)
    RsmpBufType *rsBp;
    /* return total number of 'touched' data points in buffer */
    {
    return rsBp->firstUntouched;
    }

SAMPL *RBBufPtr(rsBp, offset, size)
    RsmpBufType *rsBp;
    long offset;
    long size;
    /* Pointer to a linear block. <size> allows circular buffers to rotate */
    {
    return rsBp->buf + offset;
    }

RsmpBufType *NewResampBuf(size, fullness, rdp, desc)
    long size;
    long fullness;		/* how full initially ? */
    RsmpDescType *rdp;
    char *desc;			/* string description */
    /* allocate a new buffer, optionally part-full */
    {
    RsmpBufType	*rsBp;
    int ip,op;
    int khlen;
    long ix;

    DereferenceResampleDetails(rdp, &op, &ip, &khlen);
    rsBp = (RsmpBufType *)malloc(sizeof(RsmpBufType));
    rsBp->buf = (SAMPL *)malloc(sizeof(SAMPL)*size);
    rsBp->bufSize = size;
    /* ClearSampls(rsBp->buf, 1, size); */
    memset(rsBp->buf, 0, size*sizeof(SAMPL));
    if(fullness == 0)
	{
	rsBp->ifIndex = 0L;
	rsBp->firstInvalid = 0L;
	rsBp->firstUntouched = 0L;
	}
    else
	{
	rsBp->ifIndex = fullness*op - 1;
  /* 1994apr12 (wow - like 3 years later) discovered a half-sample phase 
     delay between successively (lower) octaves in cqt3 (by looking at 
     phase spectrogram of a pure impulse).  Figured this was due to 
     taking the odd-numbered samples when decimating rather than even 
     numbered.  Too hard to figure it all through, but subtracting one 
     from the above expression seems to do the trick (!).  dpwe */
	ix = (fullness*op + op - 1 - khlen)/op;
	if(ix < 0)		ix = 0;
	rsBp->firstInvalid = ix;
	ix = (fullness*op + khlen)/op;
	if(ix > rsBp->bufSize)	ix = rsBp->bufSize;
	}
    rsBp->rsmpDescPtr = rdp;
    if(desc)
	strncpy(rsBp->desc,desc,DESCLEN);

    return rsBp;
    }

void FreeResampBuf(rsBp)
    RsmpBufType *rsBp;
    {
    free(rsBp->buf);
    free(rsBp);
    }

long AddToResampBuf(rsBp, sptr, siz, flush)
    RsmpBufType *rsBp;
    SAMPL *sptr;
    long siz;
    int  flush;
    /* resample & overlay new data into the buffer */
    /* returns number of completely valid points in buffer */
    {
    long ifix;
    int  inPd, outPd, kernhalflen;

    DereferenceResampleDetails(rsBp->rsmpDescPtr, &outPd, &inPd, &kernhalflen);
    if(!(inPd == 1 && outPd == 1))
	{
/*	fprintf(stderr, "Add(%ld): resample %ld\n", rsBp->emFnArg, siz); */
	inPd  = rsBp->rsmpDescPtr->inPrd;
	outPd = rsBp->rsmpDescPtr->outPrd;
	kernhalflen = rsBp->rsmpDescPtr->kernhalflen;
	ifix = rsBp->ifIndex = 
	    ResampleIntoBuffer(rsBp->buf, rsBp->bufSize, sptr, siz,
			   rsBp->ifIndex, rsBp->rsmpDescPtr, !flush);
	}
    else{	/* 1:1 resampling i.e. just copy the points */
/*	fprintf(stderr, "Add(%ld): jus copy %ld\n", rsBp->emFnArg, siz); */
	/* CopySampls(sptr,1,rsBp->buf+rsBp->ifIndex,1,siz,FWD); */
	memcpy(rsBp->buf+rsBp->ifIndex, sptr, siz*sizeof(SAMPL));
	ifix = rsBp->ifIndex += siz;
	}
    rsBp->firstInvalid = (ifix+outPd-1-kernhalflen)/outPd;
    if(rsBp->firstInvalid < 0)
	rsBp->firstInvalid = 0;			/* clamp to start of buffer */
    rsBp->firstUntouched = (ifix+kernhalflen)/outPd;
    if(rsBp->firstUntouched > rsBp->bufSize)
	rsBp->firstUntouched = rsBp->bufSize;	/* clamp to end of buffer */

    return rsBp->firstInvalid;
    }

static void CopyFloats(sce, sinc, dst, dinc, len, fwdNbck)
    float *sce;
    int sinc;
    float *dst;
    int dinc; 
    long len;
    int fwdNbck;
    {
    if(fwdNbck)		/* fwdNbck => copying forward, so start with last */
	{
	sce += len * (long)sinc;
	dst += len * (long)dinc;
	while(len--)
	    {
	    dst -= dinc;	sce -= sinc;
	    *dst = *sce;
	    }
	}
    else		/* copying backward, so start with first */
	while(len--)
	    {
	    *dst = *sce;
	    dst += dinc;	sce += sinc;
	    }
    }

void memcpy_floats_rev(int *idst, int *isrc, int ilen)
{   /* copy backwards in memory, like memcpy, but predictable */
/*
    int *isrc = (int *)src;
    int *idst = (int *)dst;
    int ilen  = len/sizeof(int);

    assert((((long)src)%sizeof(int)) == 0);
    assert((len % sizeof(int)) == 0);
    assert(sizeof(int)==sizeof(float));
 */
    do  {
	*idst++ = *isrc++;
    } while(--ilen);
/*
    while(ilen--) {
	*idst++ = *isrc++;
    }
 */
}

void DropFromResampBuf(rsBp, num)
    RsmpBufType *rsBp;
    long	num;
    {
    long	ptsToSave;
    long        untch = rsBp->firstUntouched;
    long        inval = rsBp->firstInvalid;
    long    	ix    = rsBp->ifIndex;
    SAMPL	*buf  = rsBp->buf;
    int 	ip,op,khl;

    DereferenceResampleDetails(rsBp->rsmpDescPtr, &op, &ip, &khl);
    ptsToSave = untch - num;
    if(ptsToSave > 0) {
/*	CopyFloats(buf+num, 1, buf, 1, ptsToSave, REV); */
	memcpy_floats_rev((int *)buf, (int *)(buf+num), ptsToSave); 
/*	CopySampls(buf+num, 1, buf, 1, ptsToSave, REV);	*/
/*	int *src = (int *)(buf+num);
	int *dst = (int *)buf;
	int len  = ptsToSave;
	while(len--)
	    *dst++ = *src++; */
    }else{
	num = untch;
	ptsToSave = 0;
	}
    /* ClearSampls(buf+ptsToSave, 1, num); */
    memset(buf+ptsToSave, 0, num*sizeof(SAMPL));
    rsBp->firstUntouched = ptsToSave;
    inval -= num;
    if(inval < 0)	inval = 0;
    rsBp->firstInvalid = inval;
    ix -= num*op;
    if(ix < 0)		ix = 0;
    rsBp->ifIndex = ix;
    }



