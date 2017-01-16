/***************************************************************\
*   sndfaif.c			AIFF (mac or unix)		*
*   Extra functions for sndf.c					*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   based on billg's AIFF.c for csound				*
*   25mar91 dpwe						*
\***************************************************************/

/*
 * Changes 
 *
 * 1994jun06 dpwe  Fixed first fread in SFReadHdr which was making
 *                 nowpos=1 rather than =4 - messing up ssndpos
 */

#include <byteswap.h>

/* exported file type indicator */
char *sndfExtn = ".aif";		/* file extension, if used */

/* #define DEBUG 1  /* */

/* billg's cool types etc */
#ifdef THINK_C
long sndfType = (long)'AIFF';
long sndfCrea = (long)'SCPL';	/* ? */
/* #include <mactypes.h>	*/
/* #include <pascal.h>	/* for PtoCstr */
#include "macsndf.h"
#endif
#include <dpweaiff.h>
#include <IEEE80.h>
/* #include "sndffname.c"	/* define AddFname, FindFname etc */

/* subsidiary extern func protos */

/* private type declarations */

/* static variables */

/* static function prototypes ---------------------------------------------- */

/* CANT_USE_AIFFID_PTR is *never* defined, on the way to removing this 
   distinction altogether - not sure why it was ever useful... 1998mar14 */
#ifdef CANT_USE_AIFFID_PTR
/* k&r does not like taking address of 4chr consts */
typedef AiffID AiffIDPtr;
/* don't take addresses before passing */
#define ADR 
#else /* !CANT_USE_AIFFID_PTR */
typedef AiffID *AiffIDPtr;
#define ADR &
#endif /* CANT_USE_AIFFID_PTR */

static int MatchIDs(AiffIDPtr a, AiffIDPtr b);
static void SetID(AiffIDPtr d, AiffIDPtr s);
static void PutLong(LONG *dst, long val);
static long GetLong(LONG *src);
static void aiffWrite(FILE *file, char *buf, int  n);
static void aiffSeek(FILE *file, long pos);
static long ReadPstring(FILE *file, char *str);
static long SkipPstring(FILE *file);

/* ----------------------- implementations ------------------------ */

static AiffID aiffType = AIFF_TYPE;
static AiffID aifcType = AIFC_TYPE;
static AiffID formID = FORM_ID;
static AiffID commID = COMM_ID;
static AiffID ssndID = SSND_ID;
static AiffID markID = MARK_ID;
static AiffID instID = INST_ID;
static AiffID applID = APPL_ID;
static AiffID fverID = FVER_ID;
static AiffID noneID = NONE_ID;

static int MatchIDs(a, b)
    AiffIDPtr a;
    AiffIDPtr b;
    {
#ifdef OLD_THINK_C		/* then ID is long */
    return( (*a)==(*b) );
#else /* !THINK_C */	/* ID is uchar[4] */
    int mc;

/*    mc = memcmp( *a, *b, 4 );  */ /* 1994mar16 */
    mc = memcmp( a, b, 4 );
/*    fprintf(stderr, "a=%c%c%c%c b=%c%c%c%c c=%d\n",
	    (*a)[0], (*a)[1], (*a)[2], (*a)[3], 
	    (*b)[0], (*b)[1], (*b)[2], (*b)[3], mc);	*/
    return(mc==0);
#endif /* !THINK_C */
    }

static void SetID(d, s)
    AiffIDPtr d;
    AiffIDPtr s;
    {
#ifdef OLD_THINK_C		/* then ID is long */
    (*d) = (*s);
#else /* !THINK_C */	/* ID is uchar[4] */
/*    memcpy(*d, *s, 4 );  */  /* 1994mar16 */
    memcpy(d, s, 4 );
#endif /* !THINK_C */
    }

static void PutLong(dst, val)	/* copy a long onto 2 shorts, preserve binary image */
    LONG *dst;
    long val;
    {
    short *ps1,*ps2;

    ps1 = (short *)dst;
    ps2 = (short *)&val;
    *ps1++ = *ps2++;
    *ps1++ = *ps2++;	/* two shorts in a long avoids 32bit boundary q */
    }

static long GetLong(src)	/* read 2 shorts as a long, preserve binary image */
    LONG	*src;
    {
    short *ps1, *ps2;
    long  val;

    ps1 = (short *)src;
    ps2 = (short *)&val;
    *ps2++ = *ps1++;
    *ps2++ = *ps1++;
    return val;
    }

/* bill's AIFF subsidiaries */
static int samp_size;

/*
 * Write with error checking.
 */
static void aiffWrite(file,buf,n)
    FILE *file;		/* file descriptor number */
    char *buf;		/* ptr to buffer */
    int  n;		/* number bytes */
    {
    if (fwrite(buf,sizeof(char),n,file) != n)
	SFDie(SFE_UNK_ERR,"AIFF: couldn't write the outfile header");
    }

/*
 * Seek from beginning with error checking.
 */
static void aiffSeek(file,pos)
    FILE *file;		/* file descriptor number */
    long pos;		/* from start of file */
    {
    if (fseek(file,pos,SEEK_SET) != 0)
	SFDie(SFE_UNK_ERR, "AIFF: seek error while updating header");
    }

static long ReadPstring(FILE *file, char *str)
{	/* file is lined up at pstring.  Read in and convert, return
	   bytes read */
    long	strLen, redLen, red;
    char	c;

    red = 1;
    fread(&c, sizeof(char), red, file);
    strLen = (int)c;
    redLen = strLen+(((strLen&1)==0)?1:0);	/* make total read even */
    fread(str, sizeof(char), redLen, file);
    str[strLen] = '\0';				/* terminate */
    return redLen+red;				/* TOTAL read */
}

static long SkipPstring(FILE *file)
{
    char txt[256];

    return ReadPstring(file, txt);
}

#define TBSIZE 256
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* !MIN */

int SFReadHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
/* utility to sample the header of an opened file, at most infobmax info byts */
    {
    int     chans,dsize,infoSize;
    long    len;
    float   srate;
    char    *info;
    char    *fname;
    char    dmyinfo[SFDFLTBYTS];
    int     err,i;

    FormHdr form;
    CkHdr ckHdr;
    AIFCCommonChunk comm;
    FormatVersionChunk fver;
    SoundDataHdr ssnd;
    int comm_read = 0;
    int ssnd_read = 0;
    long ssnd_pos;
    long got, pos, nowpos = 0;
    long tmpl;
    long chunkLen;
    double sr;
    int bytemode = HostByteMode();
    int	isaifc = FALSE;
    char 	cmpName[256];
    char 	errmsg[256];
    int 	wasComm;	/* sgi  aifc hack dpwe 1993jan18 */
    int 	at_ssnd;	/* hack for piped aifcs - dpwe 1993apr25 */
    long	dbgvar;
    int         seekable = TRUE ; /* FALSE; /* TRUE; */
/* 1994may12: playing sounds piped across the net from a DECstation to 
   an SGI; the pipe was *allowing* seeks, but read as having 56 bytes 
   in it *prior* to the start of the soundfile.  Thus seeking to position 
   zero read some garbage in front of the file, no good at all.
     By making seekable FALSE, we will be unable to read certian AIFFs 
   written by other programs, but we will get no errors reading those 
   written by our code, even if they come across the net. 
   1994dec15:  could not reproduce this bug as described above, but 
   did fail to read piped sounds - because any *seeks* were not actually 
   consuming bytes.  Modified the skip-over-chunk code to use seek_by_read 
   for all smallish positive relative seeks, now can pipe & play OK.
     Had to do this because treating output files as unseekable caused 
   SFCloseRewriteHeader to try and re-read the header magic number at 
   the *end* of the file (i.e. without seeking to zero) which was adding 
   four junk bytes to the end of all my output files, bad news. */
    /* should we try to reread the magic number? */
    long	skipmagic = 0;

    DBGFPRINTF((stderr, "aiffReadHdr: file %lx...",file));
    if(infoBmax < SFDFLTBYTS)	infoBmax = SFDFLTBYTS;
    infoBmax = infoBmax&-4;	/* round down to multiple of four */
    infoSize = infoBmax;
    /* will only read into info for infoSize byts, updates infoSize to avai bts */
/*  len = file->len;		/* cheat by using THINK_C's internal record... */
    /* until we add in INFO chunk, just assume info is nothing special (yuk) */
    infoSize = SFDFLTBYTS;
    snd->magic = SFMAGIC;
    snd->headBsize = sizeof(SFSTRUCT) + infoSize - SFDFLTBYTS;
/*  snd->dataBsize = len;   snd->channels = chans;  snd->samplingRate = srate;	*/
    snd->dataBsize = 1;	/* preset, scale up later */

    /**** after billg's aiffReadHeader ****/
    /* read the first long and check it */
    if(seekable)	pos = ftell(file);
    else		pos = -1;
    if( pos < 0 ) {
	seekable = FALSE;
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
    } else if (pos == sizeof(INT32)) {
	/* assume we already read the magic & fake it up */
	skipmagic = pos;
    } else {
	if( fseek(file, 0, SEEK_SET) != 0)
	    seekable = FALSE;	/* error seeking - don't try again */
    }
    dbgvar = ftell(file);
    if(skipmagic) {
	nowpos += skipmagic;
    } else {
        /* (make sure we don't happen to match an old FORM magic) */
        memset(&form, 0, sizeof(form));
	nowpos += fread(&form, 1, sizeof(INT32), file);
	if (!MatchIDs(ADR form.ckHdr.ckID, ADR formID)) {
	    DBGFPRINTF((stderr, "aiffReadHdr: not AIF file (0x%lx)\n", ADR form.ckHdr.ckID));
	    return SFE_NSF; /* Not a Sound File */
	}
	DBGFPRINTF((stderr, "fread: pos=%ld->%ld\n", dbgvar, ftell(file)));
    }
    /* read the rest of FormHdr */
    nowpos += fread((char *)&form + sizeof(INT32),sizeof(char),
		    sizeof(FormHdr) - sizeof(INT32), file);
    DBGFPRINTF((stderr, "form = %c%c%c%c..",((char *)ADR form.formType)[0],
	   ((char *)ADR form.formType)[1], ((char *)ADR form.formType)[2],
	   ((char *)ADR form.formType)[3]));
    if(MatchIDs(ADR form.formType, ADR aifcType))
	isaifc = TRUE;
    else if (!MatchIDs(ADR form.formType, ADR aiffType))
	SFDie(SFE_UNK_ERR, "AIFF: bad form type in header");
/*	hdr->readlong = FALSE;	    hdr->firstlong = 0; */
    /* record the total file size as held in the form */
    SetFileLen(file, 
	       lshuffle(GetLong(& form.ckHdr.ckSize), bytemode)
	         + sizeof(CkHdr));

    while (1) {	    /* loop for rest of chunks */
	wasComm = FALSE;
	at_ssnd = FALSE;
	dbgvar = nowpos;
	got = fread(&ckHdr,sizeof(char),sizeof(CkHdr),file);
	if(got < sizeof(CkHdr))  {
	    fprintf(stderr, "sndfaif: file looked OK then went bad (EOF) (got=%ld nowpos=%ld pos=%ld)\n", got, nowpos, ftell(file));
	    return SFE_NSF;
	}
	nowpos += got;
	DBGFPRINTF((stderr, "fread: got %d of %d\n", nowpos-dbgvar, sizeof(CkHdr)));
	if(seekable)
	    nowpos = pos = ftell(file);
	else
	    pos = nowpos;
	DBGFPRINTF((stderr, "@%ld: chunk = %c%c%c%c (%ld)..", nowpos, 
		((char *)ADR ckHdr.ckID)[0],
	       ((char *)ADR ckHdr.ckID)[1], ((char *)ADR ckHdr.ckID)[2],
	       ((char *)ADR ckHdr.ckID)[3], lshuffle(GetLong(ADR ckHdr.ckSize), bytemode)));
	DBGFPRINTF((stderr, "aiffpos = %ld\n", pos));
	if (MatchIDs(ADR ckHdr.ckID, ADR commID)) {
	    /* read remainder of CommonChunk */
	    DBGFPRINTF((stderr, "starting COMM: pos=%d, nowpos=%d\n", 
			pos, nowpos));
	    wasComm = TRUE;  /* no Seek after comm : assume get to end by fread
				   (work arounf SGI4.0.5F bug (?)) */
	    if(isaifc)  {
		nowpos += fread((char *)&comm + sizeof(CkHdr), sizeof(char),
				sizeof(AIFCCommonChunk) - sizeof(CkHdr), file);
		nowpos += ReadPstring(file, cmpName);
		if(!MatchIDs(ADR comm.compressionType, ADR noneID))  {
		    /*    sprintf(errmsg, "AIFC: cannot do compression '%s'\n", 
			    cmpName);
		    SFDie(SFE_UNK_ERR, errmsg); */
		    fprintf(stderr, "snfaif: cannot handle compression '%s'\n", cmpName);
		    /* .. but continue anyway */
		}
	    }
	    else {
	    	int thenpos = nowpos;
		nowpos += fread((char *)&comm + sizeof(CkHdr), sizeof(char),
				sizeof(CommonChunk) - sizeof(CkHdr), file);
		DBGFPRINTF((stderr, "Read AIFF COMM of %d bytes (sizeof=%d)\n", nowpos-thenpos,
			    sizeof(CommonChunk) - sizeof(CkHdr)));
	    }
	    /* parse CommonChunk to hdr format */
	    if ( wshuffle(comm.sampleSize, bytemode) <= 8)
		snd->format = SFMT_CHAR;
	    else if ( wshuffle(comm.sampleSize, bytemode) <= 16)
		snd->format = SFMT_SHORT;
	    else
		snd->format = SFMT_LONG;
	    snd->channels = wshuffle(comm.numChannels, bytemode);
	    sr = ieee_80_to_double((unsigned char *)ADR comm.sampleRate);
	    snd->samplingRate = sr;
	    /* predict data size based on declared frame count */
	    snd->dataBsize = lshuffle(GetLong(&comm.numSampleFrames), bytemode) * snd->channels * SFSampleSizeOf(snd->format);
/* #ifdef _MC68881_
/* 	    x80tox96(&comm.sampleRate,&sr);
/* 	    snd->samplingRate = sr;
/* #else
/* 	    snd->samplingRate = comm.sampleRate;
/* #endif
 */
	    /* set CommonChunk read flag */
	    comm_read = TRUE;
	    DBGFPRINTF((stderr, "ending COMM: pos=%d, nowpos=%d\n", 
			pos, nowpos));
	} else if (MatchIDs(ADR ckHdr.ckID, ADR fverID)) {
	    nowpos += fread((char *)&fver + sizeof(CkHdr), sizeof(char), 
			    sizeof(FormatVersionChunk) - sizeof(CkHdr), file);
	    if(lshuffle(GetLong(&fver.timestamp), bytemode)!= AIFCVersion1)
		SFDie(SFE_UNK_ERR, "AIFC: not version 1");
	} else if (MatchIDs(ADR ckHdr.ckID, ADR ssndID)) {
	    /* read remainder of SoundDataHdr */
	    nowpos += fread((char *)&ssnd + sizeof(CkHdr), sizeof(char),
			    sizeof(SoundDataHdr) - sizeof(CkHdr), file);
	    ssnd_pos = pos - sizeof(CkHdr) + sizeof(SoundDataHdr) 
		           + lshuffle(GetLong((LONG *)&ssnd.offset), bytemode);
	/*  hdr->hdrsize = ssnd_pos;	but would be useful later */
	    /*	    snd->dataBsize = lshuffle(GetLong(&ckHdr.ckSize), bytemode) 
		             - (sizeof(SoundDataHdr) - sizeof(CkHdr))
			     - lshuffle(GetLong(&ssnd.offset), bytemode); */
	    if(snd->dataBsize == 0 
	       && lshuffle(GetLong(&form.ckHdr.ckSize),bytemode) == SF_UNK_LEN)
		snd->dataBsize = SF_UNK_LEN;
	    /* SF_UNK_LEN is coded as everything consistent at zero length
	       *except* the form size, which is set to SF_UNK_LEN (-1) */
	    ssnd_read = TRUE;
	    at_ssnd = TRUE;
	}
	/* if both CommonChunk and SoundDataHdr read, then we're done */
	if (comm_read && ssnd_read) {
	    if(nowpos != ssnd_pos) {	/* only seek if must */
		if(seekable)  {
		    if (fseek(file,ssnd_pos,SEEK_SET) != 0) {
	 		SFDie(SFE_UNK_ERR, 
			      "AIFF: error seeking to start of sound data");
		    } else {
			nowpos = ssnd_pos;
		    }
		} else {
		    nowpos += seek_by_read(file, ssnd_pos - nowpos);
		}
	    } /* else { */
	    DBGFPRINTF((stderr, "sampsize %d nchls %d sr %f\n",
			SFSampleSizeOf(snd->format),
		        snd->channels,snd->samplingRate));
	    SetSeekEnds(file, ssnd_pos, ssnd_pos+snd->dataBsize);
	    return SFE_OK;
	/*     }  */
	}
	/* seek past this chunk to next one */
	chunkLen = lshuffle(GetLong((LONG *)(short *)&ckHdr.ckSize), bytemode);
        if(chunkLen < 0) {  /* bad AIFF */
	    fprintf(stderr, "sndfaif: file looked OK then went bad\n");
	    return SFE_NSF;
	}
	DBGFPRINTF((stderr, "seeking over %ld (done %ld)\n", chunkLen, nowpos-pos));
	if(chunkLen&1)	++chunkLen;	/* must be even */
	if (wasComm == 0)  {	/* just DON'T seek after comm */
	    if(nowpos != pos+chunkLen)	{  /* NEED to seek */
	      long toseek = pos+chunkLen-nowpos;
	      /* on pipes, SEEKABLE looks OK, but it really isn't, 
		 so try and use seek_by_read in most cases */
	      if(!seekable || (toseek>0 && toseek<TBSIZE))  {
		nowpos += seek_by_read(file, pos+chunkLen - nowpos);
	      } else {
		if(fseek(file, pos + chunkLen, SEEK_SET) != 0)
		  SFDie(SFE_UNK_ERR, 
			"AIFF: error while seeking past AIFF chunk");
		else
		  nowpos = pos+chunkLen;
	      }
	    }
	}
    }
}

int SFRdXInfo(FILE *file, SFSTRUCT *psnd, int done)
/* fetch any extra info bytes needed in view of read header
    Passes open file handle, existing sound struct
    already read 'done' bytes; rest inferred from structure */
    {
    int     chans,dsize,infoSize;
    long    len;
    float   srate;
    char    *info;
    char    *fname;
    int     err,i;
    int     rem;

    /* info not supported on AIFF just yet */
    return(SFerror = SFE_OK);
    }

int SFWriteHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
/*  Utility to write out header to open file
    infoBmax optionally limits the number of info chars to write.
    infoBmax = 0 => write out all bytes implied by .headBsize field
    infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */
    {
    int     i, err = 0;
    int     infoSize;
    int     sampSize;	/* sample size in bytes */
    double  sr;     /* sampling rate, (96 bits if 68881) */
    AIFFHdr aiff;
    long    dBsize, numSamps, ssndSize, formSize, tmpl, pos;
    char    *fname;
    int     bytemode = HostByteMode();

    if(snd == NULL) {	/* rewrite but can't seek */
	return SFE_OK;
    }

    if(snd->magic != SFMAGIC)
	return(SFerror = SFE_NSF);
    if(infoBmax >= SFDFLTBYTS && infoBmax < SFInfoSize(snd) )
	infoSize = infoBmax;
    else
	infoSize = SFInfoSize(snd);

    /*** from billg's aiffWriteHdr() ***/

    sampSize = SFSampleSizeOf(snd->format);
    dBsize = snd->dataBsize;
    DBGFPRINTF((stderr, "aiffWriteHdr: file %lx sampsize %d nchls %d sr %f\n",
	file,sampSize,snd->channels,snd->samplingRate));
    if(dBsize != SF_UNK_LEN)	    /* passed supposedly valid sound length */
	{
	numSamps  = dBsize / (sampSize * snd->channels);
	ssndSize = dBsize + sizeof(SoundDataHdr) - sizeof(CkHdr);
/*	formSize = ssndSize + sizeof(CkHdr) + sizeof(CommonChunk)
		     + sizeof(FormHdr) - sizeof(CkHdr);	*/
	formSize = sizeof(AIFFHdr) - sizeof(CkHdr) + dBsize;
	}
    else{	/* if we have a real SF_UNK_LEN, maybe code all lengths as 0 */
	numSamps = 0;
	ssndSize = sizeof(SoundDataHdr) - sizeof(CkHdr);
	formSize = SF_UNK_LEN;  /* sizeof(AIFFHdr) - sizeof(CkHdr); */
	}
    DBGFPRINTF((stderr, "aiffWriteHdr: samps %ld, ssndSize %ld, formSize %ld\n", 
		numSamps, ssndSize, formSize));
    SetID(ADR aiff.form.ckHdr.ckID, ADR formID);
    PutLong(&aiff.form.ckHdr.ckSize, lshuffle(formSize, bytemode));
    /* ? aiffReWriteHdr */
    SetID(ADR aiff.form.formType, ADR aiffType);
    SetID(ADR aiff.comm.ckHdr.ckID, ADR commID);
    PutLong(&aiff.comm.ckHdr.ckSize, 
	    lshuffle(sizeof(CommonChunk) - sizeof(CkHdr), bytemode));
    aiff.comm.numChannels = wshuffle((short)snd->channels, bytemode);
    PutLong(&aiff.comm.numSampleFrames, lshuffle(numSamps, bytemode));
    /* ? aiffReWriteHdr */
    aiff.comm.sampleSize = wshuffle( (short)(sampSize * 8), bytemode);
    sr = snd->samplingRate;
    double_to_ieee_80(sr, (unsigned char *)ADR aiff.comm.sampleRate);
/* #ifdef _MC68881_
/*     x96tox80(&sr,&aiff.comm.sampleRate);
/* #else
/*     aiff.comm.sampleRate = sr;
/* #endif
 */
/* PLEASE! NO MARK/INST CHUNK (can I avoid it?) dpwe 1993apr25 */
#ifndef SHORT_AIFFS
    SetID(ADR aiff.mark.ckHdr.ckID, ADR markID);
    PutLong(&aiff.mark.ckHdr.ckSize, 
	    lshuffle( sizeof(MarkerChunk) - sizeof(CkHdr), bytemode));
    aiff.mark.numMarkers = wshuffle((short)0, bytemode);
    SetID(ADR aiff.inst.ckHdr.ckID, ADR instID);
    PutLong(&aiff.inst.ckHdr.ckSize, 
	    lshuffle(sizeof(InstrumentChunk) - sizeof(CkHdr), bytemode));
    aiff.inst.baseNote = 60;	/* middle C */
    aiff.inst.detune = 0;   aiff.inst.lowNote = 0;  aiff.inst.highNote = 127;
    aiff.inst.lowVelocity = 0;	aiff.inst.highVelocity = 127;
    aiff.inst.gain = wshuffle(0, bytemode);
    aiff.inst.sustainLoop.playMode = wshuffle(NoLooping, bytemode);
    aiff.inst.sustainLoop.beginMark = wshuffle(0, bytemode);
    aiff.inst.sustainLoop.endMark = wshuffle(0, bytemode);
    aiff.inst.releaseLoop.playMode = wshuffle(NoLooping, bytemode);
    aiff.inst.releaseLoop.beginMark = wshuffle(0, bytemode);
    aiff.inst.releaseLoop.endMark = wshuffle(0, bytemode);
#endif /* SHORT_AIFFS */
    SetID(ADR aiff.ssnd.ckHdr.ckID, ADR ssndID);
    PutLong(&aiff.ssnd.ckHdr.ckSize, lshuffle(ssndSize, bytemode));
    /* ? aiffReWriteHdr */
    PutLong(&aiff.ssnd.offset, lshuffle((long)0, bytemode));
    PutLong(&aiff.ssnd.blockSize, lshuffle((long)0, bytemode));
    /* copy the uninterpreted APPL data .. */
/*    SetID(ADR aiff.appl.ckHdr.ckID, ADR applID);
/*    PutLong(&aiff.appl.ckHdr.ckSize, 
/*          lshuffle((sizeof(ApplChunk)-sizeof(LONG))-sizeof(CkHdr),bytemode));
	    /* actual ApplChunk structure is oversized - see aiff.h (hack) */
/*    for(i=0; i<APPL_LONGS; ++i)
/*	{
/*	PutLong((&aiff.appl.applData[i]), 
/*		lshuffle(appl_chunk_data[i], bytemode));	
/*	}
 */
    fseek(file, 0, SEEK_SET);
    if (ftell(file) > 0) {
	/* we tried to seek, and ftell is working, but the seek didn't - 
	   bail */
	DBGFPRINTF((stderr, "AIFSFWr: at %d after seek to 0 - bailing\n", ftell(file)));
	return SFE_OK;
    }
    aiffWrite(file,(char *)&aiff,sizeof(AIFFHdr));
    pos = sizeof(AIFFHdr);	/* ftell(file); */ /* better for pipes */
    SetSeekEnds(file, pos, pos+snd->dataBsize);

#ifdef THINK_C	/* only relevent on macintosh */
    if( (fname = FindFname(file)) == NULL)
	return(SFerror = SFE_NOPEN);	/* couldn't find a pathname for this */
    DBGFPRINTF((stderr, "MacSet for '%s'...\n", fname));
    CtoPstr(fname);
    MacSetTypCreAs(fname,0/*vref*/, sndfType, sndfCrea); /* make an Sd2 file */
    PtoCstr((unsigned char *)fname);
#endif /* THINK_C */

    if(err)
	return(SFerror = SFE_WRERR);
    else
	return(SFerror = SFE_OK);
    }

static long SFHeDaLen(file) /* find header length for a file */
    FILE *file;
    {
    /* this number is subtracted from the ftell on header rewrite to
       figure the data byte size. */
/*    return(sizeof(AIFFHdr));	    /* billg makes it sooo easy */
    long stt, end;

    GetSeekEnds(file, &stt, &end);
    return stt;
    }

static long SFTrailerLen(FILE *file)
{   /* return data space after sound data in file (for multiple files) */
    long stt, end = -1, len = SF_UNK_LEN;

    GetSeekEnds(file, &stt, &end);
    len = GetFileLen(file);
    if (len != SF_UNK_LEN && end >= 0) {
	return(len - end);
    } else {
	return 0;	/* guess */
    }
}

static long SFLastSampTell(file)	/* return last valid ftell for smps */
    FILE	*file;
    {
    long stt, end;

    GetSeekEnds(file, &stt, &end);
    return end;
    }

static long FixupSamples(file, ptr, format, nitems, flag)
    FILE  *file;
    void  *ptr;
    int   format;
    long  nitems;
    int   flag;		/* READ_FLAG or WRITE_FLAG indicates direction */
    {
    int bytemode;
    int wordsz;
    long bytes;

    static int dbgFlag = 1;

    wordsz = SFSampleSizeOf(format);
    bytes  = nitems * (long)wordsz;
    bytemode = HostByteMode();	/* relative to 68000 mode, which we want */

#ifdef DEBUG
    if(dbgFlag) {
	fprintf(stderr, "AIF_fixup: hostbytemode is %d\n", bytemode);
	dbgFlag = 0;
    }
#endif /* DEBUG */

    if(bytemode != BM_INORDER) {	/* already OK? */
	/* otherwise need to fixup buffer .. */
	ConvertBufferBytes(ptr, bytes, wordsz, bytemode);
    }
    return nitems;
}
