/***************************************************************\
*   sndfpcm.c			HEADERLESS PCM			*
*   SNDF 'header' functions to access headerless PCM files	*
*   i.e. file is always assumed to be 44k1 stereo 16bit linear  *
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   10jul91 after 24sep90 after12aug90 dpwe			*
\***************************************************************/

#include <stdio.h>
#include <string.h>

char *sndfExtn = ".pcm";		/* file extension, if used */

/* header always reads as below.. */
#define DF_PCM_SRT 	SNDF_DFLT_SRATE
#define DF_PCM_CHN 	SNDF_DFLT_CHANS
#define DF_PCM_FMT 	SNDF_DFLT_FORMAT
#define DF_PCM_SKP	0			/* # starting bytes to skip */

#define PCM_ENV "PCMFORMAT"	/* Name of an environment string for format */

#ifdef THINK_C
#include <unix.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <fcntl.h>

/* for getenv */
#include <stdlib.h>
/* for strchr */
#undef strchr
#include <string.h>

/* hold defaults originally from environment or format string */
typedef struct _pcm_defaults {
    int srate;
    int chans;
    int format;
    int headskip;
    int bytemode;
    int miscflags;	/* e.g. SFFLAG_ABBOT */
} PCM_DEFAULTS;
static PCM_DEFAULTS *pcmDefaults = NULL;	/* implies not yet set */

static void pcmReadDefaultString(char *str, PCM_DEFAULTS **ppcmDefaults)
{   /* Modify the pcmDflt* globals according to a string 
       (e.g. read from environment) */
    char *e, *f;
    int srate, chans, i;
    DBGFPRINTF((stderr, "pcmReadDefaultString of '%s'\n", (str!=NULL)?str:"(null)"));

    if ((*ppcmDefaults) == NULL) {
	(*ppcmDefaults) = TMMALLOC(PCM_DEFAULTS, 1, "pcmReadDefaultString");
	/* preset all fields to defaults */
	(*ppcmDefaults)->format = DF_PCM_FMT;
	(*ppcmDefaults)->chans  = DF_PCM_CHN;
	(*ppcmDefaults)->srate  = DF_PCM_SRT;
	(*ppcmDefaults)->headskip = DF_PCM_SKP;
	(*ppcmDefaults)->bytemode = HostByteMode();
	(*ppcmDefaults)->miscflags = 0;
        /* but if overwriting an existing format, just let it mount up */
    }
    /* Most options stay 'as they were' if not specified.  However, 
       "Abbot" mode is always off by default, unless explicitly present 
       in the string (since there's no way to say NOT abbot) */
    (*ppcmDefaults)->miscflags &= ~SFFLAG_ABBOT;

    /* now grab them from the string */
    /* Format is "setenv PCMFORMAT R8000C1X8Fs"
       for short, 1 channel data at 8kHz, skipping first 8 bytes.
       "F" should come last. */
    if (str != NULL) {
	/* Roach-parse the string */
	e = str;
	while (e[0] != '\0') {
	    switch (e[0]) {
	    case 'A':
		if(strstr(str, "Abb")) {
		    (*ppcmDefaults)->miscflags |= SFFLAG_ABBOT;
		    e += 3;
		} else {
		    fprintf(stderr, "pcmReadDflt: format string '%s': 'A' can only prefix 'Abb' (ignored)\n", str);
		    ++e;
		}
		break;
	    case 'R':
		srate = strtol(++e, &f, 0);
		/* error if nothing found */
		if (f == e) {
		    fprintf(stderr, "pcmReadDflt: format string '%s': 'R' not followed by number (ignored)\n", str);
		} else {
		    /* move over number */
		    e = f;
		    /* map very low rates to more reasonable ones */
		    if(srate == 8)		srate = 8000;
		    else if(srate == 11)	srate = 11025;
		    else if(srate == 16)	srate = 16000;
		    else if(srate == 22)	srate = 22050;
		    else if(srate == 24)	srate = 24000;
		    else if(srate == 32)	srate = 32000;
		    else if(srate == 44)	srate = 44100;
		    else if(srate == 48)	srate = 48000;
		    /* default case */
		    else if(srate < 100)	srate = 1000*srate;
		    (*ppcmDefaults)->srate = srate;
		}
		break;
	    case 'C':
		chans = strtol(++e, &f, 0);
		/* error if nothing found */
		if (f == e) {
		    fprintf(stderr, "pcmReadDflt: format string '%s': 'C' not followed by number (ignored)\n", str);
		} else {
		    /* move over chr */
		    e = f;
		    /* chans of zero not allowed */
		    if (chans == 0) {
			fprintf(stderr, "pcmReadDflt: format string '%s': chans of zero not allowed (ignored)\n", str);
		    } else {
			(*ppcmDefaults)->chans = chans;
		    }
		}
		break;
	    case 'X':
		i = strtol(++e, &f, 0);
		/* error if nothing found */
		if (f == e) {
		    fprintf(stderr, "pcmReadDflt: format string '%s': 'X' not followed by number (ignored)\n", str);
		} else {
		    /* move over num */
		    e = f;
		    (*ppcmDefaults)->headskip = i;
		}
		break;
	    case 'E':
		++e;
		if(*e == 'b' || *e == 'B') {
		    /* big-endian */
		    if(HostByteMode() == BM_INORDER) {
			(*ppcmDefaults)->bytemode = BM_INORDER;
		    } else {
			(*ppcmDefaults)->bytemode = BM_BYWDREV;
		    }
		    ++e;
		} else if (*e == 'l' || *e == 'L') {
		    /* little-endian */
		    if(HostByteMode() != BM_INORDER) {
			(*ppcmDefaults)->bytemode = BM_INORDER;
		    } else {
			(*ppcmDefaults)->bytemode = BM_BYWDREV;
		    }
		    ++e;
		} else if (*e == 'n' || *e == 'N') {
		    /* natural-endian (yuk) */
		    (*ppcmDefaults)->bytemode = BM_INORDER;
		    ++e;
		} else if (*e == 's' || *e == 'S') {
		    /* swapped-endian (yuk) */
		    (*ppcmDefaults)->bytemode = BM_BYWDREV;
		    ++e;
		} else {
		    fprintf(stderr, "pcmReadDflt: format string '%s': Endian '%c' is not 'b' or 'l' or 'n' or 's' (ignored)\n", str, *e);
		}
		break;
	    case 'F':
		++e;
		if(*e=='s' || *e=='S' || strncmp(e, "16", 2)==0) {
		    (*ppcmDefaults)->format = SFMT_SHORT;
		} else if (*e=='c' || *e=='C' || *e=='8') {
		    (*ppcmDefaults)->format = SFMT_CHAR;
		} else if (*e=='f' || *e=='F') {
		    (*ppcmDefaults)->format = SFMT_FLOAT;
		} else if (*e=='l' || *e=='L' || strncmp(e, "32", 2)==0) {
		    (*ppcmDefaults)->format = SFMT_LONG;
		} else if (*e=='m' || *e=='M' || strncmp(e, "24", 2)==0) {
		    (*ppcmDefaults)->format = SFMT_24BL;
		} else if (*e=='u' || *e=='U') {
		    (*ppcmDefaults)->format = SFMT_ULAW;
		} else if (*e=='a' || *e=='A') {
		    (*ppcmDefaults)->format = SFMT_ALAW;
		} else if (*e=='o' || *e=='O') {
		    (*ppcmDefaults)->format = SFMT_CHAR_OFFS;
		} else if (*e=='d' || *e=='D') {
		    (*ppcmDefaults)->format = SFMT_DOUBLE;
		} else {
		    fprintf(stderr, "pcmReadDflt: format string '%s': Format '%c' is not s/c/f/l/u/a/o/d\n", str, *e);
		    --e; /* to defeat increment below */
		}
		/* special-case the 2chr formats */
		if (strncmp(e, "16", 2)==0 || strncmp(e, "32", 2)==0 || strncmp(e, "24", 2)==0) {
		    e += 2;
		} else {
		    ++e;
		}
		break;
	    default:
		fprintf(stderr, "pcmReadDflt: format string '%s': unrecognized character '%c' (ignored)\n", str, *e);
		++e;
	    }
	}
    }
}

int SFReadHdr(file, snd, infoBmax)
    FILE *file;
    SFSTRUCT *snd;
    int infoBmax;
/* utility to sample the header of an opened file, at most infoBmax info bytes */
    {
    long    infoBsize, infoXtra, bufXtra;
    long    rdsize;
    size_t  num,red;
    long    ft;
    char    *irc_infp, *sdf_infp;
    int     chans,formt,skip,i;
    float   srate;
    int     flags;
    INT32   dummy;

    if(infoBmax < SFDFLTBYTS)	infoBmax = SFDFLTBYTS;
    infoBmax = (infoBmax)&-4;	/* round DOWN to x4 (can't exceed given bfr) */
    bufXtra = (long)(infoBmax-SFDFLTBYTS);

    ft = ftell(file);
    if( ft > 0 ) {			/* don't try seek on stdin */
	if(fseek(file,(long)0,SEEK_SET) != 0 ) /* move to start of file */
	    return(SFerror = SFE_RDERR);
    }
    /* if we're at EOF, it's not a sound file */
    if (feof(file)) {
	return (SFerror = SFE_EOF);
    }
#ifdef  HAVE_FPUSH
    /* since we have a way to 'taste' a file without consuming it...
       Specify that a sound file must be at least 4 bytes long.
       If we fail to read that many, call it an SFE_EOF (early EOF) */
    if (fread(&dummy,1,4,file) < 4) {
	return (SFerror = SFE_EOF);
    }
    /* otherwise, push them back to be read properly */
    fpush(&dummy,1,4,file);
#endif /* HAVE_FPUSH */
    /* Check for environment var to override  */
    if(pcmDefaults == NULL) {
	pcmReadDefaultString(getenv(PCM_ENV), &pcmDefaults);
    }
    /* Fabricate sfheader info from defaults */
    srate = pcmDefaults->srate;
    chans = pcmDefaults->chans;
    formt = pcmDefaults->format;
    skip  = pcmDefaults->headskip;
    SetBytemode(file, pcmDefaults->bytemode);
    if(flags = pcmDefaults->miscflags)	SetFlags(file, pcmDefaults->miscflags);
    snd->magic = SFMAGIC;
    snd->headBsize = sizeof(SFSTRUCT);
    snd->dataBsize = GetFileSize(file);
    /* If this is an Abbot file, assume a 2 byte "trailer" */
    if(flags & SFFLAG_ABBOT && snd->dataBsize != SF_UNK_LEN) {
	/* snd->dataBsize -= sizeof(short); */	/* for final 0x8000 */
	/* We never *actually* know how long an abbot file is until 
	   we've read it - so just say UNK_LEN always */
	snd->dataBsize = SF_UNK_LEN;
    }
    SetSeekEnds(file, 0, snd->dataBsize);
    snd->format = formt;
    snd->channels = chans;
    snd->samplingRate = srate;
    if (skip > 0) {
	/* consume the padding bytes */
	seek_by_read(file, skip);
	/* fixup the length report */
	if (snd->dataBsize != SF_UNK_LEN) {
	    snd->dataBsize -= skip;
	}
    }
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
    size_t  num;
    long    subhlen;
    long    file_pos;
    int     rem;

    /* will not occur for PCM files */
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
    long    infoBsize,ft;
    short   yuk[2];

    if(infoBmax < 0) {
	/* flag: this is a rewrite of the header on close */
	if(GetFlags(file) & SFFLAG_ABBOT) {
	    short trailer = -32768;	/* 0x8000 to write out */
	    ConvertBufferBytes((void *)&trailer, sizeof(short), sizeof(short),
			       GetBytemode(file));
	    /* attempt to move to end */
	    fseek(file, 0, SEEK_END);
	    /* write out the Abbot trailer */
	    fwrite(&trailer, sizeof(short), 1, file);
	}
    }

    if(snd == NULL) {
	/* last-ditch look-in before close */
	return SFE_OK;
    }

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
    /* We read in the PCMENV environment var just in order to get the 
       right bytemode */
    /* Check for environment var to override  */
    if(pcmDefaults == NULL) {
	pcmReadDefaultString(getenv(PCM_ENV), &pcmDefaults);
    }
    SetBytemode(file, pcmDefaults->bytemode);
    if(pcmDefaults->miscflags)	SetFlags(file, pcmDefaults->miscflags);

    /* write nothing .. */
#ifdef OLDHACK
    /* actually, if I write nothing, I get a bus error when I try to 
       write 32768*2 shorts as the first thing in the file.  I can 
       fix it with this one sample pad (YUKYUKYUK) */
    yuk[0] = yuk[1] = 0;
    fwrite(yuk, sizeof(short), 2, file);
    /* perhaps I should skip over this on reads etc .. */
#endif /* OLDHACK */
    return(SFerror = SFE_OK);
    }

static long SFHeDaLen(file)
    FILE *file;
    /* return data space used for header in file - see SFCloseWrHdr */
{
    long hlen = 0;

    /* no header */
#if OLDHACK
    /* actually - just two bizarre pad samples */
    hlen = 4;
#endif /* OLDHACK */
    return(hlen);
}

static long SFTrailerLen(FILE *file)
{   /* how many bytes between last sample & EOF (for seeking in stream) */
    if (GetFlags(file) & SFFLAG_ABBOT) { 
	return 2;
    } else {
	return 0;
    }
}

static long SFLastSampTell(file)	/* return last valid ftell for smps */
    FILE	*file;
{
    long start, end;

    GetSeekEnds(file, &start, &end);
    return (end);
}

static void DoAbbotClip(short *data, int count)
{   /* quick hack - go through clipping 0x8000s to 0x8001's
       to permit this short data to be used in abbot pipes */
    while(count--) {
	if (*data == -32768 /* 0x8000 */) {
	    *data = -32767  /* 0x8001 */;
/*	    fprintf(stderr, "\nABBOTCLIP 0x%lx 0x%lx 0x%lx\n", 0xFFFF & data[-1], 0xFFFF & data[0], 0xFFFF & data[1]);
	    abort(); */
	}
	++data;
    }
}

static long GetAbbotLen(short *data, int count)
{   /* Scan through read data looking for 0x8000 - 
       when we find it, mark that as end of block. */
    long okcount = 0;
    long x = count;
    while(x--) {
	if (*data == -32768 /* 0x8000 */) {
	    break;
	}
	++data;
	++okcount;
    }
    return okcount;
}

/* copied from sndf.c */
#define READ_FLAG	1
#define WRITE_FLAG	0	/* flags to FixupSamples (byte ordering) */
#define UNREAD_FLAG	-1	/* means just reverse the processing 
				   on read, prior to pushing bytes back
				   onto input */

static long FixupSamples(file, ptr, format, nitems, flag)
    FILE  *file;
    void  *ptr;
    int   format;
    long  nitems;
    int   flag;		/* READ_FLAG or WRITE_FLAG indicates direction;
			   UNREAD_FLAG means return READ samples to 
			   natural state. */
{
    int bytemode;
    int wordsz;
    long bytes;

    /* maybe fixup 0x8000s */
    if(flag == WRITE_FLAG \
       && format == SFMT_SHORT \
       && (GetFlags(file) & SFFLAG_ABBOT)) {
	DoAbbotClip((short *)ptr, nitems);
    }
    wordsz = SFSampleSizeOf(format);
    bytes  = nitems * (long)wordsz;
    bytemode = GetBytemode(file);
    if(bytemode != BM_INORDER) {	/* already OK */
	/* otherwise need to fixup buffer .. */
	ConvertBufferBytes(ptr, bytes, wordsz, bytemode);
    }
    /* maybe scan for 0x8000 */
    if(flag == READ_FLAG \
       && format == SFMT_SHORT \
       && (GetFlags(file) & SFFLAG_ABBOT)) {
	nitems = GetAbbotLen((short *)ptr, nitems);
    }
    return nitems;
}	/* NOT always machine order */
