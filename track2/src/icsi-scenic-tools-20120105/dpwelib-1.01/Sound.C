//
// Sound.cc
// 
// C++ interface to soundfiles
// based on snd.c
// dpwe@media.mit.edu 1994nov02
// $Header: /u/drspeech/repos/dpwelib/Sound.C,v 1.6 2011/03/09 01:35:02 davidj Exp $

#include "Sound.H"
extern "C" {
#include <dpwelib/genutils.h>	// for strdup on Mac
}
#include <assert.h>
#include <stdio.h>

int Sound::reset(const char* mode, double sampRate, int chans, Format fmt)
{   /* Recreate the snd field */
    DBGFPRINTF((stderr, "Sound0x%lx::reset(\"%s\",%.1f,%d,%d)\n", this, mode, sampRate, chans, (int)fmt));
    if(*mode != 'r' && *mode != 'w')  {
	// Called illegally
	return 0;
    } else {
	int sffmt = SFF_UNKNOWN;
	char *sffmtname = NULL;

	if(snd) {
	    sffmt = snd->sffmt;
	    if (snd->sffmtname)	sffmtname = strdup(snd->sffmtname);
	    sndFree(snd);
	}
	snd = sndNew(NULL);
	snd->snd->samplingRate = sampRate;
	snd->snd->channels   = chans;
	snd->snd->format     = fmt;
	snd->mode = *mode;
	snd->sffmt = sffmt;
	snd->sffmtname = sffmtname;
	return 1;
    }
}
    
Sound::Sound(const char* mode, double sampRate, int chans, Format fmt)
{   /* Create a new sound object.  Default arguments specified for 
       each parameter in header file (the C++ way). */
    DBGFPRINTF((stderr, "Sound0x%lx::Sound()\n", this));
    scanbuf = NULL;
    scanbuflen = 0;
    snd = NULL;
    if(reset(mode, sampRate, chans, fmt)==0) {
	// Called illegally
	delete this;
    }
}

Sound::~Sound(void)
{   /* close the soundfile, destroy the sound object */
    DBGFPRINTF((stderr, "Sound0x%lx::~Sound()\n", this));
    if(scanbuf)	{
	delete [] scanbuf;
	scanbuf = NULL;
	scanbuflen = 0;
    }
    close();
}

int SfmtSize(int format)
{   /* return the bytes per sample of a particular data format */
    return (format & 0x0F);	// for now, in lower 4 bits of format word
}

int Sound::open(const char *filename)
{   /* open file for input, or create for output.  Returns 1 on success, 
       0 on fail*/
    char modeStr[2];
    SOUND *newsnd;

    DBGFPRINTF((stderr, "Sound0x%lx::open('%s')\n", this, filename));
    // Make sure the snd object is ready for opening
    close();

    modeStr[0] = snd->mode;	modeStr[1] = '\0';
    if( (newsnd=sndOpen(snd,filename,modeStr))==NULL)  {
	// leave snd untouched; remains reusable
	return 0;
    } else {
	snd = newsnd;	// probably does nothing, but ...
	return 1;
    }
}

void Sound::close(void)
{   /* close the soundfile (and spawn a new snd obj with the same chrs) */
    DBGFPRINTF((stderr, "Sound0x%lx::close()\n", this));
    if(snd) {
	char modeStr[2];
	modeStr[0] = snd->mode;  modeStr[1] = '\0';
	double sampRate = snd->snd->samplingRate;
	int chans = snd->snd->channels;
	Format fmt = (Format)snd->snd->format;
	reset(modeStr, sampRate, chans, fmt);
    }
}

long Sound::readFrames(void *buf, long frames)
{   /* read sound data from file */
    return sndReadFrames(snd, buf, frames);
}

long Sound::writeFrames(void *buf, long frames)
{   /* write sound data to file */
    return sndWriteFrames(snd, buf, frames);
}

long Sound::frameTell(void)
{   /* frame index of next to read */
    return sndFrameTell(snd);
}

long Sound::frameSeek(long frame, int mode /* == SEEK_SET */)
{   /* move file point to abs (or other) pos (in sample frames) */
    return sndFrameSeek(snd, frame, mode);
}

int Sound::frameFeof(void)
{   /* return nonzero if at EOF */
    return sndFeof(snd);
}

long Sound::MinMaxFrames(long startframe, long frames, float *buf)
{
    int chans = getChannels();
    Format fmt = getFormat();

    setChannels(1);
    setFormat(Sfmt_FLOAT);
    if(scanbuf == NULL || frames > scanbuflen) {
	if(scanbuf)	delete [] scanbuf;
	scanbuf = new float[frames];
	scanbuflen = frames;
    }
    frameSeek(startframe);
    long got=readFrames((void*)scanbuf, frames);
    long i;
    *scanbuf = 0;
    float *pf = scanbuf;
    double v, min = *scanbuf, max = *scanbuf;
    for(i=0; i<got; ++i) {
	v = *pf++;
	if (v > max)	max = v;
	if (v < min)	min = v;
    }
    *((float*)buf) = min;
    *(((float*)buf)+1) = max;
    setChannels(chans);
    setFormat(fmt);
    return got;
}
