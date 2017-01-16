/***********************************************************************\
*	audIOfake.c							*
*	New hardware-indep audio IO interface intended to be portable	*
*	based on lofiplay etc						*
*	dpwe 1993nov17, 13mar92						*
*	FAKE i.e. a bunch of stubs for linking on non-audio systems	*
*	dpwe 1995jan21 							*
\***********************************************************************/

typedef struct {		/* descriptor for audio IO channel */
    int	mode;			/* AU_INPUT or AU_OUTPUT */
    int chans;			
    int format;
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
    if(format != AFMT_SHORT && format != AFMT_CHAR && format != AFMT_LONG)  {
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

    if(err == 0)  {	/* actual SGI-specific port initialization */
	long width = AUSampleSizeOf(format);

	/* allocate the descriptor */
	au = TMMALLOC(AUSTRUCT, 1, "AUOpen");
	au->mode   = mode;	au->chans = chans;
	au->format = format;

	/* open the audio */
    }
    return au;
}

long	AUWrite(AUSTRUCT *au, void *buf, long frames)
{   /* write some sound to the open audio IO channel, per description */
    assert((au->mode) == AIO_OUTPUT);
    return frames;	/* presumably */
}

long	AURead(AUSTRUCT *au, void *buf, long frames)
{   /* read some sound from the open audio IO channel, per description */
    assert(au->mode == AIO_INPUT);
    return frames;	/* we presume */
}

void	AUClose(AUSTRUCT *au, int now)
{  /* close an audio IO channel. if <now> is nonzero, stop immediately
      else play out any buffered samples */
    if(now == 0
       && au->mode == AIO_OUTPUT)	/* wait for port to drain on output */
	;
    free(au);
}

long AUBufConts(AUSTRUCT *au)
{   /* see how many sample frames are currently in audio buffer.
       (can be used for synchronization, timing etc). */
    return 0;
}

long AUDroppedFrames(AUSTRUCT *au)
{   /* return the number of frames that have been under/overflowed
       on this port (very approx only) */
    return 0;
}
