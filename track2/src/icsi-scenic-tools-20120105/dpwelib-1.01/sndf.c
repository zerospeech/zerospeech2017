/***************************************************************\
*   sndf.c		    					*
*   implementation of for sound file interface			*
*   just writes files as images of memory block			*
*   'inspired' by the NeXT SND system				*
*   10aug90 dpwe						*
\***************************************************************/

#include <dpwelib.h>

#define READMODE "rb"	    /* was + for read-mod-write, but that's a lose */
#define WRITEMODE "wb+"	    /* "+" allows infoBsize reread */

#include <sndf.h>
#include <sndfutil.h>

#define READ_FLAG	1
#define WRITE_FLAG	0	/* flags to FixupSamples (byte ordering) */
#define UNREAD_FLAG	-1	/* means just reverse the processing 
				   on read, prior to pushing bytes back
				   onto input (so as *not* to eliminate 
				   future Abbot EOS flags - oops!!) */
/* static variables */

static SFSTRUCT tmphdr;     /* scratch space for pre-load */
/* static */ int	SFerror;    /* (internal) error flag */
static char	unspecMsg[] = "Unspecified error : _______";
/* want to 'fill in' at    012345678901234567890... [20..27] */
#define USMINDX 20	/* from above, chr index into string */

/* static function prototypes */
static void itoa PARG((int n, char *s));   /* for unrecognised error code */

#ifdef OLD
static void AddFname PARG((char *fname, FILE *file));	/* filename table */
static void RmvFname PARG((FILE *file));	/* implemented if needed */
static long SFHeDaLen PARG((FILE *file));  /* return space used for fi hdr */
static long FixupSamples PARG((FILE *file, void *ptr, int format, long nitems, int flag));
                        /* hook for byte reordering */
static long SFLastSampTell PARG((FILE *file));
static long SFTrailerLen PARG((FILE *file));
#endif /* OLD */

/* ----------------------- implementations ------------------------ */


#ifndef SNDF_PTRSTYLE
/* this one is always used now */
#include "sndffname.c"

/* dummies for SNDF_PTRSTYLE-specific fns */
int SFSetDefaultFormatByName(char *name) {
    fprintf(stderr, "sndf: cannot set format\n");
    /* but pretend all OK */
    return 1;
}

int SFSetDefaultFormat(int id) {
    fprintf(stderr, "sndf: cannot set format\n");
    /* but pretend all OK */
    return 1;
}

int SFSetFormat(FILE *file, int id, int bytemode) {
    fprintf(stderr, "sndf: cannot set format\n");
    /* but pretend all OK */
    return 1;
}

int SFGetFormat(FILE *file, int *pmode) {
    if(pmode)	*pmode = BM_INORDER;
    return SFF_UNKNOWN;
}

int SFIdentify(FILE *file, int *pmode) {
    if(pmode)	*pmode = BM_INORDER;
    return SFF_UNKNOWN;
}

void SFListFormats(FILE *stream) {
    fprintf(stream, "<no selectable formats>");
}

char *SFFormatName(int id) {
    /* descriptive name for this format - we don't know */
    return "SNDF";
}

int SFFormatToID(const char *name)
{   /* Return the soundfile format ID correspoding to this name; 
       -1 if not recognized (==SNDF_UNKNOWN) */
    fprintf(stderr, "sndf: cannot set format\n");
    return SFF_UNKNOWN;
}

static void SF_Setuptype(FILE *file) {
    /* nothing */
}

#define MAYBE_ID(file,err)	err = SFE_OK;	/* i.e. nothing */
#define CHECK_FT		/* nothing */

#ifdef THINK_C
#ifdef MACAIFF
#include "sndfaif.c"		/* AIFF version */
#else /* not MACAIFF -> SoundDesigner */
#include "sndfsd2.c"	    /* mac versions of header r/w &c */
#endif /* MACAIFF */
#else  /* !THINK_C */
/* #include "sndfnat.c"	    /* native (direct memory image) headers */
#ifdef NeXT
#include "sndfnxt.c"	    /* NeXT '.snd' soundfiles */
#else /* !NeXT */
#ifdef PCM		    /* headerless files */
#include "sndfpcm.c"
#else /* !PCM */
#ifdef AIFF
#include "sndfaif.c"	    /* AIFF on UNIX for Indigo */
#else /* !AIFF */
#ifdef MS_WAVE
#include "sndfwav.c"	    /* Microsoft WAV/RIFF PC format */
#else /* !MS_WAVE */
#ifdef ICSI_NIST
#include "sndfnist.c"	    /* NIST .wav headers used at ICSI */
#else /* !ICSI_NIST */
#ifdef ESPS
#include "sndfesps.c"	    /* Entropics ESPS *.sh format */
#else /* !ESPS */
#include "sndfirc.c"	    /* standard IRCAM headers */
#endif /* !ESPS */
#endif /* !ICSI_NIST */
#endif /* !MS_WAVE */
#endif /* !AIFF	*/
#endif /* !PCM	*/
#endif /* !NeXT	*/
#endif /* !THINK_C	*/
#endif /* !SNDF_PTRSTYLE */

int SFInfoSize(snd)
    SFSTRUCT *snd;	/* return number of info bytes in header */
    {
    return(snd->headBsize - sizeof(SFSTRUCT) + SFDFLTBYTS);
    }

char *SFDataLoc(snd)
    SFSTRUCT *snd;	/* return pointer to data from header */
    {
    return( ((char *)snd + snd->headBsize) );
    }

/* return number of bytes per sample in given fmt */
/* 
int SFSampleSizeOf(format)
    int format;	
    {
    return(format & SF_BYTE_SIZE_MASK);
    }
 */
/* now in sndfutil.c */

int SFSampleFrameSize(snd)
    SFSTRUCT *snd;	/* return number of bytes per sample frame in snd */
    {
    return snd->channels * SFSampleSizeOf(snd->format);
    }

int  SFAlloc(psnd, dataBsize, format, srate, chans, infoBsize)
    SFSTRUCT **psnd;
    long dataBsize;
    int format;
    FLOATARG srate;
    int chans;
    int infoBsize;
/*  Allocate memory for a new SFSTRUCT+data block;fill in header
    Returns SFE_MALLOC (& **psnd = NULL) if malloc fails    */
    {
    long    bSize;
    long    infoXtra;

    if(infoBsize < SFDFLTBYTS)	infoBsize = SFDFLTBYTS;
    infoBsize = (infoBsize + 3)&-4; /* round up to 4 byte multiple */
    infoXtra = (long)(infoBsize-SFDFLTBYTS);
    if(dataBsize == SF_UNK_LEN)
	bSize = sizeof(SFSTRUCT) + infoXtra;
    else
	bSize = dataBsize + sizeof(SFSTRUCT) + infoXtra;
    if( ( (*psnd) = (SFSTRUCT *)malloc((size_t)bSize)) == NULL)
	return(SFerror = SFE_MALLOC);
    (*psnd)->magic = SFMAGIC;
    (*psnd)->headBsize = sizeof(SFSTRUCT) + infoXtra;
    (*psnd)->dataBsize = dataBsize;
    (*psnd)->format    = format;
    (*psnd)->samplingRate = srate;
    (*psnd)->channels  = chans;
    /* leave info bytes undefined */
    return(SFerror = SFE_OK);
    }

void SFFree(snd)
    SFSTRUCT *snd;	/* destroy in-memory sound */
    {
    free(snd);
    }

FILE *SFOpenRdHdr(path, snd, infoBmax)
    char *path;
    SFSTRUCT *snd;
    int infoBmax;
/*  Opens named file as a stream, reads & checks header, returns stream
    Attempts to read up to infoBmax info bytes, at least SFDFLTBYTS.
    No error codes; returns NULL on fail (SFE_NOPEN, _NSF, _RDERR) */
    {
    FILE    *file;

    DBGFPRINTF((stderr,"SFOpRdH: entered for %s\n", path));
    if(strcmp(path, SF_STDIN_STR)==0)  {   /* special case */
        file = stdin;
    } else if( (file = fopen(path, READMODE)) == NULL) {
	DBGFPRINTF((stderr,"SFOpRdH: fopen (mode %s) failed\n", READMODE));
	SFerror = SFE_NOPEN;
	return(NULL);
    }
    AddFname(path,file);	/* create filename table entry */
    MAYBE_ID(file, SFerror);
    if(SFerror == SFE_OK) { 
	SFerror = SFReadHdr(file, snd, infoBmax);
    }
    DBGFPRINTF((stderr,"SFOpRdH: ID/RdH returned %d\n", SFerror));
    if (SFerror == SFE_OK) {
	return(file);
    } else {
	fclose(file);
	RmvFname(file);		/* remove filename table entry */
	return(NULL);
    }
}

FILE *SFOpenAllocRdHdr(path, psnd)
    char *path;
    SFSTRUCT **psnd;
     /* find sizeof header, alloc space for it, read it in.
        returns NULL on fail (SFE_NOPEN, _NSF, _RDERR) */
    {
    FILE	*retFP;
    int 	i;
    long	headBsz;

    if(strcmp(path, SF_STDIN_STR)==0)  {   /* special case */
        retFP = stdin;
    } else if( (retFP = fopen(path, READMODE)) == NULL)
	{
	SFerror = SFE_NOPEN;
	return NULL;
	}
    AddFname(path,retFP);	/* create filename table entry */
    if( (SFerror = SFAllocRdHdr(retFP, psnd)) != SFE_OK)
	{
	fclose(retFP);
	RmvFname(retFP);
	return NULL;
	}
    else
	return retFP;
    }

FILE *oldSFOpenAllocRdHdr(char *path, SFSTRUCT **psnd);	/* ob.proto for THINK_C */

FILE *oldSFOpenAllocRdHdr(path, psnd)
    char *path;
    SFSTRUCT **psnd;
     /* find sizeof header, alloc space for it, read it in.
        returns NULL on fail (SFE_NOPEN, _NSF, _RDERR) */
    {
    FILE	*retFP;
    int 	i;
    long	headBsz;

    retFP = SFOpenRdHdr(path, &tmphdr, SFDFLTBYTS);
    if(retFP != NULL)
	{
	headBsz = tmphdr.headBsize;
	*psnd = (SFSTRUCT *)malloc(headBsz);
	if(*psnd != NULL)	/* ..else ukky malloc fail */
	    {
	    for(i = 0; i<sizeof(SFSTRUCT); ++i)	/* bytewise copy */
		((char *)*psnd)[i] = ((char *)&tmphdr)[i];
	    SFRdXInfo(retFP, *psnd, SFDFLTBYTS); 
	    }
	}
    return retFP;
    }

int SFAllocRdHdr(file, psnd)	/* allocate & read a header from open file */
    FILE *file;
    SFSTRUCT **psnd;
    {
    long headBsz;
    int  i;

    if( file == NULL)
	return(SFerror = SFE_NOPEN);
    MAYBE_ID(file,SFerror);
    if( SFerror != SFE_OK \
	|| (SFerror = SFReadHdr(file, &tmphdr, SFDFLTBYTS)) != SFE_OK) {
        DBGFPRINTF((stderr, "SFAllocRdHdr: error %d in SFReadHdr\n", SFerror));
	return(SFerror);
    }
    headBsz = tmphdr.headBsize;
    *psnd = (SFSTRUCT *)malloc(headBsz);
    if(*psnd != NULL)	/* ..else ukky malloc fail */
	{
	for(i = 0; i<sizeof(SFSTRUCT); ++i)	/* bytewise copy */
	    ((char *)*psnd)[i] = ((char *)&tmphdr)[i];
	SFRdXInfo(file, *psnd, SFDFLTBYTS); 
	}
    else {
        DBGFPRINTF((stderr, "SFAllocRdHdr: error in malloc??\n"));
	return(SFerror = SFE_MALLOC);
    }

    return(SFerror = SFE_OK);
    }

FILE *SFOpenWrHdr(path, snd)
    char *path;
    SFSTRUCT *snd;
/*  Creates file in path & writes out header, returns stream for access
    No error codes, just returns NULL on fail (SFE_NSF, _NOPEN, _WRERR)  */
    {
    FILE    *file;

    if(snd->magic != SFMAGIC)
	{
	SFerror = SFE_NSF;
	return(NULL);
	}
    if(strcmp(path, SF_STDOUT_STR)==0)  {   /* special case */
        file = stdout;
    } else if( (file = fopen(path, WRITEMODE)) == NULL)
	{
	SFerror = SFE_NOPEN;
	return(NULL);	
	}
    AddFname(path,file);	/* create a filename table entry */
    CHECK_FT;
    if(SFWriteHdr(file,snd,0))
	{
	fclose(file);
	RmvFname(file);		/* remove filename table entry */
	SFerror = SFE_WRERR;
	return(NULL);	
	}
    else
	{
	SFerror = SFE_OK;
	return(file);
	}
    }

void SFFinishWrite(file, snd)
    FILE *file;
    SFSTRUCT *snd;
/*  After opening via SFOpenWrHdr and writing sound to the stream, may
    need to update header with a postieri data (sound length, more info).
    SFCloseWrHdr works 3 ways:
	- snd is NULL	    look at file ptr, rewrite length to hdr
	- snd exists, but dataBlen is 0
			    fill in new dataBlen from file ptr, write hdr
	- snd exists including dataBlen
			    rewrite whole header
    Ignores any fails & just closes file as best it can.
    Must have readback access for file (mode "w+"), else can 
    trash the header (e.g. if opened "w" under ultrix 4.0) */	
{
    long len;
    SFSTRUCT *myhdr = NULL;

    SF_Setuptype(file);
    len = ftell(file);

    DBGFPRINTF((stderr, "SFFinishWrite(file=0x%lx, type=%s, pcmflag=%d) len=%d\n", file, _sndf_type->name, pcmDefaults?pcmDefaults->miscflags:-1, len));

    if(len >= 0) {		/* ONLY if can seek i.e. not pipe */
	len -= SFHeDaLen(file);    /* knock off written hdr */
	if(snd == NULL) {
	    SFerror = SFAllocRdHdr(file, &myhdr); /* read orig hdr */
	    /* rewrite modified header */
	    if(SFerror == SFE_OK) {
		/* only write back if we read good header */
		myhdr->dataBsize = len; /* fixup data length */
		SFerror = SFWriteHdr(file, myhdr, -1);
	    } else {	/* just a quick go */
		SFWriteHdr(file, NULL, -1);
	    }
	} else {	/* we were passed a header to rewrite */
	    if(snd->dataBsize == 0 || snd->dataBsize == SF_UNK_LEN) {
		/* .. but does it have a length ? */
		snd->dataBsize = len; /* fill in if not */
	    }
	    SFerror = SFWriteHdr(file, snd, -1); 
	    /* write out whole lot (incl. all info) */
	}
    } else {
	/* Give WriteHdr a look-in, although it can't seek back.. */
	SFWriteHdr(file, NULL, -1);
    }

    /* try to ensure we exit at the end of the stream */
    fseek(file, 0, SEEK_END);
#ifdef HAVE_FPUSH
    /* reset the file pointer, if we have that capability */
    fsetpos(file, 0);
#endif /* HAVE_FPUSH */

    /* release temporary storage */
    if(myhdr) {
	SFFree(myhdr);
    }
}

void SFClose(file)
    FILE *file;
{   /* just close the file */
    fclose(file);
    RmvFname(file);	/* remove filename table entry */
}

void SFClose_Finishup(file)
     FILE *file;
{   /* provide access to the magic from outside (for pclose) */
    RmvFname(file);
}

void SFCloseWrHdr(file,snd)
    FILE *file;
    SFSTRUCT *snd;
{
    SFFinishWrite(file, snd);
    SFClose(file);
}

int SFRead(path,psnd)
    char *path;
    SFSTRUCT **psnd;
/* Opens & checks file specified in path.  Allocs for & reads whole sound.
   Poss errors: SFE_NOPEN, SFE_NSF, SFE_MALLOC, SFE_RDERR */
    {
    FILE    *file;
    int     i, err = SFE_OK;
    int     xinfo;
    int     format = 0;
    int     chans  = 0;
    float   srate  = 0.0;
    size_t  num;

#ifdef DEBUG
	fprintf(stderr,"SFRd: reading %s\n", path);
#endif
    if( (file = SFOpenRdHdr(path, &tmphdr, SFDFLTBYTS)) == NULL)
	return(SFerror);
#ifdef DEBUG
	fprintf(stderr,"SFRd: hdr of %s read\n", path);
#endif
    xinfo = tmphdr.headBsize - sizeof(SFSTRUCT);
    err = SFAlloc(psnd, tmphdr.dataBsize, format, srate, chans,
		    xinfo + SFDFLTBYTS);
#ifdef DEBUG
	fprintf(stderr,"SFRd: alloc rtns %d\n", err);
#endif
    if(err == SFE_OK)
	{
	long ssiz;

	/* copy over what we already read */
	for(i = 0; i < sizeof(SFSTRUCT)/2; ++i)
	    ((short *)*psnd)[i] = ((short *)&tmphdr)[i];
	/* grab remaining info chars (may be 'elsewhere') */
	if( xinfo > 0 )
	    SFRdXInfo(file, *psnd, SFDFLTBYTS);
	format = (*psnd)->format;
	ssiz = SFSampleSizeOf(format);
	/* read all the data */
	if((*psnd)->dataBsize != SF_UNK_LEN)	/* i.e. a valid length */
	    if( (num = SFfread(SFDataLoc(*psnd),
			     format, 
			     (size_t)(*psnd)->dataBsize/ssiz,
			     file))
	       < (*psnd)->dataBsize/ssiz )
		{
		fprintf(stderr,"SFRead: wanted %ld got %ld of %ld\n",
			(*psnd)->dataBsize/ssiz, (long) num, ssiz);	
		err = SFE_RDERR;
		}
	}
    if( (err != SFE_OK) && (*psnd != NULL))
	{
	SFFree(*psnd);
	*psnd = NULL;
	}
    fclose(file);
    RmvFname(file);	/* remove filename table entry */
    return(SFerror = err);
    }

int SFWrite(path,snd)
    char *path;
    SFSTRUCT *snd;
/* Write out the soundfile to the file named in path.
   Poss errors: SFE_NSF, SFE_NOPEN, SFE_WRERR */
    {
    FILE    *file;
    int     err = SFE_OK;
    long    bSize, items;

    if(snd->magic != SFMAGIC)
	return(SFerror = SFE_NSF);
    if( (file = SFOpenWrHdr(path, snd)) == NULL)
	return(SFerror);
    bSize = snd->dataBsize;
    if(bSize != SF_UNK_LEN)  {	/* i.e. a valid length */
	items = bSize / SFSampleSizeOf(snd->format);
	if( SFfwrite(SFDataLoc(snd), snd->format, items, file)
	   < items )
	    err = SFE_WRERR;
    }
    fclose(file);
    RmvFname(file);	/* remove filename table entry */
    return(SFerror = err);
    }

char *SFErrMsg(err)
    int err;     /* convert error code to message */
    {
    switch(err)
	{
    case SFE_OK:	return("No SF error");
    case SFE_NOPEN:	return("Cannot open sound file");
    case SFE_NSF:	return("Object/file not sound");
    case SFE_MALLOC:	return("No memory for sound");
    case SFE_RDERR:	return("Error reading sound file");
    case SFE_WRERR:	return("Error writing sound file");
    case SFE_EOF:	return("Premature EOF in sound file");
    default:
	itoa(err, &unspecMsg[USMINDX]);
	return(unspecMsg);
	}
    }

/* change this if you like. */
char *sndfProgramName = "SNDF";

void SFDie(err, msg)	/* what else to do with your error code */
    int  err;
    char *msg;
    {
    if(msg != NULL && *msg)
	fprintf(stderr,"%s: error: %s (%s)\n",sndfProgramName,SFErrMsg(err),msg);
    else
	fprintf(stderr,"%s: error: %s\n",sndfProgramName,SFErrMsg(err));
    exit(1);
    }

/* itoa from K&R p59 */
static void itoa(n,s)
    int n;
    char *s;
    {
    int     i,j,sign;
    char    c;

    if( (sign = n) < 0)     n = -n;
    i = 0;
    do	{
	s[i++] = n%10 + '0';
	} while( (n /= 10) > 0);
    if(sign < 0)
	s[i++] = '-';
    s[i--] = '\0';
    for(j=0; j<i; ++j,--i)	/* reverse the string */
	{ c = s[i]; s[i] = s[j]; s[j] = c; }
    }

/* buildsfname stolen from csound */

#define MAXFULNAME 128

char *buildsfname(name)			/*  build a soundfile name	*/
    register char *name;       		/*  <mod of carl getsfname()>	*/
    {
    static	char	fullname[MAXFULNAME];
#ifdef THINK_C
static char *sfdir_path = "dummyMacPath";
    /*
     * If name is not a pathname, then construct using sfdir_path[].
     */
    fullname[0] = 0;
    if (strchr(name,':') == NULL) {
	strcpy(fullname,sfdir_path);
	strcat(fullname,":");
	}
    strcat(fullname,name);
    return fullname;
#else
    char	*sfdir, *getenv();

    if (*name == '/' || *name == '.') /* if path already given */
	return(strcpy(fullname,name)); /*	copy as is	*/
    else {			/* else build right here  */
	if ((sfdir = getenv("SFDIR")) != NULL)
	    {
	    sprintf(fullname,"%s/%s",sfdir,name);
	    return(fullname);
	    }
	else
	    {
	    /* die("buildsfname: environment variable SFDIR undefined"); */
	    strcpy(fullname,name);
	    return(fullname);
	    }
	}
#endif
    }

/* covers for other major ANSI file functions
   - allow filesize faking and invisible byte reordering (for AIFF) */

size_t SFfread(ptr, format, nitems, stream)
    void *ptr;
    int  format;
    size_t nitems;
    FILE *stream;
    {
    int		size;
    long	red, ret;
    long 	pos, end=-1, remBytes=-1;
    int		nits = nitems;

    /* must set filetype before SFLastSampTell */
    SF_Setuptype(stream);
/*    CHECK_FT  */

    size = SFSampleSizeOf(format);
    pos = ftell(stream);
    if(pos >= 0)
	{
	end = SFLastSampTell(stream);
	if(end > 0)
	    {
	    remBytes = end - pos;
	    /* asking for more than we think we have */
	    if(remBytes < size*nits)	
		nits = remBytes/size;
	    }
	}
    if (nits < 0)	nits = 0;
/* fprintf(stderr, "SFfread(0x%lx)@%d: end=%d nitems=%d allow=%d remb=%d\n", 
	stream, pos, end, nitems, nits, remBytes); */

    red = fread(ptr, size, nits, stream);
    /* Sort through the samples and maybe end early (if a marker seen) */
    ret = FixupSamples(stream, ptr, format, red, READ_FLAG);
    assert(ret <= red);
    if(ret < red) {	/* some truncation in FixupSamps */
	long stt, end, trail;
	char *remains = ((char *)ptr)+size*ret;
/* fprintf(stderr, "SFfread: wanted %d, got %d (at %d)\n", red, ret, ftell(stream)); */
	/* rewrite where the end of data is */
	GetSeekEnds(stream, &stt, &end);
	end = ftell(stream) - size*(red-ret);
	SetSeekEnds(stream, stt, end);
#ifdef HAVE_FPUSH
	/* don't bother to push the trailling component */
	trail = SFTrailerLen(stream);
	/* undo the fixing up */
	FixupSamples(stream, remains+trail, format, red-ret-(trail/size), 
		     UNREAD_FLAG);
	/* return the unread part to the stream */
	my_fpush(remains+trail, 1, size*(red-ret)-trail, stream);
	/* force the pos to read as zero */
	/* my_fsetpos(stream, 0); */
#endif /* HAVE_FPUSH */
    }
/* fprintf(stderr, "SFfread:%dx%d(red%d)=%d\n", nitems, format, red, ret); */
    return ret;
}

long SFFlushToEof(FILE *file, int format)
{   /* Skip forward to the end of the data associated with this file.
       Return the actual number of samples of <format> of data consumed.
       Reset the virtual file pointer after reaching end, for 
       transparent reading of multiple files concatenated on 
       a single stream. */
    long nowpos, endpos = -1, sred = -1, stt, end = -1, trail;
    int rc = -1;
    int size = SFSampleSizeOf(format);

    SF_Setuptype(file);

    GetSeekEnds(file, &stt, &end);
    trail = SFTrailerLen(file);
    nowpos = ftell(file);
    rc = -1;
    DBGFPRINTF((stderr, "SFFlush(0x%lx) stt=%d end=%d trail=%d pos=%d\n",
	file, stt, end, trail, nowpos));
    /* if stream is seekable AND we already know where the end is... */
    if (nowpos >= 0 && end != SF_UNK_LEN && trail != SF_UNK_LEN) {
	endpos = end + trail;
	rc = fseek(file, endpos, SEEK_SET);
	if(endpos >= nowpos) {
	    sred = (endpos - trail - nowpos)/size;
	}
    }
    if (rc < 0) {
	/* couldn't seek - try just reading until we hit the end */
	short buf[1024];
	int bsiz = 1024;
	int got = bsiz;
	sred = 0;
	while(got > 0) {
	    got = SFfread(buf, format, bsiz, file);
	    sred += got;
	}
	/* reached eof, I guess */
	rc = 0;
    }
    /* lose the recorded data, so we can read a fresh file here */
    RmvFname(file);
#ifdef HAVE_FPUSH
    /* reset the file pointer, if we have that capability */
    fsetpos(file, 0);
#endif /* HAVE_FPUSH */
    return sred;
}

size_t SFfwrite(ptr, format, nitems, stream)
    void *ptr;
    int  format;
    size_t nitems;
    FILE *stream;
    {
    size_t	size;
    long 	retval;

    SF_Setuptype(stream);
/*     CHECK_FT   */
    nitems = FixupSamples(stream, ptr, format, nitems, WRITE_FLAG);  /* allow modification */
    size = SFSampleSizeOf(format);
    retval = fwrite(ptr, size, nitems, stream);
    /* unfix the samples again */
    FixupSamples(stream, ptr, format, nitems, READ_FLAG);
#ifdef DEBUG
    fprintf(stderr, "SFfwrite: wrote %ld of %d, pos=%ld\n", nitems, format, ftell(stream));
#endif /* DEBUG */
    return retval;
    }

int SFfseek(stream, offset, ptrname)
    FILE *stream;
    long offset;
    int  ptrname;
    {
    SF_Setuptype(stream);
/*    CHECK_FT */
    if(ptrname == SEEK_SET) {
	offset += SFHeDaLen(stream);
    } else if (ptrname == SEEK_END) {
	FNAMEINFO *fni = find_node(stream, 0);
	if(fni && fni->sndend > 0) {   /* found record for end of snd data */
	    offset += fni->sndend;
	    ptrname = SEEK_SET;
	} else {
	    /* unable to seek_end */
	    return -1;
	}
    }
    return( fseek(stream, offset, ptrname) );
    }

int SFftell(FILE *stream)
{
    long pos, hdlen;

    SF_Setuptype(stream);
/*    CHECK_FT */
    pos = ftell(stream);
    hdlen = SFHeDaLen(stream);
    if (pos >= hdlen)	{pos -= hdlen;}
    return(pos);
}

int SFfeof(stream)
    FILE *stream;
    {
    long pos, end;

    /* actually want to check for run over sample region .. */
    pos = ftell(stream);
    DBGFPRINTF((stderr, "SFfeof(0x%lx) pos=%d, lasttell=%d\n", stream, pos, SFLastSampTell(stream)));
    if(pos >= 0)
	{
	SF_Setuptype(stream);
	/* CHECK_FT */
	end = SFLastSampTell(stream);
	if(end > 0)
	    {
	    if(pos < end)
		return 0;
	    else
		return -1;
	    }
	}
    /* if that doesn't work */
    return(feof(stream));
    }
 
void SFSetFname(FILE *file, char *name)
{   /* hint helper - tell us what this file is called (for Mac) */
	AddFname(name, file);
}

