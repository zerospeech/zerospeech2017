/***********************************************************************\
*	audIOsgi.c							*
*	New hardware-indep audio IO interface intended to be portable	*
*	based on lofiplay etc						*
*	dpwe 1993nov17, 13mar92						*
*	SGI AL version - 1994mar30					*
\***********************************************************************/
static char rcsid[] = "$Header: /u/drspeech/repos/dpwelib/audIOsgi.c,v 1.3 1997/02/18 22:00:39 dpwe Exp $ @media.mit.edu Machine Listening Group";

#include <unistd.h>	/* for sginap(2) prototype */
#include <audio.h>
#include "sndfutil.h"	/* for CvtChansAndFormat() proto */

/* From man ALsetqueuesize: */
#define MIN_STEREO_QUEUE_SIZE  510
/* #define MIN_MONO_QUEUE_SIZE    1019 */
#define MIN_MONO_QUEUE_SIZE    MIN_STEREO_QUEUE_SIZE	/* seems to work */

typedef struct {		/* descriptor for audio IO channel */
    int	mode;			/* AU_INPUT or AU_OUTPUT */
    int ichans, ochans;		/* separate chans and for input and */
    int iformat, oformat;	/*  output of this layer i.e. on write, */
    long bufSize;		/*  user passes ichans, hardware gets ochans */
    void *buf;			/* temporary buffer for format conversion */
    long errorCount;
    long totalDroppedFrames;
    ALport port;
} AUSTRUCT;

#define AUSTRUCT_DEFINED	/* stop redefinition in audIO.h */

#include "audIO.h"	/* for stdio, PARG etc */

#include <assert.h>

#define DFLT_BUFTIME	(0.25)		/* default buffer length in secs */

#define NUM_PARAMS 1

static int AUSampleSizeOf(int format)
{   /* return number of bytes per sample in given fmt */
    return(format & AU_BYTE_SIZE_MASK);
}	/* copied from SFSampleSizeOf */

AUSTRUCT *AUOpen(int format, double srate, int chans, long desbuf, int mode)
{  /* open a new audio IO channel.  Define data format, #channels and sampling
      rate. Request that output buffer be desbuf frames long.  
      mode is AIO_{INPUT,OUTPUT} */
    AUSTRUCT *au = NULL;
    int	     err = 0;

    DBGFPRINTF((stderr, "AUOpen(fmt %d, sr %.0lf, ch %d, desb %ld, mo %d)\n",
		format, srate, chans, desbuf, mode));
    /* check arguments */
    if(format != AFMT_SHORT && format != AFMT_CHAR && format != AFMT_LONG
       && format != AFMT_FLOAT && format != AFMT_ULAW 
       && format != AFMT_CHAR_OFFS)  {
	/* pretty much all formats supported through Convert fn */
	fprintf(stderr, "AUOpen: format %d not supported\n", format);
	++err;
    }
    if(srate < 8000 || srate > 48000 /* || ... */)  {
	fprintf(stderr, "AUOpen: srate %.0f not supported\n", srate);
	++err;
    }
    if(chans < 1 || chans > 2 /* || ... */)  {
	fprintf(stderr, "AUOpen: chans %d not supported\n", chans);
	++err;
    }
    if(mode != AIO_INPUT && mode != AIO_OUTPUT)  {
	fprintf(stderr, "AUOpen: mode %d not supported\n", mode);
	++err;
    }
    /* not sure what to do with desbuf .. */
    if(desbuf <= 0)	
	desbuf = srate * DFLT_BUFTIME;

    if(chans==1 && desbuf < MIN_MONO_QUEUE_SIZE) {
	fprintf(stderr, "AUOpen: desbuf of %d forced to %d\n", 
		desbuf, MIN_MONO_QUEUE_SIZE);
	desbuf = MIN_MONO_QUEUE_SIZE;
    }
    if(chans==2 && desbuf < MIN_STEREO_QUEUE_SIZE) {
	fprintf(stderr, "AUOpen: desbuf of %d forced to %d\n", 
		desbuf, MIN_STEREO_QUEUE_SIZE);
	desbuf = MIN_STEREO_QUEUE_SIZE;
    }

    if(err == 0)  {	/* actual SGI-specific port initialization */
	ALconfig config;
	long params[2*NUM_PARAMS];
	long width;

	/* allocate the descriptor */
	au = TMMALLOC(AUSTRUCT, 1, "AUOpen");
	au->errorCount = 0;
	au->totalDroppedFrames = 0;
	au->mode   = mode;	au->port  = NULL;
	au->ichans = chans;	au->ochans = chans;	/* always same */
	if(format==AFMT_SHORT || format==AFMT_LONG || format==AFMT_CHAR)  {
	    au->iformat = format;	au->oformat = format;
	} else {
	    /* set up flags so that format will be converted on Read/Write */
	    if(mode == AIO_INPUT)  {
		au->iformat = AFMT_SHORT;	au->oformat = format;
	    } else {	/* AIO_OUTPUT */
		au->iformat = format;		au->oformat = AFMT_SHORT;
	    }
	}
	if(au->iformat==au->oformat && au->ichans==au->ochans)  {
	    au->bufSize = 0; au->buf = NULL;
	} else {	/* allocate buffer to hold machine-format data */
	    int bytespersamp;
	    if(mode==AIO_INPUT)
		bytespersamp = au->ichans * AUSampleSizeOf(au->iformat);
	    else
		bytespersamp = au->ochans * AUSampleSizeOf(au->oformat);
	    au->bufSize = desbuf;
	    assert(au->buf = (void *)(malloc(au->bufSize * bytespersamp)));
	}
	if(mode==AIO_INPUT) {
	    width = AUSampleSizeOf(au->iformat);
	} else {
	    width = AUSampleSizeOf(au->oformat);
	}
	/* open the audio */
	config = ALnewconfig();
	ALsetchannels(config, (long)chans);
	ALsetwidth(config, width);
	ALsetqueuesize(config, desbuf);
	DBGFPRINTF((stderr, "AUOpen(%c): desbuf %d\n", 
		    (mode==AIO_INPUT)?'r':'w',desbuf));
	if(au->mode == AIO_INPUT)
	    params[0] = AL_INPUT_RATE;	
	else
	    params[0] = AL_OUTPUT_RATE;	
	params[1] = (long)srate;
	ALsetparams(AL_DEFAULT_DEVICE, params, 2*NUM_PARAMS);
	if(au->mode == AIO_INPUT)
	    au->port = ALopenport("audIOsgi", "r", config);
	else
	    au->port = ALopenport("audIOsgi", "w", config);
	ALfreeconfig(config);
	if(au->port == NULL) {
	    /* error initializing hardware */
	    if(au->buf)	free(au->buf);
	    free(au);
	    return NULL;
	}
    }
    return au;
}

long	AUWrite(AUSTRUCT *au, void *buf, long frames)
{   /* write some sound to the open audio IO channel, per description */
    assert((au->mode) == AIO_OUTPUT);
    if(au->ichans != au->ochans || au->iformat != au->oformat)  {
	long thistime, remain = frames;

	while(remain)  {
	    thistime = MIN(remain, au->bufSize);
	    CvtChansAndFormat(buf, au->buf, au->ichans, au->ochans, 
			      au->iformat, au->oformat, thistime);
	    ALwritesamps(au->port, au->buf, thistime*au->ochans);
	    buf = (void *)(((char *)buf)+ thistime * au->ichans * AUSampleSizeOf(au->iformat));
	    remain -= thistime;
	}
    } else {
	ALwritesamps(au->port, buf, frames*au->ochans);
    }
    return frames;	/* presumably */
}

long	AURead(AUSTRUCT *au, void *buf, long frames)
{   /* read some sound from the open audio IO channel, per description */
    assert(au->mode == AIO_INPUT);
    if(au->ichans != au->ochans || au->iformat != au->oformat)  {
	long thistime, remain = frames;

	while(remain)  {
	    thistime = MIN(remain, au->bufSize);
	    ALreadsamps(au->port, au->buf, thistime*au->ichans);
	    CvtChansAndFormat(au->buf, buf, au->ichans, au->ochans, 
			      au->iformat, au->oformat, thistime);
	    buf = (void *)(((char *)buf)+ thistime * au->ochans * AUSampleSizeOf(au->oformat));
	    remain -= thistime;
	}
    } else {
	ALreadsamps(au->port, buf, frames*au->ochans);
    }
    return frames;	/* we presume */
}

void	AUClose(AUSTRUCT *au, int now)
{  /* close an audio IO channel. if <now> is nonzero, stop immediately
      else play out any buffered samples */
    if(now == 0
       && au->mode == AIO_OUTPUT)	/* wait for port to drain on output */
	while(ALgetfilled(au->port)>0)
	    sginap(1);
    ALcloseport(au->port);
    if(au->buf)	free(au->buf);
    free(au);
}

long AUBufConts(AUSTRUCT *au)
{   /* see how many sample frames are currently in audio buffer.
       (can be used for synchronization, timing etc). */
    return ALgetfilled(au->port);
}

long AUBufSpace(AUSTRUCT *au)
{   /* see how many sample frames are currently free in the audio buffer.
       (can be used for synchronization, timing etc). */
    return ALgetfillable(au->port);
}

long AUDroppedFrames(AUSTRUCT *au)
{   /* return the number of frames that have been under/overflowed 
       on this port (SGI only) */
    long buf[10];
    long droppedframes = 0;
#ifndef IRIX4
    /* these things only introduced in IRIX5 */
    buf[0] = AL_ERROR_NUMBER;
    buf[2] = AL_ERROR_TYPE;
    buf[4] = AL_ERROR_LENGTH;
    buf[6] = AL_ERROR_LOCATION_LSP;
    buf[8] = AL_ERROR_LOCATION_MSP;
    ALgetstatus(au->port, buf, 10);
    if(buf[1] > au->errorCount)  {
	if(buf[1] > au->errorCount+1)  {
	    fprintf(stderr, "AUDroppedFrames: missed %d errors\n", 
		    buf[1] - (au->errorCount + 1));
	}
	if(buf[3] != AL_ERROR_INPUT_OVERFLOW 
	   && buf[3] != AL_ERROR_OUTPUT_UNDERFLOW)  {
	    fprintf(stderr, "AUDroppedFrames: unknown error %d\n", buf[3]);
	} else {	/* is simple over/underflow */
	    droppedframes = buf[5];
	    au->totalDroppedFrames += droppedframes;
	}
	au->errorCount = buf[1];
    }
#endif /* IRIX4 */
    return droppedframes;
}
