/***********************************************************************\
*	audIOdecwmm.c							*
*	New hardware-indep audio IO interface intended to be portable	*
*	based on lofiplay etc						*
*	dpwe 1993nov17, 13mar92						*
*	SGI AL version - 1994mar30					*
\***********************************************************************/
#ifndef DEC
#define DEC	/* needed to get WAVE_OPEN_SHARABLE under OSF/1 2.0 */
#endif /* DEC */  /* added 1994jun28 */
#include <mme/mme_api.h>

/* from /usr/opt/MMEDEV110/examples/mme/wave_audio/playfile.c */
/* Buffer tracking structure */
typedef struct  {
  void *lpData;
  long conts;
  BOOL available;
} buffer_t;
#define MAX_NUM_BUFFERS	4

typedef struct {		/* descriptor for audio IO channel */
    int	mode;			/* AU_INPUT or AU_OUTPUT */
    int chans;			
    int format;
    WAVEHDR  WaveHdr;
    HWAVEOUT port;
    int	NumBuffersAllocated;
    int buffer_size;
	int open_semaph;
    buffer_t	Buffers[MAX_NUM_BUFFERS+1];
} AUSTRUCT;

HWAVEOUT horribleGlobalHack = NULL;	/* cannot close & reopen ports? */

#define AUSTRUCT_DEFINED	/* stop redefinition in audIO.h */

#include "audIO.h"	/* for stdio, PARG etc */

#include <assert.h>

#ifndef DBGFPRINTF   /* often defined in dpwelib.h */
#ifdef DEBUG
#define DBGFPRINTF(a)	fprintf a
#else /* !DEBUG */
#define DBGFPRINTF(a)	/* */
#endif /* DEBUG */
#endif /* !DBGFPRINTF */

#define DFLT_BUFTIME	(0.25)		/* default buffer length in secs */

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif /* BITS_PER_BYTE */

/***** BUFFER OPERATIONS *****/
#define MAX_ERR_STRING	128
#define BUFFER_SIZE 		4096

#define MMNew_(type) (type*)mmeAllocMem(sizeof(type))

static void exit_on_mme_error (MMRESULT err_code)
{   /* handle error codes from library */
  if (err_code != MMSYSERR_NOERROR) {
      char error_text[MAX_ERR_STRING + 1];
      waveOutGetErrorText (err_code, error_text, MAX_ERR_STRING);
      fprintf(stderr, "audIOdecwmm:%s\n", error_text);
      exit (1);
  }
}

static BOOL queue_buffer (AUSTRUCT *au, int bufnum, void *data, long size)
{   /* take some data, put it in a specified buffer, queue that buffer */
  void *lpdata = (au->Buffers[bufnum]).lpData;

  memcpy(lpdata, data, size);
  DBGFPRINTF((stderr, "q_b: data = %lx, size = %ld, conts = %lx\n", 
	      data, size, *(long *)lpdata));
  /* Setup Header */
  au->WaveHdr.lpData          = lpdata;
  au->WaveHdr.dwBufferLength  = size;
  /* Add the buffer to the driver queue */
  exit_on_mme_error (waveOutWrite (au->port, &(au->WaveHdr), sizeof(WAVEHDR)));
  au->Buffers[bufnum].available = FALSE;
  au->Buffers[bufnum].conts     = size;
  return TRUE;
} /* queue_buffer */

static int find_avai_buf(AUSTRUCT *au)
{
  int i;
  buffer_t * buffers = au->Buffers;
  int num_buffers = au->NumBuffersAllocated;

  for(i = 0; i < num_buffers; ++i)  {
      if (buffers[i].available == TRUE)	{
	  return i;
      }
  }
  return -1;	/* nothing found */
}

static long queue_all_available_buffers (AUSTRUCT *au, void *data, long size)
{	/* return data used */
  HANDLE hWaveOut = (HANDLE)(au->port);
  buffer_t * buffers = au->Buffers;
  long remain, sent, thistime;
  int i;

  sent = 0;
  remain = size;
  DBGFPRINTF((stderr, "q_aa: data = %lx, size = %ld, conts = %lx\n", 
	      data, size, *(long *)data));
  while( (i = find_avai_buf(au))>= 0 && remain > 0)  {
      thistime = MIN(remain, au->buffer_size);
      DBGFPRINTF((stderr, "Queueing buffer %d", i));
      queue_buffer (au, i, data, thistime);
      sent += thistime;
      data = (void *)(((char *)data) + thistime);
      remain -= thistime;
  }
  return sent;
}

static void allocate_buffers(AUSTRUCT *au, long size)
{
  HANDLE hWaveOut = (HANDLE)(au->port);
  buffer_t * buffers = au->Buffers;

  au->buffer_size = size;
  au->NumBuffersAllocated = 0;
  buffers[0].lpData = (char *)mmeAllocBuffer (size * MAX_NUM_BUFFERS);
  if ( !buffers[0].lpData )  {
      fprintf(stderr, "audIOdecwmm: Cannot allocate buffer memory\n");
      exit (1);
  }
  while (au->NumBuffersAllocated < MAX_NUM_BUFFERS) {
      buffers[au->NumBuffersAllocated].lpData = (char *)
	  ((unsigned long)buffers[0].lpData + size*au->NumBuffersAllocated);
      buffers[au->NumBuffersAllocated++].available = TRUE;
      buffers[au->NumBuffersAllocated].conts = 0;
    }
} /* allocate_buffers */


/* find a buffer in the buffer list */
static int find_buffer (void * lpData, buffer_t * buffers, int num_bufs)
{
  int i;

  for (i = 0; i < num_bufs; i++)
    if (buffers[i].lpData == lpData)
      return i;
  fprintf(stderr, "audIOdecwmm: Could not find buffer to mark not in use\n");
  exit (1);
} /* find_buffer () */


/* Return number of buffers in use */
static int buffers_in_use(AUSTRUCT *au)
{
  int i;
  int n = 0;
  buffer_t * buffers = au->Buffers;
  int num_buffers = au->NumBuffersAllocated;

  for(i = 0; i < num_buffers; ++i)  {
      if (buffers[i].available != TRUE)	{
	  ++n;
      }
  }
  return n;
}

static int AUSampleSizeOf(int format)
{   /* return number of bytes per sample in given fmt */
    return(format & AU_BYTE_SIZE_MASK);
}	/* copied from SFSampleSizeOf */

/* Callback function that receives the driver messages intended for
** this application */
static void play_callback (HANDLE hWaveOut,
			   UINT   wMsg,
			   DWORD  dwInstance,
			   LPARAM lParam1,
			   LPARAM lParam2)
{
  LPWAVEHDR WaveHdr_p = (LPWAVEHDR) lParam1;
  AUSTRUCT *au = (AUSTRUCT *)dwInstance;
  int i;

  switch(wMsg)
    {
    default:
      DBGFPRINTF((stderr,"Unknown callback message %d\n", wMsg));
      exit(1);
      break;
    case WOM_OPEN:
      DBGFPRINTF((stderr,"WOM_OPEN\n"));
/*      print_line ("Playing --> Ctrl-C to pause; Ctrl-C Ctrl-C to exit");
      State = Playing;
 */
	  au->open_semaph = 1;
      break;
    case WOM_DONE:
      DBGFPRINTF((stderr,"WOM_DONE(%d)\n",
		  find_buffer(WaveHdr_p->lpData, au->Buffers, 
			      au->NumBuffersAllocated)));
      i = find_buffer(WaveHdr_p->lpData, au->Buffers, au->NumBuffersAllocated);
      au->Buffers[i].available= TRUE;
      au->Buffers[i].conts    = 0;
      break;
    case WOM_CLOSE:
      DBGFPRINTF((stderr, "WOM_CLOSE\n"));
/*      State = Closed;		*/
	  au->open_semaph = 0;
      break;
   }
}/* play_callback() */

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

	/* allocate the descriptor */
/*	au = TMMALLOC(AUSTRUCT, 1, "AUOpen");  */
	au = MMNew_(AUSTRUCT);
	if(au == NULL)  {
		fprintf(stderr, "AUOpen: mmeAllocMem failed - no server?\n");
		++err;
	}

    /* no Record at present */
	if(mode == AIO_INPUT) {
	    fprintf(stderr, "audIOdecwmm:AUOpen: Input(record) not yet supported on DEC wmm (sorry).\n");
	    return NULL;
	}

    if(err == 0)  {	/* actual SGI-specific port initialization */
	/* from /usr/opt/MMEDEV110/examples/mme/wave_audio/playfile.c */
	long width = AUSampleSizeOf(format);
	DWORD dwFlags = CALLBACK_FUNCTION | WAVE_OPEN_SHAREABLE;
	PCMWAVEFORMAT 	*PcmOutp;
	UINT		uDeviceId = WAVE_MAPPER;
	long		buffer_size;

	PcmOutp = MMNew_(PCMWAVEFORMAT);

	au->mode   = mode;	au->chans = chans;
	au->format = format;	au->port  = NULL;
	au->open_semaph = 0;

	/* initialize header struct */
	au->WaveHdr.dwBytesRecorded = 0;
	au->WaveHdr.dwUser          = 0;
	au->WaveHdr.dwFlags         = 0;
	au->WaveHdr.dwLoops         = 0;
	au->WaveHdr.lpNext          = NULL;
	au->WaveHdr.reserved        = 0;

	/* open the audio */
	PcmOutp->wf.nChannels = chans;
	PcmOutp->wf.nSamplesPerSec = srate;
	PcmOutp->wBitsPerSample = width*BITS_PER_BYTE;
	PcmOutp->wf.nBlockAlign = (PcmOutp->wBitsPerSample+7)/8
	    * PcmOutp->wf.nChannels;
	PcmOutp->wf.nAvgBytesPerSec = PcmOutp->wf.nBlockAlign
	    * PcmOutp->wf.nSamplesPerSec;
	PcmOutp->wf.wFormatTag = WAVE_FORMAT_PCM;

/* on Alpha:
Device 0: format c chans 2 support c
Formats: 11kM16 11kS16 (EXTERNAL)
Supt: vol lrvol 
Device 1: format 1fff chans 1 support 4 (INTERNAL SPEAKER)
Formats: 11kM8 11kS8 11kM16 11kS16 22kM8 22kS8 22kM16 22kS16 44kM8 44kS8 44kM16 44kS16 8kMu8 
Supt: vol 
*/
/*	uDeviceId = 0; /* force external port ?? */

	DBGFPRINTF((stderr, "waveOutOpen(%lx, %d, %lx, %lx, %lx, %d)\n", 
		    &(au->port),uDeviceId, &PcmOutp->wf, play_callback, 
		    (long)au, dwFlags));
    if(horribleGlobalHack)  {
		au->port = horribleGlobalHack;
	} else {
		if(waveOutOpen(&(au->port),uDeviceId, (LPWAVEFORMAT)PcmOutp, 
		       play_callback, (long)au, dwFlags))   	{
			/* error initializing hardware */
			/* free(au); */
			mmeFreeMem(PcmOutp);
			mmeFreeMem(au);
			return NULL;
		} else {
			horribleGlobalHack = au->port;
		}
	}
	DBGFPRINTF((stderr, "port = %lx\n", au->port));
	if (uDeviceId == WAVE_MAPPER) {
	    DBGFPRINTF((stderr, "waveOutGetID(%lx, %lx)\n", au->port, &uDeviceId));
	    waveOutGetID (au->port, &uDeviceId);
	    DBGFPRINTF((stderr, "Selected device %d for output\n", uDeviceId));
	}
	buffer_size = ((BUFFER_SIZE-1) / PcmOutp->wf.nBlockAlign + 1) 
	    * PcmOutp->wf.nBlockAlign;
	/* Allocate all of our data buffers */
	mmeFreeMem(PcmOutp);
	allocate_buffers(au, 2*buffer_size);
	DBGFPRINTF((stderr, "done\n"));
    }
    return au;
}

long  AUWrite(AUSTRUCT *au, void *buf, long frames)
{   /* write some sound to the open audio IO channel, per description */
    long byPerSamp = au->chans * AUSampleSizeOf(au->format);
    long thistime, done = 0;
    long remain	= frames;
    char *cbuf = (char *)buf;
    int num_buffers = au->NumBuffersAllocated;

    DBGFPRINTF((stderr, "AUWrite(%lx, %lx(=&%lx), %lx)\n", au, buf, 
		*(long *)buf, frames));
    assert((au->mode) == AIO_OUTPUT);
    while(remain)  {
	while(buffers_in_use(au)==num_buffers)  {  /* loop until some buffers free */
	    mmeWaitForCallbacks(); /* block so we don't hog 100% of the CPU */
	    mmeProcessCallbacks();
	}
	thistime = (queue_all_available_buffers(au, cbuf+byPerSamp*done, 
						remain*byPerSamp))/ byPerSamp;
	done += thistime;
	remain -= thistime;
	    mmeWaitForCallbacks(); /* block so we don't hog 100% of the CPU */
	    mmeProcessCallbacks();
    }
    return frames;	/* presumably */
}

long	AURead(AUSTRUCT *au, void *buf, long frames)
{   /* read some sound from the open audio IO channel, per description */
    assert(au->mode == AIO_INPUT);
    fprintf(stderr, "AURead: not yet implemented\n");
    return 0*frames;	/* we presume */
}

void	AUClose(AUSTRUCT *au, int now)
{  /* close an audio IO channel. if <now> is nonzero, stop immediately
      else play out any buffered samples */
    DBGFPRINTF((stderr, "AUClose(%lx, %d)\n", au, now));
    if(!(now && au->mode == AIO_OUTPUT)) { 	/* flush output */
		/* do something */
		while(buffers_in_use(au))  {
			mmeWaitForCallbacks(); /* block so we don't hog 100% of the CPU */
			mmeProcessCallbacks();
		}
	}
	waveOutReset(au->port);
	while(buffers_in_use(au))  {
		mmeWaitForCallbacks(); /* block so we don't hog 100% of the CPU */
		mmeProcessCallbacks();
	}
#ifndef HORRIBLE_GLOBAL_HACK
    waveOutClose(au->port);
	while(au->open_semaph)  {
		mmeWaitForCallbacks(); /* block so we don't hog 100% of the CPU */
		mmeProcessCallbacks();
    }
	horribleGlobalHack = NULL;
#endif /* !HORRIBLE_GLOBAL_HACK */

    mmeFreeBuffer(au->Buffers[0].lpData);
    /* free(au); */
    mmeFreeMem(au);
}

long AUBufConts(AUSTRUCT *au)
{   /* see how many sample frames are currently in audio buffer.
       (can be used for synchronization, timing etc). */
    /* very coarse & approx .. ugh */
    long byPerSamp = au->chans * AUSampleSizeOf(au->format);

    return au->buffer_size/byPerSamp * buffers_in_use(au);
}

long AUBufSpace(AUSTRUCT *au)
{   /* see how many sample frames are currently free in the audio buffer.
       (can be used for synchronization, timing etc). */
    /* very coarse & approx .. ugh */
    long byPerSamp = au->chans * AUSampleSizeOf(au->format);

    return au->buffer_size/byPerSamp 
	          * (au->NumBuffersAllocated - buffers_in_use(au));
}

long AUDroppedFrames(AUSTRUCT *au)
{   /* return the number of frames that have been under/overflowed 
       on this port (SGI/Sun only) */
    long droppedframes = 0;

    return droppedframes;
}
