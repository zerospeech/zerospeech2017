/***************************************************************\
*   snd.c							*
*   A new soundfile interface layer				*
*   1993mar03  dpwe  after convo.c				*
\***************************************************************/

/* std libs */
#include "snd.h"
#include <stdlib.h>
#include <string.h>
/* both in libdpwe.h, itself in snd.h */
#include <assert.h>
/* personal libs */
#include <genutils.h>	/* for fSize(), NumericQ() and SubstExtn() */
#include <sndfutil.h>	/* for Convert* */
#include <audIO.h>	/* for real-time sound IO */

/* Make sure we use the pushfileio covers for fopen/fclose */
#define REDEFINE_FOPEN
#include "pushfileio.h"

/* default pattern for identifying compressed files (null terminated) */
const char* fnpats_gz_dft[] = {"*.gz", "*.Z", ""};
const char* fnpats_shn_dft[] = {"*.shn", "*.wv1", "*.wv2", ""};
const char* fnpats_mp3_dft[] = {"*.mp3", ""};
const char* fnpats_flac_dft[] = {"*.flac", ""};

/* private internal constants */
static const char** fnpats_gz = 0;
static const char** fnpats_shn = 0;
static const char** fnpats_mp3 = 0;
static const char** fnpats_flac = 0;

enum { /* for returns from sndGZopen */
    FNPAT_NONE = 0, FNPAT_GZ, FNPAT_SHN, FNPAT_MP3, FNPAT_FLAC
};


/* constants */
#define SND_BUFSZ	16384	/* frames held from disk */
#define SND_AUP_LEN	(sizeof(SND_AUDIO_PATH)/sizeof(char)-1)

/* typedefs */

/* static function prototype */
static void sndDerived(SOUND *snd);

/* defaults for open-write (new) file */

/* implementations */

static void sndDerived(SOUND *snd)
{	/* fill in derived slots based on SFSTRUCT data */
    if(snd->snd->dataBsize != SF_UNK_LEN)  {
	snd->frames = snd->snd->dataBsize / SFSampleFrameSize(snd->snd);
        snd->duration = (float)snd->frames / snd->snd->samplingRate;
    } else {
	snd->frames = SF_UNK_LEN;
	snd->duration = 0.0;
    }
}

SOUND *sndNew(SOUND *snd)
{	/* just allocate a new SOUND structure
	   Clone format parameters (chans, srate) from <snd> if any. */
    SOUND *rslt;

    rslt = TMMALLOC(SOUND, 1, "sndNew");
    rslt->path = NULL;
    rslt->file = NULL;
    rslt->mode = '\0';
    rslt->isStdio = FALSE;
    rslt->isRealtime = FALSE;
    rslt->isCmdPipe = FALSE;
    rslt->snd  = NULL;
    rslt->frames   = 0L;
    rslt->duration = 0.0;
    rslt->userFormat = -1;
    rslt->userChans  = -1;
    rslt->userGain   = 1.0;
    rslt->chanMode = SCMD_MONO;		/* if one channel, mix it */
    rslt->flags   = 0;
    rslt->isOpen  = 0;
    rslt->touchedCurrent = 0;
    rslt->defaultSFfmt = SFF_UNKNOWN;
    rslt->buf	  = NULL;
    rslt->bufSize = 0L;
    rslt->bufCont = 0L;
    rslt->sffmt	  = SFF_UNKNOWN;
    rslt->sffmtname = NULL;

    if(snd != NULL)  {
	SFAlloc(&(rslt->snd), 0L, snd->snd->format, snd->snd->samplingRate,
		snd->snd->channels, SFInfoSize(snd->snd));
	memcpy(rslt->snd->info, snd->snd->info, SFInfoSize(snd->snd));
	/* rslt->snd->dataBsize = snd->snd->dataBsize; */ /* leave as 0 */
	rslt->userFormat = snd->userFormat;
	rslt->userChans  = snd->userChans;
	if(snd->sffmtname) { rslt->sffmtname = strdup(snd->sffmtname); }
	rslt->sffmt = snd->sffmt;
    }
    else {		/* alloc default SF header */
	SFAlloc(&(rslt->snd), 0L, DFLT_FMT, (double)DFLT_SR, DFLT_CHNS, 0);
    }
    /* derived fields */
    sndDerived(rslt);

    rslt->data = NULL;	/* pointer to frames in memory , for dspB only */

    return rslt;
}

void sndClose(SOUND *snd)
{	/* close sound file attached to snd */
    int rc;
    DBGFPRINTF((stderr, "sndClose(0x%lx) mode=%c RT=%d CmdP=%d stdio=%d\n", 
		snd,snd->mode, snd->isRealtime, snd->isCmdPipe, snd->isStdio));
    if(snd->file)  {			/* file really is open */
	if(snd->isRealtime) {
	    int stop_immediately = 0;
	    AUClose((AUSTRUCT*)snd->file, stop_immediately);
	} else if (snd->isCmdPipe) {
	    /* we won't be able to rewrite the header, so don't bother */
	/*    if (snd->mode == 'r') {
	 * 	SFFlushToEof(snd->file, snd->snd->format);
	 *    }
         */
	    /* it's a fine principle to read to EOF, but it kills 
	       me for very long compressed files.  If I'm about to kill
	       the process, who cares? (1998jun02) */
#ifdef HAVE_POPEN
	    rc = pclose(snd->file);
	    /* what SFClose would do if it could */
	    SFClose_Finishup(snd->file);
	    DBGFPRINTF((stderr, "pclose(0x%lx)=%d\n", snd->file, rc));
#else /* !HAVE_POPEN */
	    SFClose(snd->file);
#endif /* HAVE_POPEN */
	} else if (snd->mode == 'w')  {	/* open for write - careful close */
	    if (snd->touchedCurrent) {
		DBGFPRINTF((stderr, "snd: FinishWrite in Close\n"));
		SFFinishWrite(snd->file, snd->snd);
	    }
	    SFClose(snd->file);
	} else {	/* reading a file */
	    /* make sure we move to the end of this file & close it off */
	    if(!snd->isStdio) {
		/* fclose(snd->file); */
		SFClose(snd->file);
	    } else {
		/* if it's stdin, can't close it; try consuming all the ip */
		SFFlushToEof(snd->file, snd->snd->format);
		/* you have to do this to be able to read multiple Abbot 
		   segments from stdin without missing them. 
		   This is needed to make feacalc pass one of the RASTA 
		   testsuite tests when reading abbot from stdin.  
		   Actual abbot files on disk don't work anyway, of 
		   course. */
		/* fclose(snd->file); */
	    }
	}
    }
    snd->file = NULL;
    snd->isOpen = 0;

    /* restore the default mode (guessing or forced) */
    SFSetDefaultFormat(snd->defaultSFfmt);

}

void sndFree(SOUND *snd)
{	/* destroy sound, including closing any file */
    if(snd->file != NULL)  {
	sndClose(snd);
    }
    if (snd->sffmtname != NULL) {
	free(snd->sffmtname);
    }
    if(snd->buf != NULL)  {
	free(snd->buf);
    }
    if(snd->snd != NULL)
	SFFree(snd->snd);
    free(snd);
}

void sndPrint(SOUND *snd, FILE *stream, const char *tag)
{   /* print a text description of a snd struct */
    fprintf(stream, "SOUND '%s'%s:\n", (tag==NULL)?"(noname)":tag, 
	    snd->isCmdPipe?" (compressed)":(snd->isStdio?" (stdio)":(snd->isRealtime?" (realtime)":"")));
    PrintSNDFinfo(snd->snd, snd->path, 
		  SFFormatName(SFGetFormat(snd->file, NULL)), stream);
}

void sndDebugPrint(SOUND *snd, FILE *stream, const char *tag)
{   /* print a text description of a snd struct */
    fprintf(stream, "SOUND '%s':\n", (tag==NULL)?"(noname)":tag);
    fprintf(stream, "path='%s', file=%lx, mode=%c\n", 
	    (snd->path==NULL)?"(null)":snd->path, (unsigned long)snd->file, snd->mode);
    fprintf(stream, "isStdio=%d, isRealTime=%d\n", 
	    snd->isStdio, snd->isRealtime);
    fprintf(stream, "srate=%.0f, chans=%d, format=0x%x, dataBsize=%ld\n", 
	    snd->snd->samplingRate, snd->snd->channels, snd->snd->format, 
	    snd->snd->dataBsize);
    fprintf(stream, "frames=%ld, duration=%.3f s, uformat=%d, uchans=%d gain=%f\n", 
	    snd->frames, snd->duration, snd->userFormat, snd->userChans, snd->userGain);
    fprintf(stream, "buf=%lx, bufSize=%ld, bufCont=%ld\n", 
	    (unsigned long)snd->buf, snd->bufSize, snd->bufCont);
}

static AUSTRUCT *sndAuOpenWrite(SOUND *snd)
{   /* open audio per set values */
    AUSTRUCT *au = NULL;
    double srate = snd->snd->samplingRate;
    int    chans = snd->snd->channels;
    int    format= snd->snd->format;
    int    bufsize = srate/4;
    const char     *nums = "0123456789.";
    const char     *mypath = snd->path+SND_AUP_LEN;
    char   c;
    int    err = 0;

    while(c = *mypath++)  {
	switch(c)  {
	case 'B':
	case 'b':
	    bufsize = atoi(mypath);   mypath += strspn(mypath, nums);
	    if(bufsize == 0)  {
		fprintf(stderr, "sndAuOpenW: bad rate in %s\n", snd->path);
		err = 1;
	    }
	default:
	    fprintf(stderr, "sndAuOpenW: unknown key '%c' in %s\n", 
		    c, snd->path);
	    break;
	}
    }
    if(err == 0)  {
	au = AUOpen(format, srate, chans, bufsize, AIO_OUTPUT);
    }
    return au;
}

static AUSTRUCT *sndAuOpenRead(SOUND *snd)
{   /* parse path name, open audio */
    AUSTRUCT *au = NULL;
    double   srate = 22050;	/* default */
    int      rate  = 0;
    int      chans = 1;
    long     bufsize = srate/4;
    long     totframes = -1;
    float    durn = -1.0;
    char     c;
    int      err = 0;
    const char     *nums = "0123456789.";
    const char     *mypath = snd->path+SND_AUP_LEN;

    /* `path' is of form "R22C1B5000" 
       to specify sampling rate (kHz), chans and frames of buffer. 
       R is sampling rate
       C is num channels
       B is buffer size (output too)
       L is number of frames, or
       D is duration in seconds 
       */

    while(c = *mypath++)  {
	switch(c)  {
	case 'R':
	case 'r':
	    rate = atoi(mypath);  mypath += strspn(mypath, nums);
	    if(rate==44)	srate = 44100;
	    else if(rate==22)	srate = 22050;
	    else if(rate==11)	srate = 11025;
	    else srate = 1000*rate;
	    if(srate == 0)  {
		fprintf(stderr, "sndAuOpen: bad rate in %s\n", snd->path);
		err = 1;
	    }
	    break;
	case 'D':
	case 'd':
	    durn = atof(mypath);    mypath += strspn(mypath, nums);
	    if(durn == 0)  {
		fprintf(stderr, "sndAuOpen: bad duration in %s\n", snd->path);
		err = 1;
	    }
	    break;
	case 'L':
	case 'l':
	    totframes = atol(mypath);    mypath += strspn(mypath, nums);
	    if(totframes == 0)  {
		fprintf(stderr, "sndAuOpen: bad length in %s\n", snd->path);
		err = 1;
	    }
	    break;
	case 'C':
	case 'c':
	    chans = atoi(mypath);   mypath += strspn(mypath, nums);
	    if(chans == 0)  {
		fprintf(stderr, "sndAuOpen: bad chans in %s\n", snd->path);
		err = 1;
	    }
	    break;
	case 'B':
	case 'b':
	    bufsize = atoi(mypath);   mypath += strspn(mypath, nums);
	    if(bufsize == 0)  {
		fprintf(stderr, "sndAuOpen: bad bufsz in %s\n", snd->path);
		err = 1;
	    }
	    break;
	default:
	    fprintf(stderr, "sndAuOpen: unknown key '%c' in %s\n", 
		    c, snd->path);
	    break;
	}
    }
    if(err == 0)  {
	au = AUOpen(AFMT_SHORT, srate, chans, bufsize, AIO_INPUT);
	if(au) {
	    if(snd->snd)
		SFFree(snd->snd);    /* free the old SFSTRUCT prior to new */
	    SFAlloc(&snd->snd, 0, SFMT_SHORT, srate, chans, 0);	    
	    if(durn > 0)  {
		totframes = durn * srate;
	    }
	    if(totframes > 0)
		snd->snd->dataBsize = SFSampleFrameSize(snd->snd) * totframes;
	    else 
		snd->snd->dataBsize = SF_UNK_LEN;
	}
    }
    return au;
}

static SOUND *sndOpenRTAudio(SOUND *snd, const char *mode)
{   /* open a sound for read or write, but as a pipe to hardware */
    char *newname;
    AUSTRUCT *au;

    /* filename is validly prefixed by filename that indicates it 
       should be used to open a real-time audio input */
    snd->isRealtime = TRUE;
    if(*mode == 'r')  {
	if( (au =sndAuOpenRead(snd))==NULL){
	    fprintf(stderr, "sndOpen: RT audio in fail %s\n", snd->path);
	    /* sndFree(snd); */
	    return NULL;
	}
	newname = "(realtime input)";
	snd->mode = 'r';
    } else {
	if( (au =sndAuOpenWrite(snd))==NULL){
	    fprintf(stderr, "sndOpen: RT audio out fail %s\n", snd->path);
	    /* sndFree(snd); */
	    return NULL;
	}
	newname = "(realtime output)";
	snd->mode = 'w';
    }
    sndDerived(snd);
    TMFREE(snd->path, "sndOpen rewrite path:free");
    snd->path = TMMALLOC(char, strlen(newname)+1, "sndOpen path rewrite");
    strcpy(snd->path, newname);
    snd->file = (FILE *)au;	/* hack, for now */
    return snd;
}

static int globmatch(const char *pat, const char *str)
{   /* cheap version of glob pattern matching - just handles *.xxx */
    int rc = 0;

    if (pat[0] == '*' && strpbrk(pat+1, "*?[{") == NULL) {
	int plen = strlen(pat+1);
	int slen = strlen(str);
	if (plen <= slen && strcmp(pat+1, str+slen-plen) == 0) {
	    rc = 1;
	}
    } else {
	fprintf(stderr, "globmatch called with unsupported pattern '%s'\n", 
		pat);
	abort();
    }
    return rc;
}

// strndup is defined in <string>
// #ifndef HAVE_STRNDUP
// char *strndup(const char *s, size_t n)
// {	/* needed for Darwin? */
//     char *t = NULL;
// 
//     if(s)  {
// 	t = malloc(n+1);
// 	strncpy(t, s, n);
// 	t[n] = '\0';
// 	}
//     return t;
// }
// #endif /* HAVE_STRNDUP */

void sndUpdateFnPats(void)
{   /* maybe update the fnpats_ structures dynamically from the 
       environment. */
    int i, n, c;
    char *env, *s, *t, *nt;
    if (fnpats_shn == NULL) {
	env = getenv("SND_PATS_SHN");
	if (env == NULL) {
	    fnpats_shn = fnpats_shn_dft;
	} else {
#define MAX_FN_PATS 16
	    fnpats_shn = (const char**)malloc(MAX_FN_PATS * sizeof(char*));
	    t = env;
	    s = t;
	    c = 0;
	    for (i = 0; i < MAX_FN_PATS-1 && s != NULL; ++i) {
		s = strchr(t, ':');
		if (s == NULL) {
		    n = strlen(t);
		    nt = NULL;
		} else {
		    n = s - t;
		    nt = s+1;
		}
		if (n > 0) {
		    fnpats_shn[c++] = strndup((const char *)t,(size_t)n);
		}
		t = nt;
	    }
	    fnpats_shn[c++] = strdup("");
	}
	env = getenv("SND_PATS_GZ");
	if (env == NULL) {
	    fnpats_gz = fnpats_gz_dft;
	} else {	
	    fnpats_gz = (const char**)malloc(MAX_FN_PATS * sizeof(char*));
	    t = env;
	    s = t;
	    c = 0;
	    for (i = 0; i < MAX_FN_PATS-1 && s != NULL; ++i) {
		s = strchr(t, ':');
		if (s == NULL) {
		    n = strlen(t);
		    nt = NULL;
		} else {
		    n = s - t;
		    nt = s+1;
		}
		if (n > 0) {
		    fnpats_gz[c++] = strndup(t,n);
		}
		t = nt;
	    }    
	    fnpats_gz[c++] = strdup("");
	}
	env = getenv("SND_PATS_MP3");
	if (env == NULL) {
	    fnpats_mp3 = fnpats_mp3_dft;
	} else {	
	    fnpats_mp3 = (const char**)malloc(MAX_FN_PATS * sizeof(char*));
	    t = env;
	    s = t;
	    c = 0;
	    for (i = 0; i < MAX_FN_PATS-1 && s != NULL; ++i) {
		s = strchr(t, ':');
		if (s == NULL) {
		    n = strlen(t);
		    nt = NULL;
		} else {
		    n = s - t;
		    nt = s+1;
		}
		if (n > 0) {
		    fnpats_mp3[c++] = strndup(t,n);
		}
		t = nt;
	    }    
	    fnpats_mp3[c++] = strdup("");
	}
	env = getenv("SND_PATS_FLAC");
	if (env == NULL) {
	    fnpats_flac = fnpats_flac_dft;
	} else {	
	    fnpats_flac = (const char**)malloc(MAX_FN_PATS * sizeof(char*));
	    t = env;
	    s = t;
	    c = 0;
	    for (i = 0; i < MAX_FN_PATS-1 && s != NULL; ++i) {
		s = strchr(t, ':');
		if (s == NULL) {
		    n = strlen(t);
		    nt = NULL;
		} else {
		    n = s - t;
		    nt = s+1;
		}
		if (n > 0) {
		    fnpats_flac[c++] = strndup(t,n);
		}
		t = nt;
	    }    
	    fnpats_flac[c++] = strdup("");
	}
	/*
	for (i = 0; i < MAX_FN_PATS && strlen(fnpats_shn[i]); ++i) {
	    fprintf(stderr, "fnpats_shn[%d]='%s'\n", i, fnpats_shn[i]);
	}
	for (i = 0; i < MAX_FN_PATS && strlen(fnpats_gz[i]); ++i) {
	    fprintf(stderr, "fnpats_gz[%d]='%s'\n", i, fnpats_gz[i]);
	}
	*/
    }
}

static int sndIsGzipName(const char *path)
{   /* does this filename look like a compressed file? */
    /* empty string ("") terminates list of compressed file patterns */
    int i = 0; 

    sndUpdateFnPats();
    
    while(strlen(fnpats_gz[i])) {
	if (globmatch(fnpats_gz[i], path)) {
	    return FNPAT_GZ;
	}
	++i;
    }
    /* test for shorten too */
    i = 0; 
    while(strlen(fnpats_shn[i])) {
	if (globmatch(fnpats_shn[i], path)) {
	    return FNPAT_SHN;
	}
	++i;
    }
    /* test for MP3 too */
    i = 0; 
    while(strlen(fnpats_mp3[i])) {
	if (globmatch(fnpats_mp3[i], path)) {
	    return FNPAT_MP3;
	}
	++i;
    }
    /* test for FLAC too */
    i = 0; 
    while(strlen(fnpats_flac[i])) {
	if (globmatch(fnpats_flac[i], path)) {
	    return FNPAT_FLAC;
	}
	++i;
    }
    return 0;
}

/* from ~dpwe/projects/rasta/feacalc/ExecUtils.C */

#ifdef HAVE_POPEN

#define MAXPATHLEN 1024

static int exuSearchExecPaths(const char *cmd, const char *paths, char *rslt, int rsltlen)
{   /* Search through the WS or colon-separated list of paths
       for an executable file whose tail is cmd.  Return absolute 
       path in rslt, which is allocated at rsltlen.  Return 1 
       if executable found & path successfully returned, else 0. */
    char testpath[MAXPATHLEN];
    int cmdlen = strlen(cmd);
    
    /* zero-len testpath means not found, so preset it that way */
    testpath[0] = '\0';
    /* If the cmd contains a slash, don't search paths */
    if (strchr(cmd, '/') != NULL) {
	/* if it *starts* with a "/", treat it as absolute, else prepend cwd */
	if(cmd[0] == '/') {
	    assert(cmdlen+1 < MAXPATHLEN);
	    strcpy(testpath, cmd);
	} else {
	    testpath[0] = '\0';
	    getcwd(testpath, MAXPATHLEN);
	    assert(strlen(testpath) > 0 \
		   && strlen(testpath)+cmdlen+2 < MAXPATHLEN);
	    strcat(testpath, "/");
	    strcat(testpath, cmd);
	}    
	/* See if this path is executable */
	/* fprintf(stderr, "testing '%s'...\n", testpath); */
	if(access(testpath, X_OK) != 0) {
	    /* exec access failed, fall through with zero-len */
	    testpath[0] = '\0';
	}
    } else {  /* no slash in cmd, search the path components */
	/* Loop through the pieces of <paths> trying each one */
	int plen;
	while(strlen(paths)) {
	    /* Skip over current separator (or excess WS) */
	    paths += strspn(paths, ": \t\r\n");
	    /* extract next path */
	    plen = strcspn(paths, ": \t\r\n");
	    if(plen) {
		assert(plen+cmdlen+2 < MAXPATHLEN);
		/* copy the path part */
		strncpy(testpath, paths, plen);
		/* shift on so next path is grabbed next time */
		paths += plen;
		/* build the rest of the path */
		testpath[plen] = '/';
		strcpy(testpath+plen+1, cmd);
		/* fprintf(stderr, "testing '%s'...\n", testpath); */
		/* see if it's there */
                if(access(testpath, X_OK) != 0) {
	            /* exec access failed, fall through with zero-len */
		    testpath[0] = '\0';
		} else {
		    /* found the ex, drop out' the loop */
		    break;
		}
	    }
	}
    }
    /* Now we either have a working path in testpath, or we found nothing */
    if(strlen(testpath) == 0) {
	return 0;
    } else {
	if(rslt) {	/* we have a string to copy to */
	    if(strlen(testpath)+1 < rsltlen) {
		strcpy(rslt, testpath);
	    } else {
		fprintf(stderr, "SearchExec: rsltlen (%d) too small for %ld\n", rsltlen, (long) strlen(testpath));
		return 0;
	    }
	}
	return 1;
    }
}
#endif /* HAVE_POPEN */

static FILE *sndGZopen(const char *path, const char *mode, SOUND *snd, int which)
{   /* popen a gzip to or from the named file.
       Need snd passed for format-dependent options to shorten. */
#ifndef HAVE_POPEN
	/* no exec, so cannot do it - print error */
	fprintf(stderr, "sndGZopen: no online compression for this architecture\n");
	return NULL;
#else /* HAVE_POPEN */
    char cmdpath[512];
    char *srchpath = getenv("PATH");
    int maxcplen = 512 - strlen(path) - 2;
    FILE *rslt;
    int i = 0;

    sndUpdateFnPats();
    
    if (mode[0] == 'r') {
	/* do we want shorten? */
	if (which == FNPAT_SHN) {
	    if (exuSearchExecPaths("shorten", srchpath, cmdpath, maxcplen)) {
		/* add decompress options */
		strcat(cmdpath, " -x");
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find shorten (r)\n");
		return NULL;
	    }
	    {    /* Maybe use shorten options from the environment */
		char *shortropts = getenv("SHORTEN_ROPTS");
		if (shortropts) {
		    strcat(cmdpath, " ");
		    strcat(cmdpath, shortropts);
		}
	    }
	} else if (which == FNPAT_MP3) {
	    if (exuSearchExecPaths("mpg123", srchpath, cmdpath, maxcplen)) {
		/* add decompress options */
		strcat(cmdpath, " -q -s"); /* raw samples out */
		/* strcat(cmdpath, " -q -w -"); /* MSWAVE output */
		/* without ability to rewind, mpg123 writes a default MSWAVE header that is just a default 44kHz stereo - no help! */
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find mpg123 (r)\n");
		return NULL;
	    }
	    {    /* Maybe use shorten options from the environment */
		char *shortropts = getenv("MP3_ROPTS");
		if (shortropts) {
		    strcat(cmdpath, " ");
		    strcat(cmdpath, shortropts);
		}
	    }
	} else if (which == FNPAT_FLAC) {
	    if (exuSearchExecPaths("flac", srchpath, cmdpath, maxcplen)) {
		/* add decompress options - silent, stdout, decode */
		/* strcat(cmdpath, " -s -c -d --force-raw-format --sign=signed --endian="); */
		strcat(cmdpath, " -s -c -d");
		/* actually, writing out a WAV file as a stream works fine */
#ifdef WORDS_BIGENDIAN
		/* strcat(cmdpath, "big"); */
#else /* !WORDS_BIGENDIAN */
		/* 		strcat(cmdpath, "little"); */
		/* strcat(cmdpath, "big"); */
		/* default PCMFORMAT is endian big */
#endif /* WORDS_BIGENDIAN */
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find flac (r)\n");
		return NULL;
	    }
	    {    /* Maybe use shorten options from the environment */
		char *shortropts = getenv("FLAC_ROPTS");
		if (shortropts) {
		    strcat(cmdpath, " ");
		    strcat(cmdpath, shortropts);
		}
	    }
	} else {
	    /* try to gzcat or gunzip -c the file */
	    if (exuSearchExecPaths("gzcat", srchpath, cmdpath, maxcplen)) {
		/* cmdpath is now fine */
	    } else if (exuSearchExecPaths("gunzip", srchpath, cmdpath, maxcplen)) {
		/* needs to be gunzip -c */
		strcat(cmdpath, " -c");
	    } else if (exuSearchExecPaths("gzip", srchpath, cmdpath, maxcplen)) {
		/* needs to be gzip -d -c */
		strcat(cmdpath, " -d -c");
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find gzip/gunzip/gzcat\n");
		return NULL;
	    }
	}
	/* append file name to gzcat command */
	strcat(cmdpath, " ");
	{
	    char mystr[2];
	    mystr[0] = '"';
	    mystr[1] = '\0';
	    strcat(cmdpath, mystr);
	    strcat(cmdpath, path);
	    strcat(cmdpath, mystr);
	}
	if (which == FNPAT_SHN) {
	    /* add "-" as the second arg to get shorten to write to stdout */
	    strcat(cmdpath, " -");
	}
    } else if (mode[0] == 'w') {
	if (which == FNPAT_SHN) {
	    if (exuSearchExecPaths("shorten", srchpath, cmdpath, maxcplen)) {
		/* add compress options */
		char opts[32];
		char *shortwopts = getenv("SHORTEN_WOPTS");
		sprintf(opts, " -c %d", sndGetChans(snd));
		strcat(cmdpath, opts);
		/* Maybe use shorten options from the environment */
		if (shortwopts) {
		    strcat(cmdpath, " ");
		    strcat(cmdpath, shortwopts);
		}
		strcat(cmdpath, " -t s16hl - ");
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find shorten (w)\n");
		return NULL;
	    }
	} else if (which == FNPAT_MP3) {
	    if (exuSearchExecPaths("lame", srchpath, cmdpath, maxcplen)) {
		/* add compress options */
		char opts[32];
		char *shortwopts = getenv("MP3_WOPTS");
#ifdef WORDS_BIGENDIAN
		/* strcat(cmdpath, "big"); */
#else /* !WORDS_BIGENDIAN */
		strcat(cmdpath, " -x");
#endif /* WORDS_BIGENDIAN */
		sprintf(opts, " --quiet -r -m %c", (sndGetChans(snd)==1)?'m':'s');
		strcat(cmdpath, opts);
		sprintf(opts, " -s %.1f", sndGetSrate(snd)/1000);
		strcat(cmdpath, opts);
		/* Maybe use shorten options from the environment */
		if (shortwopts) {
		    strcat(cmdpath, " ");
		    strcat(cmdpath, shortwopts);
		}
		strcat(cmdpath, " - ");
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find lame (w)\n");
		return NULL;
	    }
	} else if (which == FNPAT_FLAC) {
	    if (exuSearchExecPaths("flac", srchpath, cmdpath, maxcplen)) {
		/* add compress options */
		char opts[128];
		char *shortwopts = getenv("FLAC_WOPTS");
		sprintf(opts, " --silent --force-raw-format --channels=%d --sign=signed --bps=16  --sample-rate=%.0f --endian=", sndGetChans(snd), sndGetSrate(snd));
		strcat(cmdpath, opts);
#ifdef WORDS_BIGENDIAN
		/* emitted samples are indeed in machine order... */
		strcat(cmdpath, "big");
#else /* !WORDS_BIGENDIAN */
		strcat(cmdpath, "little");
#endif /* WORDS_BIGENDIAN */
		/* Maybe use options from the environment */
		if (shortwopts) {
		    strcat(cmdpath, " ");
		    strcat(cmdpath, shortwopts);
		}
		/* make flac write to stdout */
		strcat(cmdpath, " - > ");
		fprintf(stderr, "%s\n", cmdpath);
	    } else {
		fprintf(stderr, "sndGZopen: couldn't find flac (w)\n");
		return NULL;
	    }
	} else {
	    /* writing to a compressed file - only gzip will do */
	    if (!exuSearchExecPaths("gzip", srchpath, cmdpath, maxcplen)) {
		fprintf(stderr, "sndGZopen: couldn't find gzip\n");
		return NULL;
	    }
	    strcat(cmdpath, " - > ");
	}
	{
	    char mystr[2];
	    mystr[0] = '"';
	    mystr[1] = '\0';
	    strcat(cmdpath, mystr);
	    strcat(cmdpath, path);
	    strcat(cmdpath, mystr);
	}
	/* make sure the file doesn't exist */
	unlink(path);
    }
    /* debug */
    DBGFPRINTF((stderr, "sndGZopen: abt to popen('%s','%s')...\n", cmdpath, mode));
    rslt = popen(cmdpath, mode);
    DBGFPRINTF((stderr, "popen('%s','%s')=0x%lx\n", cmdpath, mode, rslt));
    return rslt;
#endif /* HAVE_POPEN */
}

static SOUND *sndOpenFileRead(SOUND *snd)
{   /* Finish off opening a soundfile with its own path */
    int gztype;
    if(snd->isStdio)  {
	snd->file = stdin;
    } else if ( (gztype=sndIsGzipName(snd->path)) != FNPAT_NONE ) {
	if( (snd->file = sndGZopen(snd->path, "r", snd, gztype)) == NULL)  {
	    fprintf(stderr, "sndOpen: Cannot open '%s' for decompress/read\n", 
		    snd->path);
	    /* sndFree(snd); */
	    return NULL;
	}
	snd->isCmdPipe = TRUE;	
    } else {
	if( (snd->file = fopen(snd->path, "rb")) == NULL)  {
	    fprintf(stderr, "sndOpen: Cannot open '%s' for read!\n", 
		    snd->path);
	    /* sndFree(snd); */
	    return NULL;
	}
    }
    /* record the current _sndf_type format, so we can restore it */
    snd->defaultSFfmt = SFGetDefaultFormat();

    if(snd->snd)
	SFFree(snd->snd);	/* free the old SFSTRUCT prior to new */

    if( SFAllocRdHdr(snd->file, &(snd->snd)) )  {
	fprintf(stderr, "sndOpen: '%s' is not a soundfile\n", snd->path);
	/* sndFree(snd); */
	return NULL;
    }
    /* fill in derived fields */
    sndDerived(snd);
    snd->mode = 'r';
    /* .. including recording the actual format of this file, even if 
       we guessed it */
    snd->sffmt = SFGetFormat(snd->file, NULL);
    /* make sure that an immediate call to sndNext will be OK */
    snd->touchedCurrent = 0;

    return snd;
}

static SOUND *sndOpenFileWrite(SOUND *snd)
{   /* Finish off opening a soundfile with its own path */
    int gztype;
    if(snd->isStdio)  {
	snd->file = stdout;
    } else if ( (gztype = sndIsGzipName(snd->path)) != FNPAT_NONE) {
	if( (snd->file = sndGZopen(snd->path, "w", snd, gztype)) == NULL)  {
	    fprintf(stderr, "sndOpen: Cannot open '%s' for compress/write\n", 
		    snd->path);
	    /* sndFree(snd); */
	    return NULL;
	}
	snd->isCmdPipe = TRUE;
    } else {
	if( (snd->file = fopen(snd->path, "w+b")) == NULL)  {
	    fprintf(stderr, "sndOpen: Cannot open '%s' for write\n", 
		    snd->path);
	    /* sndFree(snd); */
	    return NULL;
	}
    }
    SFSetFname(snd->file, snd->path);
    if( SFWriteHdr(snd->file, snd->snd, 0) )  {
	fprintf(stderr, "sndOpen: Error writing to  '%s'\n",snd->path);
	/* sndFree(snd); */
	return NULL;
    }
    snd->mode = 'w';
    snd->touchedCurrent = 1;
    return snd;
}

SOUND *sndOpen(SOUND *snd, const char *path, const char *mode)
{   /* open sound. If <snd> is NULL, allocate new one.
	If <path> is NULL, take from snd as passed in (or err).
	<mode> is "r" or "w" only */
    int allocd = 0;

#ifdef DEBUG
    if(snd!=NULL) {
	sndPrint(snd, stderr, "sndOpen: entry snd");
    }
#endif /* DEBUG */

    if( !(*mode == 'r' || *mode == 'w')) {
	fprintf(stderr, "sndOpen: mode '%s' not 'r' or 'w'\n", mode);
	return (SOUND *)NULL;
    }
    if(path == NULL &&
       (snd == NULL || snd->path == NULL || strlen(snd->path) == 0))  {
	/* must either specify explicit path, or have path present
	   in snd structure passed in. */
	fprintf(stderr, "sndOpen: path = '%s', snd = 0x%lx, no path\n",
		path, (unsigned long)snd);
	return (SOUND *)NULL;
    }
    if(snd == NULL)  {
	snd = sndNew(NULL);
	allocd = 1;
    }
    if(path != NULL) {
	/* passed path takes priority */
	snd->path = TMMALLOC(char, strlen(path)+1, "sndOpen path");
	strcpy(snd->path, path);
    }
    /* now have a snd structure, and its path field is the true path 
       we want to access */
    if(strncmp(snd->path, SND_AUDIO_PATH, SND_AUP_LEN) == 0)  {
	snd = sndOpenRTAudio(snd, mode);
    } else {
	int istty = 0;
	if(strcmp(snd->path, SND_STDIO_PATH)==0)  { /* magic name for stdin */ 
	    char *newname;

	    snd->isStdio = TRUE;
	    if(*mode == 'r') {
#ifdef HAVE_ISATTY
		istty = isatty(fileno(stdin));
#endif /* HAVE_ISATTY */
		newname = "(stdin)";
	    } else {
#ifdef HAVE_ISATTY
		istty = isatty(fileno(stdout));
#endif /* HAVE_ISATTY */
		newname = "(stdout)";
	    }
	    TMFREE(snd->path, "sndOpen rewrite path:free");
	    snd->path = TMMALLOC(char, strlen(newname)+1, 
				 "sndOpen path rewrite");
	    strcpy(snd->path, newname);
	}
	if(istty) {
	    fprintf(stderr, "sndOpen: I refuse to %s a tty\n", 
		    *mode == 'r'?"read from":"write to");
	    /* sndFree(snd); */
	    goto errorexit;
	} else {
	    /* maybe set up the sffmt before doing the open */
	    if (snd->sffmtname != NULL) {
		SFSetDefaultFormatByName(snd->sffmtname);
	    } else if (*mode == 'r' || snd->sffmt != SFF_UNKNOWN) {
		/* If we *haven't* specified a format, on read this 
		   will set up the default format of SFF_UNKNOWN, which 
		   means guess the format */
		SFSetDefaultFormat(snd->sffmt);
	    }
	    if(*mode == 'r')  {
		snd=sndOpenFileRead(snd);
	    } else if(*mode == 'w')  {
		snd=sndOpenFileWrite(snd);
	    }
	    /* unset any defaults for soundfile */
	    SFSetDefaultFormat(SFF_UNKNOWN);
	}
    }
    if(snd)  {
	/* 'snd' now valid, ready for return */
	if(snd->buf == NULL)  {	/* allocate a buffer to read from disk */
	    snd->bufSize = SND_BUFSZ; /* space for this many bytes */
	    snd->buf= TMMALLOC(short, snd->bufSize, 
			       "sndOpen:buf");
	    /* CCKR assert can't handle double-quotes in the expression */
	    assert(snd->buf);
	    snd->bufCont = 0;	/* .. but empty at present */
	    snd->isOpen = 1;
	}
	if(snd->userFormat == -1)
	    snd->userFormat = SFMT_FLOAT; /* by default ..? */
    /* leave the userChans at -1 - then sndGetUchans returns *actual* chans */
    }
#ifdef DEBUG 
    if(snd) {
	fprintf(stderr, "sndOpen(%lx,%s,%s):dz %ld, fr %ld, ch %d, fmt %d\n", 
		snd, path, mode, snd->snd->dataBsize, 
		snd->frames, snd->snd->channels, snd->snd->format);
	sndPrint(snd, stderr, "sndOpen: exit snd");
    } else {
	fprintf(stderr, "sndOpen(0x%lx,%s,%s)->NULL\n", snd, path, mode);
    }
#endif /* DEBUG */

    return snd;
 errorexit:
    if(allocd&1)	sndFree(snd);
    return (SOUND *)NULL;
}

int sndNext(SOUND *snd)
{   /* returns 1 if there is another soundfile to read in the 
       currently open file.  Normally returns 1 before any accesses 
       to the current soundfile, and 0 afterwards, but for multi-file 
       streams, may return 1 several times; each time it is called, 
       subsequent accesses are for the next soundfile. 
       On write, indicates the end of one file, and that another is 
       likely to follow. */
    int rc = 0;

    if (snd->isOpen && ! snd->isRealtime) {
	if (snd->mode == 'r') {
	    /* We can only do this with open, read files */
	    if (snd->touchedCurrent == 0) {
		/* The currently lined-up file hasn't been read at all, 
		   so just say it's OK to use - but don't do it more than 1ce! */
		rc = 1;
	    } else {
		/* Skip to the end of any current file, and see if there's 
		   another behind it in the same file/stream */
		int mode, fmt;
		SFSTRUCT *tmpsnd;
		
		SFFlushToEof(snd->file, snd->snd->format);
		
		/* .. but also maybe reset our explicit format */
		if (snd->sffmtname != NULL) {
		    SFSetDefaultFormatByName(snd->sffmtname);
		} else if (snd->sffmt != SFF_UNKNOWN) {
		    SFSetDefaultFormat(snd->sffmt);
		} else {
		    /* restore the default mode (guessing or forced) */
		    SFSetDefaultFormat(snd->defaultSFfmt);
		}

		if( SFAllocRdHdr(snd->file, &tmpsnd) == SFE_OK ) {
		    /* succeeded in reading another header from this stream */
		    if(snd->snd)
			SFFree(snd->snd);  /* free the old SFSTRUCT prior to new */
		    snd->snd = tmpsnd;
		    /* fill in derived fields */
		    sndDerived(snd);
		    rc = 1;
		}
		/* whatever happened, make sure nxt time we move to nxt file */
		snd->touchedCurrent = 1;
	    }
	} else { /* assume snd->mode == 'w' i.e. checkpointing */
	    if (snd->touchedCurrent) {
		/* tie up that file */
		DBGFPRINTF((stderr, "snd: FinishWrite in Next\n"));
		SFFinishWrite(snd->file, snd->snd);
		/* start up the next file */
		snd->snd->dataBsize = SF_UNK_LEN;
		sndDerived(snd);
		snd->touchedCurrent = 0;
	    }
	 /*   SFWriteHdr(snd->file, snd->snd, 0);
	    snd->touchedCurrent = 1; */
	    rc = 1;
	}
    }
    return rc;
}

long snduConvertChans(void *inbuf, void *outbuf, int inchans, int outchans, 
		      int format, int mode,long frames)
{   /* convert the number of channels in a sound of arbitrary format */
    int bps = SFSampleSizeOf(format);

    if(inchans == outchans)  {
	if(inbuf == outbuf)	return frames;
	else  {
	    memcpy(outbuf, inbuf, frames*inchans*bps);
	    return frames;
	}
    }
    if(mode == SCMD_MONO) {
	if(format == SFMT_SHORT)  {
	    ConvertChannels(inbuf, outbuf, inchans, outchans, frames);
	} else if(format == SFMT_FLOAT) {
	    ConvertFlChans(inbuf, outbuf, inchans, outchans, frames);
	} else {
	    fprintf(stderr, "snduCnvChans: cannot convert %d to %d chans for data format %d\n", inchans, outchans, format);
	    return 0;
	}
    } else {  /* if not SCMD_MONO, must be selecting a channel */
	int choosechan = mode - SCMD_CHAN0;
	
	if(choosechan > MAX(inchans, outchans)-1)  {
	    fprintf(stderr, "snduCnvChans: wierd SCMD %d\n", mode);
	    return 0;
	}
	if(format == SFMT_SHORT)  {
	    short *sptr = ((short *)inbuf)+(choosechan%inchans);
	    short *dptr = ((short *)outbuf)+(choosechan%outchans);
	    long cnt = frames;
	    int j;
	    if (outchans == 1) {
		while(cnt--)  {
		    *dptr++ = *sptr;
		    sptr += inchans;
		}
	    } else {
		while(cnt--)  {
		    *dptr++ = *sptr;
		    for (j = outchans - 1; j > 0; --j) {
			*dptr ++ = 0;
			}
		    sptr += inchans;
		}
	    }
	} else if(format == SFMT_FLOAT)  {
	    float *sptr = ((float *)inbuf)+(choosechan%inchans);
	    float *dptr = ((float *)outbuf)+(choosechan%outchans);
	    long cnt = frames;
	    int j;
	    if (outchans == 1) {
		while(cnt--)  {
		    *dptr++ = *sptr;
		    sptr += inchans;
		}
	    } else {
		while(cnt--)  {
		    *dptr++ = *sptr;
		    for (j = outchans - 1; j > 0; --j) {
			*dptr ++ = 0;
			}
		    sptr += inchans;
		}
	    }
	} else { 
	    fprintf(stderr, "snduCnvChans: cannot convt format %d\n", format);
	    return 0;
	}
    }	    
    return frames;
}

long snduConvertChansFmt(void *inbuf, void *outbuf, int inchans, int outchans, 
			 int chanmode, float gain, 
			 int infmt, int outfmt, long frames)
{   /* convert the data format and the number of channels for a buffer 
       of sound.  Use smallest possible buffer space */
    int ibs = SFSampleSizeOf(infmt);
    int obs = SFSampleSizeOf(outfmt);
    void *txbuf, *tobuf;
    static void *tmpBuf = NULL;
    static int tmpBufSize = 0;
    int reqBufSize = 0;
    int txfmt;

    /* Track applied gain */
    float totalgain = 1.0;

    /* New strategy : first convert to output format, then combine 
       channels.  However, if output format is not short or float, 
       convert to short or float, do combination, then convert back. */

    /* Choose intermediate format */
    if (infmt == SFMT_SHORT || infmt == SFMT_FLOAT) {
	txfmt = infmt;
    } else if (obs >= 4) {
	txfmt = SFMT_FLOAT;
    } else {
	txfmt = SFMT_SHORT;
    }

    /* Make sure we have enough buffer space */
    reqBufSize = SFSampleSizeOf(txfmt) * MAX(inchans, outchans) * frames;
    if (tmpBuf == NULL || tmpBufSize < reqBufSize) {
	if (tmpBuf != NULL)	TMFREE(tmpBuf, "snduCnvCF");
	tmpBuf = TMMALLOC(char, reqBufSize, "snduCnvCF");
	tmpBufSize = reqBufSize;
    }
    
    /* first, convert to intermediate format (if necessary) */
    if (infmt != txfmt) {
	ConvertSampleBlockFormat(inbuf, tmpBuf, infmt, txfmt, frames*inchans);
	txbuf = tmpBuf;
    } else if (gain != 1.0) {
	if (txfmt == SFMT_SHORT) {
	    short *psi = inbuf;
	    short *pso = tmpBuf;
	    long todo = frames*inchans;
	    while (todo-- > 0) {
		*pso++ = gain * *psi++;
	    }
	} else {
	    float *pfi = inbuf;
	    float *pfo = tmpBuf;
	    long todo = frames*inchans;
	    assert(txfmt == SFMT_FLOAT);
	    while (todo-- > 0) {
		*pfo++ = gain * *pfi++;
	    }
	}
	txbuf = tmpBuf;
	totalgain *= gain;
    } else {
	txbuf = inbuf;
    }
    /* apply any needed gain */
    if (totalgain != gain) {
	float newgain = gain/totalgain;
	if (txfmt == SFMT_SHORT) {
	    short *ps = txbuf;
	    long todo = frames*inchans;
	    while (todo-- > 0) {
		*ps++ *= newgain;
	    }
	} else {
	    float *pf = txbuf;
	    long todo = frames*inchans;
	    assert(txfmt == SFMT_FLOAT);
	    while (todo-- > 0) {
		*pf++ *= newgain;
	    }
	}
	totalgain *= newgain;
    }

    /* will we need output format conversion? */
    if (outfmt != txfmt) {
	tobuf = tmpBuf;
    } else {
	/* Can write channel conversion straight into output buffer */
	tobuf = outbuf;
    }

    /* Convert channels */
    snduConvertChans(txbuf, tobuf, inchans, outchans,txfmt, chanmode, frames); 
    
    /* Maybe convert to output format */
    if (outfmt != txfmt) {
	ConvertSampleBlockFormat(tobuf, outbuf, txfmt, outfmt,frames*outchans);
    }

    return frames;
}

static long sndReadRTFrames(SOUND *snd, void *buf, long frames)
{   /* read <frames> sample frames from RT device.
       return number actually read. */
    long framesToDo = frames;
    long framesThisTime, framesRead;
    long samples, smpsRead, totalFramesRead = 0;
    int  channels = snd->snd->channels;
    int  uchans   = sndGetUchans(snd);
    /* only half-fill the buffer if we're doubling the number of channels */
    int  ubps = SFSampleSizeOf(snd->userFormat);
    int  bps = SFSampleSizeOf(snd->snd->format);
    int  maxbufframes = snd->bufSize / MAX(uchans*ubps, channels*bps);
    AUSTRUCT *au = (AUSTRUCT *)snd->file;
    int df;

    {
      assert(snd->bufSize>0);
      while(framesToDo)  {
	framesThisTime = MIN(framesToDo, maxbufframes);
	framesRead = AURead(au, snd->buf, framesThisTime);
#define REPORTDROPS
#ifdef REPORTDROPS
	if((df=AUDroppedFrames(au))>0)  {
	    fprintf(stderr, "sndRdRT:%d frames dropped\n", df);
	}
#endif /* REPORTDROPS */
	totalFramesRead += framesRead;
	snduConvertChansFmt(snd->buf, buf, channels, uchans, snd->chanMode, 
			    snd->userGain, snd->snd->format, snd->userFormat, 
			    framesRead);
	if(framesRead < framesThisTime)	
	    framesToDo = 0;		/* we hit EOF */
	else			
	    framesToDo -= framesThisTime;
	buf = ((char *)buf) + framesRead * ubps * uchans;
      }
    }
    return totalFramesRead;
}

static long sndReadFileFrames(SOUND *snd, void *buf, long frames)
{   /* read <frames> sample frames as floats into the buffer,
       return number actually read. */
    long framesToDo = frames;
    long framesThisTime, framesRead;
    long samples, smpsRead, totalFramesRead = 0;
    int  channels = snd->snd->channels;
    int  uchans   = sndGetUchans(snd);
    /* only half-fill the buffer if we're doubling the number of channels */
    int  ubps = SFSampleSizeOf(snd->userFormat);
    int  bps = SFSampleSizeOf(snd->snd->format);
    int  maxbufframes = snd->bufSize / MAX(uchans*ubps, channels*bps);

#ifdef DEBUG
    if(snd->flags == 0)  {
	fprintf(stderr, "sndRF: %d fr; fmt,ch %d,%d; ufmt,uch %d,%d\n", 
		frames, snd->snd->format, snd->snd->channels, 
		sndGetUformat(snd), sndGetUchans(snd));
	snd->flags = 1;
    }
#endif /* DEBUG */

    if(snd->snd->format == snd->userFormat && channels == uchans && snd->userGain == 1.0)  {
      totalFramesRead = SFfread(buf, snd->snd->format, frames*channels, 
				snd->file)/channels;
    } else {
      assert(snd->bufSize>0);
      while(framesToDo)  {
	framesThisTime = MIN(framesToDo, maxbufframes);
	samples = framesThisTime * channels;
	smpsRead = SFfread(snd->buf, snd->snd->format, samples, snd->file);
	framesRead = smpsRead/channels;
	totalFramesRead += framesRead;
	snduConvertChansFmt(snd->buf, buf, channels, uchans, snd->chanMode, 
			    snd->userGain, snd->snd->format, snd->userFormat, 
			    framesRead);
	if(smpsRead < samples)	framesToDo = 0;		/* we hit EOF */
	else			framesToDo -= framesThisTime;
/* was:	buf = ((char *)buf) + smpsRead * ubps * uchans; */
	buf = ((char *)buf) + framesRead * ubps * uchans;
      }
    }

    /* Now that we've read from this, any future sndNext's will 
       advance us to any following sound */
    snd->touchedCurrent = 1;

    return totalFramesRead;
}

long sndReadFrames(SOUND *snd, void *buf, long frames)
{   /* read <frames> sample frames as floats into the buffer,
       return number actually read. */
    if(snd->isRealtime)  {
	return sndReadRTFrames(snd, buf, frames);
    } else {
	return sndReadFileFrames(snd, buf, frames);
    }
}

static long sndWriteFileFrames(SOUND *snd, void *buf, long frames)
{	/* write <frames> sample frames (floats) to file from the buffer,
	   return number actually written. */
    long framesToDo = frames;
    long framesThisTime;
    long osamples, smpsWrit, totalFramesWrit = 0;
    int  channels = snd->snd->channels;
    int  uchans   = sndGetUchans(snd);
    int  ubps = SFSampleSizeOf(snd->userFormat);
    int  bps = SFSampleSizeOf(snd->snd->format);
    int  maxbufframes = snd->bufSize / MAX(uchans*ubps, channels*bps);

#ifdef DEBUG
    if(snd->flags == 0)  {
	fprintf(stderr, "sndWF: %d fr; fmt,ch %d,%d; ufmt,uch %d,%d\n", 
		frames, snd->snd->format, snd->snd->channels, 
		sndGetUformat(snd), sndGetUchans(snd));
	/* snd->flags = 1; */
    }
#endif /* DEBUG */

    /* do deferred header write (for multi-file streams) */
    if ( !snd->touchedCurrent ) {
	/* .. but also maybe reset our explicit format */
	if (snd->sffmtname != NULL) {
	    SFSetDefaultFormatByName(snd->sffmtname);
	} else if (snd->sffmt != SFF_UNKNOWN) {
	    SFSetDefaultFormat(snd->sffmt);
	} else {
	    /* restore the default mode (guessing or forced) */
	    SFSetDefaultFormat(snd->defaultSFfmt);
	}

	if( SFWriteHdr(snd->file, snd->snd, 0) )  {
	    fprintf(stderr, "sndWrite: Error writing hdr  to  '%s'\n",snd->path);
	    return 0;
	}
	snd->touchedCurrent = 1;
    }

    assert(snd->bufSize>0);
    while(framesToDo)  {
	framesThisTime = MIN(framesToDo, maxbufframes);
	snduConvertChansFmt(buf, snd->buf, uchans, channels, snd->chanMode,
			    snd->userGain, snd->userFormat, snd->snd->format, 
			    framesThisTime);
	buf = ((char *)buf) + framesThisTime * uchans * ubps;
	osamples = framesThisTime * channels;
	smpsWrit = SFfwrite(snd->buf, snd->snd->format, osamples, snd->file);
	totalFramesWrit += smpsWrit/channels;
	if(smpsWrit < osamples)	framesToDo = 0;		/* disk full? */
	else			framesToDo -= framesThisTime;
    }
    return totalFramesWrit;
}

static long sndWriteRTFrames(SOUND *snd, void *buf, long frames)
{	/* write <frames> sample frames (floats) to devaudio from the buffer,
	   return number actually written. */
    long framesToDo = frames;
    long framesThisTime;
    long frmsWrit, totalFramesWrit = 0;
    int  channels = snd->snd->channels;
    int  uchans   = sndGetUchans(snd);
    int  ubps = SFSampleSizeOf(snd->userFormat);
    int  bps = SFSampleSizeOf(snd->snd->format);
    int  maxbufframes = snd->bufSize / MAX(uchans*ubps, channels*bps);
    AUSTRUCT *au = (AUSTRUCT *)snd->file;

    assert(snd->bufSize>0);
    while(framesToDo)  {
	framesThisTime = MIN(framesToDo, maxbufframes);
	snduConvertChansFmt(buf, snd->buf, uchans, channels, snd->chanMode, 
			    snd->userGain, snd->userFormat, snd->snd->format, 
			    framesThisTime);
	buf = ((char *)buf) + framesThisTime * uchans * ubps;
	frmsWrit = AUWrite(au, snd->buf, framesThisTime);
	totalFramesWrit += frmsWrit;
	if(frmsWrit < framesThisTime)	
	    framesToDo = 0;		/* disk full? */
	else
	    framesToDo -= framesThisTime;
    }
    return totalFramesWrit;
}

long sndWriteFrames(SOUND *snd, void *buf, long frames)
{	/* write <frames> sample frames (floats) to file from the buffer,
	   return number actually written. */
    if(snd->isRealtime)  {
	return sndWriteRTFrames(snd, buf, frames);
    } else {
	return sndWriteFileFrames(snd, buf, frames);
    }
}

/* accessor / settor functions */

int sndSetChans(SOUND *snd, int chans)
{
    int oldchans = snd->snd->channels;

    snd->snd->channels = chans;
    return oldchans;
}

int sndGetChans(SOUND *snd)
{
    return snd->snd->channels;
}

int sndSetChanmode(SOUND *snd, int mode)
{
    int oldmode = snd->chanMode;

    snd->chanMode = mode;
    return oldmode;
}

int sndGetChanmode(SOUND *snd)
{
    return snd->chanMode;
}

int sndSetFormat(SOUND *snd, int format)
{
    int oldformat = snd->snd->format;

    snd->snd->format = format;
    /* If the user format is not yet set, set it now too */
    if(snd->userFormat == -1)
	snd->userFormat = format;
    return oldformat;
}

int sndGetFormat(SOUND *snd)
{
    return snd->snd->format;
}

int sndSetUformat(SOUND *snd, int format)
{
    int oldformat = snd->userFormat;

    snd->userFormat = format;
    return oldformat;
}

int sndGetUformat(SOUND *snd)
{
    if(snd->userFormat == -1)
	return snd->snd->format;
    return snd->userFormat;
}

int sndSetUchans(SOUND *snd, int chans)
{
    int oldchans = snd->userChans;

    snd->userChans = chans;
    return oldchans;
}

int sndGetUchans(SOUND *snd)
{
    if(snd->userChans == -1)
	return snd->snd->channels;
    return snd->userChans;
}

float sndSetUgain(SOUND *snd, float gain)
{
    float oldgain = snd->userGain;

    snd->userGain = gain;
    return oldgain;
}

float sndGetUgain(SOUND *snd)
{
    return snd->userGain;
}

long sndSetFrames(SOUND *snd, long frames)
{
    long oldframes = snd->frames;

    snd->frames = frames;	/* what *does* this mean? */
    /* keep snd->snd fields consistent */
    snd->snd->dataBsize = SFSampleFrameSize(snd->snd) * snd->frames;
    snd->duration = (float)snd->frames / snd->snd->samplingRate;
    return oldframes;
}

long sndGetFrames(SOUND *snd)
{
    return snd->frames;
}

double sndSetSrate(SOUND *snd, double srate)
{	
    double oldsrate = snd->snd->samplingRate;

    snd->snd->samplingRate = srate;
    return oldsrate;
}

double sndGetSrate(SOUND *snd)
{
    return snd->snd->samplingRate;
}

long sndFrameTell (SOUND *snd)
{   /* return frame index of next sample frame to be read or written */
    /* Mark this file as being somewhat used */
    snd->touchedCurrent = 1;
    return SFftell(snd->file)/SFSampleFrameSize(snd->snd);
}

long sndFrameSeek(SOUND *snd, long frameoffset, int whence)
{   /* seek to a certain point in the file.  Args as fseek(3s) */
    /* Mark this file as being somewhat used */
    snd->touchedCurrent = 1;
    return SFfseek(snd->file, frameoffset*SFSampleFrameSize(snd->snd), whence);
}

int sndFeof(SOUND *snd)
{   /* non-zero if we have hit eof on reading a soundfile */
    /* Mark this file as being somewhat used */
    snd->touchedCurrent = 1;
    return SFfeof(snd->file);
}

int sndBytesPerFrame(SOUND *snd)
{
    return sndGetUchans(snd) * SFSampleSizeOf(sndGetUformat(snd));
}

/* 1997jan31: new run-time adaptive soundfile formats */
int sndSetSFformat(SOUND *snd, int fmt)
{   /* Set the soundfile format for this file */
    int mode; /* byteswap mode */
    int id = SFGetFormat(snd->file, &mode);
    /* Mark this file as being somewhat used */
    /* snd->touchedCurrent = 1; */
/*    SFSetFormat(snd->file, fmt, mode); */
    if (snd->isOpen) {
	fprintf(stderr, "sndSetSFformat: you cannot set the format of an open file\n");
	abort();
    } else {
	/* record the request to use when we open the file */
	snd->sffmt = fmt;
    }

    return id;
}

int sndSetSFformatByName(SOUND *snd, const char *name)
{   /* Set the soundfile format for this file by name 
       To be like SFSetDefaultFmtByName, return 0 if name is bad, else 0 */
    int rc = 1;
    /* Mark this file as being somewhat used */
    /* snd->touchedCurrent = 1; */
/*     SFSetFormatByName(snd->file, name);  */
    if (snd->isOpen) {
	fprintf(stderr, "sndSetSFformatByName: you cannot set the format of an open file\n");
	abort();
    } else {
	/* record the request to use when we open the file */
	if (snd->sffmtname) {
	    free(snd->sffmtname);
	    snd->sffmtname = NULL;
	}
	if (name != NULL) {
	    snd->sffmtname = strdup(name);
	}
    }
    DBGFPRINTF((stderr, "sndSetSFFbyname snd=0x%lx name='%s' (0x%lx=%s)\n", snd, name, snd->sffmtname, snd->sffmtname));

    if (SFFormatToID(name) == -1)	rc = 0;

    return rc;
}

int sndGetSFformat(SOUND *snd)
{   /* return the sound file format to be used with the file */
    /* Mark this file as being somewhat used */
    /*    snd->touchedCurrent = 1; */
    return SFGetFormat(snd->file, NULL);
}
