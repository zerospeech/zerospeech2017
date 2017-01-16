/***************************************************************\
*   sndfnxt.c			NeXT				*
*   Header functions for SNeXT .snd header version of sndf.c	*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   25apr91 after 24sep90 after12aug90 dpwe			*
\***************************************************************/

char *sndfExtn = ".snd";		/* file extension, if used */

#ifdef THINK_C
#include <unix.h>
#else
#ifdef NeXT
#else
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#endif
#include <fcntl.h>

#include "byteswap.h"

#ifdef THINK_C
#define FOURBYTE long
#else
#define FOURBYTE int
#endif

#define NXT_DFLT_INFO	4
typedef struct {
	/* were all ints - called FOURBYTE for Macintosh to make 32 bit */
    FOURBYTE magic;		/* must be equal to NXT_MAGIC */
    FOURBYTE dataLocation;	/* Offset or pointer to the raw data */
    FOURBYTE dataSize;	/* Number of bytes of data in the raw data */
    FOURBYTE dataFormat;	/* The data format code */
    FOURBYTE samplingRate;	/* The sampling rate */
    FOURBYTE channelCount;	/* The number of channels */
    char info[NXT_DFLT_INFO];	/* Textual info relating to the sound. */
} SNDSoundStruct;

#define NXT_MAGIC ((FOURBYTE)0x2e736e64)
#define NXT_FORMAT_UNSPECIFIED		(0)
#define NXT_FORMAT_MULAW_8		(1)
#define NXT_FORMAT_LINEAR_8		(2)
#define NXT_FORMAT_LINEAR_16		(3)
#define NXT_FORMAT_LINEAR_24		(4)
#define NXT_FORMAT_LINEAR_32		(5)
#define NXT_FORMAT_FLOAT		(6)
#define NXT_FORMAT_DOUBLE		(7)
#define NXT_FORMAT_INDIRECT		(8)
#define NXT_FORMAT_NESTED		(9)
#define NXT_FORMAT_DSP_CORE		(10)
#define NXT_FORMAT_DSP_DATA_8		(11)
#define NXT_FORMAT_DSP_DATA_16		(12)
#define NXT_FORMAT_DSP_DATA_24		(13)
#define NXT_FORMAT_DSP_DATA_32		(14)
#define NXT_FORMAT_DISPLAY		(16)
#define NXT_FORMAT_MULAW_SQUELCH	(17)


static SNDSoundStruct *pnxtsfh = NULL;	/* just to grab some static space */


/* ----------------------- implementations ------------------------ */

#ifdef OLD
/* static function prototypes */
#ifdef __STDC__
static char *FindFname(FILE *file);
    /* looks up file ptr, returns mapped name if any */
#else
static char *FindFname();
    /* looks up file ptr, returns mapped name if any */
#endif

static void AddFname(fname,file)
    char *fname;
    FILE *file;
    {
    }

static void RmvFname(file)
    FILE *file;
    {
    }

static char *FindFname(file)
    FILE *file;
    {
    }
#endif /* OLD */

static int nxtbytemode = -1;

int SFReadHdr(file, snd, infoBmax)
    FILE *file;
    SFSTRUCT *snd;
    int infoBmax;
/* util to sample the header of an opened file, at most infoBmax info bytes */
    {
    long    infoBsize, infoXtra, bufXtra;
    long    rdsize;
    size_t  num,red;
    long    ft;
    char    *sdf_infp;
    int     chans,formt,i;
    float   srate;
    int     skipmagic = 0;	/* or SIZEOF(MAGIC) to skip it */
    int	    bm;
    
    if((bm = GetBytemode(file)) != -1)	nxtbytemode = bm;
    if(nxtbytemode < 0)	nxtbytemode = HostByteMode();
    SetBytemode(file, nxtbytemode);
    DBGFPRINTF((stderr, "NeXT RdHd: nxtbm = %d\n", nxtbytemode));

    if(pnxtsfh == NULL)		/* check our personal NeXT .snd hdr buf */
	if( (pnxtsfh = 
	     (SNDSoundStruct *)malloc((size_t)sizeof(SNDSoundStruct)))==NULL)
	    return(SFE_MALLOC);	/* couldn't alloc our static buffer */

    if(infoBmax < SFDFLTBYTS)	infoBmax = SFDFLTBYTS;
    infoBmax = (infoBmax)&-4;	/* round DOWN to x4 (can't exceed given bfr) */
    bufXtra = (long)(infoBmax-SFDFLTBYTS);

    pnxtsfh->magic = 0L;	/* can test for valid read.. */
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
    if(skipmagic)	pnxtsfh->magic = lshuffle(NXT_MAGIC, nxtbytemode);

    rdsize = sizeof(SNDSoundStruct) - NXT_DFLT_INFO;  /* don't read any info */
    /* fill up our buffer, starting *after* the magic number, if appropriate */
    num = skipmagic;
    do	{
	red = fread((void *)((char *)pnxtsfh + num), 
		    (size_t)1, (size_t)rdsize-num, file);
	num += red;
	} while(num < rdsize && red > 0);
    if(num != rdsize)		/* couldn't get that many bytes */
	return(SFerror = SFE_RDERR); /* .. call it a read error */

    /* fixup byte order, not for the info chars */
    ConvertBufferBytes((char *)pnxtsfh, num, 4, nxtbytemode);

    if(pnxtsfh->magic != NXT_MAGIC) /* compare with NeXT magic number */
	return(SFerror = SFE_NSF); /* got the bytes, but look wrong */
    /* Else must be ok.  Extract rest of sfheader info */
    srate = (float)pnxtsfh->samplingRate;	/* stored as integer */
    chans = pnxtsfh->channelCount;
    formt = pnxtsfh->dataFormat;
    switch(formt)
	{
    case NXT_FORMAT_MULAW_8:	formt = SFMT_ULAW;	break;
    case NXT_FORMAT_LINEAR_8:	formt = SFMT_CHAR;	break;
    case NXT_FORMAT_LINEAR_16:	formt = SFMT_SHORT;	break;
    case NXT_FORMAT_LINEAR_32:	formt = SFMT_LONG;	break;
    case NXT_FORMAT_FLOAT:	formt = SFMT_FLOAT;	break;
    case NXT_FORMAT_DOUBLE:	formt = SFMT_DOUBLE;	break;
    default:			/* leave formt unchanged ! */
	fprintf(stderr,"sndfnxt: unrecog. format %d\n",formt);
	break;
	}
    /* read the info straight into the buffer */
    sdf_infp = &(snd->info[0]);
    infoBsize = pnxtsfh->dataLocation - sizeof(SNDSoundStruct) + NXT_DFLT_INFO;
    if(infoBmax >= infoBsize)
	i = infoBsize;
    else
	i = infoBmax;		/* can't copy the whole info, just this bit */
    infoXtra = infoBsize - i;	/* bytes of info we couldn't yet read */
    /* fill up our buffer */
    rdsize = i;
    num = 0;
    do	{
	red = fread((void *)((char *)sdf_infp + num), 
		    (size_t)1, (size_t)rdsize-num, file);
	num += red;
	} while(num < rdsize && red > 0);
    if(num != rdsize)		/* couldn't get that many bytes */
	return(SFerror = SFE_RDERR); /* .. call it a read error */
    /* rewrite into different data structure */
    snd->magic = SFMAGIC;	/* OUR magic number, not NeXT's */
    snd->headBsize = infoBsize + sizeof(SFSTRUCT) - SFDFLTBYTS;
/*    snd->dataBsize = GetFileSize(file);
    if(snd->dataBsize != SF_UNK_LEN)
	snd->dataBsize -= sizeof(SFHEADER);
 */
    snd->dataBsize = pnxtsfh->dataSize;
    snd->format = formt;
    snd->channels = chans;
    snd->samplingRate = srate;

    SetSeekEnds(file, pnxtsfh->dataLocation, 
		pnxtsfh->dataLocation + snd->dataBsize);

    return(SFerror = SFE_OK);
    }

int SFRdXInfo(file, psnd, done)
    FILE *file;
    SFSTRUCT *psnd;
    int done;
/*  fetch any extra info bytes needed in view of read header
    Passes open file handle, existing sound struct
    already read 'done' bytes; rest inferred from structure */
    {
    size_t  num;
    long    subhlen;
    long    file_pos;
    int     rem;

    rem = SFInfoSize(psnd) - done;
    if(rem <= 0)
	return(SFerror = SFE_OK);	/* nuthin more to read */
    /* do not assume seek already at right place.. */
    subhlen = (((pnxtsfh->info)) - (char *)pnxtsfh) + (long)done;
    file_pos = ftell(file);
    if( (subhlen - file_pos) != 0) 	/* if not at right place.. */
	if(fseek(file, subhlen - file_pos, SEEK_CUR) != 0 )	/* relative */
	    return(SFerror = SFE_RDERR);
    num = fread((void *)(psnd->info+done), (size_t)1, (size_t)rem, file);
    if(num < (size_t)rem)
	return(SFerror = SFE_RDERR);
    else
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
    char    *nxt_infp, *sdf_infp;
    int     i;
    long    infoBsize,ft;
    int bm;

    if(snd == NULL) {	/* rewrite but can't seek */
	return SFE_OK;
    }

    if((bm = GetBytemode(file)) != -1)	nxtbytemode = bm;
    if(nxtbytemode < 0)	nxtbytemode = HostByteMode();
    SetBytemode(file, nxtbytemode);
    DBGFPRINTF((stderr, "NeXT WrHd: nxtbm = %d\n", nxtbytemode));

    if(pnxtsfh == NULL)		/* check our personal NeXT .snd hdr buf */
	if( (pnxtsfh = 
	     (SNDSoundStruct *)malloc((size_t)sizeof(SNDSoundStruct)))==NULL)
	    return(SFE_MALLOC);	/* couldn't alloc our static buffer */

    if(snd->magic != SFMAGIC)
	return(SFerror = SFE_NSF);
    if(infoBmax < SFDFLTBYTS || infoBmax > SFInfoSize(snd) )
	infoBmax = SFInfoSize(snd);
    ft = ftell(file);
    if( ft > 0 )			/* don't try seek on pipe */
	{
	if(fseek(file, (long)0, SEEK_SET) != 0 )   /* move to start of file */
	return(SFerror = SFE_WRERR);
	}
    /* copy to private buffer.. */
    pnxtsfh->magic = NXT_MAGIC;
    pnxtsfh->samplingRate  = (long)snd->samplingRate;
    pnxtsfh->channelCount  = snd->channels;
    pnxtsfh->dataSize      = snd->dataBsize;
    pnxtsfh->dataLocation  = sizeof(SNDSoundStruct)+infoBmax-NXT_DFLT_INFO;
    switch(snd->format)
	{
    case SFMT_CHAR:	pnxtsfh->dataFormat = NXT_FORMAT_LINEAR_8;	break;
    case SFMT_ALAW:	/* can't handle this */
    case SFMT_ULAW:	pnxtsfh->dataFormat = NXT_FORMAT_MULAW_8;	break;
    case SFMT_SHORT:	pnxtsfh->dataFormat = NXT_FORMAT_LINEAR_16;	break;
    case SFMT_LONG:	pnxtsfh->dataFormat = NXT_FORMAT_LINEAR_32;	break;
    case SFMT_FLOAT:	pnxtsfh->dataFormat = NXT_FORMAT_FLOAT;	break;
    case SFMT_DOUBLE:	pnxtsfh->dataFormat = NXT_FORMAT_DOUBLE;	break;
    default:	    /* leave formt unchanged ! */
	fprintf(stderr,"sndfnxt: unrecog. format %d\n",snd->format);
	pnxtsfh->dataFormat = snd->format;
	break;
	}
    /* copy across info provided */
    nxt_infp = &(pnxtsfh->info[0]);
    sdf_infp = &(snd->info[0]);
    infoBsize = NXT_DFLT_INFO;
    if(infoBmax >= infoBsize)
	i = infoBsize;
    else
	i = infoBmax;
    while(i>0)
	{
	--i;
	*nxt_infp++ = *sdf_infp++;  /* copy between sep buffers */
	}
    /* write out 'core' header */
    hedSiz = sizeof(SNDSoundStruct);  /* write a whole header in any case */
    /* fixup byte order */
    ConvertBufferBytes((char *)pnxtsfh, hedSiz - NXT_DFLT_INFO, 4, nxtbytemode);
    if( (num = fwrite((void *)pnxtsfh, (size_t)1, (size_t)hedSiz,
			 file))< (size_t)hedSiz)
	{
    	fprintf(stderr, "SFWrH: tried %ld managed %ld\n",
		hedSiz, (long)num);	
	return(SFerror = SFE_WRERR);
	}
    /* reset byte order */
    ConvertBufferBytes((char *)pnxtsfh, hedSiz - NXT_DFLT_INFO, 4, nxtbytemode);
    /* write out any additional info */
    if(infoBmax > infoBsize)
	{
	i = infoBmax - infoBsize;
	if( (num = fwrite((void *)(&(snd->info[infoBsize])), (size_t)1, 
			  (size_t)i, file))<(size_t)i)
	    {
	    fprintf(stderr, "SFWrH: tried %ld info, managed %ld\n",
		    (long)i, (long)num);	
	    return(SFerror = SFE_WRERR);
	    }
	}

    return(SFerror = SFE_OK);
    }

static long SFHeDaLen(FILE *file)
    /* return data space used for header in file - see SFCloseWrHdr */
    {
    long hlen = 0;

    if(fseek(file, (long) &((SNDSoundStruct *) 0)->dataLocation, SEEK_SET)
	!= 0 )	    /* need to re-read infoBsize */
	{
	fread((void *)&hlen, (size_t)sizeof(INT32), (size_t)1,file);
	/* fixup byte order */
	ConvertBufferBytes((char *)hlen, 4, 4, nxtbytemode);
	}
    else
	hlen = (long)sizeof(SNDSoundStruct);	/* default */
    return(hlen);
    }

static long SFTrailerLen(FILE *file)
{   /* return data space after sound data in file (for multiple files) */
    return(0);
}

static long SFLastSampTell(file)	/* return last valid ftell for smps */
    FILE	*file;
    {
    return (0L);	/* punt on this for here - just use EOF */
    }

static long FixupSamples(file, ptr, format, nitems, flag)
    FILE  *file;
    void  *ptr;
    int   format;
    long  nitems;
    int   flag;		/* READ_FLAG or WRITE_FLAG indicates direction */
{
    int wordsz;
    long bytes;

    wordsz = SFSampleSizeOf(format);
    bytes  = nitems * (long)wordsz;
    if(nxtbytemode != BM_INORDER) {	/* already OK */
	/* otherwise need to fixup buffer .. */
	ConvertBufferBytes(ptr, bytes, wordsz, nxtbytemode);
    }
    return nitems;
}

#ifdef WASMACHORDER
static long FixupSamples(file, ptr, format, nitems, flag)
    FILE  *file;
    void  *ptr;
    int   format;
    long  nitems;
    int   flag;		/* READ_FLAG or WRITE_FLAG indicates direction */
    {
	return nitems;
    }	/* always machine order */
#endif /* WASMACHORDER */
