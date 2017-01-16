/*
 * sndfbyptr.c
 *
 * Like sndf.c, but handles each file type dynamically at run time
 * with function pointers.
 *
 * 1997jan29 dpwe@icsi.berkeley.edu
 * $Header: /u/drspeech/repos/dpwelib/sndfbyptr.c,v 1.27 2011/03/09 01:35:03 davidj Exp $
 */

#include "dpwelib.h"
#include "genutils.h"	/* for strdup proto on non-strdup systems */
#include "sndf.h"
#include "sndfutil.h"

#define REDEFINE_FOPEN  /* fopen->my_fopen etc */
#include "pushfileio.h"	/* for my_fopen etc definitions */

typedef struct _sndf_ftype {
    /* default file extension (including ".") */
    char**	_sndfExtnPtr;
    /* function to read a header */
    int (*_readHdr)(FILE *file, SFSTRUCT *snd, int infoBmax);
    /* function to read extra (dynamically-sized) header information */
    int (*_readXInfo)(FILE *file, SFSTRUCT *snd, int done);
    /* function to write a header */
    int (*_writeHdr)(FILE *file, SFSTRUCT *snd, int infoBmax);
    /* returns the length in bytes of the header */
    long (*_hdrLen)(FILE *file);
    /* returns the length in bytes of the trailer (bytes after sound data) */
    long (*_trlrLen)(FILE *file);
    /* returns the offset of the last sample byte in the file */
    long (*_lastTell)(FILE *file);
    /* performs any necessary sample conversion (in-place) */
    long (*_fixSamps)(FILE *file, void *ptr, int fmt, long nitems, int rw);
    /* default description */
    int id;		/* one of the SFF_ codes */
    char *name;		/* ascii keyword */
} SNDF_FTYPE;

#define SNDF_FTYPE_DEFINED
#include "sndffname.c"

/* for each flagset, define the DFLT_SNDF_ID, which the 
   dflt_sndf_type is set to on startup (unless SNDFFTYPE overrides)
   and also re-redefine one of the 'hidden' include-file versions of 
   sndfExtn so that it resolves to the plain version, as used by 
   applications (hairy but works). */
#ifdef AIFF
#define DFLT_SNDF_ID SFF_AIFF
#define SF_AIF_sndfExtn	sndfExtn
#endif /* AIFF */
#ifdef NeXT
#define DFLT_SNDF_ID SFF_NEXT
#define SF_NXT_sndfExtn	sndfExtn
#endif /* NeXT */
#ifdef ICSI_NIST
#define DFLT_SNDF_ID SFF_NIST
#define SF_NST_sndfExtn	sndfExtn
#endif /* ICSI_NIST */
#ifdef PHONDAT
#define DFLT_SNDF_ID SFF_PHDAT
#define SF_PDT_sndfExtn	sndfExtn
#endif /* PHONDAT */
#ifdef MS_WAVE
#define DFLT_SNDF_ID SFF_IRCAM
#define SF_WAV_sndfExtn	sndfExtn
#endif /* MS_WAVE */
#ifdef IRCAM
#define DFLT_SNDF_ID SFF_IRCAM
#define SF_IRC_sndfExtn sndfExtn
#endif /* IRCAM */
/* default case is PCM */
#ifndef DFLT_SNDF_ID
#define DFLT_SNDF_ID SFF_RAW
#define SF_PCM_sndfExtn sndfExtn
#endif /* DFLT_SNDF_ID */

#define FNPREFIX SF_IRC_
#include "sndfbyptr.h"
#include "sndfirc.c"
static SNDF_FTYPE sfft_irc = {
    &sndfExtn,
    SF_IRC_readHdr,
    SF_IRC_readXInfo,
    SF_IRC_writeHdr,
    SF_IRC_hdrLen, 
    SF_IRC_trlrLen, 
    SF_IRC_lastTell,
    SF_IRC_fixSamps,
    SFF_IRCAM,
    "IRCAM"
};

#undef FNPREFIX
#define FNPREFIX SF_NXT_
#include "sndfbyptr.h"
#include "sndfnxt.c"
static SNDF_FTYPE sfft_nxt = {
    &sndfExtn,
    SF_NXT_readHdr,
    SF_NXT_readXInfo,
    SF_NXT_writeHdr, 
    SF_NXT_hdrLen, 
    SF_NXT_trlrLen, 
    SF_NXT_lastTell, 
    SF_NXT_fixSamps,
    SFF_NEXT,
    "NeXT"
};

#undef FNPREFIX
#define FNPREFIX SF_AIF_
#include "sndfbyptr.h"
#include "sndfaif.c"
static SNDF_FTYPE sfft_aif = {
    &sndfExtn,
    SF_AIF_readHdr,
    SF_AIF_readXInfo,
    SF_AIF_writeHdr, 
    SF_AIF_hdrLen, 
    SF_AIF_trlrLen, 
    SF_AIF_lastTell, 
    SF_AIF_fixSamps, 
    SFF_AIFF, 
    "AIFF"
};

#undef FNPREFIX
#define FNPREFIX SF_WAV_
#include "sndfbyptr.h"
#include "sndfwav.c"
static SNDF_FTYPE sfft_wav = {
    &sndfExtn,
    SF_WAV_readHdr,
    SF_WAV_readXInfo,
    SF_WAV_writeHdr, 
    SF_WAV_hdrLen, 
    SF_WAV_trlrLen, 
    SF_WAV_lastTell, 
    SF_WAV_fixSamps,
    SFF_MSWAVE, 
    "MSWAVE"
};

#undef FNPREFIX
#define FNPREFIX SF_NST_
#include "sndfbyptr.h"
#include "sndfnist.c"
static SNDF_FTYPE sfft_nst = {
    &sndfExtn,
    SF_NST_readHdr,
    SF_NST_readXInfo,
    SF_NST_writeHdr, 
    SF_NST_hdrLen, 
    SF_NST_trlrLen, 
    SF_NST_lastTell, 
    SF_NST_fixSamps,
    SFF_NIST, 
    "NIST"
};

#undef FNPREFIX
#define FNPREFIX SF_ESP_
#include "sndfbyptr.h"
#include "sndfesps.c"
static SNDF_FTYPE sfft_esp = {
    &sndfExtn,
    SF_ESP_readHdr,
    SF_ESP_readXInfo,
    SF_ESP_writeHdr, 
    SF_ESP_hdrLen, 
    SF_ESP_trlrLen, 
    SF_ESP_lastTell, 
    SF_ESP_fixSamps,
    SFF_ESPS, 
    "ESPS"
};

#undef FNPREFIX
#define FNPREFIX SF_PDT_
#include "sndfbyptr.h"
#include "sndfphdat.c"
static SNDF_FTYPE sfft_pdt = {
    &sndfExtn,
    SF_PDT_readHdr,
    SF_PDT_readXInfo,
    SF_PDT_writeHdr, 
    SF_PDT_hdrLen, 
    SF_PDT_trlrLen, 
    SF_PDT_lastTell, 
    SF_PDT_fixSamps,
    SFF_PHDAT, 
    "PHONDAT"
};

#undef FNPREFIX
#define FNPREFIX SF_STR_
#include "sndfbyptr.h"
#include "sndfstrut.c"
static SNDF_FTYPE sfft_str = {
    &sndfExtn,
    SF_STR_readHdr,
    SF_STR_readXInfo,
    SF_STR_writeHdr, 
    SF_STR_hdrLen, 
    SF_STR_trlrLen, 
    SF_STR_lastTell, 
    SF_STR_fixSamps,
    SFF_STRUT, 
    "STRUT"
};

#undef FNPREFIX
#define FNPREFIX SF_PCM_
#include "sndfbyptr.h"
#include "sndfpcm.c"
static SNDF_FTYPE sfft_pcm = {
    &sndfExtn,
    SF_PCM_readHdr,
    SF_PCM_readXInfo,
    SF_PCM_writeHdr, 
    SF_PCM_hdrLen, 
    SF_PCM_trlrLen, 
    SF_PCM_lastTell, 
    SF_PCM_fixSamps, 
    SFF_RAW, 
    "PCM"
};

static SNDF_FTYPE* sndf_ftypes[] = {
    &sfft_irc,
    &sfft_nxt, 
    &sfft_aif,
    &sfft_wav,
    &sfft_nst,
    &sfft_esp,
    &sfft_pdt,
    &sfft_str,
    &sfft_pcm
};

static SNDF_FTYPE* dflt_sndf_type = NULL; /* &DFLT_SNDF; */
static SNDF_FTYPE* _sndf_type = NULL;
/* char *sndfExtn = *DFLT_SNDF._sndfExtnPtr); */
/* char *sndfExtn = ".NONE"; */
/* set by #define of sndfExtn to particular default SF_*_sndfExtn above */
/* (horribly convoluted but seems to work) */

/* extra format info string */
static char *_sndf_type_info = NULL;

/* Provide actual hooks for programs that call these directly */
#undef sndfExtn
#undef SFReadHdr
#undef SFRdXInfo
#undef SFWriteHdr
#undef SFHeDaLen
#undef SFTrailerLen
#undef SFLastSampTell
#undef FixupSamples

static SNDF_FTYPE *SFFindFormat(int id);

static void SF_SetupDefaultType(void) {
    /* a one off - set the default type from an environment variable, 
       if it's set.  Else use the compile-time flag. */
    char *s;
    if( (s = getenv(SFF_DFLT_ENV)) != NULL) {
	SFSetDefaultFormatByName(s);
    }
    if(dflt_sndf_type == NULL) {
	SFSetDefaultFormat(DFLT_SNDF_ID);
	assert(dflt_sndf_type);		/* DFLT_SNDF_ID must be valid */
    }
    DBGFPRINTF((stderr, "SF_SetupDflt: $%s = %s\n", SFF_DFLT_ENV, (s==NULL)?"(null)":s));
}

void SF_Setuptype(FILE *file)
{   /* setup the global fn ptr for the format registered with this file */
    SNDF_FTYPE *sfft = GetSFFType(file);
    if (sfft != NULL) {
	if(_sndf_type == sfft)	return;
	else _sndf_type = sfft;
    }
    if (_sndf_type == NULL) {
	if (!dflt_sndf_type)	SF_SetupDefaultType();
	_sndf_type = dflt_sndf_type;
    }
    if(sfft == NULL) {	/* there was no type before */
	SetSFFType(file, _sndf_type);
	DBGFPRINTF((stderr, "SF_Setuptype: Set type for file 0x%lx to %s\n", file, _sndf_type->name));
    }
    sndfExtn = *(_sndf_type->_sndfExtnPtr);
    DBGFPRINTF((stderr, "SF_Setuptype: file 0x%lx set to %s (pcmflg=%d)\n", file, _sndf_type->name, pcmDefaults?pcmDefaults->miscflags:-1));
}

int SFReadHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
{
    SF_Setuptype(file);
    return _sndf_type->_readHdr(file, snd, infoBmax);
}

int SFRdXInfo(FILE *file, SFSTRUCT *snd, int done)
{
    SF_Setuptype(file);
    return _sndf_type->_readXInfo(file, snd, done);
}

int SFWriteHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
{
    DBGFPRINTF((stderr, "byptr:SFWrHd for 0x%lx\n", file));
    SF_Setuptype(file);
    return _sndf_type->_writeHdr(file, snd, infoBmax);
}

#undef FNPREFIX
#define FNPREFIX _sndf_type->_
#include "sndfbyptr.h"
/* Run inside SFOpen type calls */
#define MAYBE_ID(file,err)	\
    err = SFE_OK;							\
    if(GetSFFType(file) == NULL) {					\
	if(_sndf_type != NULL) {					\
	    SetSFFType(file, _sndf_type);	 			\
	} else {							\
            int _sfft, _mode;						\
	    if( (_sfft = SFIdentify(file, &_mode)) >= 0) {		\
	        SetSFFType(file, SFFindFormat(_sfft));			\
	        SetBytemode(file, _mode);				\
	    } else if (_sfft == SFF_ZLEN) {				\
		err = SFE_EOF;						\
	    } else { /* unrecognized, use default */			\
	        if (!dflt_sndf_type)	SF_SetupDefaultType();		\
	        SetSFFType(file, dflt_sndf_type);			\
	    }								\
        }								\
    }  									\
    if (err == SFE_OK)  SF_Setuptype(file);				\
    DBGFPRINTF((stderr, "MAYBE_ID: file 0x%lx = %s (rc=%d)\n", file, _sndf_type->name, err));

/* for writing - check that we have some kind of ftype defined */
#define CHECK_FT	\
     if(_sndf_type == NULL) {						\
	 if (!dflt_sndf_type)	SF_SetupDefaultType();			\
	 _sndf_type = dflt_sndf_type;					\
     }

#undef sndfExtn

#define SNDF_PTRSTYLE
#include "sndf.c"

/* Function to choose soundfile format */
int SFFormatToID(const char *name)
{   /* Return the soundfile format ID correspoding to this name; 
       -1 if not recognized (==SNDF_UNKNOWN) */
    int nfmts = sizeof(sndf_ftypes)/sizeof(SNDF_FTYPE*);
    int i;
    char *slashpos;

    if (name) {
        char tmpname[32];
	/* make a copy of the string in a buffer that we can modify */
	strcpy(tmpname, name);
	/* clear our record of format name with a following slash */
	if (_sndf_type_info != NULL)	{ 
	    free(_sndf_type_info);
	    _sndf_type_info = NULL;	/* ..don't free it again 2000-02-02 */
	}
	/* separate out any part following a slash */
	slashpos = strchr(tmpname, '/');
	/* if there was one, temporarily end the string there */
	if(slashpos) {
	    *slashpos = '\0';
	    /* .. and record what follows it in the format string */
	    _sndf_type_info = strdup(slashpos+1);
	}
	for (i = 0; i<nfmts; ++i) {
	    if (strcmp(tmpname, sndf_ftypes[i]->name) == 0) {
		return sndf_ftypes[i]->id;
	    }
	}
	if(slashpos)	*slashpos = '/';
    }
    return SFF_UNKNOWN;
}

/* Function to choose soundfile format */
int SFSetDefaultFormatByName(char *name)
{   /* Set the default soundfile format for future operations 
       according to the one that matches the extension?
       Return 1 on success, 0 if no such format is known. */
    int rc = 0;
    int id = SFFormatToID(name);

    DBGFPRINTF((stderr, "SFSetDfltByName: %s\n", name));

    if(id != SFF_UNKNOWN) {
	/* install the new type */
	SFSetDefaultFormat(id);
	rc = 1;
	/* At this point, if the format was set to PCM and there was 
	   some post-slash part, initialize the PCM defaults with it. */
	if (id == SFF_RAW && _sndf_type_info != NULL) {
	    pcmReadDefaultString(_sndf_type_info, &pcmDefaults);
	}
    }
    return rc;
}

static SNDF_FTYPE *SFFindFormat(int id)
{   /* Find an SFFMT by its ID; return it */
    int nfmts = sizeof(sndf_ftypes)/sizeof(SNDF_FTYPE*);
    int i;
    SNDF_FTYPE *rslt = NULL;

    for (i = 0; i<nfmts; ++i) {
	if (sndf_ftypes[i]->id == id) {
	    rslt = sndf_ftypes[i];
	    return rslt;
	}
    }
    return NULL;
}

int SFSetDefaultFormat(int id)
{   /* Set the default soundfile format according to its id */
    _sndf_type = SFFindFormat(id);
    if(_sndf_type != NULL) {
	dflt_sndf_type = _sndf_type;
	sndfExtn = *(_sndf_type->_sndfExtnPtr);
	if(id == SFF_RAW) {
	    /* special case: read environment variable for PCM */
	    pcmReadDefaultString(getenv(PCM_ENV), &pcmDefaults);
	}
	return 1;
    } else {
	/* reset to default, so clear pcmDefaults) */
	pcmReadDefaultString("", &pcmDefaults);
	DBGFPRINTF((stderr, "SFSDF(%d): reread pcmD (flag=%d)\n", id, pcmDefaults->miscflags));
    }
    return 0;
}

int SFGetDefaultFormat(void)
{   /* return the current default format */
    int fmt = SFF_UNKNOWN;
    if (_sndf_type != NULL) {
	fmt = _sndf_type->id;
    }
    return fmt;
}

int SFSetFormat(FILE *file, int id, int bytemode)
{   /* Set the default soundfile format according to its id.
       Specifying SFF_UNKNOWN means take the default. */
    SNDF_FTYPE *my_sndf_type;

    if (id == SFF_UNKNOWN) {
	if (!dflt_sndf_type)	SF_SetupDefaultType();
	my_sndf_type = dflt_sndf_type;
    } else {
	my_sndf_type = SFFindFormat(id);
    }
    if(my_sndf_type != NULL && file != NULL) {
	SetSFFType(file, my_sndf_type);
	SetBytemode(file, bytemode);
	return 1;
    } 
    return 0;
}

int SFSetFormatByName(FILE *file, char *name)
{   /* Set the format for this file using a name. */
    SNDF_FTYPE *my_sndf_type;
    int id = SFFormatToID(name);
    int bytemode = BM_INORDER;
 
    DBGFPRINTF((stderr, "SFSetByName(0x%lx): %s\n", file, name));

    if (id == SFF_RAW && _sndf_type_info != NULL) {
	pcmReadDefaultString(_sndf_type_info, &pcmDefaults);
	bytemode = pcmDefaults->bytemode;
    }
    return SFSetFormat(file, id, bytemode);
}

int SFGetFormat(FILE *file, int *pmode)
{   /* return the soundfile format associated with the file, 
       or the default format if FILE is NULL or unknown */
    SNDF_FTYPE *ftp = GetSFFType(file);
    int mode = GetBytemode(file);
    if(pmode)	*pmode = mode;
    if(ftp)	return ftp->id;
    else if(_sndf_type != NULL) {
	return _sndf_type->id;
    } else {
	if (!dflt_sndf_type)	SF_SetupDefaultType();
	return dflt_sndf_type->id;
    }
}

void SFListFormats(FILE *stream)
{   /* print a list of all known formats to <stream> */
    int nfmts = sizeof(sndf_ftypes)/sizeof(SNDF_FTYPE*);
    int i;

    for (i = 0; i<nfmts; ++i) {
	fprintf(stream, "%s%s ", sndf_ftypes[i]->name, 
		(sndf_ftypes[i]->id == SFF_RAW)?"[/[R<rate>][C<chs>][F<fmt>][E<endian>][X<skip>][Abb]]":"");
    }
}

char *SFFormatName(int id) {
    /* descriptive name for this format */
    SNDF_FTYPE *sfft = SFFindFormat(id);
    if (sfft == NULL)
	return "<UNKNOWN>";
    else
	return sfft->name;
}

/* from sndcvt.c */
int SFIdentify(FILE *file, int *pmode)
{   /* Figure out the type of a soundfile.  Use *pmode to rtn byteswap mode */
    INT32	magique;
    INT32 	aiffMgc;
    INT32	riffMgc;
    INT32	nistMgc;
    INT32	strutMgc;
    char	*pc;
    int 	bm;
    
    /* tolerate pmode as NULL */
    if (!pmode)	pmode = &bm;

    DBGFPRINTF((stderr, "SFIdentify called.. "));
    /* fake up magic word for AIFFs */
    pc = (char *)&aiffMgc;
    *pc++ = 'F';	*pc++ = 'O';	*pc++ = 'R';	*pc++ = 'M';
    /* fake up magic word for WAVs */
    pc = (char *)&riffMgc;
    *pc++ = 'R';	*pc++ = 'I';	*pc++ = 'F';	*pc++ = 'F';
    /* & for NISTs */
    pc = (char *)&nistMgc;
    *pc++ = 'N';	*pc++ = 'I';	*pc++ = 'S';	*pc++ = 'T';
    /* for strut */
    pc = (char *)&strutMgc;
    *pc++ = 'S';	*pc++ = 'T';	*pc++ = 'R';	*pc++ = 'U';

    if( (fread(&magique, sizeof(INT32), 1, file)) != 1) {
	return(SFF_ZLEN);
    }
#ifdef HAVE_FPUSH
    /* put the words back on the stream */
    fpush(&magique, sizeof(INT32), 1, file);
#endif /* HAVE_FPUSH */
    DBGFPRINTF((stderr, "magique = 0x%lx\n", magique)); 
    if( (bm = MatchByteMode(magique, NXT_MAGIC)) != -1) { 
	*pmode = bm;	return(SFF_NEXT);
    }
    if( (bm = MatchByteMode(magique, IRC_MAGIC)) != -1) { 
	*pmode = bm;	return(SFF_IRCAM);
    }
    if( (bm = MatchByteMode(magique, aiffMgc)) != -1) { 
	*pmode = bm;	return(SFF_AIFF);
    }
    if( (bm = MatchByteMode(magique, riffMgc)) != -1) { 
	*pmode = bm;	return(SFF_MSWAVE);
    }
    if( (bm = MatchByteMode(magique, nistMgc)) != -1) { 
	*pmode = bm;	return(SFF_NIST);
    }
    if( (bm = MatchByteMode(magique, strutMgc)) != -1) { 
	*pmode = bm;	return(SFF_STRUT);
    }
    *pmode = BM_INORDER;
    return(SFF_UNKNOWN);
}

