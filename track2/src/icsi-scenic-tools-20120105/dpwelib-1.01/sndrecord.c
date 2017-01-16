/***************************************************************\
*	sndrecord.c    						*
*   Just read a sound into a file - predef len, srate		*
*   after Xt sndplay.c						*
*   1996jun19 dpwe@icsi.berkeley.edu after record.c 20nov90	*
* 1996nov19 dpwe@icsi.berkeley.edu  was called newrecord	*
\***************************************************************/

#include <dpwelib.h>
#include <stdlib.h>
#include <snd.h>
#include <sndfutil.h>
#include <audIO.h>
#include "cle.h"

#define RECBTIME  0.25		/* seconds of sound to buffer */
#define DEF_SRATE SNDF_DFLT_SRATE
#define DEF_CHNS  SNDF_DFLT_CHANS
#define DEF_FMT	  SNDF_DFLT_FORMAT
#define DEF_DURN  1.0
#define DEF_BUF   0
#define DEF_CHUNK 0

/* static data */
static	   float	durn;
static	   float	sRate;
static     int 		chans;
static	   long		chunksize;
static	   long		bufsize;
static     int 		verbose;
static     int          doMon;
static	   char 	*outName;
static	   char		*sffmt;

           char		*programName;

CLE_ENTRY clargs[] = {
    { "-d", CLE_T_FLOAT, "length in seconds to record", QUOTE(DEF_DURN), 
      &durn, 0,0},
    { "-r", CLE_T_FLOAT, "desired sampling rate in Hz", QUOTE(DEF_SRATE), 
      &sRate,0,0},
    { "-c", CLE_T_INT, "number of channels to record", QUOTE(DEF_CHNS), 
      &chans,0,0}, 
    { "-b", CLE_T_INT, "buffer size to request (frames)", QUOTE(DEF_BUF), 
      &bufsize,0,0},
    { "-k", CLE_T_INT, "chunk size for each transfer", QUOTE(DEF_CHUNK), 
      &chunksize,0,0},
    { "-m", CLE_T_BOOL, "monitor on output too", NULL, 
      &doMon, 0,0}, 
    { "-v", CLE_T_BOOL, "verbose status reporting", NULL, 
      &verbose, 0,0}, 
    { "-T", CLE_T_STRING, "output file format keyword", NULL, 
      &sffmt, 0,0}, 
    { "", CLE_T_STRING, "file to record to (default to stdout)", NULL, 
      &outName, 0,0},
    { NULL, CLE_T_END, NULL, NULL, NULL, NULL, NULL}
};

#define BUFTOT 2000
#define BUFMAX 1000000

#ifdef __sgi
#define POSIXSIGS
#endif /* __sgi */

/* POSIX signal handling for Ctl-C trap */
#include <signal.h>

#ifdef POSIXSIGS
/* typedef void (*sigfnptrtype)(int, ...); */
#define sigfnptrtype SIG_PF	/* defined in signal.h (irix 5.3) */
static struct sigaction origCCact;
#else /* !POSIXSIGS */
typedef void (*sigfnptrtype)(int);
static void (*origCCact)();
#endif /* POSIXSIGS */

static int origCCactSet = 0;	/* semaphore so enable can't be called 2ce */

static void disable_ctlc_trap(void)
{
    assert(origCCactSet);
#ifdef POSIXSIGS
    sigaction(SIGINT, &origCCact, NULL);
#else /* !POSIXSIGS */
    signal(SIGINT, origCCact);
#endif /* POSIXSIGS */
    origCCactSet = 0;
}

static SOUND *staticSnd = NULL;
static long staticTotal = 0;

static void abort_record(void)
{   /* Called on control-C to close the output file somewhat cleanly */
    disable_ctlc_trap();

    if(staticSnd) {
	sndClose(staticSnd);
	if(verbose) {
	    sndSetFrames(staticSnd, staticTotal);
	    sndPrint(staticSnd, stderr, "recorded");
	}
    }
    exit(1);
}

static void enable_ctlc_trap(void)
{
#ifdef POSIXSIGS
    struct sigaction new;
#endif /* POSIXSIGS */

    assert(!origCCactSet);
#ifdef POSIXSIGS
    new.sa_flags = 0;
    sigemptyset(&new.sa_mask);
    new.sa_handler = abort_record;
    sigaction(SIGINT, &new, &origCCact);
#else /* !POSIXSIGS */
    origCCact = signal(SIGINT, (sigfnptrtype)abort_record);
#endif /* POSIXSIGS */
    origCCactSet = 1;
}

static int RecordFile(char *filename, long bufsize, long chunksize, float dur, 
	       double srate, int chans, int format, int doMon);

static int RecordFile(char *filename, long bufsize, long chunksize, float dur, 
		      double srate, int chans, int format, int doMon)
{   /* Copy audio input to a file */
    SOUND	*snd = NULL;
    AUSTRUCT	*iport = NULL;
    AUSTRUCT	*oport = NULL;
    void 	*buf = NULL;
    long 	frames, remains, toget, got = -1, lastgot, opBuffer;
    int 	err = 0;

    if(bufsize == 0)	bufsize = BUFTOT;
    if(bufsize > BUFMAX)  bufsize = BUFMAX;
    if(chunksize <= 0 || chunksize > bufsize)	chunksize= bufsize/2;

    if(dur > 0) {
	frames = dur*srate;
    } else {
	frames = -1;
    }

    snd = sndNew(NULL);
    sndSetChans(snd, chans);
    sndSetFormat(snd, format);
    sndSetSrate(snd, srate);
    /* guess at the length too, in case we don't get to rewrite the hdr */
    sndSetFrames(snd, frames);

    if( (snd = sndOpen(snd, filename, "w")) == NULL)  {
	/* could not create sound file */
	err = -1;
    }
    if( err == 0 ) {
	iport = AUOpen(format,srate,chans, opBuffer = bufsize, AIO_INPUT);
	if(iport == NULL)
	    err = -2;
    }
    if( err == 0 ) {
	if( (buf = malloc(SFSampleSizeOf(format)*chans*bufsize)) == NULL)
	    err = -3;		/* could not allocate buffer */
    }
    if( err == 0 && doMon) {
	oport = AUOpen(format, srate, chans, bufsize, AIO_OUTPUT);
	if(oport == NULL)
	    err = -4;
    }
    if( err == 0 ) {
	/* record the snd structure for the ctl-c trap */
	staticSnd = snd;
	staticTotal = 0;
	enable_ctlc_trap();

	DBGFPRINTF((stderr, "buf=0x%lx\n", buf));

	/* actually copy the samples from input to file */
	if(frames > 0)
	    remains = frames;
	else
	    remains = -1;
	do {
	    if(remains > 0)
		toget = MIN(chunksize, remains);
	    else
		toget = chunksize;
	    lastgot = got;
	    got = AURead(iport, buf, toget);
	    sndWriteFrames(snd, buf, got);
	    if(doMon)
		AUWrite(oport, buf, got);
	    if(remains > 0)
		remains -= got;
	    staticTotal += got;
	} while(remains != 0 && (lastgot > 0 || got > 0));

	disable_ctlc_trap();
	if(verbose) {
	    sndSetFrames(staticSnd, staticTotal);
	    sndPrint(staticSnd, stdout, "recorded");
	    fprintf(stdout, "Dropped frames = %ld\n", AUDroppedFrames(iport));
	}
    }
    if(oport)	AUClose(oport,0);
    if(iport)	AUClose(iport,0);
    if(snd)	sndClose(snd);
    if(buf)	free(buf);
    return err;
}

#if defined(__MWERKS__)
#include <SIOUX.h>
extern short InstallConsole(short fd);
#include <console.h>
#endif /* MACINTOSH */

int
main(int argc, char **argv)
{
    int 	i;
    int 	err = 0;
    char	c;
    int		format= SFMT_SHORT;

#if defined(__MWERKS__)
    /* Set options for CodeWarrior SIOUX package */
    SIOUXSettings.autocloseonquit = false;
    SIOUXSettings.showstatusline = true;
    SIOUXSettings.asktosaveonclose = false;
    InstallConsole(0);
    SIOUXSetTitle("\psndrecord");

    argc = ccommand(&argv);
#endif /* macintosh */

    programName = argv[0];
    cleSetDefaults(clargs);
    err=cleHandleArgs(clargs, argc, argv);
    if(err == 0 && sffmt != NULL) {   /* user requested soundfile fmt */
	if(SFSetDefaultFormatByName(sffmt) == 0) {
	    fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
		    programName, sffmt);
	    SFListFormats(stderr);    fprintf(stderr, "\n");
	    err = 1;
	}
    }
    if(err) {
	cleUsage(clargs, programName);
	exit(-1);
    }

    if(outName == NULL)	{	/* no output name -> stdout */
	outName = SND_STDIO_PATH;  /* special name causes STDOUT in <snd.h> */
    }
    if(verbose)  {
	fprintf(stderr,"%s: %.3f secs at %5.2f Hz\n", programName, durn,sRate);
	fprintf(stderr,"%s: %d chans, bufsize %ld\n", programName, 
		chans, bufsize);
    }

    RecordFile(outName, bufsize, chunksize, durn, sRate, chans, 
	       SFMT_SHORT, doMon);
    exit(0);
}

