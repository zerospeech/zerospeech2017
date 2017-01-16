/***************************************************************\
*   snd.h							*
*   A new soundfile interface layer				*
*   Header file							*
*   1993mar03  dpwe  after convo.c				*
\***************************************************************/

#ifndef _SND_H

#define _SND_H
/* std libs */
#include "dpwelib.h"
/* personal libs */
#include "sndf.h"

/* constants */
#define SND_BUFSZ	16384	/* frames held from disk */
#define SND_STDIO_PATH	"-"	/* this filename causes access on stdin/out */
#define SND_AUDIO_PATH	"@"	/* this filename connects to real-time IO */

/* typedefs */
typedef struct _sound {	/* sound file and descriptor */
    char	*path;		/* file info */
    FILE	*file;
    char	mode;
    int		isStdio;	/* TRUE if file is pipe to stdin/stdout */
    int		isRealtime;	/* TRUE if connected to audIO, not file */
    int 	isCmdPipe;	/* TRUE if file is a popen pipe */
    SFSTRUCT	*snd;		/* raw header info read from file */
    long	frames;		/* fields derived from header */
    float	duration;
    int 	userFormat;	/* user sees data in this format */
    int 	userChans;	/* user sees this many channels of data */
    int		chanMode;	/* flag to subselect channel */
    int 	flags;
    int		isOpen;		/* flag that file is actually active */
    int		touchedCurrent;	/* flag that the current file is touched
				   & will be skipped on next sndNext() */
    int		defaultSFfmt;	/* what to reset the sndf format to 
				   before re-reading files in stream (ugh!) */
    /* buffer unconverted frames from disk */
    short	*buf;
    long	bufSize;	/* actual frames space in *buf */
    long	bufCont;	/* amt of valid data in *buf */
    /* nice little pointer for dspB */
    void 	*data;		/* pointer to memory buffer with data !? */
    /* to accommodate sndf's ugly soundfile typing stuff */
    int		sffmt;		/* the format we want to open this file in */
    char	*sffmtname;	/* name of the format we want for this file */
    /* fix to have gain inside format conversion (2005-03-17) */
    float	userGain;	/* gain factor transparently applied before type conversion */
} SOUND;

/* constants for chanMode */
enum {
    SCMD_MONO, 		/* mix channels together to convert them */
    SCMD_CHAN0, 	/* mono of multichannel sound is just chan 0 (L) */
    SCMD_CHAN1,		/* ..etc */
    SCMD_CHAN2,
    SCMD_CHAN3,
    SCMD_CHAN4,
    SCMD_CHAN5,
    SCMD_CHAN6,
    SCMD_CHAN7		/* assumed in order in snduConvertChans */
};

/* defaults for open-write (new) file - use sndf.h defaults */
#define DFLT_CHNS	SNDF_DFLT_CHANS
#define DFLT_FMT	SNDF_DFLT_FORMAT
#define DFLT_SR		SNDF_DFLT_SRATE

SOUND *sndNew PARG((SOUND *snd));
    /* just allocate a new SOUND structure
       Clone format parameters (chans, srate) from <snd> if any. */
void sndFree PARG((SOUND *snd));
    /* destroy sound, including closing any file */
SOUND *sndOpen PARG((SOUND *snd, const char *path, const char *mode));
    /* open sound.  If <snd> is NULL, allocate new one.
       If <path> is NULL, take from snd as passed in (or err).
       <mode> is "r" or "w" only */
int sndNext PARG((SOUND *snd));
    /* returns 1 if there is another soundfile to read in the
       currently open file.  Normally returns 1 before any accesses to
       the current soundfile, and 0 afterwards (i.e. it's safe to call
       once on a 'conventional', single-sound file), but for
       multi-file streams, may return 1 several times; each time it is
       called, subsequent accesses are for the next soundfile. 
       On write, indicates the end of one file, and that another is 
       likely to follow. */
void sndClose PARG((SOUND *snd));
    /* close file attached to snd */
void sndPrint PARG((SOUND *snd, FILE *stream, const char *tag));
    /* print a text description of a snd struct */
void sndDebugPrint PARG((SOUND *snd, FILE *stream, const char *tag));
    /* print a detailed description of the snd struct fields */
long sndReadFrames PARG((SOUND *snd, void *buf, long frames));
    /* read <frames> sample frames as floats into the buffer,
       return number actually read. */
long sndWriteFrames PARG((SOUND *snd, void *buf, long frames));
    /* write <frames> sample frames (floats) to file from the buffer,
       return number actually written. */
int sndSetChans PARG((SOUND *snd, int chans));
int sndGetChans PARG((SOUND *snd));
    /* set number of chans for output, read back for input and output */
int sndSetChanmode PARG((SOUND *snd, int mode));
int sndGetChanmode PARG((SOUND *snd));
    /* set the SCMD_ mode which determines how multichannel<>mono convs done */
int sndSetUchans PARG((SOUND *snd, int chans));
int sndGetUchans PARG((SOUND *snd));
    /* optional global gain to be optimized within type conversions */
float sndSetUgain PARG((SOUND *snd, float gain));
float sndGetUgain PARG((SOUND *snd));
    /* set `user' chans i.e. the number of channels that sndRead returns, 
       regardless of the actual number of channels in the sound */
int sndSetFormat PARG((SOUND *snd, int format));
int sndGetFormat PARG((SOUND *snd));
    /* set on-disk format for output, query on-disk format for in/output */
int sndSetUformat PARG((SOUND *snd, int format));
int sndGetUformat PARG((SOUND *snd));
    /* set `user' format i.e. what sndRead returns or what sndWrite accepts */
long sndSetFrames PARG((SOUND *snd, long frames));
long sndGetFrames PARG((SOUND *snd));
    /* total number of frames in the file */
double sndSetSrate PARG((SOUND *snd, double srate));
double sndGetSrate PARG((SOUND *snd));
    /* sampling rate of the file */
int sndSetSFformat PARG((SOUND *snd, int fmt));
int sndSetSFformatByName PARG((SOUND *snd, const char *name));
int sndGetSFformat PARG((SOUND *snd));
    /* sound file format to be used with the file */
long sndFrameTell PARG((SOUND *snd));
    /* return frame index of next sample frame to be read or written */
long sndFrameSeek PARG((SOUND *snd, long frameoffset, int whence));
    /* seek to a certain point in the file.  Args as fseek(3s) */
int sndFeof PARG((SOUND *snd));
    /* non-zero if we have hit eof on reading a soundfile */
int sndBytesPerFrame PARG((SOUND *snd));
    /* bytes for each sample frame, in the user-visible format */

#endif /* _SND_H */
