/***************************************************************\
*   sndf.h							*
*   header defs for sound files					*
*   'inspired' by the NeXT SND system				*
*   10aug90 dpwe						*
\***************************************************************/

#ifndef _SNDF_H_
#define _SNDF_H_

extern char *programName;	/* for SFDie() - you must provide */

extern char *sndfExtn;		/* defined as file extn for mode e.g. ".irc" */
extern int  SFerror;    	/* internal error flag */

/* these special strings, passed as paths to open functions, use 
   the appropriate streams instead of actual files - dpwe 1993may26 */
#define SF_STDIN_STR "(stdin)"
#define SF_STDOUT_STR "(stdout)"

#define SFDFLTBYTS 4

typedef struct sfstruct
    {
    long    magic;	    /* magic number to identify */
    long    headBsize;	    /* byte offset from start to data */
    long    dataBsize;	    /* number of bytes of data */
    int     format;	    /* format specifier */
    int     channels;	    /* mono/stereo etc  */
    float   samplingRate;   /* .. of the data   */
    char    info[SFDFLTBYTS];	/* extendable byte area */
    } SFSTRUCT;

/***********************/
/* Codes for structure */
/***********************/

#define SFMAGIC 	107365L
#define SF_UNK_LEN	-1L		/* when dataBsize is unavailable */

#define SF_BYTE_SIZE_MASK	15	/* SFMT && SIZE_MASK == # bytes */
#define _ONEBYTE		1
#define _TWOBYTE		2
#define _FOURBYTE		4
#define _EIGHTBYTE		8
#define _LINEAR			0
#define	_FLOAT			32
#define _ALAW			64
#define _ULAW			128
#define _OFFSET			256
#define _BYTESWAP		512
#define _BOTTOM3QS		1024	/* i.e. 24 bits LSB aligned in 32 */
/* so.. */
#define SFMT_CHAR  	(_ONEBYTE+_LINEAR)
#define SFMT_CHAR_OFFS  (_ONEBYTE+_LINEAR+_OFFSET)
#define SFMT_ALAW  	(_ONEBYTE+_ALAW)
#define SFMT_ULAW  	(_ONEBYTE+_ULAW)
#define SFMT_SHORT 	(_TWOBYTE+_LINEAR)
#define SFMT_SHORT_SWAB 	(_TWOBYTE+_LINEAR+_BYTESWAP)
#define SFMT_24BL	(_FOURBYTE+_LINEAR+_BOTTOM3QS)
#define SFMT_24BL_SWAB	(_FOURBYTE+_LINEAR+_BOTTOM3QS+_BYTESWAP)
#define SFMT_LONG  	(_FOURBYTE+_LINEAR)
#define SFMT_LONG_SWAB  	(_FOURBYTE+_LINEAR+_BYTESWAP)
#define SFMT_FLOAT 	(_FOURBYTE+_FLOAT)
#define SFMT_DOUBLE  	(_EIGHTBYTE+_FLOAT)

/* just convenient symbols for the channels field */
#define SF_MONO		1
#define SF_STEREO	2
#define SF_QUAD		4

/* Error codes returned by PVOC file functions */
#define SFE_UNK_ERR	1	/* unknown (other) error (any number other than below) */
#define SFE_OK	    0	/* no error*/
#define SFE_NOPEN   -1	/* couldn't open file */
#define SFE_NSF     -2	/* not a sound file */
#define SFE_MALLOC  -3	/* couldn't allocate memory */
#define SFE_RDERR   -4	/* read error */
#define SFE_WRERR   -5	/* write error */
#define SFE_EOF	    -6  /* eof before read through header */

/* Known file formats */
#define SFF_ZLEN	-2	/* returned when asked to identify empt file */
#define SFF_UNKNOWN	-1	/* unrecognized format (is it SFF_RAW?) */
#define SFF_RAW 	0	/* Raw/PCM data */
#define SFF_NEXT	1	/* NeXT/Sun .au format */
#define SFF_AIFF	2	/* Mac/SGI AIF format */
#define SFF_MSWAVE	3	/* M$ RIFF/WAVE format */
#define SFF_NIST	4	/* NIST/SPHERE format */
#define SFF_IRCAM	5	/* Csound/IRCAM format */
#define SFF_ESPS	6	/* Entropics ESPS *.sd format (read only) */
#define SFF_PHDAT	7       /* VerbMobil/ILS phondat format */
#define SFF_STRUT	8       /* STRUT variant of NIST */

/* Name for environment variable that defines default sndf ftype */
#define SFF_DFLT_ENV "SNDFFTYPE"

/* defaults e.g. for sndfpcm and sndrecord */
/* #define SNDF_DFLT_SRATE 	22050 */
#define SNDF_DFLT_SRATE 	16000  /* 2000-02-01: too much hassle */
#define SNDF_DFLT_CHANS 	1
#define SNDF_DFLT_FORMAT	SFMT_SHORT

/* In general, the most channels we'll allow */
#define SNDF_MAXCHANS		8

/****************************************/
/**** Function prototypes for export ****/
/****************************************/

int  SFInfoSize PARG((SFSTRUCT *snd));
     /* Return number of info bytes in header */

char *SFDataLoc PARG((SFSTRUCT *snd)); 
     /* Return pointer to data from header */

/* int  SFSampleSizeOf PARG((int format)); */ /* defined in sndfutil.c now */
     /* return number of bytes per sample in given fmt */

int SFSampleFrameSize PARG((SFSTRUCT *snd));	
     /* return number of bytes per sample frame in snd */

int  SFAlloc PARG((SFSTRUCT **psnd, long dataBsize, int format, FLOATARG srate,
	     int chans, int infoBsize));
     /* Allocate memory for a new SFSTRUCT & data block; fill in header
        Returns SFE_MALLOC (& **psnd = NULL) if malloc fails    */

void SFFree PARG((SFSTRUCT *snd));	
     /* destroy in-memory sound */

FILE *SFOpenRdHdr PARG((char *path, SFSTRUCT *snd, int infoBmax));
     /* Opens named file as a stream, reads & checks header, returns stream
        Attempts to read up to infoBmax info bytes, at least 4.
        No error codes; returns NULL on fail (SFE_NOPEN, _NSF, _RDERR) */

int  SFRdXInfo PARG((FILE *file, SFSTRUCT *snd, int done));
     /* fetch any extra info bytes needed in view of newly-read header
        Passed open file handle, snd includes destination for new info,
        done = infobytes already read, other info read from snd */

FILE *SFOpenAllocRdHdr PARG((char *path, SFSTRUCT **psnd));
     /* find sizeof header, alloc space for it, read it in.
        returns NULL on fail (SFE_NOPEN, _NSF, _RDERR) */

int  SFAllocRdHdr PARG((FILE *file, SFSTRUCT **psnd));	
    /* allocate & read a header from open file */

FILE *SFOpenWrHdr PARG((char *path, SFSTRUCT *snd));
     /*  Creates file in path & writes out header, returns stream for access
         No error codes, just returns NULL on fail 
         (SFE_NSF, _NOPEN, _WRERR)	*/

void SFCloseWrHdr PARG((FILE *file, SFSTRUCT *snd));			
     /* After opening via SFOpenWrHdr and writing sound to the stream, may
        need to update header with a postieri data (sound length, more info).
        SFCloseWrHdr works 3 ways:
	- snd is NULL	 ->   look at file ptr, rewrite length to hdr
	- snd exists, but dataBlen is 0
			 ->   fill in new dataBlen from file ptr, write hdr
	- snd exists including dataBlen
			 ->   rewrite whole header
        Ignores any fails & just closes file as best it can.
	file MUST BE READWRITE (mode "w+" or "a+")
	IF MODE JUST "w", THIS silently TRASHES THE HEADER on ultrix 4.0 ! */

void SFFinishWrite PARG((FILE *file, SFSTRUCT *snd));
     /* Do the finishing touches in SFCloseWrHdr, but don't actually 
	close the file or destroy its records (for writing multiple 
	files on a single pipe) */

void SFClose PARG((FILE *file));
     /* just close the file & clean up */

void SFClose_Finishup PARG((FILE *file));
     /* just clean up, for when client has done close (e.g. for popen) */

int  SFReadHdr PARG((FILE *file, SFSTRUCT *snd, int infoBmax));
/* sample the header of an opened file, at most infoBmax info bytes */

int  SFWriteHdr PARG((FILE *file, SFSTRUCT *snd, int infoBmax));
/*  Utility to write out header to open file
    infoBmax optionally limits the number of info chars to write.
    infoBmax = 0 => write out all bytes implied by .headBsize field
    infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */

int  SFRead PARG((char *path, SFSTRUCT **psnd));
     /* Opens & checks file specified in path.  Allocs for & reads whole sound.
        Poss errors: SFE_NOPEN, SFE_NSF, SFE_MALLOC, SFE_RDERR */

int  SFWrite PARG((char *path, SFSTRUCT *snd));
     /* Write out the soundfile to the file named in path.
        Poss errors: SFE_NSF, SFE_NOPEN, SFE_WRERR */

char *SFErrMsg PARG((int err));	
     /* convert error code to message */
void  SFDie PARG((int err, char *msg));	/* exit routine */

char *buildsfname PARG((char *name));		/*  build a soundfile name (SFDIR) */

size_t SFfread PARG((void *ptr, int type, size_t nitems, FILE *stream));
size_t SFfwrite PARG((void *ptr, int type, size_t nitems, FILE *stream));
int SFfseek PARG((FILE *stream, long offset, int ptrname));
int SFftell PARG((FILE *stream));
int SFfeof PARG((FILE *stream));

long SFFlushToEof PARG((FILE *file, int format));
    /* Skip forward to the end of the data associated with this file.
       Return the actual number of samples of <format> of data consumed.
       Reset the virtual file pointer after reaching end, for 
       transparent reading of multiple files concatenated on 
       a single stream. */

/** Additions for on-the-fly file typing **/

int SFSetDefaultFormat PARG((int id));
    /* Set the default soundfile format according to its SFF_ id */

int SFSetDefaultFormatByName PARG((char *name));
    /* Set the default soundfile format for future operations 
       according to the one that matches this "name"
       Return 1 on success, 0 if no such format is known. */

int SFGetDefaultFormat PARG((void));
    /* return the current default format, or SFF_UKNOWN if in guess mode.
       (compare to SFGetFileFormat(NULL, ...)) */

int SFSetFormat PARG((FILE *file, int id, int bytemode));
    /* Set the soundfile format for this file according to its SFF_ id.
       Specifying SFF_UNKNOWN means take the default. */

int SFSetFormatByName PARG((FILE *file, char *name));
    /* Set the format for this file using a name. */

int SFGetFormat PARG((FILE *file, int *pbytemode));
    /* return the soundfile format associated with the file, 
       or the default format if FILE is NULL or unknown */

int SFIdentify PARG((FILE *file, int *pmode));
    /* Figure out the type of a soundfile.  Use *pmode to rtn byteswap mode */

void SFListFormats PARG((FILE *stream));
    /* print a list of all known format names to <stream> */

char *SFFormatName PARG((int id));
    /* return the ASCII tag (name) for the specified SFF_ format */

int SFFormatToID PARG((const char *name));
    /* Return the soundfile format ID correspoding to this name; 
       -1 if not recognized  (==SFF_UNKNOWN) */

void SFSetFname PARG((FILE *file, char *name));
    /* provide hint as to what this file is called */

#endif /* _SNDF_H_ */
