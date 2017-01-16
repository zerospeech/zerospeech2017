// This may look like C code, but it is really -*- C++ -*-
//  
// Sound.h
// 
// C++ interface to soundfiles
// based on snd.c calls
// dpwe@media.mit.edu 1994nov02
// $Header: /u/drspeech/repos/dpwelib/Sound.H,v 1.10 2011/03/09 01:35:02 davidj Exp $

#ifndef _SOUND_H
#define _SOUND_H

#ifdef __MWERKS__
#include <fp.h>
#endif /* __MWERKS__ */
#include <math.h>

extern "C" {
#include <dpwelib/snd.h>
}

typedef unsigned char byte;

#ifdef USE_ENUMS
enum  Format {
    Sfmt_SHORT = SFMT_SHORT,
    Sfmt_LONG  = SFMT_LONG,
    Sfmt_FLOAT = SFMT_FLOAT
};
enum  ChanMode {
    Scmd_MONO  = SCMD_MONO,
    Scmd_CHAN0 = SCMD_CHAN0,
    Scmd_CHAN1 = SCMD_CHAN1,
    Scmd_CHAN2 = SCMD_CHAN2,
    Scmd_CHAN3 = SCMD_CHAN3,
    Scmd_CHAN4 = SCMD_CHAN4,
    Scmd_CHAN5 = SCMD_CHAN5,
    Scmd_CHAN6 = SCMD_CHAN6,
    Scmd_CHAN7 = SCMD_CHAN7
};
#define EnumFormat	enum Format
#define	EnumChanMode	enum ChanMode

#else /* !USE_ENUMS */
#define Format int
#define ChanMode int

#define EnumFormat	int
#define EnumChanMode	int

#define Sfmt_SHORT	SFMT_SHORT
#define Sfmt_LONG	SFMT_LONG
#define Sfmt_FLOAT	SFMT_FLOAT
#define Scmd_MONO	SCMD_MONO
#define Scmd_CHAN0	SCMD_CHAN0
#define Scmd_CHAN1	SCMD_CHAN1
#define Scmd_CHAN2	SCMD_CHAN2
#define Scmd_CHAN4	SCMD_CHAN3

#endif /* USE_ENUMS */


int SfmtSize(int format);
    /* return the bytes per sample of a particular data format */

class Sound {
private:
// private buffer for MinMaxFrames
    float *scanbuf;
    long scanbuflen;
// Details of the source or destination of the bits
    SOUND *snd;
public:
// Constructors & destructor
    Sound(const char* mode = "r", 
	  double sampRate = 44100.0, int chans = 1, Format fmt = Sfmt_SHORT);
                                        // create a new sound object
    ~Sound(void);
    int reset(const char* mode = "r", double sampRate = 44100.0, 
	      int chans = 1, Format fmt = Sfmt_SHORT);  // re-initialize
// Accessors
    int getChannels(void)		{ return sndGetUchans(snd); };
    int setChannels(int chans)		{ sndSetUchans(snd, chans); 
					  return sndGetUchans(snd); };
    ChanMode getChanMode(void)		{ return (EnumChanMode)sndGetChanmode(snd); };
    ChanMode setChanMode(ChanMode mode) { sndSetChanmode(snd, (int)mode); 
					  return (EnumChanMode)sndGetChanmode(snd); };
    Format getFormat(void)	    { return (EnumFormat)sndGetUformat(snd);};
    Format setFormat(Format fmt)    { sndSetUformat(snd, (int)fmt);
				      return (EnumFormat)sndGetUformat(snd);};
    int getSFformat(void)	    { return sndGetSFformat(snd);};
    int setSFformat(int sffmt)      { sndSetSFformat(snd, sffmt);
				      return sndGetSFformat(snd);};
    int setSFformatByName(const char *name) { sndSetSFformatByName(snd, name);
				      return sndGetSFformat(snd);};
    double getSampleRate(void)		{ return sndGetSrate(snd); };
    double setSampleRate(double srate)	{ sndSetSrate(snd, srate);
					  return sndGetSrate(snd); };
    long getFrames(void)		{ return sndGetFrames(snd); };
    long setFrames(long frms)		{ sndSetFrames(snd, frms);
					  return sndGetFrames(snd); };
            // does setFrames make sense?  Not easily changed.
            // But could be handy for hinting the size of an output 
            // file to be written.
    char *getFilename(void)		{ return snd->path; }
    char getMode(void)			{ return snd->mode; };
    char setMode(char modechr)		{  if( /* flags == 0 && */
					      (modechr == 'r'  
					       || modechr=='w')) 
					       snd->mode=modechr;
					   return snd->mode; };
    int isOpen()                        { return snd->isOpen; };

// debugging
    void print(FILE *stream=stdout, const char *tag=NULL) {
	sndPrint(snd, stream, tag);
    }

// File operations
    int open(const char *filename);			// open file for input, 
                                                // or create for output.
                                                // Returns one on success.
    int next(void) { return sndNext(snd);} 	// Moves to the start 
						// of the next unaccessed
						// sound fragment; usually 
						// not needed, but some 
						// streams can contain
						// multiple sound files
						// (e.g. PCM/Abb).
						// Return 1 if available,
						// else 0.
    void close(void);				// Force a file close.
                                                // must be followed by delete
    long readFrames(void *buf, long frames);	// read sound data from file
    long writeFrames(void *buf, long frames);	// write sound data to file
    long frameTell(void);			// frame index of next to read
    long frameSeek(long frame, int mode=SEEK_SET); // move file point to pos
    int frameFeof(void);			// return nonzero if at EOF

    long MinMaxFrames(long startframe, long frames, float *buf);
    // scan for the minimum and maximum values over the specified range
    // return always as two floats, for now anyway

    // hack to interface to older code - DON'T USE!
    SOUND *_giveSOUND(void) { return snd;}
};

#endif /* _SOUND_H */
