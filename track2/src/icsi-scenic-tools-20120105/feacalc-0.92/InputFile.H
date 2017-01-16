//
// InputFile.H
//
// Define class for reading input soundfiles into feacalc.
//
// 1997jul25 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/InputFile.H,v 1.7 2001/12/04 20:53:10 dpwe Exp $

// Base this on dpwelib's snd

extern "C" {
#include <dpwelib/snd.h>
};

//#include <floatArray/floatArray.H>
#include "floatVec.H"

struct _sound;

class InputFile {
    // Abstract class for input data from different sources
private:
    struct _sound *snd;
    int frameLen;
    int stepLen;
    int stepCents;	// Hop step involves this many *hundredths* of a sample
    int padLen;		// number of samples of padding to add at both ends
    int zeropad;	// pad with zeros? (else attempt to reflect actual)
    int pos;	       	// number of samples transferred, including padding
    int eof;		// flag that we've reached final EOF includding padding
    int hiteof;		// flag that we've hit the file EOF (pre-padding)
    int eofpos;		// the position, in our frames, where file EOF happened
    floatVec cache;	// where to store what we read last time
    floatVec window;	// hamming window applied to each frame
    int doHpf;		// should we remove DC from signal?
    float hpf_fc;	// break frq for HP filter
    double hpf_coefA;	// HP filter coefs
    double hpf_coefB;	//  ''
    float hpf_lastx;	// HP filter state
    int extra_prepad;	// additional zero padding at start of sound from range
    int extra_postpad;	// additional zero padding at end of sound from range

    float startTime;	// time to start reading file
    float lenTime;	// total duration to read from file (0 means to EOF)
    int lenpos;		// where to stop in terms of pos (0 means EOF)
    int channel;	// which channel to read (for ? stereo files)

    int posCents;	// our accumulation of fractions of a sample

    int GetNewFrames(floatRef& buf);
    void HPfilter(floatVec& buf, int start);

public:
    float sampleRate;

    InputFile(char *name, int framelen, int step, int cents, \
	      float start=0.0, float len=0.0, int chan=0, \
	      int padlen = 0, float a_hpf_fc = 0, int zpad = 0, 
	      char *ipformatname = 0);
    ~InputFile(void);

    int GetFrames(floatVec& buf);
    // Return 1 if end of file reached
    int Eof(void);
    // Return 1 if we actually succeeded in opening the file
    int OK(void) {return snd != NULL;}
    // if it's not OK, you best not do anything else.
};

