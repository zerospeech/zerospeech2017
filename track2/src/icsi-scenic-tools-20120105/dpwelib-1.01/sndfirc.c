/***************************************************************\
*   sndfirc.c			IRCAM				*
*   Header functions for SFIRCAM header version of sndf.c	*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   24sep90 after12aug90 dpwe					*
\***************************************************************/

char *sndfExtn = ".irc";		/* file extension, if used */

#ifdef THINK_C
#include <unix.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <fcntl.h>

#include <byteswap.h>

/* bodge bits to allow irc to correctly represent these formats */
#define IRCX_LONG		 32
#define IRCX_ALAW                 64
#define IRCX_ULAW                 128
/* from <sfheader.h> .. */
# define SIZEOF_HEADER 1024
# define IRC_MAGIC 107364L
# define IRC_CHAR  sizeof(char)    /* new sfclass, not SFIRCAM standard */
# define IRC_ALAW  sizeof(char)+IRCX_ALAW    /* ditto */
# define IRC_ULAW  sizeof(char)+IRCX_ULAW    /* ditto */
# define IRC_SHORT sizeof(short)
# define IRC_LONG  sizeof(INT32)+IRCX_LONG    /* ditto */
# define IRC_FLOAT sizeof(float)	     /* must remain 4 for 'compat' */
# define IRC_BUFSIZE	(16*1024) /* used only in play */

typedef union sfheader {
	struct {
		INT32	  sf_magic;
		float	  sf_srate;
		INT32	  sf_chans;
		INT32	  sf_packmode;
		char	  sf_codes;
	} sfinfo;
	char	filler[SIZEOF_HEADER];
} SFHEADER;


static SFHEADER *psfh = NULL;	    /* just to grab some static space */

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

int SFReadHdr(
    FILE *file,
    SFSTRUCT *snd,
    int infoBmax)
{   /* sample the header of an opened file, at most infoBmax info bytes */
/* BE WARNED: Reading back the header of a file opened as "w" (not "w+")
   seems to overwrite it with 0x0300 !!! (ultrix 4.0)	*/
    long    infoBsize;
    long    rdsize;
    size_t  num,red;
    long    ft;
    char    *irc_infp, *sdf_infp;
    int     chans,formt,i;
    float   srate;
    int 	skipmagic = 0;
    int bytemode = GetBytemode(file);

    if(bytemode < 0) {
	bytemode = HostByteMode();
	SetBytemode(file, bytemode);
    }

    if(psfh == NULL)		/* check our personal IRCAM sfheader buf */
	if( (psfh = (SFHEADER *)malloc((size_t)sizeof(SFHEADER)))==NULL)
	    return(SFerror=SFE_MALLOC); /* couldn't alloc our static buffer */

    if(infoBmax < SFDFLTBYTS)	infoBmax = SFDFLTBYTS;
    infoBmax = (infoBmax)&-4;	/* round DOWN to x4 (can't exceed given bfr) */

    infoBsize = sizeof(SFHEADER) - ((&(psfh->sfinfo.sf_codes)) - (char *)psfh);

    psfh->sfinfo.sf_magic = 0L;	/* can test for valid read.. */
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
    if(skipmagic)	psfh->sfinfo.sf_magic = lshuffle(IRC_MAGIC, bytemode);

    rdsize = sizeof(SFHEADER);
    if(infoBmax < infoBsize)
	rdsize -= infoBsize - infoBmax;		/* only read info as req'd */
    /* fill up our buffer */
    num = skipmagic;
    do	{
	red = fread(((char *)psfh)+num, (size_t)1, (size_t)rdsize-num, file);
	num += red;
	} while(num < rdsize && red > 0);
    if(num != rdsize)		/* couldn't get that many bytes */
	return(SFerror = SFE_RDERR); /* .. call it a read error */

    /* fixup byte order, not for the info chars */
    ConvertBufferBytes((char *)psfh, num, 4, bytemode);

    if(psfh->sfinfo.sf_magic != IRC_MAGIC) /* compare with IRCAM magic number */
	return(SFerror = SFE_NSF); /* got the bytes, but look wrong */
    /* Else must be ok.  Extract rest of sfheader info */
    srate = psfh->sfinfo.sf_srate;
    chans = psfh->sfinfo.sf_chans;
    formt = psfh->sfinfo.sf_packmode;
    switch(formt)
	{
    case IRC_CHAR:   formt = SFMT_CHAR;	break;
    case IRC_ALAW:   formt = SFMT_ALAW;	break;
    case IRC_ULAW:   formt = SFMT_ULAW;	break;	
    case IRC_SHORT:  formt = SFMT_SHORT; break;
    case IRC_LONG:   formt = SFMT_LONG;	break;
    case IRC_FLOAT:  formt = SFMT_FLOAT; break;
    default:			/* leave formt unchanged ! */
	fprintf(stderr,"sndfirc: unrecog. format %d\n",formt);
	break;
	}
    /* copy across info as read */
    irc_infp = &(psfh->sfinfo.sf_codes);
    sdf_infp = &(snd->info[0]);
/*    infoBsize = sizeof(SFHEADER) - (irc_infp - (char *)psfh);	*/
    /* by def, SFIRCAM has a lot of potential info area to be hauled about */
    if(infoBmax >= infoBsize)
	i = infoBsize;
    else
	i = infoBmax;	      /* can't copy the whole header, just this bit */
    while(i>0)
	{
	--i;
	*sdf_infp++ = *irc_infp++; /* copy between sep buffers */
	}
    /* rewrite into different data structure */
    snd->magic = SFMAGIC;	/* OUR magic number, not the IRCAM IRC_MAGIC */
    snd->headBsize = infoBsize + sizeof(SFSTRUCT) - SFDFLTBYTS;
    if( (snd->dataBsize = GetFileSize(file)) == -1)
	snd->dataBsize = SF_UNK_LEN;
    SetSeekEnds(file, sizeof(SFHEADER), snd->dataBsize);
    if(snd->dataBsize != SF_UNK_LEN) {  /* we could find length */
	snd->dataBsize -= sizeof(SFHEADER); /* .. knock off hdr */
    }
    snd->format = formt;
    snd->channels = chans;
    snd->samplingRate = srate;

    return(SFerror = SFE_OK);
}

int SFRdXInfo(
    FILE *file,
    SFSTRUCT *psnd,
    int done)
{ /* fetch any extra info bytes needed in view of read header
     Passes open file handle, existing sound struct
     already read 'done' bytes; rest inferred from structure */
    size_t  num;
    long    subhlen;
    long    file_pos;
    int     rem;

    rem = SFInfoSize(psnd) - done;
    if(rem <= 0)
	return(SFerror = SFE_OK);	/* nuthin more to read */
    /* do not assume seek already at right place.. */
    subhlen = ((&(psfh->sfinfo.sf_codes)) - (char *)psfh) + (long)done;
    file_pos = ftell(file);
    if( file_pos >= 0	/* lose on pipe */
       && (subhlen - file_pos) != 0) 	/* if not at right place.. */
	if(fseek(file, subhlen - file_pos, SEEK_CUR) != 0 )	/* relative */
	    return(SFerror = SFE_RDERR);
    num = fread((void *)(psnd->info+done), (size_t)1, (size_t)rem, file);
    if(num < (size_t)rem)
	return(SFerror = SFE_RDERR);
    else
	return(SFerror = SFE_OK);
}

int SFWriteHdr(
    FILE *file,
    SFSTRUCT *snd,
    int infoBmax)
{ /* Utility to write out header to open file
     infoBmax optionally limits the number of info chars to write.
     infoBmax = 0 => write out all bytes implied by .headBsize field
     infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */
    size_t  num;
    long    hedSiz;
    char    *irc_infp, *sdf_infp;
    int     i;
    long    infoBsize,ft;
    int bytemode = GetBytemode(file);

    if(snd == NULL) {	/* rewrite but can't seek */
	return SFE_OK;
    }

    if(bytemode < 0) {
	bytemode = HostByteMode(); /* standard is to write big-endian */
	SetBytemode(file, bytemode);
    }

    if(psfh == NULL)		    /* check our personal IRCAM sfheader buf */
	if( (psfh = (SFHEADER *)malloc((size_t)sizeof(SFHEADER)))==NULL)
	    return(SFerror=SFE_MALLOC); /* couldn't alloc our static buffer */

    if(snd->magic != SFMAGIC)
	return(SFerror = SFE_NSF);
    if(infoBmax < SFDFLTBYTS || infoBmax > SFInfoSize(snd) )
	infoBmax = SFInfoSize(snd);
    ft = ftell(file);
    if( ft >= 0 ) {			/* don't try seek on pipe */
	if(fseek(file, (long)0, SEEK_SET) != 0 )   /* move to start of file */
	    return(SFerror = SFE_WRERR);
    }
    /* copy to private buffer.. */
    psfh->sfinfo.sf_magic = IRC_MAGIC;
    psfh->sfinfo.sf_srate  = snd->samplingRate;
    psfh->sfinfo.sf_chans  = snd->channels;
    switch(snd->format) {
    case SFMT_CHAR:	psfh->sfinfo.sf_packmode = IRC_CHAR;	break;
    case SFMT_ALAW:	psfh->sfinfo.sf_packmode = IRC_ALAW;	break;
    case SFMT_ULAW:	psfh->sfinfo.sf_packmode = IRC_ULAW;	break;
    case SFMT_SHORT:	psfh->sfinfo.sf_packmode = IRC_SHORT;	break;
    case SFMT_LONG:	psfh->sfinfo.sf_packmode = IRC_LONG;	break;
    case SFMT_FLOAT:	psfh->sfinfo.sf_packmode = IRC_FLOAT;	break;
    default:	    /* leave formt unchanged ! */
	fprintf(stderr,"sndfirc: unrecog. format %d\n",snd->format);
	psfh->sfinfo.sf_packmode = snd->format;
	break;
    }
    /* copy across info provided */
    irc_infp = &(psfh->sfinfo.sf_codes);
    sdf_infp = &(snd->info[0]);
    infoBsize = sizeof(SFHEADER) - (irc_infp - (char *)psfh);
    /* by def, SFIRCAM has a lot of potential info area to be hauled about */
    if(infoBmax >= infoBsize)
	i = infoBsize;
    else
	i = infoBmax;	/* can't copy the whole header, just this bit */
/*  infoXtra = infoBsize - i;	/* bytes of info we couldn't yet read */
    while(i>0) {
	--i;
	*irc_infp++ = *sdf_infp++;  /* copy between sep buffers */
    }

    hedSiz = sizeof(SFHEADER);	    /* write a whole header in any case */
    ConvertBufferBytes((char *)psfh, hedSiz, 4, bytemode);
    if( (num = fwrite((void *)psfh, (size_t)1, (size_t)hedSiz,
			 file))< (size_t)hedSiz) {
    	fprintf(stderr, "SFWrH: tried %ld managed %ld\n",
		hedSiz, (long)num);	
	return(SFerror = SFE_WRERR);
    }
    ConvertBufferBytes((char *)psfh, hedSiz, 4, bytemode);
    return(SFerror = SFE_OK);
}

static long SFHeDaLen(FILE *file)
{   /* return data space used for header in file - see SFCloseWrHdr */
    /* always the same for IRCAM soundfiles */
    return(sizeof(SFHEADER));
}

static long SFTrailerLen(FILE *file)
{   /* return data space after sound data in file (for multiple files) */
    return(0);
}

static long SFLastSampTell(FILE *file)
{   /* return last valid ftell for smps */
    return (0L);	/* punt on this for IRC - just use EOF */
}

static long FixupSamples(
    FILE  *file,
    void  *ptr,
    int   format,
    long  nitems,
    int   flag) 	/* READ_FLAG or WRITE_FLAG indicates direction */
{
    int wordsz;
    long bytes;
    int bytemode = GetBytemode(file);

    if(bytemode < 0) {
	bytemode = HostByteMode();
    }

    wordsz = SFSampleSizeOf(format);
    bytes  = nitems * (long)wordsz;
    if(bytemode != BM_INORDER) {	/* already OK? */
	/* otherwise need to fixup buffer .. */
	ConvertBufferBytes(ptr, bytes, wordsz, bytemode);
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
}
#endif /* WASMACHORDER */
