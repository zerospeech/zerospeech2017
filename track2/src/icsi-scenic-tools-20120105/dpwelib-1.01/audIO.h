/***********************************************************************\
*	audIO.h								*
*	Interface definition for new hardware-indep audio IO interface	*
*	based on lofiplay etc						*
*	dpwe 13mar92							*
\***********************************************************************/

#include "dpwelib.h"	/* for stdio, PARG etc */

#ifndef AUSTRUCT_DEFINED
typedef void *AUSTRUCT;		/* opaque handle */
#define AUSTRUCT_DEFINED
#endif /* AUSTRUCT_DEFINED */

/* mode flags */
#define AIO_INPUT	1
#define AIO_OUTPUT	2

/* format type flags */
#define AU_BYTE_SIZE_MASK	15	/* AUMT && SIZE_MASK == # bytes */
#define _ONEBYTE		1
#define _TWOBYTE		2
#define _FOURBYTE		4
#define _EIGHTBYTE		8
#define _LINEAR			0
#define	_FLOAT			32
#define _ALAW			64
#define _ULAW			128
#define _OFFSET			256
#define _BYTESWAP		512
/* so.. */
#define AFMT_CHAR  	(_ONEBYTE+_LINEAR)
#define AFMT_CHAR_OFFS  (_ONEBYTE+_LINEAR+_OFFSET)
#define AFMT_ALAW  	(_ONEBYTE+_ALAW)
#define AFMT_ULAW  	(_ONEBYTE+_ULAW)
#define AFMT_SHORT 	(_TWOBYTE+_LINEAR)
#define AFMT_LONG  	(_FOURBYTE+_LINEAR)
#define AFMT_SHORT_SWAB 	(_TWOBYTE+_LINEAR+_BYTESWAP)
#define AFMT_LONG_SWAB  	(_FOURBYTE+_LINEAR+_BYTESWAP)
#define AFMT_FLOAT 	(_FOURBYTE+_FLOAT)
#define AFMT_DOUBLE  	(_EIGHTBYTE+_FLOAT)

/* sick of no protos on DECstation */
#undef PARG
#define PARG(a)   a

AUSTRUCT *AUOpen PARG((int format, double srate, int chans, long desbuf, 
		       int mode));
    /* Open a new audio IO channel.  
       Define data format, #channels and sampling rate. 
       Request that output buffer be desbuf frames long (0 gives default).
       <mode> is AIO_INPUT or AIO_OUTPUT  */

long	AUWrite PARG((AUSTRUCT *auch, void *buf, long frames));
    /* write some sound to the open audio IO channel, per description */

long	AURead PARG((AUSTRUCT *auch, void *buf, long frames));
    /* read some sound from the open audio IO channel, per description */

long	AUBufConts PARG((AUSTRUCT *auch));
    /* see how many sample frames are currently in audio buffer. 
       (can be used for synchronization, timing etc). */

long	AUBufSpace PARG((AUSTRUCT *auch));
    /* see how many sample frames are currently in audio buffer. 
       (can be used for synchronization, timing etc). (SGI only) */

void	AUClose PARG((AUSTRUCT *auch, int now));
    /* close an audio IO channel.  <now> nonzero kills play immediately */

long    AUDroppedFrames PARG((AUSTRUCT *au));
    /* return the number of frames that have been under/overflowed 
       on this port (SGI only) */
