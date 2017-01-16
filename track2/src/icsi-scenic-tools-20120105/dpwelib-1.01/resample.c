/***************************************************************\
*	resample.c  (was diffuse.c)				*
* A kind of integer ratio resampler				*
* that takes input a bit at a time - for resmp.c		*
* dpwe 23feb91							*
* 1997feb18 dpwe@icsi.berkeley.edu SPRACHworks release		*
* $Header: /u/drspeech/repos/dpwelib/resample.c,v 1.2 2011/03/08 22:17:06 davidj Exp $
\***************************************************************/

#include <dpwelib.h>
#include <math.h>		/* for fabs() */

#include <genutils.h>		/* for Mymalloc() */
#include <resample.h>

#define DEF_MAX_DENOM 100 	/* default limit on if frq term */

void FindGoodRatio(ratio, p_o, p_i, acc)
    FLOATARG ratio;
    int *p_o;	
    int *p_i;
    FLOATARG acc;
    /* find an integer ratio *p_o/*p_i closest to 'ratio', with *p_i
       smaller than its entry value.  Returns immediately if error
       is less than acc or .1%		*/
    {
    int i,max, besti = 0;
    float fo,err, besterr = acc+1.0;

/*    fprintf(stderr, "FGR: %f to %f (%d)\n", ratio, acc, *p_i);  */
    if(acc<= 0.0)	acc = 0.001;
    max = *p_i;		/* entry value of *p_i is maximum for i*/
    if(max<=0)	max = DEF_MAX_DENOM;
    i = 0;
    while(besterr > acc /* && i < max */)	/* try every denominator */
	{
	++i;
	fo = ratio * (float)i;
	err = fabs( (fo - (float)((int)(fo+.5)) )/fo );
	if(err < besterr)
	    {
	    besterr = err;
	    besti = i;		/* remember this one, it's good */
	    }
	}
    *p_i = besti;
    *p_o = (int)( (ratio * (float)besti)+.5);
    }

long MinResampleBufLen(rdp, holdSize, feedSize)
    /* return a minimum size for the buffer given it is fed through
       the passed rdp, may not be emptied until holdSize full, and
       may be fed in chunks feedSize big */
    RsmpDescType *rdp;
    long	holdSize;
    long	feedSize;
    {
    long actLen;
    int  inPd;
    int  outPd;
    int  khLen;

    DereferenceResampleDetails(rdp, &outPd, &inPd, &khLen);

    actLen = holdSize + (feedSize*inPd + 2*khLen + outPd - 1)/outPd;

    return actLen;
    }


void DereferenceResampleDetails(rdp, pop, pip, pkhlen)
    /* deref or supply default chrs for NULL resampling descriptor */
    RsmpDescType *rdp;
    int 	*pop;		/* returns outPrd */
    int 	*pip;		/* returns inPrd */
    int 	*pkhlen;	/* returns kernel half length */
    {
    if(rdp != NULL)
	{
	*pip = rdp->inPrd;
	*pop = rdp->outPrd;
	*pkhlen = rdp->kernhalflen;
	}
    else	/* defaults for null = 1:1 */
	{
	*pip = 1; *pop = 1; *pkhlen = 0;
	}
    }

RsmpDescType *NewResampKernel(outP, inP, lobecount, gain, prop)
    int inP;
    int outP;
    int lobecount;
    FLOATARG gain;
    FLOATARG prop;
    {
    int   i, lobePrd;
    float f,x;
    LONGSAMPL v;
    SAMPL *pkp, *pkm, *kernCtr;
    int   kernhalflen;
    RsmpDescType *rdptr;

    if(gain == 0.0)	gain = sqrt((float)outP/(float)inP);
    /* if inP = 1 and outP = 2, we get half as many points out, so to
       preserve the sum energy in the signal, scale peak amplitude by
       sqrt(2/1) */

    lobePrd = (inP>outP)?inP:outP;
    kernhalflen = lobePrd * lobecount;
    rdptr = (RsmpDescType *)Mymalloc(1, sizeof(RsmpDescType)+
				     (sizeof(SAMPL)*(2*kernhalflen + 1)), 
				     "NewResampK");
    rdptr->inPrd = inP;	    rdptr->outPrd = outP;
    rdptr->kernhalflen = kernhalflen;
    rdptr->kernCtr = kernCtr = kernhalflen + (SAMPL *)(rdptr+1);
    f = SAMPLUNITY*gain*(float)inP/(float)lobePrd; 
    *kernCtr = f;
    pkp = kernCtr + 1;	pkm = kernCtr - 1;
    for(i=1; i<=kernhalflen; ++i)
	{
	x = prop*PI*i/(float)lobePrd;
	v = (SAMPL)(f*sin(x)/x*(.54 + .46 * cos(PI*i/(float)kernhalflen)));
	*pkp++ = v;
	*pkm-- = v;
	}
    return rdptr;
    }

RsmpDescType *MakeResampKernel(outP, inP, len, kern)
    /* generate a structure from the passed filter & values */
    int inP;
    int outP;
    long len;
    float *kern;
    {
    int   i, lobePrd;
    float f,x;
    LONGSAMPL v;
    SAMPL *pkp, *pkm, *kernCtr;
    int   kernhalflen;
    RsmpDescType *rdptr;
    float gain = 1.0;

    if(gain == 0.0)	gain = sqrt((float)outP/(float)inP);
    /* if inP = 1 and outP = 2, we get half as many points out, so to
       preserve the sum energy in the signal, scale peak amplitude by
       sqrt(2/1) */

    lobePrd = (inP>outP)?inP:outP;
    kernhalflen = (len/2); 	/* lobePrd * lobecount; */
    rdptr = (RsmpDescType *)Mymalloc(1, sizeof(RsmpDescType)+
				     (sizeof(SAMPL)*(2*kernhalflen + 1)), 
				     "MakeResampJ");
    rdptr->inPrd = inP;	    rdptr->outPrd = outP;
    rdptr->kernhalflen = kernhalflen;
    rdptr->kernCtr = kernCtr = kernhalflen + (SAMPL *)(rdptr+1);
    f = SAMPLUNITY*gain;
    *kernCtr = f * kern[kernhalflen];
    pkp = kernCtr + 1;	pkm = kernCtr - 1;
    for(i=1; i<=kernhalflen; ++i)
	{
	*pkp++ = f * kern[kernhalflen+i];
	*pkm-- = f * kern[kernhalflen-i];
	}
    return rdptr;
    }

void FreeResampKernel(rdp)
    RsmpDescType *rdp;
    {
    free(rdp);	/* allocated as single block along with lookup table */
    }

long oldResampleIntoBuffer(dstBuf, dbSize, sceBuf, sbSize, ifStart, rdp) /* */
/* long ResampleIntoBuffer(dstBuf, dbSize, sceBuf, sbSize, ifStart, rdp) /* */
    SAMPL *dstBuf;
    long dbSize;
    SAMPL *sceBuf;
    long sbSize;
    long ifStart;
    RsmpDescType *rdp;
    {
    register LONGSAMPL d;
#ifdef UNITYCLIP
    register LONGSAMPL e;
#endif
    register SAMPL *dstptr, *krnptr;
    register int   i, max; 
    register int   op = rdp->outPrd;
    int	  kernhalflen = rdp->kernhalflen;
    SAMPL *kBas = rdp->kernCtr - kernhalflen;
    long  nrst;
    int   phas;

    nrst = ifStart/op;
    phas = ifStart%op;
    for(;sbSize>0; --sbSize)
	{
	d = *sceBuf++;
	i = -(kernhalflen-phas)/op;
	if(nrst+i < 0 )
	    i = -nrst;
	dstptr = dstBuf + nrst + i;
	krnptr = rdp->kernCtr + i*op - phas;
	max = (kernhalflen+phas)/op;
	if( (nrst+max)>dbSize )
	    max = dbSize-nrst;
	for(;i<max;++i)
	    {
#ifdef UNITYCLIP
	    e = *dstptr;
	    e += SCALEDOWN(d * *krnptr);
	    if(e > SAMPLUNITY)		e = SAMPLUNITY;
	    else if(e < -SAMPLUNITY)	e = -SAMPLUNITY;
	    *dstptr++ = e;
#else
	    *dstptr++ += SCALEDOWN(d * *krnptr);
#endif
	    krnptr += op;
	    }
	phas += rdp->inPrd;
	if(phas >= op)
	    {
	    nrst += phas / op;
	    phas %= op;
	    }
	}
    return(nrst*op + phas);
    }

/****
  Q: How can I interpret and exploit ifStart?
  A: ifStart defines the precise delay and phase alignment between input
     and output streams in the resampler.  Since this is an integer-ratio 
     resampler, we have the concept of the intermediate-frequency (IF);
     the desired input and output signals' periods are defined in units 
     of the period of the IF (rdp->{in,out}Prd), and hence their ratio.
     The ifStart is the (positive) delay, in IF periods, between the 
     first sample of the output and the first sample of the input.
     For instance, with inPrd = 3, outPrd = 4 and ifStart = 9..
                                 /--- input samples start at tick 9
     input samples		 v  v  v  v  v  v  v  v  v  v  v  v  v  
     IF 'ticks'		||||||||||||||||||||||||||||||||||||||||||||||
     output samples     ^   ^   ^   ^   ^   ^   ^   ^   ^   ^   ^   ^   
                        \--output samples always start at IF tick=0
     ifStart is always >= 0 because the software is limited.  But that's 
     OK because our zero phase interpolation filters will always introduce 
     some pre-ring.  In order not to miss any of that crucial signal 
     energy, you need to ensure enough slack at the start of dstBuf 
     to be able to start ifStart at at least kernhalflen.
     You can change an ifIndex as returned as long as you do it in steps 
     of outPrd, and move your dstBuf index one whole sample each time 
     you do.

     dpwe 09nov91 - not when I wrote the code, just when I needed to 
     understand it (use it) the second time.
 ****/

long ResampleIntoBuffer(dstBuf, dbSize, sceBuf, sbSize, ifStart, rdp, chk)
    SAMPL *dstBuf;
    long dbSize;
    SAMPL *sceBuf;
    long sbSize;
    long ifStart;
    RsmpDescType *rdp;
    int	chk;	/* flag to perform bounds checks on o/p size */
    {
    register LONGSAMPL d;
    register int       hi;
    register SAMPL     *sPtr;
/*    register SAMPL     *kPtr;	*/
    /* by making kPtr a char *, I can add inCPrd to it without the
       compiler having to do a left shift (gcc) or multiply (cc) -
       saving one cycle in ten in the very inner loop.
       But it only saves ~ 5% */
    register char      *kPtr;
    register int       inCPrd = sizeof(SAMPL)*rdp->inPrd;
    register SAMPL     *dPtr;
    int       inPrd = rdp->inPrd;
    SAMPL *spBase;
    int   op = rdp->outPrd;
    int	  khlen = rdp->kernhalflen;
    int   sPhas;
    long  opStart, opEnd, opNorm, dbWrSize, dbNormSize;
    long  out2in;
    int   sPlim,hiBase;
    SAMPL *kBas = rdp->kernCtr - khlen;

    out2in = (2*khlen+1)/inPrd;		/* typically, number of outs per in */
/*    sPlim  = inPrd - (2*khlen+1)%inPrd; */
    sPlim  = (2*khlen+1)%inPrd; /* phase point to include extra */

    sPhas = /* 2 * khlen - ((op - (ifStart%op))%op); */
	    khlen + khlen - 
		((ifStart*op + khlen - ifStart)%op);
    opStart = -khlen+((khlen*op)+ifStart - khlen + op - 1)/op; /* tick */
    if(opStart < 0)
	{
	sPhas += op*opStart;	/* reduce initial phase */
	opStart = 0;
	}
    dPtr = dstBuf + opStart;
    opEnd   = (ifStart + (sbSize-1)*inPrd + khlen)/op; /* this pt used */
    opNorm  = (ifStart + (sbSize-1)*inPrd - khlen)/op; 
    /* opNorm is the last output point that doesn't run off sce end */
    if(opEnd > dbSize)
	{
	if(chk)
	    {
	    fprintf(stderr, "ResampleIntoBuffer: needed pt %ld of buf %ld\n",
		    opEnd, dbSize);
	    abort();
	    }
	else
	    opEnd = dbSize;
	}
    if(opNorm > dbSize)
	opNorm = dbSize;
    dbNormSize = opNorm - opStart + 1;
    dbWrSize = opEnd - opStart + 1;	/* inclusive difference */
    /* run in.  Calc from first src pt to some later point */
    spBase = sceBuf; /* + sNrst, which must be zero */
    while(dbWrSize > 0 && sPhas > 0)
	{
	dbNormSize--;		dbWrSize--;
	d = *dPtr;		sPtr = spBase; /* == sceBuf; */
	hi = (2*khlen + inPrd - sPhas)/inPrd;
	kPtr = (char *)(kBas + sPhas);
	if(hi>sbSize)	hi = sbSize;
	while(hi--)
	    {
	    d += SCALEDOWN(*sPtr++ * *(SAMPL *)kPtr);
	    kPtr += inCPrd;
	    }
#ifdef UNITYCLIP
	if(d > SAMPLUNITY)		d = SAMPLUNITY;
	else if(d < -SAMPLUNITY)	d = -SAMPLUNITY;
#endif
	*dPtr++ = d;		sPhas -= op;
	}
    if(dbNormSize > 0)	/* some 'normal' points to do */
	{
	dbWrSize -= dbNormSize;
	while(dbNormSize-- > 0)
	    {
	    while(sPhas < 0)
		{  sPhas += inPrd;	++spBase;	}
	    d = *dPtr;
/*	    hi = out2in + ( (sPhas == 0 || sPhas > sPlim)?1:0);	*/
	    hi = out2in + ( (sPhas < sPlim)?1:0);
	    kPtr = (char *)(kBas + sPhas);	sPtr = spBase;
	    while(hi--)
		{
		d += SCALEDOWN(*sPtr++ * *(SAMPL *)kPtr);
		kPtr += inCPrd;
		}
#ifdef UNITYCLIP
	    if(d > SAMPLUNITY)		d = SAMPLUNITY;
	    else if(d < -SAMPLUNITY)	d = -SAMPLUNITY;
#endif
	    *dPtr++ = d;	    sPhas -= op;
	    }
	}
    hiBase = sbSize - (spBase - sceBuf);
    while(dbWrSize-- > 0)
	{
	while(sPhas < 0)
	    {	sPhas += inPrd;  ++spBase;  --hiBase;   }
	d = *dPtr;		hi = hiBase;
	kPtr = (char *)(kBas + sPhas);	sPtr = spBase;
	while(hi--)
	    {
	    d += SCALEDOWN(*sPtr++ * *(SAMPL *)kPtr);
	    kPtr += inCPrd;
	    }
#ifdef UNITYCLIP
	if(d > SAMPLUNITY)		d = SAMPLUNITY;
	else if(d < -SAMPLUNITY)	d = -SAMPLUNITY;
#endif
	*dPtr++ = d;		sPhas -= op;
	}
    return(ifStart + sbSize * inPrd);	/* by def.. */
    }

