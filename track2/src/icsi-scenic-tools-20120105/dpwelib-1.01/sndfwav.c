/***************************************************************\
*   sndfwav.c			MS-RIFF/WAVE			*
*   Header functions for MS WAVE header version of sndf.c	*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   1994sep20 after sndfirc.c 24sep90 after 12aug90 dpwe	*
\***************************************************************/

char *sndfExtn = ".wav";		/* file extension, if used */

#ifdef THINK_C
#include <unix.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <fcntl.h>

#include <byteswap.h>		/* for lshuffle() */

typedef struct wav_head {
  INT32	magic;			/* 'RIFF' */
  INT32	riffsize;		/* size of whole file excluding these 8 bytes*/
  INT32	magic1;			/* 'WAVE' */
  INT32	magic2;			/* 'fmt ' */
  INT32	fmt_size;		/* size of fmt chunk == 16 or 18 */
  short	format;			/* 1 is PCM, 7 is ulaw, rest not known */
  short	nchns;			/* Number of channels */
  INT32	rate;			/* sampling frequency */
  INT32	aver;			/* Average bytes/sec !! */
  short	nBlockAlign;		/* (rate*nch +7)/8 */
  short	size;			/* size of each sample (8,16,32) */
  /* sometimes another short in here (for 18-byte fmt_size) */
  /* sometimes a 'fact' block in here, or whatever */
  INT32	magic3;			/* 'data' */
  INT32	datasize;		/* data chunk size */
} WAVHEADER;

typedef struct wav_head_base {
  INT32	magic;			/* 'RIFF' */
  INT32	riffsize;		/* size of whole file excluding these 8 bytes*/
  INT32	magic1;			/* 'WAVE' */
  INT32	magic2;			/* 'fmt ' */
  INT32	fmt_size;		/* size of fmt chunk == 16? 18? */
  short	format;			/* 1 is PCM, 7 is ulaw, rest not known */
  short	nchns;			/* Number of chanels */
  INT32	rate;			/* sampling frequency */
  INT32	aver;			/* Average bytes/sec !! */
  short	nBlockAlign;		/* (rate*nch +7)/8 */
  short	size;			/* size of each sample (8,16,32) */
} WAVHEAD0;

typedef struct wav_head_safe {  /* this much we can read for sure 2007-05-15 */
  INT32	magic;			/* 'RIFF' */
  INT32	riffsize;		/* size of whole file excluding these 8 bytes*/
  INT32	magic1;			/* 'WAVE' */
  INT32	magic2;			/* 'fmt ' */
  INT32	fmt_size;		/* size of fmt chunk == 16? 18? */
} WAVHEADSAFE;

typedef struct wav_fact {
  INT32	magic4;			/* 'fact' */
  INT32 factsize;		/* 0x000004 in this example */
  INT32	datasize;		/* data chunk size (bytes or frames?) */
} WAVFACT;

typedef struct wav_blkhd {
  INT32	magic5;			/* 4 chr block type */
  INT32 size;		        /* block length (excluding these 8 bytes) */
} WAVBLK;


#define WAV_FMT_PCM 	1
#define WAV_FMT_ULAW 	7


static char     RIFF_ID[4] = {'R','I','F','F'};
static char     WAVE_ID[4] = {'W','A','V','E'};
static char     FMT_ID[4]  = {'f','m','t',' '};
static char     DATA_ID[4] = {'d','a','t','a'};

static WAVHEADER *pwavsfh = NULL;	    /* just to grab some static space */
static WAVBLK *pwavblk = NULL;

/* ----------------------- implementations ------------------------ */

#ifdef OLD

static void AddFname(fname,file)
    char *fname;
    FILE *file;
    /* associates name 'fname\0' with file ptr 'file' in our lookup table */
    {
    }

static void RmvFname(file)
    FILE *file;
    /* removes any fnameinfo record associated with 'file' */
    {
    }

#endif /* OLD */

static void FixWavHeader(WAVHEADER *wvh, int bytemode)
{	/* swap the bytes for native machine order */
    /* leave magics in string order */
    wvh->riffsize   = lshuffle(wvh->riffsize, bytemode);
    wvh->fmt_size   = lshuffle(wvh->fmt_size, bytemode);
    wvh->format = wshuffle(wvh->format, bytemode);
    wvh->nchns  = wshuffle(wvh->nchns, bytemode);
    wvh->rate   = lshuffle(wvh->rate, bytemode);
    wvh->aver   = lshuffle(wvh->aver, bytemode);
    wvh->nBlockAlign = wshuffle(wvh->nBlockAlign, bytemode);
    wvh->size   = wshuffle(wvh->size, bytemode);
    wvh->datasize = lshuffle(wvh->datasize, bytemode);
}

static int RightByteMode(void)
{	/* return the right byte mode to apply to WAV headers.
	   They store 0x12345678 as 0x78 0x56 0x34 0x12, so find 
	   how to do that. */
    INT32 sigl1 = 0x44434241;
    INT32 sigl2;

    memcpy(&sigl2, "ABCD", 4);
    return MatchByteMode(sigl1, sigl2);
}

int SFReadHdr(file, snd, infoBmax)
    FILE *file;
    SFSTRUCT *snd;
    int infoBmax;
    {
    long    infoBsize;
    long    rdsize;
    size_t  num,red;
    long    ft;
    char    *irc_infp, *sdf_infp;
    int     chans,formt,size,i;
    float   srate;
    int	    bytemode = RightByteMode();
    long    skipmagic = 0;
    int	    hdr_extra = 0;

    if(pwavsfh == NULL)	{	/* check our personal header buf */
      if( (pwavsfh = (WAVHEADER *)malloc((size_t)sizeof(WAVHEADER)))==NULL)
	return(SFerror=SFE_MALLOC); /* couldn't alloc our static buffer */
      if( (pwavblk = (WAVBLK *)malloc((size_t)sizeof(WAVBLK)))==NULL)
	return(SFerror=SFE_MALLOC); /* couldn't alloc our static buffer */
    }

    if(infoBmax < SFDFLTBYTS)	infoBmax = SFDFLTBYTS;
    infoBmax = (infoBmax)&-4;	/* round DOWN to x4 (can't exceed given bfr) */

    pwavsfh->magic = 0L;		/* can test for valid read.. */
    ft = ftell(file);
    if( ft < 0 ) {			/* don't try seek on stdin */
	/* pipe - skip magic if already ID'd */
	if (GetSFFType(file) != NULL) {
#ifdef HAVE_FPUSH
	    /* If we have fpush, we'll have pushed back to zero anyway */
	    skipmagic = 0;
#else /* !HAVE_FPUSH */
	    /* we'll guess that the type came from SFIdentify, although 
	       it *could* have come from SFSetFileType, in which case 
	       we'll fail */
	    skipmagic = sizeof(INT32);
#endif /* HAVE_FPUSH */
	}
    } else if (ft == sizeof(INT32)) {
	/* assume we already read the magic & fake it up */
	skipmagic = ft;
    } else {
	if(fseek(file,(long)0,SEEK_SET) != 0 ) /* move to start of file */
	    return(SFerror = SFE_RDERR);
    }
    /* strings stay in string order */
    if(skipmagic)    pwavsfh->magic = *(INT32*)(RIFF_ID);

    /* rdsize = sizeof(WAVHEADER); */
    rdsize = sizeof(WAVHEADSAFE);
    /* fill up our buffer */
    num = skipmagic;
    do	{
	red = fread(((char *)pwavsfh)+num, (size_t)1, (size_t)rdsize-num, file);
	num += red;
	} while(num < rdsize && red > 0);
    if(num != rdsize)		/* couldn't get that many bytes */
	return(SFerror = SFE_RDERR); /* .. call it a read error */

    DBGFPRINTF((stderr, "WAV RdHdr: magic = 0x%lx (should be 0x%lx)\n", 
		pwavsfh->magic, *(INT32*)RIFF_ID));

    if(memcmp(RIFF_ID, &pwavsfh->magic, 4)!=0) /* compare with RIFF magicnum */
	return(SFerror = SFE_NSF); /* got the bytes, but look wrong */

    /* 2007-05-15: now read rest of 'fmt ' per declared size */
    rdsize = num + lshuffle(pwavsfh->fmt_size, bytemode);
    assert(rdsize < sizeof(WAVHEADER)); /* else will overrun buffer */
    do	{
	red = fread(((char *)pwavsfh)+num, (size_t)1, (size_t)rdsize-num, file);
	num += red;
	} while(num < rdsize && red > 0);
    if(num != rdsize)		/* couldn't get that many bytes */
	return(SFerror = SFE_RDERR); /* .. call it a read error */
    hdr_extra += rdsize - sizeof(WAVHEAD0);
    /* now read ptr is at start of next chunk - 'fact' or 'data' */

    /* read in the chunk type and its size */
    red = fread((char *)pwavblk, (size_t)1, (size_t)(sizeof(WAVBLK)), file);
    if(red != sizeof(WAVBLK))	/* couldn't get that many bytes */
      return(SFerror = SFE_RDERR); /* .. call it a read error */
    /* this should obviate the off-by-2 check below, but leave it in */

    /* skip over non-data blocks 2007-12-21 */
    while(memcmp(DATA_ID, &pwavblk->magic5, 4) != 0) {
        /* seek over unknown block */
        rdsize = lshuffle(pwavblk->size,bytemode);
        seek_by_read(file, rdsize);
	hdr_extra += rdsize;
	/* read in next header */
        red = fread((char *)pwavblk, (size_t)1, sizeof(WAVBLK), file);
	if(red != sizeof(WAVBLK))	/* couldn't get that many bytes */
	  return(SFerror = SFE_RDERR); /* .. call it a read error */
	hdr_extra += red;
    }
    /* fixup for later - copy header*/
    memcpy(&pwavsfh->magic3, DATA_ID, 4);
    /* set data size */
    pwavsfh->datasize = pwavblk->size;

    FixWavHeader(pwavsfh, bytemode);

    /* Else must be ok.  Extract rest of sfheader info */
    srate = pwavsfh->rate;
    chans = pwavsfh->nchns;
    formt = pwavsfh->format;
    size  = pwavsfh->size;
    if(formt != WAV_FMT_PCM && formt != WAV_FMT_ULAW)  {	/* only types of WAV we know */
	fprintf(stderr,"sndfwav: unrecog. format %d\n",formt);
	return(SFerror = SFE_NSF);
    }
    switch(size)
	{
    case 8:   
	if(formt == WAV_FMT_ULAW) {
	    formt = SFMT_ULAW;
	} else {
	    formt = SFMT_CHAR;
	}
	break;
    case 16:  formt = SFMT_SHORT; 	break;
    case 32:  formt = SFMT_LONG;	break;
    default:			/* unknown size */
	fprintf(stderr,"sndfwav: bad sample size %d\n",size);
	return(SFerror = SFE_NSF);
    }
    /* copy across info as read */
    /* rewrite into different data structure */
    snd->magic = SFMAGIC;	/* OUR magic number */
    snd->headBsize = sizeof(SFSTRUCT);	/* no info for WAV */
    snd->dataBsize = pwavsfh->datasize;
    /* maybe treat zero as unknown (for mpg123 stream output) */
    if (snd->dataBsize == 0) {
	snd->dataBsize = SF_UNK_LEN;
	SetSeekEnds(file, sizeof(WAVHEADER)+hdr_extra, 
		    SF_UNK_LEN);
    } else {
	SetSeekEnds(file, sizeof(WAVHEADER)+hdr_extra, 
		    sizeof(WAVHEADER)+hdr_extra+snd->dataBsize);
    }
    if( snd->dataBsize == SF_UNK_LEN)  {
	snd->dataBsize = GetFileSize(file);
	if(snd->dataBsize == -1)  {
	    snd->dataBsize = SF_UNK_LEN;
	} else {  /* got file size - adjust for length of header */
	    snd->dataBsize -= sizeof(WAVHEADER)+hdr_extra;
	}
    }
    snd->format = formt;
    snd->channels = chans;
    snd->samplingRate = srate;

    /* fprintf(stderr, "wav::SFReadHdr: fmt=%d, chs=%d, sr=%.0f, dby=%ld\n", 
	    snd->format, snd->channels, snd->samplingRate, snd->dataBsize);
     */
    return(SFerror = SFE_OK);
}

int SFRdXInfo(file, psnd, done)
    FILE *file;
    SFSTRUCT *psnd;
    int done;
/* fetch any extra info bytes needed in view of read header
    Passes open file handle, existing sound struct
    already read 'done' bytes; rest inferred from structure */
    {
    /* does nothing for WAV */
    return(SFerror = SFE_OK);
    }

int SFWriteHdr(file, snd, infoBmax)
    FILE *file;
    SFSTRUCT *snd;
    int infoBmax;
/*  Utility to write out header to open file
    infoBmax optionally limits the number of info chars to write.
    infoBmax = 0 => write out all bytes implied by .headBsize field
    infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */
    {
    size_t  num;
    long    hedSiz;
    char    *irc_infp, *sdf_infp;
    int     i;
    long    infoBsize,ft,len;
    int	    bytemode = RightByteMode();

    if(snd == NULL) {	/* rewrite but can't seek */
	return SFE_OK;
    }

    if(pwavsfh == NULL) {	   /* check our personal sfheader buf */
      if( (pwavsfh = (WAVHEADER *)malloc((size_t)sizeof(WAVHEADER)))==NULL)
	return(SFerror=SFE_MALLOC); /* couldn't alloc our static buffer */
      if( (pwavblk = (WAVBLK *)malloc((size_t)sizeof(WAVBLK)))==NULL)
	return(SFerror=SFE_MALLOC); /* couldn't alloc our static buffer */
    }

    if(snd->magic != SFMAGIC)
	return(SFerror = SFE_NSF);
    if(infoBmax < SFDFLTBYTS || infoBmax > SFInfoSize(snd) )
	infoBmax = SFInfoSize(snd);
    ft = ftell(file);
    if( ft >= 0 )			/* don't try seek on pipe */
	{
	if(fseek(file, (long)0, SEEK_SET) != 0 )   /* move to start of file */
	return(SFerror = SFE_WRERR);
	}
    /* configure private buffer.. */
    memcpy(&pwavsfh->magic, RIFF_ID, 4);
    memcpy(&pwavsfh->magic1, WAVE_ID, 4);
    memcpy(&pwavsfh->magic2, FMT_ID, 4);
    memcpy(&pwavsfh->magic3, DATA_ID, 4);
    pwavsfh->rate   = (INT32)snd->samplingRate;
    pwavsfh->nchns  = snd->channels;
    pwavsfh->format = WAV_FMT_PCM;
    switch(snd->format)
	{
    case SFMT_ULAW: pwavsfh->size = 8; pwavsfh->format = WAV_FMT_ULAW; break;
    case SFMT_CHAR:	pwavsfh->size = 8;		break;
    case SFMT_SHORT:	pwavsfh->size = 16;	break;
    case SFMT_LONG:	pwavsfh->size = 32;	break;
    default:	    
	fprintf(stderr,"sndfwav: format %d cannot go in WAV\n",snd->format);
	return(SFerror = SFE_NSF);
    }
    /* attempt to set up size */
    pwavsfh->fmt_size = 16;	/* always */
    if( (len = snd->dataBsize)==SF_UNK_LEN)  {
	pwavsfh->riffsize = -1;
	pwavsfh->datasize = -1;
    } else {
	pwavsfh->datasize = snd->dataBsize;
	pwavsfh->riffsize = pwavsfh->datasize + sizeof(WAVHEADER) - 8;
	/* - 8 because riff size does not count RIFF and the size itself */
    }
    pwavsfh->aver = snd->samplingRate * snd->channels * (pwavsfh->size/8);
    pwavsfh->nBlockAlign = snd->channels * (pwavsfh->size/8);
    FixWavHeader(pwavsfh, bytemode);
    hedSiz = sizeof(WAVHEADER);	    /* write a whole header in any case */
    if( (num = fwrite((void *)pwavsfh, (size_t)1, (size_t)hedSiz,
			 file))< (size_t)hedSiz)
	{
    	fprintf(stderr, "SFWrH: tried %ld managed %ld\n",
		hedSiz, (long)num);	
	return(SFerror = SFE_WRERR);
	}
    return(SFerror = SFE_OK);
    }

static long SFHeDaLen(FILE *file)
{   /* return data space used for header in file - see SFCloseWrHdr */
    /* always the same for WAV soundfiles */
    /* EXCEPT when they're 'fact' style, and have e.g. 14 extra bytes */
    /*	 return(sizeof(WAVHEADER)); */
    long stt, end;

    GetSeekEnds(file, &stt, &end);

    if (stt == 0) {
	/* default for when GetSeekEnds fails e.g. for file being written */
	stt = sizeof(WAVHEADER);  
    }

    return stt;
}

static long SFTrailerLen(FILE *file)
{   /* return data space after sound data in file (for multiple files) */
    return(0);
}

static long SFLastSampTell(file)	/* return last valid ftell for smps */
    FILE	*file;
    {
    long stt, end;

    GetSeekEnds(file, &stt, &end);
    return end;
    }

static void FlipTopBit(unsigned char *ptr, long len)
{	/* need to offset bytes by 128 */
    while(len--)  {
	*ptr++ ^= 0x80;
    }
}

static long FixupSamples(file, ptr, format, nitems, flag)
    FILE  *file;
    void  *ptr;
    int   format;
    long  nitems;
    int   flag;		/* READ_FLAG or WRITE_FLAG indicates direction */
{
    int	    bytemode = RightByteMode();
#ifdef DEBUG
    static int debug = 1;
    if(debug) {
	fprintf(stderr, "MSWAV FxSmps: rbm = %d (fmt=%d)\n", bytemode, format);
	debug = 0;
    }
#endif /* DEBUG */

    switch(format) {
    case SFMT_CHAR:	FlipTopBit(ptr, nitems);		break;
    case SFMT_ULAW:	/* assume it's OK */		break;
    case SFMT_SHORT:	ConvertBufferBytes(ptr, nitems*sizeof(short), 
					   sizeof(short), bytemode);	break;
    case SFMT_LONG:	ConvertBufferBytes(ptr, nitems*sizeof(INT32), 
					   sizeof(INT32), bytemode);	break;
    default:	    
	fprintf(stderr,"sndfwav: format %d not known for WAV\n",format);
	SFerror = SFE_NSF;
	return 0;
    }
    return nitems;
}
