/***********************************************************************\
*	sndplay.c							*
*	Play a soundfile using generic interface librarys		*
* 	<snd.h> to read in sound files					*
* 	<audIO.h> to write to audio hardware				*
*	Intended to work on a variety of platforms/fileformats		*
*	dpwe 1993nov17 							*
* 1996nov19: dpwe@icsi.berkeley.edu  was called 'newplay'      		*
\***********************************************************************/

#ifdef __MWERKS__
#include <fp.h>	        /* needed for rint() under metrowerks IDE 2.0 */
#endif /* __MWERKS__ */

#include <dpwelib.h>	/* std includes, defs like stdio.h */
#include <stdlib.h>
#include <snd.h>	/* soundfile handling functions */
#include <audIO.h>	/* sound hardware handling functions */

#include <genutils.h>	/* for NumericQ */
#include <sndfutil.h>	/* for SFSampleSizeOf */

#ifdef __alpha
#define BUFTOT		10000	       	/* alpha loses on short bufs */
#else /* !__alpha */
#ifdef ultrix
#define BUFTOT 		       2501	/* half a second of 44k sound */
/* 1994apr07: I had BUFTOT up at 22050, but I couldn't play back 
   44k stereo on a DECstation without dropouts.  So I profiled it 
   in comparison to `old' play, which played fine.  The only material 
   difference I could find was that oldplay was using 10004 byte buffers 
   to read from disk.  So I cut this down such that stereo files would 
   use 10004 byte buffers; lo and behold, it plays without dropouts too.  
    I guess the point is that the very short buffers on the lofi card 
   make it crucial to spend only little moments away reading disk - 
   small nibbles, not big buffers. */
#else /* !ultrix */
/* Suns like 2000 frame buffers */
#define BUFTOT   2000
#endif /* ultrix */
#endif /* __alpha */
#define BUFMAX 1000000


static void usage(char *);
static int HandleClArgs(int argc, char **argv);
static int play_file(char *filename, long bufsize, long chunksize, 
	      float forcedSrate, int fchans, int skip, int maxframes, 
	      float gain,
	      float starttime, float durtime, float endtime, int loop);

#define MAXPLAYFILES 256

char *playfiles[MAXPLAYFILES];
int nplayfiles = 0;
double srate = -1.0;	/* negative means use rate of file */
int loop = FALSE;	/* loop-the-sound flag */
int verb = FALSE;	/* verbosity flag */
long bufsize = 0;	/* how big is the audio output buffer */
long chunksize = 0;	/* how many sample frames to transfer in a block */
int chans = 0;
int skip = 0;		/* frames to skip at front of file */
int maxframes = 0;	/* max count of frames to play */
float starttime = 0.0;	/* like skip, but in secs */
float durtime = 0.0;	/* like maxframes, but in secs */
float endtime = 0.0;	/* alternative way to specify duration */
float gain = 1.0;	/* amplify or whatever */
int dontplay = 0;	/* don't *actually* open output hardware or play */
char *sffmtname = NULL;	/* name of output soundfile format requested */
int sffmt = SFF_UNKNOWN;

int domulti = 0;        /* Attempt to read multiple sounds with sndNext() */

static int play_file(char *filename, long bufsize, long chunksize, 
	      float forcedSrate, int fchans, int skip, int maxframes, 
	      float gain, 
	      float starttime, float durtime, float endtime, int loop)
{	/* open the named file and play it */
    SOUND	*snd = NULL;
    AUSTRUCT	*port = NULL;
    void 	*buf = NULL;
    int 	chans, format, outformat = SFMT_SHORT;
    long 	frames, remains, toget, got, opBuffer, doneframes=0;
    double 	srate;
    int 	err = 0;
    int         firsttime = 1;

    if(bufsize == 0)	bufsize = BUFTOT;
    if(bufsize > BUFMAX)  bufsize = BUFMAX;
    if(chunksize <= 0 || chunksize > bufsize)	chunksize= bufsize/2;

    snd = sndNew(NULL);
    sndSetSFformatByName(snd, sffmtname);

    if( (snd = sndOpen(snd, filename, "r")) == NULL)  {
	/* could not open as sound file */
	err = -1;
    } else {
	/* sndNext is a flag that checks to see if there are 
	   more soundfiles on this single pipe */
	while (err == 0 && (firsttime || (domulti && sndNext(snd)))) {
	    firsttime = 0;
	    if(verb) {
		sndPrint(snd, stderr, "playing");
	    }
	    chans   = sndGetChans(snd);
	    format  = sndGetFormat(snd);
	    frames  = sndGetFrames(snd);
	    srate   = sndGetSrate(snd);
	    sndSetUformat(snd, outformat);	/* we want data as shorts */

	    if (starttime > 0) {
		skip = rint(starttime * srate);
	    }
	    if (endtime > 0 && endtime > starttime) {
		durtime = endtime - starttime;
	    }
	    if (durtime > 0) {
		maxframes = rint(durtime * srate);
	    }

	    if(skip) {
		sndFrameSeek(snd, skip, SEEK_SET);
	    }
	    if(maxframes > 0) {
		if (frames > maxframes) {
		    frames = maxframes;
		} else if (frames < 0) {
		    frames = maxframes;
		}
	    }
	    if(forcedSrate > 0)	
		srate = forcedSrate;	/* override file's sr */
	    if(fchans > 0)  {
		sndSetUchans(snd, fchans);   /* force e.g. to mono */
		chans = fchans;
	    }
	    sndSetUgain(snd, gain);	/* gain internal to reading */
	    if(!dontplay) {
		port = AUOpen(outformat,srate,chans, opBuffer=bufsize, AIO_OUTPUT);
		if(port == NULL) {
		    err = -2;
		    continue;
		}
	    }
	    if( (buf = malloc(SFSampleSizeOf(outformat)*chans*bufsize)) 
		== NULL) {
		err = -3;		/* could not allocate play buffer */
		continue;
	    }
	    doneframes = 0;
	    do {
		/* actually copy the samples from file to output */
		if(frames > 0)
		    remains = frames;
		else
		    remains = -1;
		do {
		    if(remains > 0)
			toget = MIN(chunksize, remains);
		    else
			toget = chunksize;
		    got = sndReadFrames(snd, buf, toget);

		    DBGFPRINTF((stderr, "sndplay: sample %d = %d\n", doneframes, *(short *)buf));

		    /* gain now handled via sndSetUserGain */
    /*		    if (gain != 1.0) {
		        int i;
			short *sp = buf;
			for (i=0; i<got*chans; ++i) {
			    *sp++ *= gain;
			}
		    } */

		    if (!dontplay) {
			AUWrite(port, buf, got);
		    }
		    doneframes += got;
		    if(remains > 0)
			remains -= got;
		} while(remains != 0 && got > 0);
		if(loop)    sndFrameSeek(snd, skip, SEEK_SET);
	    } while (loop);
	    if(port && !dontplay)	AUClose(port,0);
	    if(buf)	free(buf);
	    if(verb) {
		fprintf(stderr, "(%ld frames)\n", doneframes);
	    }
	}
    }
    if(snd)	sndClose(snd);
    return err;
}

static void ReportPlayError(FILE *stream,  int err) 
{  /* interpret the error codes returned by play_file */
    if(err == 0) {
	fprintf(stream, "no error");
    } else if(err == -1) {
	fprintf(stream, "could not read file");
    } else if(err == -2) {
	fprintf(stream, "could not initialize audio hardware");
    } else if(err == -3) {
	fprintf(stream, "error allocating memory");
    } else {
	fprintf(stream, "unknown error");
    }
}

#ifdef MACINTOSH
#include <console.h>
#endif /* MACINTOSH */

int main(int argc, char *argv[])
{
    int 	i = 0;
    int		err = 0;
    char *programName;
    
#ifdef MACINTOSH
    argc = ccommand(&argv);
#endif /* MACINTOSH */

    programName = argv[0];
    HandleClArgs(argc, argv);

    if(sffmtname != NULL) {
/*	if(SFSetDefaultFormatByName(sffmtname) == 0) { */
	if ( (sffmt = SFFormatToID(sffmtname)) == SFF_UNKNOWN) {
	    fprintf(stderr, "%s: soundfile format '%s' not known\n",
		    programName, sffmtname);
	    usage(programName);
	}
    } else {
	sffmt = SFF_UNKNOWN;
    }

    if(nplayfiles == 0)  {
#ifndef MACINTOSH
	/* no arguments - play stdin using magic name */
	playfiles[0] = SND_STDIO_PATH;
	nplayfiles = 1;
#else /* MACINTOSH - no stdin */
	usage(programName);
#endif /* MACINTOSH */
    }
    for(i = 0; i<nplayfiles; ++i)  {
	if(nplayfiles > 1)
	    fprintf(stderr, "%s: playing `%s' ...\n", programName, playfiles[i]);

	err = play_file(playfiles[i], bufsize, chunksize, srate, chans, 
		skip, maxframes, gain, starttime, durtime, endtime, loop);

	if(err) {
	    fprintf(stderr, "%s: Error playing '%s':\n", 
		    programName, playfiles[i]);
	    ReportPlayError(stderr, err); 
	    fprintf(stderr, "\n");
	}
    }	
    exit(err);
}

static int HandleClArgs(int argc, char **argv)
{
    int 	i = 0;
    int 	err = 0;

    nplayfiles = 0;

    i = 0;
    while(++i<argc && err == 0)	 {
	char c, *token, *arg, *nextArg;
	int  argUsed;

	token = argv[i];
	if(*token++ == '-')  {
	    if(i+1 < argc)	nextArg = argv[i+1];
	    else		nextArg = "";
	    argUsed = 0;
	    if(*token == '\0')  {	/* special case for '-' alone */
		if(nplayfiles < MAXPLAYFILES) 
		    playfiles[nplayfiles++] = "-";  /* copy to output list */
	    } else {
		while(c = *token++)  {
		    if(NumericQ(token))	arg = token;
		    else			arg = nextArg;
		    switch(c)  {
		    case 'r':	srate = atof(arg); argUsed = 1; break;
		    case 'b':	bufsize = atoi(arg); argUsed = 1; break;
		    case 'z':	chunksize = atoi(arg); argUsed = 1; break;
		    case 'c':	chans = atoi(arg); argUsed = 1; break;
		    case 'S':	sffmtname = arg; argUsed = 1; break;
		    case 'K':	skip = atoi(arg); argUsed = 1; break;
		    case 'D':	maxframes = atoi(arg); argUsed = 1; break;
		    case 'k':	starttime = atof(arg); argUsed = 1; break;
		    case 'd':	durtime = atof(arg); argUsed = 1; break;
		    case 'e':	endtime = atof(arg); argUsed = 1; break;
		    case 'g':	gain = atof(arg); argUsed = 1; break;
		    case 'q':	dontplay = 1; break;
		    case 'l': 	loop = 1; break;
		    case 'v': 	verb = 1; break;
		    case 'm': 	domulti = 1; break;
		    default:	fprintf(stderr,"%s: unrec option %c\n",
					argv[0], c);
			err = 1; break;
		    }
		    if(argUsed)  {
			if(arg == token)  token = ""; /* no more from token */
			else		  ++i;        /* skip arg we used */
			arg = ""; argUsed = 0;
		    }
		}
	    }
	} else {
	    if(nplayfiles < MAXPLAYFILES) 
		playfiles[nplayfiles++] = argv[i];  /* copy to output list */
	}
    }
    if(err)
	usage(argv[0]);		/* never returns */

}

static void usage(char *programName)
{   /* print usage message & exit */
    fprintf(stderr,
	    "usage: %s [-r samprate][-v] [soundfile1 ...]\n",programName);
    fprintf(stderr,"where (defaults in brackets)\n");
    fprintf(stderr," -r samprate   forces playback to be at samprate\n");
    fprintf(stderr," -g gain       scale sound by specified factor\n");
    fprintf(stderr," -b bufsize    sample frames to request for hardware buffer (%d)\n",
	    BUFTOT);
    fprintf(stderr," -z chunksize  how many sample frames to write at once (bufsize/2)\n");
    fprintf(stderr," -c ch         convert to <ch> channels before playing\n");
    fprintf(stderr," -l            loop the sound (infinitely)\n");
    fprintf(stderr," -v            verbose mode\n");
    fprintf(stderr," -K n          skip this many frames at start of sound\n");
    fprintf(stderr," -D n          play at most this many frames\n");
    fprintf(stderr," -k time       skip this many  *seconds* at start of sound\n");
    fprintf(stderr," -d time       play at most this many *seconds*\n");
    fprintf(stderr," -e time       play through to this time in orig file\n");
    fprintf(stderr," -m            attempt to read multiple soundfiles from stream\n");
    fprintf(stderr," -q            don't actually open hardware or play\n");
    fprintf(stderr," -S sffmt      force soundfile to be treated as sffmt (see sndf(3)) one of:\n");
    fprintf(stderr,"               ");
    SFListFormats(stderr); fprintf(stderr,"\n");
    fprintf(stderr," soundfile1 .. files to play (if none, uses stdin)\n");
    exit(1);
}
