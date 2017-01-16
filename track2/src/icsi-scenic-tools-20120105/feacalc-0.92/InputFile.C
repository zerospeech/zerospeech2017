//
// InputFile.C
//
// Class for reading input soundfiles into feacalc.
//
// 1997jul25 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/InputFile.C,v 1.8 2001/12/04 20:53:10 dpwe Exp $

// Base this on dpwelib's snd

#include "InputFile.H"

InputFile::InputFile(char *name, int framelen, int step, int cents, 
		     float start /*=0.0*/, float len /*=0.0*/, int chan /*=0*/,
		     int padlen /*=0*/, float a_hpf_fc /*=0*/, int zpad /* =0 */, 
		     char* ipfmtname /*=0*/) {
    // Setup the named file for sound input
    frameLen = framelen;
    stepLen  = step;
    stepCents = cents;
    padLen   = padlen;
    doHpf    = a_hpf_fc > 0;
    hpf_fc   = a_hpf_fc;
    zeropad  = zpad;
    startTime = start;
    lenTime = len;
    channel = chan;
    lenpos = 0;
    pos = 0;
    eof = 0;
    eofpos = 0;
    hiteof = 0;
    extra_prepad = 0;
    extra_postpad = 0;
    cache.Resize(frameLen);
    cache.Clear();
    posCents = 0;

    // infinite loop if framelen is zero
    assert(framelen);

    // Set up the weighting window
    window.Resize(frameLen);
    int i;
    double scale = 32768.0;	/* scale up to match rasta short inputs */
    double a = 0.54;
    double b = 1 - a;
    for(i=0; i<frameLen; ++i) {
	window[i] = scale * (a - b * cos(2*M_PI*((double)i)/(frameLen-1)));
    }

    snd = sndNew(NULL);
    if (ipfmtname && strlen(ipfmtname)) {
	sndSetSFformatByName(snd,ipfmtname);
    }

    snd = sndOpen(snd, name, "r");
    if(snd == NULL) {
	// fprintf(stderr, "InputFile: couldn't read '%s'\n", name);
	// exit(-1);
	/* this->OK() will be 0 */
	return;	/* i.e. return NULL */
    }
    // Configure it how we want it
    sndSetUchans(snd, 1);
    // which channel? from input index, Snd Chan MoDe symbols assumed in order
    sndSetChanmode(snd, SCMD_CHAN0 + channel);
    sndSetUformat(snd, SFMT_FLOAT);
    // Read back the sampling rate
    sampleRate = sndGetSrate(snd);

    // Set up the high-pass filter (from rasta-v2_4/io.c:fvec_HPfilter())
    if (doHpf) {
	/* computing filter coefficients */
	double c, d, p2;
#define MINUS_3DB (0.501187) /* i.e. 10^-0.3 */
	d = MINUS_3DB;   /* 3dB passband attenuation at frequency f_p */
	c = cos(2.*M_PI* hpf_fc/sampleRate); /* frequency f_p about 45 Hz */
	p2 = (1.-c+2.*d*c)/(1.-c-2.*d);
	/* to conform to older version... */
	hpf_coefA = floor((-p2-sqrt(p2*p2-1.))*1000000.) / 1000000.;
	hpf_coefB = floor((1.+hpf_coefA)*500000.) / 1000000.;
	// clear state
        hpf_lastx = 0;
	
	//	fprintf(stderr, "hpf: fc=%f, A=%f, b=%f\n", hpf_fc, hpf_coefA, hpf_coefB);
    }

    // Seek to start as specified by startTime
    if (startTime > 0.0) {
	int startpos = (int)rint(sampleRate*startTime);
	sndFrameSeek(snd, startpos, SEEK_SET);
    } else if (startTime < 0.0) {
	// could probably push this into the prepad len, but for
	// now just flag an error
//	fprintf(stderr, "InputFile('%s'): Error: start time of %.3f is < 0\n", 
//		name, startTime);
//	exit(-1);
	extra_prepad = (int)rint(-sampleRate*startTime);
    }
    // setup lenpos to tell us where to stop
    if (lenTime > 0.0) {
	lenpos = (int)rint(sampleRate*lenTime);
    }
    // Make sure that any range-derived padding is taken as zero padding, 
    // unless real nonzero padding has been requested
    if (padLen == 0) {
	zeropad = 1;
    }
    DBGFPRINTF((stderr, "InputFile: start=%.3f (%d), len=%.3f(%d)\n", startTime, sndFrameTell(snd), lenTime, lenpos));
}

InputFile::~InputFile(void) {
    if(OK())	sndFree(snd);
}

int InputFile::GetNewFrames(floatRef& buf) {
    // Get next sequential samples from file
    // Return number of samples gotten, will be buf.len except at EOF
    int got, totalgot = 0, toget = buf.Len();
    buf.Clear();

    // We're going to assume that the buf is contiguous
    assert(buf.Step() == 1);

    if (toget && !hiteof) {
	if (lenpos) {
	    // If we've set an upper limit on how many frames to get, 
	    // enforce it.
	    toget = MIN(toget, lenpos-pos);
	}
	got = sndReadFrames(snd, &(buf.El(totalgot)), toget);
	pos += got;
	totalgot += got;
	DBGFPRINTF((stderr, "GNF: got=%d, toget=%d, pos=%d, lenpos=%d\n", 
		    got, toget, pos, lenpos));
	if (got == 0 || got < toget || (lenpos && pos >= lenpos)) {
	    // We hit file eof - record its logical pos, also flag eof
	    eofpos = pos;
	    hiteof = 1;
	    // If we had a lenpos but didn't reach it, invoke postpadding
	    if (lenpos > eofpos) {
		extra_postpad = lenpos - eofpos;
	    }
	}
	toget -= got;
    }

    return totalgot;
}

int InputFile::GetFrames(floatVec& buf) {
    // Return a frame of points for processing.
    // Does all the smarts for overlapping, windowing and padding
    // (including reflecting padded points from the actual data)
    int toget, newgot, fromcache, prepad = 0, postpad = 0;
    int i, reflectpts, filtered = 0, totalgot = 0;
    floatRef tmp;

    // don't do anything if we're not really open
    if(!OK())	return -1;

    // Make sure the buf is the right size
    buf.Resize(frameLen);
    // clear it (wasteful but convenient.  We may skip pad frames, want 
    //  them as zero).
    buf.Clear();

    if (pos > 0) {
	// Copy some from cache
	fromcache = min(pos, frameLen - stepLen);
	// take the *last* <fromcache> pts in cache
	buf.CopyFrom(cache.Len() - fromcache, cache);
	totalgot += fromcache;
	// points from cache are already filtered
	filtered = fromcache;
    }
    // Maybe apply front padding
    if (pos < (padLen+extra_prepad)) {
	prepad = min((padLen+extra_prepad) - pos, frameLen - totalgot);
	DBGFPRINTF((stderr, "zpad:front: %d of %d at %d\n", prepad, frameLen, pos));
	// advance the notional file pos
	pos += prepad;
	totalgot += prepad;
    }
    // Read in the new pts
    tmp.CloneRefFrom(totalgot, buf);
    toget = frameLen - totalgot;
    assert (tmp.Len() == toget);
    newgot = GetNewFrames(tmp);
DBGFPRINTF((stderr, "toget=%d, newgot=%d\n", toget, newgot));
    totalgot += newgot;
    // notional file pos is advanced within GetNewFrames

    // If there was front padding, reflect what we got of input into it
    if (prepad) {
	reflectpts = 0;
	if (!zeropad) {
	    reflectpts = max(0, min(prepad, newgot - 1)); // newgot can =0
	    for (i=0; i < reflectpts; ++i) {
		buf.El(prepad - 1 - i) = buf.El(prepad + 1 + i);
	    }
	}
	// Zero out others
	for (i=reflectpts; i < prepad; ++i) {
	    buf.El(prepad - 1 - i) = 0;
	}
    }

    // If we didn't get enough pts from read, maybe apply post-padding
    if (newgot < toget) {
	postpad = min(toget - newgot, max(0, padLen+extra_postpad - (pos - eofpos)));
	DBGFPRINTF((stderr, "zpad:back: %d of %d at %d\n", postpad, frameLen, pos));
	reflectpts = 0;
	if (!zeropad) {
	    // reflect across pts from what's read / cache if we can
	    // In theory, could end up reflecting post-HPF pts which 
	    // would be wrong (since reflected pts are subject to filter 
	    // again), but this doesn't come up, so wing it.
	    reflectpts = max(0, min(postpad, totalgot-1)); // =0 if totalgot=0
	    for (i=0; i < reflectpts; ++i) {
		buf.El(totalgot+i) 
		    = buf.El(totalgot-2-i);
	    }
	}
	// zero out others
	for (i=reflectpts; i < postpad; ++i) {
	    buf.El(totalgot+i) = 0;
    	}
	totalgot += postpad;
	// advance the filepos
	pos += postpad;

	// Have we reached the end of file, even including padding?
	DBGFPRINTF((stderr, "pos=%d, eofpos=%d, padlen=%d extra_post=%d\n", pos, eofpos, padLen, extra_postpad));
	if ( pos-eofpos >= padLen+extra_postpad) {
	    eof = 1;
	}
    }

    // Apply the highpass filter (maybe)
    if (doHpf) {
	HPfilter(buf, filtered);
    }
    // Save into the cache
    cache = buf;
    // Apply the window
    buf *= window;

    // Advance the notional, fractional pointer too
    posCents += stepCents;
    if (posCents > 100) {
      floatVec fv(1);
      GetNewFrames(fv);
      posCents -= 100;
      // Update the cache
      memcpy(&cache.El(1), &cache.El(0), cache.Len()-1);
      cache.El(cache.Len()-1) = fv.El(0);
      // .. including filtering the new point
      if (doHpf) {
	HPfilter(cache, cache.Len()-1);
      }
    }

    // Return the count of pts gotten
    return totalgot;
}

int InputFile::Eof(void) {
    return eof;
}

// From rasta-v2_4/io.c:fvec_HPfilter()..
/* digital IIR highpass filter on waveform to remove DC offset
   H(z) = (0.993076-0.993076*pow(z,-1))/(1-0.986152*pow(z,-1)) 
   offline and online version, inplace calculating */

void InputFile::HPfilter(floatVec& buf, int start)
{
    float thisx, lastx, thisy, lasty;
    int i;
        
    if (start == 0) {
	// first call - preset state
	hpf_lastx = buf[0];
	// don't filter the first point
	start = 1;
    }
    lastx = hpf_lastx;
    lasty = buf[start - 1];
    for (i = start; i < buf.Len(); i++) {
	thisx = buf[i];
        thisy =  hpf_coefB * (thisx - lastx) + hpf_coefA * lasty;
	buf[i] = thisy;
	lastx = thisx;
	lasty = thisy;
    }
    hpf_lastx = lastx;
}

