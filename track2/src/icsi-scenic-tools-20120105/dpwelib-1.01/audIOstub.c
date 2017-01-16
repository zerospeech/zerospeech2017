/***********************************************************************\
*	audIOstub.c							*
*	Hardware-indep audio IO interface				*
*	Dummy backend for systems without sound
*	1998feb27 dpwe@icsi.berkeley.edu				*
*	based on audIOetc 1995oct24, 1993nov17, 13mar92			*
*	$Header: /u/drspeech/repos/dpwelib/audIOstub.c,v 1.1 1998/03/10 07:42:44 dpwe Exp $						        *
\***********************************************************************/

#include <fcntl.h>   	/* for open */

typedef struct {		/* descriptor for audio IO channel */
    int	mode;			/* AU_INPUT or AU_OUTPUT */
    int ichans, ochans;		/* separate chans and for input and */
    int iformat, oformat;	/*  output of this layer i.e. on write, */
    long bufSize;		/*  user passes ichans, hardware gets ochans */
    void *buf;			/* temporary buffer for format conversion */
    long totalFrames;		/* total number of frames we think are done */
    long errorCount;
    long totalDroppedFrames;
    int  port;
} AUSTRUCT;

#define AUSTRUCT_DEFINED	/* stop redefinition in audIO.h */

#include "audIO.h"	/* for stdio, PARG etc */

#include <assert.h>

#define DFLT_BUFTIME	(0.25)		/* default buffer length in secs */

#ifndef BITSPERBYTE
#define BITSPERBYTE 	8
#endif /* BITSPERBYTE */

static int AUSampleSizeOf(int format)
{   /* return number of bytes per sample in given fmt */
    return(format & AU_BYTE_SIZE_MASK);
}	/* copied from SFSampleSizeOf */

static void AUError(char *msg) 
{   /* Fatal error within AU routine */
    fprintf(stderr, "AUError: %s\n", msg);
    abort();
}

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
    fprintf(stderr, "AUOpen: Audio IO not supported on this hardware\n");

    return au;
}

long	AUWrite(AUSTRUCT *au, void *buf, long frames)
{   /* write some sound to the open audio IO channel, per description */
    int bytesperframe;
    assert((au && au->mode) == AIO_OUTPUT);
    return 0;
}

long	AURead(AUSTRUCT *au, void *buf, long frames)
{   /* read some sound from the open audio IO channel, per description */
    int bytesperframe;
    assert(au && au->mode == AIO_INPUT);
    return 0;
}

void	AUClose(AUSTRUCT *au, int now)
{   /* close an audio IO channel. if <now> is nonzero, stop immediately
       else play out any buffered samples */
    free(au);
}

long AUBufConts(AUSTRUCT *au)
{   /* see how many sample frames are currently in audio buffer.
       (can be used for synchronization, timing etc). */
    /* audio_info_t ainfo;
       ioctl(au->port, AUDIO_GETINFO, &ainfo);
       return au->totalFrames - ainfo.play.samples; */
    return -1;
}

long AUBufSpace(AUSTRUCT *au)
{   /* see how many sample frames are currently free in the audio buffer.
       (can be used for synchronization, timing etc). */
    /* audio_info_t ainfo;
       ioctl(au->port, AUDIO_GETINFO, &ainfo);
       return ainfo.record.samples - au->totalFrames; */
    return -1;
}

long AUDroppedFrames(AUSTRUCT *au)
{   /* return the number of frames that have been under/overflowed 
       on this port (very approx only) */ 
    /* audio_info_t ainfo;
       audio_prinfo_t *api;
       long droppedframes;

       if(au->mode == AIO_INPUT)  {
           api = &ainfo.record;
       } else {
           api = &ainfo.play;
       }
       ioctl(au->port, AUDIO_GETINFO, &ainfo);
       return 5000*api->error; */
    return 0;
}
