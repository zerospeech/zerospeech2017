/***************************************************************\
*	sndfutil.c						*
*	Some handy extra soundfile utilities			*
*	built on the sndf system				*
*	02dec90	dpwe						*
\***************************************************************/

/*
 * Modifications:
 * 1996aug26 dpwe
 *	float<->int scaling changed to have +1.0 correspond to 32768 
 *      i.e. unattainable, rather than 32767 as it did.  Thus 0x8000
 *      maps to -1.000000 rather than -1.00003 - it's just better.
 *	Basically, I changed most occurrences of SHORTMAX (which is 
 *	32767) to (SHORTMAX+1).
 */

/*
!! The sound format matrix should be complete, but the ulaw and alaw
!! convert routines aren't written yet.. just treated as linear char
!! at present.
!! Format convert strategy:
!! Provide routines to convert between short and all other types (to/from)
!! Also provide double<-->float and long<-->float to allow these without
!! loss of accuracy implicit in using short.
!! ConvertSoundFormat may call itself twice, with a maximum 2 stage conversion.
!! Formats that occupy the same number of bytes (l<-->f) done in place
!! (byte-byte go via short)
!! In fixed formats, FSD maps to FSD
!! in floating point, FSD maps to 1.0
!! (should perhaps put clipping & detection into F2S,L & D2S,L)
 */

#include "dpwelib.h"
#include "genutils.h"	/* for Mymalloc() */
#include "sndfutil.h"
#include "ulaw.h"
#include "byteswap.h"

#ifndef THINK_C
#define DO_CLIP
#endif /* THINK_C */	/* no need to clip on think_c because FPU does it */


/* static fn prototypes */
static void cvC2S(char *sce, short *dst, long smps);
static void cvO2S(char *sce, short *dst, long smps);
static void cvU2S(char *sce, short *dst, long smps);
static void cvA2S(char *sce, short *dst, long smps);
static void cvL2S(long *sce, short *dst, long smps);
static void cvM2S(unsigned long *sce, short *dst, long smps);
static void cvF2S(float *sce, short *dst, long smps);
static void cvD2S(double *sce, short *dst, long smps);
static void cvS2C(short *sce, char *dst, long smps);
static void cvS2O(short *sce, char *dst, long smps);
static void cvS2U(short *sce, char *dst, long smps);
static void cvS2A(short *sce, char *dst, long smps);
static void cvS2M(short *sce, unsigned long *dst, long smps);
static void cvS2L(short *sce, long *dst, long smps);
static void cvS2F(short *sce, float *dst, long smps);
static void cvS2D(short *sce, double *dst, long smps);
static void cvL2F(long *sce, float *dst, long smps);
static void cvF2L(float *sce, long *dst, long smps);
static void cvL2M(long *sce, unsigned long *dst, long smps);
static void cvM2L(unsigned long *sce, long *dst, long smps);
static void cvD2F(double *sce, float *dst, long smps);
static void cvF2D(float *sce, double *dst, long smps);

#ifdef THINK_C
#define msgTag "sndfutil"	/* no static intialized data in THINK_C */
#else /* !THINK_C */
static char *msgTag = "sndfutil";	/* for error messages we print */
#endif /* THINK_C */

int SFSampleSizeOf(int format)
{	/* return number of bytes per sample in given fmt */
    return(format & SF_BYTE_SIZE_MASK);
}

void ConvertSampleBlockFormat(
    char	*inptr,
    char	*outptr,
    int 	infmt_e,
    int 	outfmt_e,
    long 	samps)
{
    int		inbps, outbps;
    char 	*tmpbuf;
    int	infmt, outfmt;

    infmt = infmt_e & ~_BYTESWAP;
    outfmt = outfmt_e & ~_BYTESWAP;

    inbps  = SFSampleSizeOf(infmt);
    outbps = SFSampleSizeOf(outfmt);
    if( inbps == 0 )
    { FPRINTF((stderr,"%s: unrecognised source format %d\n",msgTag,infmt));
      abort(); }
    if( outbps == 0 )
    { FPRINTF((stderr,"%s: unrecognised dest format %d\n",msgTag,outfmt));
      abort(); }
    if(infmt_e==outfmt_e)		/* don't complain; may need to copy */
    {
	if(inptr != outptr)
	    memcpy(outptr, inptr, inbps*samps);
	return;
    }
    if (infmt_e & _BYTESWAP) {
	ConvertBufferBytes(inptr, inbps*samps, inbps, BM_BYWDREV);
    }

    if(( (infmt == SFMT_CHAR || infmt == SFMT_CHAR_OFFS || infmt == SFMT_ALAW || infmt == SFMT_ULAW)
        && outfmt != SFMT_SHORT )  
       /* byte words go via short samples */
       ||( (infmt == SFMT_LONG || infmt == SFMT_24BL || infmt == SFMT_FLOAT || infmt == SFMT_DOUBLE)
	  &&(outfmt == SFMT_CHAR || outfmt == SFMT_CHAR_OFFS || outfmt == SFMT_ALAW || outfmt == SFMT_ULAW))) {
	/* longer words go to bytes via shorts */
	if( (tmpbuf = (char *)Mymalloc(samps, sizeof(short), 
				       "ConvertSampB1")) == NULL)
	{ FPRINTF((stderr, "ConvertBufferBytes: malloc fail for %ld\n", 
		   sizeof(short)*samps));  abort();  }
	ConvertSampleBlockFormat(inptr, tmpbuf, infmt, SFMT_SHORT, samps);
	ConvertSampleBlockFormat(tmpbuf, outptr, SFMT_SHORT, outfmt, samps);
	free(tmpbuf);
    } else if(((infmt == SFMT_LONG || infmt == SFMT_24BL) \
	         && outfmt == SFMT_DOUBLE)
	      || (infmt == SFMT_DOUBLE \
		  && (outfmt == SFMT_LONG || outfmt == SFMT_24BL)) ) {
	/* long <--> double via float; map 24 to L first if nec */
	int tmpfmt = SFMT_FLOAT;
	if( (tmpbuf = (char *)Mymalloc(samps, sizeof(float), 
				       "ConvertSampB2")) == NULL)
	{ FPRINTF((stderr, "ConvertBufferBytes: malloc fail for %ld\n", 
		   sizeof(short)*samps));  abort();  }
	if (infmt == SFMT_24BL) {
	    ConvertSampleBlockFormat(inptr, tmpbuf, infmt, SFMT_LONG, samps);
	    infmt == SFMT_LONG;
	}
	ConvertSampleBlockFormat(inptr, tmpbuf, infmt, tmpfmt, samps);
	if (outfmt == SFMT_24BL) {
	    ConvertSampleBlockFormat(tmpbuf, tmpbuf, tmpfmt, SFMT_LONG, samps);
	    tmpfmt == SFMT_LONG;
	}
	ConvertSampleBlockFormat(tmpbuf, outptr, tmpfmt, outfmt, samps);
	free(tmpbuf);
    } else {
	/* else we really should be able to do it */
	/* dispatch to our limited set of convert routines */
	if( outfmt == SFMT_SHORT ) {
	    switch( infmt ) {
	    case SFMT_CHAR:	    cvC2S(inptr,(short *)outptr,samps); break;
	    case SFMT_CHAR_OFFS: cvO2S(inptr,(short *)outptr,samps); break;
	    case SFMT_ALAW:	    cvA2S(inptr,(short *)outptr,samps); break;
	    case SFMT_ULAW:     cvU2S(inptr,(short *)outptr,samps); break;
	    case SFMT_LONG:     cvL2S((long *)inptr,(short *)outptr,samps); break;
	    case SFMT_24BL:     cvM2S((unsigned long *)inptr,(short *)outptr,samps); break;
	    case SFMT_FLOAT:    cvF2S((float *)inptr,(short *)outptr,samps); break;
	    case SFMT_DOUBLE:   cvD2S((double *)inptr,(short *)outptr,samps);break;
	    default:            FPRINTF((stderr,"%s:failed convert %d->short\n",
					 msgTag, infmt));
		abort();
	    }
	} else if( infmt == SFMT_SHORT ) {
	    switch( outfmt ) {
	    case SFMT_CHAR:	    cvS2C((short *)inptr,outptr,samps); break;
	    case SFMT_CHAR_OFFS: cvS2O((short *)inptr,outptr,samps); break;
	    case SFMT_ALAW:	    cvS2A((short *)inptr,outptr,samps); break;
	    case SFMT_ULAW:     cvS2U((short *)inptr,outptr,samps); break;
	    case SFMT_LONG:     cvS2L((short *)inptr,(long *)outptr,samps); break;
	    case SFMT_24BL:     cvS2M((short *)inptr,(unsigned long *)outptr,samps); break;
	    case SFMT_FLOAT:    cvS2F((short *)inptr,(float *)outptr,samps); break;
	    case SFMT_DOUBLE:   cvS2D((short *)inptr,(double *)outptr,samps);break;
	    default:            FPRINTF((stderr,"%s:failed convert short->%d\n",
					 msgTag, outfmt));
		abort();
	    }
	} else if( infmt == SFMT_DOUBLE && outfmt == SFMT_FLOAT) {
	    cvD2F((double *)inptr,(float *)outptr,samps);
	} else if( infmt == SFMT_FLOAT && outfmt == SFMT_DOUBLE) {
	    cvF2D((float *)inptr,(double *)outptr,samps);
	} else if( infmt == SFMT_LONG && outfmt == SFMT_FLOAT) {
	    cvL2F((long *)inptr,(float *)outptr,samps);
	} else if( infmt == SFMT_FLOAT && outfmt == SFMT_LONG) {
	    cvF2L((float *)inptr,(long *)outptr,samps);
	} else if( infmt == SFMT_24BL && outfmt == SFMT_FLOAT) {
	    cvM2L((unsigned long *)inptr,(long *)outptr,samps);
	    cvL2F((long *)outptr,(float *)outptr,samps);
	} else if( infmt == SFMT_FLOAT && outfmt == SFMT_24BL) {
	    cvF2L((float *)inptr,(long *)outptr,samps);
	    cvL2M((long *)outptr,(unsigned long *)outptr,samps);
	} else {
	    FPRINTF((stderr,"%s:failed convert %d-%d\n",msgTag,infmt,outfmt));
	    abort();
	}
    }
    if (infmt_e & _BYTESWAP) {
	/* swap them back to their entry state */
	ConvertBufferBytes(inptr, inbps*samps, inbps, BM_BYWDREV);
    }
    if (outfmt_e & _BYTESWAP) {
	ConvertBufferBytes(outptr, outbps*samps, outbps, BM_BYWDREV);
    }
    return;
}

#ifndef FOR_MATLAB
int ConvertSoundFormat(
    SFSTRUCT	**psnd,
    int 	newFmt)
{   /* convert the format for a whole sound held in memory
       Reallocate if necessary, hence passed pointer to sound ptr */
    int    oldFmt;
    int			oldbps, newbps;
    char 	*sce, *dst;
    long	samps;
    int 	infoSz;
    int 	err;
    int		needToFree = 0;
    SFSTRUCT	*newSnd;

    oldFmt = (*psnd)->format;
    if(oldFmt == newFmt)
    	return SFE_OK;	/* why argue? */
    oldbps = SFSampleSizeOf(oldFmt);
    newbps = SFSampleSizeOf(newFmt);
    samps  = (*psnd)->dataBsize / oldbps;
    if(oldbps != newbps)  {
    	char 		*isrc, *idst;
	/* sound changes size, have to reallocate */
    	infoSz = SFInfoSize(*psnd);
	if( err = SFAlloc(&newSnd, samps * newbps, newFmt,
			  (*psnd)->samplingRate, (*psnd)->channels, infoSz) ){
	    SFDie(err, NULL);
	}
	/* copy across info */
	isrc = (*psnd)->info;
	idst = newSnd->info;
	while(infoSz--)
	    *idst++ = *isrc++;
	/* set up data pointers to use */
	sce = (char *)SFDataLoc(*psnd);
    	dst = (char *)SFDataLoc(newSnd);
	/* remember to free and point output to the new sound */
	needToFree = 1;
    } else {
	/* can do conversion in place, we hope */
	sce = dst = SFDataLoc(*psnd);
    }
    ConvertSampleBlockFormat(sce, dst, oldFmt, newFmt, samps);
    if(needToFree)  {
	SFFree(*psnd);	/* and then trash the original */
	*psnd = newSnd;	/* out with the old, in with the new */
    } else{		/* or just one - in which case, fix header */
	(*psnd)->dataBsize = samps * newbps;	/* shouldn't have changed */
	(*psnd)->format    = newFmt;
    }
    return SFE_OK;
}

#endif /* !FOR_MATLAB */

static void cvC2S(
    char	*sce,
    short	*dst,
    long	smps)
    {	/* assume char is signed i.e. castes to -128..127 */
    register	int 	d, m;

    m = 1<<SFU_CHARBITS;
    sce += smps;	dst += smps;	/* start at end, allows in place */
    while(smps--)
	{
	d = *--sce;
	if(d>SFU_CHARMAX)  d -= m;	/* fix possibly unsigned char */
	*--dst = d<<(SFU_SHORTBITS-SFU_CHARBITS);
	}
    }
static void cvO2S(
    char	*sce,
    short	*dst,
    long	smps)
    {
    register	int 	d, m;

    m = 1<<SFU_CHARBITS;
    sce += smps;	dst += smps;	/* start at end, allows in place */
    while(smps--)
	{
	d = *--sce;
	if(d<0)	d += m;		/* fix possibly unsigned char */
	*--dst = (d-128)<<(SFU_SHORTBITS-SFU_CHARBITS);
	}
    }
static void cvU2S(
    char	*sce,
    short	*dst,
    long	smps)
    {
    Ulaw2Short((unsigned char *)sce, dst, 1, 1, smps);
    }
static void cvA2S(
    char	*sce,
    short	*dst,
    long	smps)
    {
    alaw2lin((unsigned char *)sce,dst,1,1,smps);
    }
static void cvL2S(
    long	*sce,
    short	*dst,
    long	smps)
    {				/* Want to map FSD-FSD, but not sure what 
				   FSD *is* in long samples.  SGI favors 
				   24 bit unity in 32 bit samples - follow 
				   that. (thanks Tommi Ilmonen 1998feb20) */
    while(smps--)
	*dst++ = (short)((*sce++)>>(SFU_24B_BITS-SFU_SHORTBITS));
    }
static void cvM2S(
    unsigned long	*sce,
    short	*dst,
    long	smps)
    {	/* just ignore top 8 bits */
    unsigned short *udst = (unsigned short *)dst;
    while(smps--)
	*udst++ = (unsigned short)((*sce++)>>(SFU_24B_BITS-SFU_SHORTBITS));
    }
static void cvF2S(
    float	*sce,
    short	*dst,
    long	smps)
    {
    long	d;

    while(smps--)
	{
#ifdef DO_CLIP
	d = (*sce++)*(float)(SFU_SHORTMAX+1);
	if(d > SFU_SHORTMAX)	d = SFU_SHORTMAX;
	if(d < -SFU_SHORTMAX-1)	d = -SFU_SHORTMAX-1;	/* clip */
	*dst++ = (short)d;
#else
	*dst++ = (short)((SFU_SHORTMAX+1)*(*sce++));
#endif /* DO_CLIP */
	}
    }
static void cvD2S(
    double	*sce,
    short	*dst,
    long	smps)
    {
    long	d;

    while(smps--)
	{
#ifdef DO_CLIP
	d = (*sce++)*(float)(SFU_SHORTMAX+1);
	if(d > SFU_SHORTMAX)	d = SFU_SHORTMAX;
	if(d < -SFU_SHORTMAX-1)	d = -SFU_SHORTMAX-1;	/* clip */
	*dst++ = (short)d;
#else
	*dst++ = (short)((SFU_SHORTMAX+1)*(*sce++));
#endif /* DO_CLIP */
	}
    }
static void cvS2C(
    short	*sce,
    char	*dst,
    long	smps)
    {
    while(smps--)
	*dst++ = (char)((*sce++)>>(SFU_SHORTBITS-SFU_CHARBITS));
    }
static void cvS2O(
    short	*sce,
    char	*dst,
    long	smps)
    {
    while(smps--)
	*dst++ = (char)(0x80^((*sce++)>>(SFU_SHORTBITS-SFU_CHARBITS)));
    }
static void cvS2U(
    short	*sce,
    char	*dst,
    long	smps)
    {
    Short2Ulaw(sce, (unsigned char *)dst, 1, 1, smps);
    }
static void cvS2A(
    short	*sce,
    char	*dst,
    long	smps)
    {
    lin2alaw(sce, (unsigned char*)dst,1,1,smps);
    }
static void cvS2M(
    short	*sce,
    unsigned long	*dst,
    long	smps)
    {
    dst += smps;	sce += smps;	/* start at end; allow in place */
    while(smps--)
	*--dst = ((long)*--sce)<<(SFU_24B_BITS-SFU_SHORTBITS);
    }
static void cvS2L(
    short	*sce,
    long	*dst,
    long	smps)
    {
    dst += smps;	sce += smps;	/* start at end; allow in place */
    while(smps--)
	*--dst = ((long)*--sce)<<(SFU_24B_BITS-SFU_SHORTBITS);
    }
static void cvS2F(
    short	*sce,
    float	*dst,
    long	smps)
    {
    dst += smps;	sce += smps;	/* start at end; allow in place */
    while(smps--)
	*--dst = ((float)*--sce)*(1.0/(SFU_SHORTMAX+1));
    }
static void cvS2D(
    short	*sce,
    double	*dst,
    long	smps)
    {
    dst += smps;	sce += smps;	/* start at end; allow in place */
    while(smps--)
	*--dst = ((float)*--sce)*(1.0/(SFU_SHORTMAX+1));
    }
static void cvL2F(
    long	*sce,
    float	*dst,
    long	smps)
    {
    while(smps--)
	*dst++ = ((float)*sce++)*(1.0/(SFU_24B_MAX+1));
    }
static void cvF2L(
    float	*sce,
    long	*dst,
    long	smps)
    {
    while(smps--)
	*dst++ = (long)((*sce++)*(float)(SFU_24B_MAX+1));
    }
static void cvL2M(
    long	*sce,
    unsigned long	*dst,
    long	smps)
    {
    long x;
    long *sdst = (long *)dst;
    while(smps--) {
	x = *sce++;
	if (x > SFU_24B_MAX)	x = SFU_24B_MAX;
	else if (x < -SFU_24B_MAX-1)	x = -SFU_24B_MAX-1;
	*sdst++ = x;
    }
    }
static void cvM2L(
    unsigned long	*sce,
    long	*dst,
    long	smps)
    {
    unsigned long *udst = (unsigned long *)dst;
    unsigned long x;
    while(smps--) {
	x = *sce++;
        if ( x & 0x00800000) x |= 0xFF000000;
	*udst++ = x;
    }
    }
static void cvD2F(
    double	*sce,
    float	*dst,
    long	smps)
    {
    while(smps--)
	*dst++ = (float)*sce++;
    }
static void cvF2D(
    float	*sce,
    double	*dst,
    long	smps)
    {
    dst += smps;	sce += smps;	/* start at end; allow in place */
    while(smps--)
	*--dst = (double)*--sce;
    }

/* after sndrfmt.c */

void ConvertChannels(
    short *inBuf,
    short *outBuf,
    int inch,
    int ouch,
    long len)	/* number of SAMPLE FRAMES to transfer */
    {
    int inc, i, totch;
    long scale = 0;
    short tmp[SNDF_MAXCHANS];

    if(inch <= 0 || inch > SNDF_MAXCHANS || ouch <= 0 || ouch > SNDF_MAXCHANS)
	{  FPRINTF((stderr, "ConvCh: silly inch %d or ouch %d\n",
		   inch, ouch));	abort(); }
    while( (ouch<<scale) < inch)	++scale;

    if(inch>ouch)	{  inc = 1;  totch = inch; }
    else		{  inc = -1; totch = ouch;
			   inBuf += inch * (len-1); 
			   outBuf += ouch * (len-1); }  
                        /* start at end */
    while(len--)
	{
	for(i=0; i<ouch; ++i)
	    tmp[i] = 0;
	for(i=0; i<totch; ++i)
	    tmp[(i%ouch)] += (*(inBuf+(i%inch)))>>scale;
	for(i=0; i<ouch; ++i)
	    *(outBuf+i) = tmp[i];
	inBuf  += inc*inch;
	outBuf += inc*ouch;
	}
    }

void ConvertFlChans(
    float *inBuf,
    float *outBuf,
    int inch,
    int ouch,
    long len)	/* number of SAMPLE FRAMES to transfer */
    {
    int inc, i, totch;
    long scale = 0;
    float scalefact = 1.0;
    float tmp[SNDF_MAXCHANS];

    if(inch <= 0 || inch > SNDF_MAXCHANS || ouch <= 0 || ouch > SNDF_MAXCHANS)
	{  FPRINTF((stderr, "ConvFCh: silly inch %d or ouch %d\n",
		   inch, ouch));	abort(); }
    while( (ouch<<scale) < inch)	{  ++scale;  scalefact /= 2.0; }

    if(inch>ouch)	{  inc = 1;  totch = inch; }
    else		{  inc = -1; totch = ouch;
			   inBuf += inch * (len-1); 
			   outBuf += ouch * (len-1); }  
                        /* start at end */
    while(len--)
	{
	for(i=0; i<ouch; ++i)
	    tmp[i] = 0;
	for(i=0; i<totch; ++i)
	    tmp[(i%ouch)] += (*(inBuf+(i%inch)))*scalefact;
	for(i=0; i<ouch; ++i)
	    *(outBuf+i) = tmp[i];
	inBuf  += inc*inch;
	outBuf += inc*ouch;
	}
    }


/* from snd.c */
long ConvertChansAnyFmt(void *inbuf, void *outbuf, int inchans, int outchans, 
		      			int format, long frames)
{   /* convert the number of channels in a sound of arbitrary format */
    int bps = SFSampleSizeOf(format);

    if(inchans == outchans)  {
	if(inbuf == outbuf)	
	    return frames;
	else  {
	    memcpy(outbuf, inbuf, frames*inchans*bps);
	    return frames;
	}
    }
    if(format == SFMT_SHORT)  {
	ConvertChannels(inbuf, outbuf, inchans, outchans, frames);
    } else if(format == SFMT_FLOAT) {
	ConvertFlChans(inbuf, outbuf, inchans, outchans, frames);
    } else {
	DBGFPRINTF((stderr, 
	 "CvtChansAnyFmt: cannot convert %d to %d chans for data format %d\n", 
	 inchans, outchans, format));
	return 0;
    }
    return frames;
}

long CvtChansAndFormat(void *inbuf, void *outbuf, int inchans, int outchans, 
		    		 int infmt, int outfmt, long frames)
{   /* convert the data format and the number of channels for a buffer 
       of sound.  Use smallest possible buffer space */
    int ibs = SFSampleSizeOf(infmt);
    int obs = SFSampleSizeOf(outfmt);

    if( (ibs*outchans) <= (obs*outchans) )  {
	/* size of channel conversion ( <ibs,ichans> => <ibs,ochans> )
	   prior to format conversion will fit in output buffer */
	ConvertChansAnyFmt(inbuf, outbuf, inchans, outchans, infmt, frames);
	ConvertSampleBlockFormat(outbuf, outbuf, infmt,outfmt,frames*outchans);
    } else if( (obs*inchans) <= (obs*outchans) )  {
	/* size of format coversion first will fit in output buffer */
	ConvertSampleBlockFormat(inbuf, outbuf, infmt, outfmt, frames*inchans);
	ConvertChansAnyFmt(outbuf, outbuf, inchans, outchans, outfmt, frames);
    } else {	/* neither stage will fit in output buffer - must both be 
		   reducing (e.g float->short *and* stereo->mono) */
	void *tmpBuf = TMMALLOC(char, ibs*outchans*frames, "CnvCF");
	
	ConvertChansAnyFmt(inbuf, tmpBuf, inchans, outchans, infmt, frames);
	ConvertSampleBlockFormat(tmpBuf, outbuf, infmt,outfmt,frames*outchans);
	TMFREE(tmpBuf, "CnvCF");
    }
    return frames;
}

#ifndef FOR_MATLAB
/* from sndinfo.c ... */

void PrintSNDFinfo(
    SFSTRUCT *theSound,
    char *name,
    char *tag,
    FILE *stream)	/* where to print */
    {
    float	durn;
    int 	dsize,chans,infolen;
    long 	frames,dBytes;
    char	type[32];

    if (name == NULL)	name = "";
    if (tag == NULL)	tag = "";
    if (theSound == NULL) {
	fprintf(stream, "%s : (%s) NULL pointer\n", name, tag);
	return;
    } else if (theSound->magic != SFMAGIC) {
	fprintf(stream, "%s : (%s) Invalid magic (0x%lx not 0x%lx)\n", 
		name, tag, theSound->magic, SFMAGIC);
	return;
    }

    switch(theSound->format)
	{
    case SFMT_CHAR:
	strcpy(type, "linear bytes");
	dsize = 1;
	break;
    case SFMT_ALAW:
	strcpy(type, "a-law bytes");
	dsize = 1;
	break;
    case SFMT_ULAW:
	strcpy(type, "mu-law bytes");
	dsize = 1;
	break;
    case SFMT_SHORT:
	strcpy(type, "16 bit linear");
	dsize = 2;
	break;
    case SFMT_LONG:
	strcpy(type, "32 bit linear");
	dsize = 4;
	break;
    case SFMT_FLOAT:
	strcpy(type, "4 byte floating point");
	dsize = 4;
	break;
    default:	sprintf(type, "unrecognised type (0x%lx) - assuming 16 bit", 
			(unsigned long)theSound->format);
	dsize = 2;
	break;
	}
    infolen = SFInfoSize(theSound);
    chans = theSound->channels;
    dBytes = theSound->dataBsize;
    if(dBytes != SF_UNK_LEN)
	{
	frames = theSound->dataBsize/(long)(dsize*chans);
	durn = ((float)frames)/theSound->samplingRate;
	fprintf(stream,
		"%s : (%s)\n  Sample rate %5.0f Hz\t Duration %6.3f s\n",
		name, tag, theSound->samplingRate, durn);
	}
    else{
	fprintf(stream, 
		"%s : (%s)\n  Sample rate %5.0f Hz\t Duration  (unknown)\n",
		name, tag, theSound->samplingRate);
	}
    if(chans==1)
        fprintf(stream, "  Mono   \t \t %s\n",type);
    else if(chans==2)
        fprintf(stream, "  Stereo \t \t %s\n",type);
    else 
	fprintf(stream, "  %d channels, %s\n",chans,type);
    if(theSound->dataBsize != SF_UNK_LEN)
        fprintf(stream, "  data bytes %ld \t info bytes  %5d\n", 
	       theSound->dataBsize, infolen);
    else
        fprintf(stream, "  data bytes UNK \t info bytes  %5d\n", 
	       infolen);
    if(PrintableStr(theSound->info, infolen))
	fprintf(stream, "'%s'\n", theSound->info);
    }

int PrintableStr(
    char *s,
    int max)
    /* return whether to print string s, of len <= max, coz it looks good */
    {
    char c, *t;
    int  g=0;

    if(max > 0)
	{
	g = 1;
	t = s;
	while( g && (c = *t++) && --max)
	    {
	    if( (c < ' ') && !( c=='\n' || c=='\r' || c=='\t'))
		g = 0;	/* nasty control code */
	    /* one of these won't work dep on char u/s */
#ifdef __CHAR_UNSIGNED__
	    if(c>0x7F)	
#else /* !__CHAR_UNSIGNED */
	    if(c<0x0)
#endif /* __CHAR_UNSIGNED */
		g = 0;	/* beyond ascii */
	    }
	if(c)	g = 0;	/* must finish on nul */
	if(t == s+1)	g = 0;	/* null string - don't bother */
	}
    return(g);
    }

#endif /* !FOR_MATLAB */
