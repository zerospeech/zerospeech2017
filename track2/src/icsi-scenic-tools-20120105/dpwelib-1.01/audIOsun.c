/***********************************************************************\
*	audIOsun.c							*
*	New hardware-indep audio IO interface intended to be portable	*
*	based on lofiplay etc						*
*	dpwe 1993nov17, 13mar92						*
*	SUNos 4.1.3 version - dpwe@icsi.berkeley.edu - 1995oct24        *
\***********************************************************************/

#include <dpwelib.h>	/* for config.h, sets up GNU autoconf flags */

#include <fcntl.h>   	/* for open */
#include <sys/ioctl.h>  /* for ioctl */
#include <sys/types.h>
#include <stropts.h>
#include <sys/conf.h>	/* for streamio(7) I_FLUSH */
#include <stdlib.h>	/* for getenv() (of DEVAUDIO) */

#ifdef HAVE_SYS_AUDIOIO_H
/* Solaris */
#include <sys/audioio.h>
#else /* assume SunOS4 */
#include <sun/audioio.h>
#endif /* sys/audioio.h */

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
    int paused;			/* to permit delayed startup on playback */
} AUSTRUCT;

#define AUSTRUCT_DEFINED	/* stop redefinition in audIO.h */

#include "audIO.h"	/* for stdio, PARG etc */

#include <assert.h>

#define DFLT_BUFTIME	(0.25)		/* default buffer length in secs */

#define NUM_PARAMS 1

/* For XXMPEG, provide the AUSampleSizeOf() here */
#ifndef XXMPEG
static 
#endif /* !XXMPEG */
       int AUSampleSizeOf(int format)
{   /* return number of bytes per sample in given fmt */
    return(format & AU_BYTE_SIZE_MASK);
}	/* copied from SFSampleSizeOf */

static void PrintAudioPortInfo(FILE *f, audio_prinfo_t *api, char *tag)
{
    fprintf(f, "PrintAudioPortInfo for '%s'\n", tag);
    fprintf(f, "  samp_rate=%d\n", api->sample_rate);
    fprintf(f, "  channels =%d\n", api->channels);
    fprintf(f, "  precision=%d\n", api->precision);
    fprintf(f, "  encoding =%d\n", api->encoding);
    fprintf(f, "  gain     =%d\n", api->gain);
    fprintf(f, "  port     =%d\n", api->port);
#ifdef HAVE_SYS_AUDIOIO_H
    fprintf(f, "  buf_size =%d\n", api->buffer_size);
#endif /* Solaris */
    fprintf(f, "  samples  =%d\n", api->samples);
    fprintf(f, "  eof      =%d\n", api->eof);
    fprintf(f, "  pause    =%d\n", api->pause);
    fprintf(f, "  error    =%d\n", api->error);
    fprintf(f, "  waiting  =%d\n", api->waiting);
    fprintf(f, "  balance  =%d\n", api->balance);
    fprintf(f, "  open     =%d\n", api->open);
    fprintf(f, "  active   =%d\n", api->active);
    fprintf(f, "  av_ports =%d\n", api->avail_ports);
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
    /* not sure what to do with desbuf .. */
    if(desbuf <= 0)	
	desbuf = srate * DFLT_BUFTIME;

    if(err == 0)  {	/* actual port initialization */
	audio_info_t ainfo;
	audio_prinfo_t *api;
#ifndef HAVE_SYS_AUDIOIO_H
	int atype;
#else /* Solaris */
	audio_device_t adev;
#endif /* SUNOS/Solaris */
	int afd;
	int mulaw = 0;
	int hfmt  = AFMT_SHORT;
	int hchans = chans;
	int opflag;
	char *devaudio;
	static int warned = 0;

	if( (devaudio = getenv("AUDIODEV"))==NULL) {
	    /* recommended to test env in man audio */
	    devaudio = "/dev/audio";
	}
	
	/* It seems that desbuf > 2000 stereo short frames hangs 
	   /dev/audio, so clip it at there */
#define DEVAUDIO_MAXBUFSIZE 32768
	if(desbuf*chans > DEVAUDIO_MAXBUFSIZE) {
	    if (!warned) {
		fprintf(stderr, "AUOpen: warning: desbuf of %d frames clipped to %d to avoid Solaris hanging\n", desbuf, DEVAUDIO_MAXBUFSIZE);
		warned = 1;
	    }
	    desbuf = DEVAUDIO_MAXBUFSIZE/chans;
	}

	if(mode == AIO_OUTPUT) {
	    opflag = O_WRONLY;
	} else {
	    opflag = O_RDONLY;
	}
	afd = open(devaudio, opflag | O_NDELAY);
	if(afd < 0)  {
	    fprintf(stderr, "AUOpen: couldn't open %s (is AUDIODEV set?)\n",
		    devaudio);
	    return NULL;
	} else {
	    /* Have to close it and re-open it to make sure the 
	       accesses are in fact blocking. (per Jeffy) */
	    DBGFPRINTF((stderr, "AUOpen: %s opened O_NDELAY (%d)\n", 
			devaudio, afd));
	    close(afd);
	    afd = open(devaudio, opflag);
	    DBGFPRINTF((stderr, "AUOpen: %s opened plain (%d)\n", 
			devaudio, afd));
	}
	/* query the hardware type */
#ifndef HAVE_SYS_AUDIOIO_H
	ioctl(afd, AUDIO_GETDEV, &atype);

	DBGFPRINTF((stderr, "AUOpen: sunaudio is %s\n", 
		    atype==AUDIO_DEV_AMD?"mulaw":"16bit"));
	if(atype == AUDIO_DEV_AMD) {
	    mulaw = 1;	/* 8bit mulaw audio only */
	    hfmt  = AFMT_ULAW;
	    hchans = 1;
	}
#else /* Solaris */
	ioctl(afd, AUDIO_GETDEV, &adev);
	DBGFPRINTF((stderr, "Audio device is '%s' version '%s' config '%s'\n",
		   adev.name, adev.version, adev.config));
	if (strstr(adev.name, "am") != NULL) {
	    /* this is a sparc2-type machine running solaris */
	    mulaw = 1;
	    hfmt  = AFMT_ULAW;
	    hchans = 1;
	}
#endif /* SUNOS/Solaris */
	
	/* allocate the descriptor */
	au = TMMALLOC(AUSTRUCT, 1, "AUOpen");
	au->errorCount = 0;
	au->totalDroppedFrames = 0;
	au->totalFrames = 0;
	au->mode   = mode;	au->port  = NULL;
	au->ichans = chans;	au->ochans = chans;	/* always same */
	au->paused = 0;		/* isn't paused until we explicitly pause it */
	/* set up flags so that format will be converted on Read/Write */
        if(mode == AIO_INPUT)  {
	    au->iformat = hfmt;		au->oformat = format;
	    au->ichans  = hchans;       au->ochans  = chans;
	} else {	/* AIO_OUTPUT */
	    au->iformat = format;	au->oformat = hfmt;
	    au->ichans  = chans;	au->ochans  = hchans;
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
	    api = &ainfo.record;
	} else {
	    api = &ainfo.play;
	}
	/* open the audio */
	/* first, initialize all the fields to DONT CARE */
	AUDIO_INITINFO(&ainfo);
	/* modify the fields of interest to us */
	if(mode==AIO_OUTPUT) {	    /* Use headphones output by default */
	/*    api->port = AUDIO_HEADPHONE;   */
	} else {	/* default input is microphone? (see man 7 audio) */
	/*    api->port = AUDIO_MICROPHONE;  */
	}
	
#ifdef HAVE_SYS_AUDIOIO_H
	/* setup desbuf; api->buffer_size is in BYTES */
	api->buffer_size = desbuf*hchans*AUSampleSizeOf(hfmt);
#endif /* SUNOS41 */
	/* no buffer_size until SUNOS5 */

	if(mulaw) {
	    api->sample_rate = 8000;
	} else {
	    /* If we have the dbri speakerbox, choose the best 
	     * supported sampling rate */
	    int srates[] = {8000,9600,11025,16000,18900,22050,32000,37800,
			    44100,48000};
	    int nsrates = sizeof(srates)/sizeof(int);
	    int i;
	    int d,ld;
	    int asr = srates[0];
	    
	    ld = srate-asr; if(ld<0) ld = -ld;
	    for(i=1; i<nsrates; ++i)  {
		d = srate - srates[i]; if(d<0) d = -d;
		if(d > ld)  {
		    break;
		}
		asr = srates[i];
		ld = d;
	    }
	    /* asr is now the element of srates closest to srate */
	    api->sample_rate = asr;
	    DBGFPRINTF((stderr, "AUOpen: selected srate of %d for %d\n", asr, (int)srate));
	}
	if(srate != api->sample_rate)  {
	    fprintf(stderr, "AUOpen: warning: playing data of %d Hz at %d Hz\n",
		    (int)srate, (int)api->sample_rate);
	}
	api->channels = hchans;
	switch(hfmt)  {
	case AFMT_ULAW:
	    api->precision = 8;
	    api->encoding  = AUDIO_ENCODING_ULAW;
	    break;
	case AFMT_CHAR:
	    api->precision = 8;
	    api->encoding  = AUDIO_ENCODING_LINEAR;
	    break;
	case AFMT_SHORT:
	    api->precision = 16;
	    api->encoding  = AUDIO_ENCODING_LINEAR;
	    break;
	case AFMT_LONG:
	    api->precision = 32;
	    api->encoding  = AUDIO_ENCODING_LINEAR;
	    break;
	default:
	    DBGFPRINTF((stderr, "couldn't init hardware for format %d\n", hfmt));
	    abort();
	}
	/* Pause the port on both input and output.  
	   On input, this stops it reading in data at the incorrect,
	   unconfigured data format so that we can flush it before 
	   beginning to read in data in the selected format.
	   On play, the port remains paused until the end of 
	   the first AUWrite command, for glitch-free playback. */
#ifdef HAVE_SYS_AUDIOIO_H
	api->pause = 1;
#endif /* !SUNOS41 */
	au->paused = 1;		/* mark the port structure as paused */
	if(ioctl(afd, AUDIO_SETINFO, &ainfo)) {
	   DBGFPRINTF((stderr, "AUOpen: error setting pause\n"));
	}
#ifdef HAVE_SYS_AUDIOIO_H
	if(mode==AIO_INPUT) {
	    /* The input port has been accumulating ulaw bytes since 
	       we opened it.  Pause this, then flush the input to 
	       ensure word-aligned reads in future. */
	    if(ioctl(afd, I_FLUSH, FLUSHR)) {
		DBGFPRINTF((stderr, "AUOpen: error flushing input\n"));
	    }
	    /* On record, unpause immediately to start accumulating samples */
	    api->pause = 0;
	    au->paused = 0;
	    
	    /* Now, only release pause for record (else fails on SunOS41) */
            /* api->error = 0; */
	    err = ioctl(afd, AUDIO_SETINFO, &ainfo);
	    if(err) {
		/* error initializing hardware */
		DBGFPRINTF((stderr, "AUOpen: error clearing pause\n"));
		close(afd);
		if(au->buf)	free(au->buf);
		free(au);
		return NULL;
	    }
	}
#endif /* !SUNOS41 */
	au->port = afd;
#ifdef DEBUG
	PrintAudioPortInfo(stderr, &ainfo.play, "play req");
	PrintAudioPortInfo(stderr, &ainfo.record, "record req");
	err = ioctl(afd, AUDIO_SETINFO, &ainfo);
	PrintAudioPortInfo(stderr, &ainfo.play, "play act");
	PrintAudioPortInfo(stderr, &ainfo.record, "record act");
#endif /* DEBUG */
    }
    return au;
}

static int Write(int fd, void *buf, int nbytes)
{   /* Write bytes using repeated write(2) commands. */
    int thistime = -1;
    int totwrit = 0;
    int remain = nbytes;
    
    /* Make sure previous write is complete before starting 
       this one, so we don't get too far ahead */
    /*    ioctl(fd, AUDIO_DRAIN, 0);  */

    while(thistime && remain > 0) {
	thistime = remain;
        thistime = write(fd, (void*)(((char *)buf)+totwrit), thistime);
	if(thistime>0) {
	    remain -= thistime;
	    totwrit += thistime;
	}
	DBGFPRINTF((stderr, "Write: %d of %d bytes\n", thistime, nbytes));
    }
    return totwrit;
}

static int Read(int fd, void *buf, int nbytes)
{   /* Read bytes using repeated read(2) commands */
    int totred = 0;
    int thistime = -1;

    while(thistime && totred < nbytes) {
        thistime = read(fd, (void*)(((char *)buf)+totred), nbytes-totred);
	if(thistime>0)
	    totred += thistime;
	DBGFPRINTF((stderr, "Read: %d of %d bytes (tot: %d)\n", thistime, nbytes, totred));
    }
    return totred;
}

long	AUWrite(AUSTRUCT *au, void *buf, long frames)
{   /* write some sound to the open audio IO channel, per description */
    int bytesperframe;
    DBGFPRINTF((stderr, "AUWrite(port=%d, frames=%d)\n", au->port, frames));
    assert((au->mode) == AIO_OUTPUT);
    bytesperframe = au->ochans * AUSampleSizeOf(au->oformat);
    if(au->paused) {
	/* This is a hack, I know.  AUOpen(AIO_OUTPUT) pauses 
	   the play port.  Make sure we've attempted to write 
	   at least one buffer before unpausing.  But don't 
	   wait for it to block!  Unpause immediately after 
	   first return. */
	audio_info_t ainfo;
	/*	AUDIO_INITINFO(&ainfo);   */
	ioctl(au->port, AUDIO_GETINFO, &ainfo);
#ifdef HAVE_SYS_AUDIOIO_H
	ainfo.play.pause = 0;
#endif /* !SUNOS41 */
	ioctl(au->port, AUDIO_SETINFO, &ainfo);
	au->paused = 0;
	DBGFPRINTF((stderr, "AUWrite: unpause\n"));
    }
    if(au->ichans != au->ochans || au->iformat != au->oformat)  {
	long thistime, remain = frames;

	while(remain)  {
	    thistime = MIN(remain, au->bufSize);
	    CvtChansAndFormat(buf, au->buf, au->ichans, au->ochans, 
			      au->iformat, au->oformat, thistime);
	    Write(au->port, au->buf, thistime*bytesperframe);
	    buf = (void *)(((char *)buf)+ thistime * au->ichans * AUSampleSizeOf(au->iformat));
	    remain -= thistime;
	}
    } else {
	Write(au->port, buf, frames*bytesperframe);
    }
    au->totalFrames += frames;
    return frames;	/* presumably */
}

long	AURead(AUSTRUCT *au, void *buf, long frames)
{   /* read some sound from the open audio IO channel, per description */
    int bytesperframe;
    DBGFPRINTF((stderr, "AURead(prt=%d, frames=%d)\n", au->port, frames));
    assert(au->mode == AIO_INPUT);
    bytesperframe = au->ichans * AUSampleSizeOf(au->iformat);
    if(au->ichans != au->ochans || au->iformat != au->oformat)  {
	long thistime, remain = frames;

	while(remain)  {
	    thistime = MIN(remain, au->bufSize);
	    Read(au->port, au->buf, thistime*bytesperframe);
	    CvtChansAndFormat(au->buf, buf, au->ichans, au->ochans, 
			      au->iformat, au->oformat, thistime);
	    buf = (void *)(((char *)buf)+ thistime * au->ochans * AUSampleSizeOf(au->oformat));
	    remain -= thistime;
	}
    } else {
	Read(au->port, buf, frames*bytesperframe);
    }
    au->totalFrames += frames;
    return frames;	/* we presume */
}

void	AUClose(AUSTRUCT *au, int now)
{  /* close an audio IO channel. if <now> is nonzero, stop immediately
      else play out any buffered samples */
#ifdef DEBUG
    audio_info_t ainfo;
    int err = ioctl(au->port, AUDIO_GETINFO, &ainfo);
    PrintAudioPortInfo(stderr, &ainfo.play, "play before close");
    PrintAudioPortInfo(stderr, &ainfo.record, "record before close");
#endif /* DEBUG */
    if(now == 0
       && au->mode == AIO_OUTPUT)	/* wait for port to drain on output */
	ioctl(au->port, AUDIO_DRAIN, 0);
    close(au->port);
    if(au->buf)	free(au->buf);
    free(au);
}

long AUBufConts(AUSTRUCT *au)
{   /* see how many sample frames are currently in audio buffer.
       (can be used for synchronization, timing etc). */
    audio_info_t ainfo;
    long conts;

    ioctl(au->port, AUDIO_GETINFO, &ainfo);
    if(au->mode == AIO_INPUT) {
	conts = ainfo.record.samples - au->totalFrames;
    } else {
	conts = au->totalFrames - ainfo.play.samples;
    }
    return conts;
}

long AUBufSpace(AUSTRUCT *au)
{   /* see how many sample frames are currently free in the audio buffer.
       (can be used for synchronization, timing etc). */
    audio_info_t ainfo;

    ioctl(au->port, AUDIO_GETINFO, &ainfo);

    return ainfo.record.samples - au->totalFrames;
}

long AUDroppedFrames(AUSTRUCT *au)
{   /* return the number of frames that have been under/overflowed 
       on this port (very approx only) */ 
    audio_info_t ainfo;
    audio_prinfo_t *api;
    long droppedframes;

    if(au->mode == AIO_INPUT)  {
	api = &ainfo.record;
    } else {
	api = &ainfo.play;
    }

    ioctl(au->port, AUDIO_GETINFO, &ainfo);

    return 5000*api->error;
}
