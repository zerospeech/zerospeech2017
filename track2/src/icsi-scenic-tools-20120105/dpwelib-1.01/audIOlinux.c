/***********************************************************************\
*	audIOlinux.c							*
*	Hardware-indep audio IO interface				*
*	Linux backend							*
*	1997feb05 dpwe@icsi.berkeley.edu				*
*	based on audIOetc 1995oct24, 1993nov17, 13mar92			*
*	$Header: /u/drspeech/repos/dpwelib/audIOlinux.c,v 1.16 2011/03/09 02:06:06 davidj Exp $						        *
\***********************************************************************/

#include <fcntl.h>   	/* for open */
#include <sys/ioctl.h>  /* for ioctl */

#include <errno.h>	/* needed on RedHat 9 */

#include <sys/soundcard.h>	/* Linux API definitions */
/*#include <fakesoundcard.h>	/* fakes for debugging on solaris */

#ifndef AFMT_S16_NE	/* 'native endianness' */
#ifdef WORDS_BIGENDIAN
#define AFMT_S16_NE	AFMT_S16_BE
#else 
#define AFMT_S16_NE	AFMT_S16_LE
#endif
#endif /* AFMT_S16_NE */

#define DEVDSP	"/dev/dsp"	/* linux audio pseudodevice */

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

static int AUafmt2OSSafmt(int auafmt) 
{   /* convert between the audio format AFMT_ codes defined in audIO.h
       and the bits defined with very similar names in linux/soundcard.h */
    switch (auafmt) {
    case AFMT_CHAR:		return AFMT_S8;
    case AFMT_CHAR_OFFS:	return AFMT_U8;
    case AFMT_ULAW:		return AFMT_MU_LAW;
    case AFMT_ALAW:		return AFMT_A_LAW;
    case AFMT_SHORT:		return AFMT_S16_NE;
/* these codes not yet defined
    case AFMT_LONG:		return AFMT_S32_NE;
    case AFMT_FLOAT:
    case AFMT_DOUBLE:
 */
    default:
	;
    }
    /* indicate code unrecognized or not supported */
    return -1;
}

#  define ___quote(ARG)      #ARG
#  define ___QUOTE(ARG)	___quote(ARG)
// #define AUIOCTL(fid,code,ptr) \
//     if(ioctl((fid), (code), (ptr)) == -1)   \
//         AUError("AUOpen: " ## ___QUOTE(code));
#define AUIOCTL(fid,code,ptr) \
     if(ioctl((fid), (code), (ptr)) == -1)   \
         AUError("AUOpen: " #code);
//         AUError("AUOpen: " ## #code);

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
	/* This code from Tony Robinson's linux audio example */
	/* open the audio device */
	int audiofid;
    	int nbitPerSample, actBPS;
    	int stereo, samplerate;
    	int bufsiz = -1;
	int hfmt  = AFMT_SHORT;
	int hchans = chans;
	int avfmts, scfmt;

	int checkformats = 1;

	char *devaudio = NULL;

	if( (devaudio = getenv("AUDIODEV"))==NULL) {
	    /* recommended to test env in man audio */
	    devaudio = DEVDSP;
	}

	if (mode == AIO_OUTPUT) {
	    audiofid = open(devaudio, O_WRONLY, 0);
	} else {
	    audiofid = open(devaudio, O_RDONLY, 0);
	}
    	if(audiofid < 0)  {
	    DBGFPRINTF((stderr, "AUopen: couldn't open device '%s'\n", devaudio));
	    perror("couldn't open audio device");
	    fprintf(stderr, "audIOlinux: cannot open audio port '%s' for %s\n",
		    devaudio, (mode == AIO_OUTPUT)?"output":"input");
	    return NULL;
	}
	if (mode == AIO_OUTPUT) {
	    /* Set the crazy buffering very small 
	       so that playback in sndview is closely synchronized to display.
	       (see http://www.opensound.com/pguide/audio2.html) */
	    int arg = 0x00100008;   /* 32 fragments of 2^8=256 bytes */
	    AUIOCTL(audiofid, SNDCTL_DSP_SETFRAGMENT, &arg);
	}

	if (checkformats) {
	    /* figure out which formats are supported */
	    AUIOCTL(audiofid, SNDCTL_DSP_GETFMTS, &avfmts);
	} else {
	    /* just assume it will be OK */
	    avfmts = -1;
	}
	/* Choose the format to request */
	/* unfortunately both audIO.h and soundcard.h define AFMT_
	   codes.  Luckily, they don't collide */
	/* first, attempt to match the requested format */
	scfmt = AUafmt2OSSafmt(format);
	if(scfmt != -1 && (avfmts & scfmt)) {
	    /* whatever the requested format maps to, we support it */
	    hfmt = format;
	} else {
	    /* select S16, MU_LAW, S8 or U8 in declining preference */
	    if (avfmts & AFMT_S16_NE) {
		hfmt = AFMT_SHORT;
	/*    } else if (avfmts & AFMT_MU_LAW) {
		hfmt = AFMT_ULAW; */ /* avoid mulaw->8 bit reduction */
	    } else if (avfmts & AFMT_S8) {
		hfmt = AFMT_CHAR;
	    } else if (avfmts & AFMT_U8) {
		hfmt = AFMT_CHAR_OFFS;
	    } else {
		DBGFPRINTF((stderr, "AUOpen: no reasonable sound data formats in 0x%lx\n", avfmts));
		close(audiofid);
		return NULL;
	    }
	}
	/* We chose an hfmt; configure the device */
	scfmt = AUafmt2OSSafmt(hfmt);
	AUIOCTL(audiofid, SNDCTL_DSP_SETFMT, &scfmt);
	/* check it ??? */
	if( scfmt != AUafmt2OSSafmt(hfmt) ) {
	    fprintf(stderr, "AUOpen: rq fmt 0x%lx, got 0x%lx\n", (unsigned long) AUafmt2OSSafmt(hfmt), (unsigned long) scfmt);
	    /* bug in LinuxPPC 2.1 - supports both BE and LE, but sets them 
	       backwards - set it again to flip it again */
	    if ((scfmt == AFMT_S16_BE || scfmt == AFMT_S16_LE)) {
	        AUIOCTL(audiofid, SNDCTL_DSP_SETFMT, &scfmt);
		/* still reads wrong, but maybe works */
	    }
	}
	/* Set hfmt to reflect what was actually set */
	if (scfmt == AFMT_S16_NE) {
	    hfmt = AFMT_SHORT;
	} else if (scfmt == AFMT_S16_BE || scfmt == AFMT_S16_LE) {
	    hfmt = AFMT_SHORT_SWAB;
	} else if (scfmt == AFMT_S8) {
	    hfmt = AFMT_CHAR;
	} else if (scfmt == AFMT_U8) {
	    hfmt = AFMT_CHAR_OFFS;
	} else if (scfmt == AFMT_MU_LAW) {
	    hfmt = AFMT_ULAW;
	} else {
	    fprintf(stderr, "AUOpen: actual format 0x%x is unknown\n", scfmt);
	    return NULL;
	}

	/* Set the channel mode.
	   OSS programming manual says you must do this BEFORE the sampling 
	   rate (http://www.4front-tech.com/pguide/audio.html) */
	if(hchans == 1)		stereo = 0;
	else			stereo = 1;
	AUIOCTL(audiofid, SNDCTL_DSP_STEREO, &stereo);
	/* check for cards that insist on stereo only */
	if (stereo && (hchans == 1)) {
	    fprintf(stderr, "AUopen: warning: promoting mono to stereo!\n");
	    hchans = 2;	/* hardware chans != logical chans! */
	}

	/* set sample rate */
    	samplerate = (int)srate;
	AUIOCTL(audiofid, SNDCTL_DSP_SPEED, &samplerate);
	if(samplerate != (int)srate)  {
	    fprintf(stderr, "AUopen: warning: %sing data of %dHz at %dHz\n",
		    mode==AIO_INPUT?"record":"play", (int)srate, (int)samplerate);
	}

	/* get buffer size and allocate space */
	AUIOCTL(audiofid, SNDCTL_DSP_GETBLKSIZE, &bufsiz);
	/*	fprintf(stderr, "AUOpen: bufsize = %d\n", bufsiz); */
	if(bufsiz < 0)
	    AUError("AUOpen: SNDCTL_DSP_GETBLKSIZE bufsiz<0");

	/* Try to get the device to sync up on record */
	if (mode == AIO_INPUT) {
/*	  AUIOCTL(audiofid, SNDCTL_DSP_SYNC, 0); */
/*	  AUIOCTL(audiofid, SNDCTL_DSP_RESET, 0); */
/*	  AUIOCTL(audiofid, SNDCTL_DSP_POST, 0);  */
	}

	/* allocate the descriptor */
	au = TMMALLOC(AUSTRUCT, 1, "AUOpen");
	au->errorCount = 0;
	au->totalDroppedFrames = 0;
	au->totalFrames = 0;
	au->mode   = mode;	au->port  = audiofid;
	au->ichans = chans;	au->ochans = chans;	/* always same */
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
    }
    return au;
}

long	AUWrite(AUSTRUCT *au, void *buf, long frames)
{   /* write some sound to the open audio IO channel, per description */
    int bytesperframe;
    assert((au->mode) == AIO_OUTPUT);
    bytesperframe = au->ochans * AUSampleSizeOf(au->oformat);
    if(au->ichans != au->ochans || au->iformat != au->oformat)  {
	long thistime, todo, done, remain = frames;
	
	done = 1;
	while(done > 0 && remain > 0)  {
	    thistime = MIN(remain, au->bufSize);
	    CvtChansAndFormat(buf, au->buf, au->ichans, au->ochans, 
			      au->iformat, au->oformat, thistime);
	    todo = thistime*bytesperframe;
	    done = 0;
	    do {
		done = write(au->port, ((char *)au->buf)+done, 
			     thistime*bytesperframe - done);
		todo -= done;
		if (todo > 0) {fprintf(stderr, "AUWrite: wrote %ld leaving %ld bytes\n", 
				       done, todo); }
	    } while (done > 0 && todo > 0);
	    if (todo > 0) {
		fprintf(stderr, "AUWrite: failed to write %ld of %ld bytes\n", 
			todo, thistime*bytesperframe);
	    }
	    buf = (void *)(((char *)buf)+ thistime * au->ichans \
			   * AUSampleSizeOf(au->iformat));
	    remain -= thistime;
	}
/* fprintf(stderr, "buf[0]=0x%x abuf[0]=0x%x\n", ((short *)buf)[0], ((short *)au->buf)[0]); */
    } else {
	/* write(au->port, buf, frames*bytesperframe); */
	int todo = frames*bytesperframe;
	int done = 0;
	/*	do {	*/
	    done = write(au->port, ((char *)buf)+done, 
			 frames*bytesperframe - done);
	    todo -= done;
	    if (todo > 0) {fprintf(stderr, "AUWrite: wrote %d leaving %d bytes\n", 
				   done, todo); }
	    /*	} while (done > 0 && todo > 0);  */
	if (todo > 0) {
	    fprintf(stderr, "AUWrite: failed to write %ld of %ld bytes\n", 
		    (long) todo, frames*bytesperframe);
	}
    }
    au->totalFrames += frames;
    return frames;		/* presumably */
}

long	AURead(AUSTRUCT *au, void *buf, long frames)
{   /* read some sound from the open audio IO channel, per description */
    int bytesperframe;
    assert(au->mode == AIO_INPUT);
    bytesperframe = au->ichans * AUSampleSizeOf(au->iformat);
    if(au->ichans != au->ochans || au->iformat != au->oformat)  {
	long got, thistime, remain = frames;

	while(remain)  {
	    thistime = MIN(remain, au->bufSize);
	    got = read(au->port, au->buf, thistime*bytesperframe);
	    if (got != thistime*bytesperframe) {
	      fprintf(stderr, "AURead: wanted %ld of %ld = %ld, got %ld\n", 
		      thistime, (long) bytesperframe, thistime*bytesperframe, got);
	    }
	    CvtChansAndFormat(au->buf, buf, au->ichans, au->ochans, 
			      au->iformat, au->oformat, thistime);
	    buf = (void *)(((char *)buf)+ thistime * au->ochans \
			   * AUSampleSizeOf(au->oformat));
	    remain -= thistime;
	}
    } else {
	extern int errno;
      int got = read(au->port, buf, frames*bytesperframe);
      if (got != frames*bytesperframe) {
	fprintf(stderr, "AURead: wanted %ld of %ld = %ld, got %ld (errno=%d)\n", 
		(long) frames, (long) bytesperframe, (long) frames*bytesperframe, (long) got, errno);
	if (got == -1) {
	  perror("AURead");
	  got = 0;
	}
	frames = got / bytesperframe;
      } 
    }
    au->totalFrames += frames;
    return frames;		/* we presume */
}

void	AUClose(AUSTRUCT *au, int now)
{   /* close an audio IO channel. if <now> is nonzero, stop immediately
       else play out any buffered samples */
  /*  fprintf(stderr, "AUClose...\n"); */
    if(now == 0 \
       && au->mode == AIO_OUTPUT) {	/* wait for port to drain on output */
	ioctl(au->port, SNDCTL_DSP_SYNC, 0);    /* ? for linux */;
    } else {
	ioctl(au->port, SNDCTL_DSP_RESET, 0);	/* stop now */
    }
    close(au->port);
    if(au->buf)	free(au->buf);
    free(au);
    /*  fprintf(stderr, "AUClose...done\n"); */
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
