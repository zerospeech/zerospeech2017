/*
 * Definitions for writing AIFF files.
 *
 * Bill Gardner, March 1991.
 * modified for AIFC's  dpwe 1993feb
 * 1994may10 some of the old file added back in for unification - dpwe
 */

typedef unsigned char my_extended[10];

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef OLD_THINK_C
typedef long AiffID;
typedef long LONG;
#define AIFF_TYPE       ((AiffID) 'AIFF')
#define AIFC_TYPE       ((AiffID) 'AIFC')
#define FORM_ID         ((AiffID) 'FORM')
#define FVER_ID         ((AiffID) 'FVER')
#define COMM_ID         ((AiffID) 'COMM')
#define SSND_ID         ((AiffID) 'SSND')
#define MARK_ID         ((AiffID) 'MARK')
#define INST_ID         ((AiffID) 'INST')
#define APPL_ID         ((AiffID) 'APPL')
#define NAME_ID         ((AiffID) 'NAME')
#define AUTH_ID         ((AiffID) 'AUTH')
#define COPY_ID         ((AiffID) '(c) ')
#define ANNO_ID         ((AiffID) 'ANNO')
#define COMT_ID         ((AiffID) 'COMT')
#define NONE_ID         ((AiffID) 'NONE')
#define SAXL_ID			((AiffID) 'SAXL')
#define MIDI_ID			((AiffID) 'MIDI')
#define AESD_ID			((AiffID) 'AESD')
#else /* !THINK_C */
typedef char AiffID[4];
typedef struct { short hw; short lw; } LONG;    /* MIPS is nervous w/ longs */
#define AIFF_TYPE   {'A','I','F','F'}
#define AIFC_TYPE   {'A','I','F','C'}
#define FORM_ID     {'F','O','R','M'}
#define FVER_ID     {'F','V','E','R'}
#define COMM_ID     {'C','O','M','M'}
#define SSND_ID     {'S','S','N','D'}
#define MARK_ID     {'M','A','R','K'}
#define INST_ID     {'I','N','S','T'}
#define APPL_ID     {'A','P','P','L'}
#define NAME_ID     {'N','A','M','E'}
#define AUTH_ID     {'A','U','T','H'}
#define COPY_ID     {'(','c',')',' '}
#define ANNO_ID     {'A','N','N','O'}
#define COMT_ID     {'C','O','M','T'}
#define NONE_ID     {'N','O','N','E'}
#define SAXL_ID		{'S','A','X','L'}
#define MIDI_ID		{'M','I','D','I'}
#define AESD_ID		{'A','E','S','D'}
#endif /* !THINK_C */

typedef struct {
    AiffID  ckID;
    LONG    ckSize;		/* a regular long since always word-aligned */
    /* ckSize USED to be a regular long (since always word-aligned)
       However, MIPS behaviour is mighty weird:  with ckSize defined
       as a "long", sizeof(CommonChunk) == 28 i.e. a multiple of 
       four - even though all the fields stay in their proper places!  
       The mere presence of long I suppose makes the compiler want to 
       ensure that the whole structure is longword-aligned, so it makes 
       sure that two CommonChunks in a row will both be this.  Hmmm.. */
} CkHdr;        /* 8 bytes */

typedef struct {
    CkHdr   ckHdr;
    AiffID  formType;
} FormHdr;      /* 12 bytes */

typedef struct {
    CkHdr       ckHdr;
    short       numChannels;
    LONG        numSampleFrames;        /* MIPS won't allow unaligned longs */
    short       sampleSize;
    my_extended sampleRate;
} CommonChunk;  /* 26 bytes ... ugh! not multiple of 4 -> needs fixup */

/* typedef char  pstring[0];    /* first byte is len, always pad to even */

typedef struct {
    CkHdr       ckHdr;
    short       numChannels;
    LONG        numSampleFrames;        /* MIPS won't allow unaligned longs */
    short       sampleSize;
    my_extended sampleRate;
    AiffID      compressionType;
/*    pstring   compressionName; */     /* pstrings make length odd - no go */
} AIFCCommonChunk;              /* revised for new format */

#define AIFCVersion1  0xA2805140        /* only value for timestamp blw */

typedef struct {
    CkHdr       ckHdr;
    LONG        timestamp;
} FormatVersionChunk;

typedef short MarkerId;

typedef struct {
    MarkerId    id;
    LONG        position;
/*    pstring   markerName;     */      /* pstrings make length odd - no go */
} Marker;

typedef struct {
    CkHdr       ckHdr;
    short       numMarkers;
/*    Marker    markers[];      */
} MarkerChunk;  /* 10 bytes ... not mult of 4 */

typedef struct {
    LONG        timeStamp;
    MarkerId    marker;
    short       count;
/*    char      text[];  */
} Comment;

typedef struct {
    CkHdr       ckHdr;
    short       numComments;
/*    Comment   comments[];     */
} CommentsChunk;

#define NoLooping               0
#define ForwardLooping          1
#define ForwardBackwardLooping  2

typedef struct {
    short   playMode;
    short   beginMark;
    short   endMark;
} Loop;         /* 6 bytes */

typedef struct {
    CkHdr   ckHdr;
    char    baseNote;
    char    detune;
    char    lowNote;
    char    highNote;
    char    lowVelocity;
    char    highVelocity;
    short   gain;
    Loop    sustainLoop;
    Loop    releaseLoop;
} InstrumentChunk;

typedef struct {
    CkHdr   ckHdr;
    LONG    offset;
    LONG    blockSize;
} SoundDataHdr;

typedef struct {
    CkHdr   ckHdr;
    AiffID  appSig;
/*    char    applData[0];      */
} ApplChunk;

/* SAXEL (sound accelerator) extension - for AIFC compressor state */
/* added dpwe 1993feb24 */
typedef struct {
	MarkerId	id;				/* accelerator data linked to this point */
	unsigned short	size;		/* size of saxelData */
/*	char	saxelData[0];	*/	/* algo-specific accelerator data */
} Saxel;

typedef struct {
	CkHdr	ckHdr;
	unsigned short	numSaxels;
/*	Saxel	saxels[0];		*/
} SaxelChunk;

/*
 * Form, CommonChunk, and SoundData lumped into one.
 */
typedef struct {       /* stick some together for sndfaif.c ease of writing */
    FormHdr	    form;
    CommonChunk     comm;
#ifndef SHORT_AIFFS 
    MarkerChunk	    mark;
    InstrumentChunk inst;	/* trying to cut down .. 1993apr25 */
#endif /* SHORT_AIFFS */
/*  ApplChunk       appl;       /* nice try .. but don't work */
    SoundDataHdr    ssnd;
} AIFFHdr;
