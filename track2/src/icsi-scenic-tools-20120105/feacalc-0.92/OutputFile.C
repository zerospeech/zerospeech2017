//
// OutputFile.H
//
// Definitions for OutputFile class
// which writes output features as pfiles or online.
//
// 1997jul25 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/OutputFile.C,v 1.10 2012/01/05 20:30:15 dpwe Exp $
//

#include "OutputFile.H"
#include <string.h>
#include <stdlib.h>
#include <QN_SRIfeat.h>   // This is missing in some versions of QuickNet.h

static void endian_swap_short(short *sbuf, int nshorts) {
    /* Swap the bytes in a buffer full of shorts if this machine 
       is not big-endian */
    int i, x;
    for (i=0; i<nshorts; ++i) {
	x = *sbuf;
	*sbuf++ = ((x & 0xFF) << 8) + ((x >> 8) & 0xFF);
    }
}

static void endian_swap_int(int *buf, int nints) {
    /* Swap the bytes in a buffer full of ints if this machine 
       is not big-endian */
    int i, x;
    assert(sizeof(int)==4);
    for (i=0; i<nints; ++i) {
	x = *buf;
	*buf++ = ((x & 0xFFL) << 24L) + ((x & 0xFF00L) << 8L) \
	           + ((x & 0xFF0000L) >> 8L) + ((x >> 24L) & 0xFF);
    }
}

static void endian_swap_long(long *buf, int nints) {
    /* Swap the bytes in a buffer full of longs if this machine 
       is not big-endian */
    int i;
    long x;
    assert(sizeof(long)==4);
    for (i=0; i<nints; ++i) {
	x = *buf;
	*buf++ = ((x & 0xFFL) << 24L) + ((x & 0xFF00L) << 8L) \
	           + ((x & 0xFF0000L) >> 8L) + ((x >> 24L) & 0xFF);
    }
}

static void endian_swap_float(float *buf, int nints) {
    /* Swap the bytes in a buffer full of floats if this machine 
       is not big-endian */
    assert(sizeof(float)==4);
    endian_swap_int((int*)buf, nints);
}

OutputFile::OutputFile(char *n, int nf) {
    open = 0;
    shouldClose = 1;
    name = strdup(n);
    nfeatures = nf;
    framecount = 0;
}

OutputFile::~OutputFile(void) {
//    if(open)	Close();
// seems like, reasonably, you can't use virtual fns in the base class 
// destructor (since the derived classes have already been partially
// destroyed).  So, each derived class has its own destructor calling 
// its own close fn.
    if(name) {
	free(name);
	name = 0;
    }
}

// ---- OutputFile_Online ----

OutputFile_Online::OutputFile_Online(char *n, int nf, int swap /* =0 */)
    : OutputFile(n, nf) {
    // Open an output file as a stream of online features
    if(strcmp(name, "-") == 0) {
	// Write to stdout
	stream = stdout;
	free(name);
	name = strdup("(stdout)");
	shouldClose = 0;
    } else {
	stream = fopen(name, "w");
	if (stream == NULL) {
	    fprintf(stderr, "OutputFile_Online: couldn't open '%s'\n", 
		    name);
	    exit(-1);
	}
    }
    byteswap = (swap != 0);
    open = 1;
}

OutputFile_Online::~OutputFile_Online(void) {
    if(open)	Close();
}

int OutputFile_Online::WriteLine(char flag, floatRef& vec) {
    // Write one frame of actual online feature output
    int i;
    float f;

    assert(sizeof(float)==4);	// implicit in byteswap code
    assert(nfeatures == vec.Len());

    fwrite(&flag, 1, 1, stream);
    for(i = 0; i < nfeatures; ++i) {
	// fwrite(&(vec[i]), sizeof(float), 1, stream);
	f = vec[i];
	if(byteswap) {
	    endian_swap_float(&f, 1);
	}
	fwrite(&f, sizeof(float), 1, stream);
    }
    // flush the stream
    fflush(stream);

    return nfeatures;
}

int OutputFile_Online::WriteFrame(floatRef& frame) {
    // Write a frame of features in online mode
    // That means: a one-byte flag, then a string of binary floats
    // I think.
    char flag = '\0';
    int val;

    // If we have been called before, write those features
    if(last.Len()) {
	val = WriteLine(flag, last);
    }
    // Save the feas we're passed to write next time
    last.CloneVec(frame);

    ++framecount;

    return val;
}

int OutputFile_Online::WriteUttStart(char *utid) {
    // Mark/record the start of the next utterance
    // Still don't know what to do with this
    return 1;
}

int OutputFile_Online::WriteUttEnd(void) {
    // Write the utterance end
    // .. which means writing out the last frame of features with the flag set
    char flag = '\200';	// 0x80
    int val;
    // Write the last set of features (presumably had 1!)
    val = WriteLine(flag, last);
    // Mark "last" as empty
    last.Resize(0);

    return val;
}

void OutputFile_Online::Close(void) {
    // Close the file
    if(shouldClose) {
	fclose(stream);
    } else {
	fflush(stream);
    }
    open = 0;
}


// ---- OutputFile_Pfile ----
// We'll use the existing pfif wrapper & just indirect
//#include <pfif.H>

// #include <QN_PFile.h>

OutputFile_Pfile::OutputFile_Pfile(char *n, int nf)  : OutputFile(n, nf) {
    // Create a new pfile for output
    if(strcmp(name, "-") == 0) {
	fprintf(stderr, "OutputFile_Pfile: cannot write to stdout ('%s')\n", 
		name);
	exit(-1);
    }
    //  pfif = new pfileif();
    //  pfif->setNumFeatures(nfeatures);
    //  if (!pfif->open(name, "w")) {
    //	fprintf(stderr, "OutputFile_Pfile: error opening pfile '%s'\n", name);
    //	exit(-1);
    //
    if ( (file = fopen(name, "w")) == NULL) {
	fprintf(stderr, "OutputFile_Pfile: error opening pfile '%s'\n", name);
	exit(-1);
    }
    int debug = 0;
    const char *dtag = "feacalc::Pfile";
    int nlabels = 0;
    int indexed = 1;
    opf = new QN_OutFtrLabStream_PFile(debug, dtag, file, nfeatures, 
				       nlabels, indexed);

    current_utt = 0;
    open = 1;
}

OutputFile_Pfile::~OutputFile_Pfile(void) {
    // Destructor
    if (open) {
	Close();
    }
    //    delete pfif;
}

void OutputFile_Pfile::Close(void) {
    // Close the file
    delete opf;
    if(shouldClose) {
	// pfif->close();
	fclose(file);
    }
    open = 0;
}

int OutputFile_Pfile::WriteUttStart(char *utid) {
    // Mark/record the start of the next utterance
    // Still don't know what to do with this
    return 1;
}

int OutputFile_Pfile::WriteFrame(floatRef& frame) {
    // Write a single frame of features to the pfile
    //    return pfif->writeFrame(frame);
    opf->write_ftrs(1,frame.Data());
    return 1;
}

int OutputFile_Pfile::WriteUttEnd(void) {
    // Mark that the features are finished
    //	    return pfif->writeUttEnd();
    opf->doneseg(current_utt++);
    return 1;
}

// ---- OutputFile_SRI ----
// This is based on OutputFile_Pfile above
// #include <QN_SRIfeat.h>

OutputFile_SRI::OutputFile_SRI(char *n, int nf)  : OutputFile(n, nf) {
    // Create a new sri feature file for output
    if(strcmp(name, "-") == 0) {
	fprintf(stderr, "OutputFile_SRI: cannot write to stdout ('%s')\n", 
		name);
	exit(-1);
    }
    if ( (file = fopen(name, "w")) == NULL) {
	fprintf(stderr, "OutputFile_SRI: error opening pfile '%s'\n", name);
	exit(-1);
    }
    int debug = 0;
    const char *dtag = "feacalc::SRIfeat";
    opf = new QN_OutFtrStream_SRI(debug, dtag, file, nfeatures);
    current_utt = 0;
    open = 1;
}

OutputFile_SRI::~OutputFile_SRI(void) {
    // Destructor
    if (open) {
	Close();
    }
}

void OutputFile_SRI::Close(void) {
    // Close the file
    delete opf;
    if(shouldClose) {
	fclose(file);
    }
    open = 0;
}

int OutputFile_SRI::WriteUttStart(char *utid) {
    // Mark/record the start of the next utterance
    // Still don't know what to do with this
    return 1;
}

int OutputFile_SRI::WriteFrame(floatRef& frame) {
    // Write a single frame of features to the file
    opf->write_ftrs(1,frame.Data());
    return 1;
}

int OutputFile_SRI::WriteUttEnd(void) {
    // Mark that the features are finished
    opf->doneseg(current_utt++);
    return 1;
}

// ---- OutputFile_Raw ----
// Write raw binary output

OutputFile_Raw::OutputFile_Raw(char *n, int nf, int bs) : OutputFile(n, nf) {
    // Create a new Raw for output
    byteswap = bs;
    if(strcmp(name, "-") == 0) {
	stream = stdout;
	free(name);
	name = strdup("(stdout)");
	shouldClose = 0;
    } else {
	stream = fopen(n, "w");
    }
    if(stream == NULL) {
	fprintf(stderr, "OutputFile_Raw: error opening '%s'\n", name);
	exit(-1);
    }
    open = 1;
}

OutputFile_Raw::~OutputFile_Raw(void) {
    if(open)	Close();
}

void OutputFile_Raw::Close(void) {
    // Close the file
    if(shouldClose) {
	fclose(stream);
    }
    open = 0;
}

int OutputFile_Raw::WriteUttStart(char *utid) {
    // Mark/record the start of the next utterance
    // Still don't know what to do with this
    return 1;
}

int OutputFile_Raw::WriteFrame(floatRef& frame) {
    // Write a single frame of features to the Raw
    int i;
    int rc = 0;
    float f;

    assert(sizeof(float)==4);	// implicit in byteswap code

    for (i = 0; i < frame.Len(); ++i) {
	f = frame[i];	
	if(byteswap) {
	    endian_swap_float(&f, 1);
	}
	rc += fwrite(&f, sizeof(float), 1, stream);
    }
    return rc;
}

int OutputFile_Raw::WriteUttEnd(void) {
    // Mark that the features are finished
    // does nothing for raw output
    return 1;
}

// ---- OutputFile_Ascii ----
// Write ascii output

OutputFile_Ascii::OutputFile_Ascii(char *n, int nf) : OutputFile(n, nf) {
    // Prepare to write ascii
    if(strcmp(name, "-") == 0) {
	stream = stdout;
	free(name);
	name = strdup("(stdout)");
	shouldClose = 0;
    } else {
	stream = fopen(n, "w");
    }
    if(stream == NULL) {
	fprintf(stderr, "OutputFile_Ascii: error opening '%s'\n", name);
	exit(-1);
    }
    open = 1;
}

OutputFile_Ascii::~OutputFile_Ascii(void) {
    if(open)	Close();
}

void OutputFile_Ascii::Close(void) {
    // Close the file
    if(shouldClose) {
	fclose(stream);
    }
    open = 0;
}

int OutputFile_Ascii::WriteUttStart(char *utid) {
    // Mark/record the start of the next utterance
    // Still don't know what to do with this
    return 1;
}

int OutputFile_Ascii::WriteFrame(floatRef& frame) {
    // Write a single frame of features as ascii
    int i;
    int rc = 0;
    float f;
    char *pc = (char*)&f;
    char x;

    for (i = 0; i < frame.Len(); ++i) {
	fprintf(stream, "%g ", frame.Val(i));
    }
    // line break after each row
    fprintf(stream, "\n");
    return rc;
}

int OutputFile_Ascii::WriteUttEnd(void) {
    // Mark that the features are finished
    // does nothing for raw output
    return 1;
}

OutputFile_HTK::~OutputFile_HTK(void) {
    if(open)	Close();
}

// -------------- OutputFile_HTK ----------------------
// Write HTK format feature files

typedef struct {              /* HTK File Header */
    int nSamples;
    int sampPeriod;
    short sampSize;
    short sampKind;
} HTKhdr;

/* sampKind flag for user-defined features (after htk/HTKLib/HParm.h) */
#define HPARM_USER 9


int OutputFile_HTK::_writeHdr(void) {
    // OK, now write the HTK-specific header
    HTKhdr hdr;
    hdr.nSamples = framecount;
    hdr.sampPeriod = (int) (0.5 + (stepTimeMs * 10000));  /* in 0.1us units */
    hdr.sampSize = (short)nfeatures * sizeof(float);	  /* bytes per frame */
    hdr.sampKind = HPARM_USER;
    if (byteswap) {
	endian_swap_int(&hdr.nSamples, 2);
	endian_swap_short(&hdr.sampSize, 2);
    }

    // Remember where we wrote this header so we can seek back to it
    hdrPos = ftell(stream);
    if ( fwrite(&hdr, sizeof(HTKhdr), 1, stream) < 1) {
	fprintf(stderr, "OutputFile_HTK: error writing hdr to '%s'\n", name);
	exit(-1);
    }

    return 1;
}

OutputFile_HTK::OutputFile_HTK(char *n, int nf, int sw, float step) 
    : OutputFile(n, nf), byteswap(sw), stepTimeMs(step) {
    // Write an HTK output file
    hdrPos = 0;

    // This should be in the OutputFile_File base class
    if(strcmp(name, "-") == 0) {
	stream = stdout;
	free(name);
	name = strdup("(stdout)");
	shouldClose = 0;
    } else {
	stream = fopen(n, "w");
    }
    if(stream == NULL) {
	fprintf(stderr, "OutputFile_HTK: error opening '%s'\n", name);
	exit(-1);
    }
    open = 1;

    // Header will be written by WriteUttStart
}

void OutputFile_HTK::Close(void) {
    // Close the file
    if(shouldClose) {
	fclose(stream);
    }
    open = 0;
}

int OutputFile_HTK::WriteUttStart(char *utid) {
    // Mark/record the start of the next utterance
    // Still don't know what to do with this
    framecount = 0;
    _writeHdr();
    return 1;
}

int OutputFile_HTK::WriteFrame(floatRef& frame) {
    // Write a single frame of features as HTK
    int i;
    int rc = 0;
    float f;

    assert(sizeof(float)==4);	// implicit in byteswap code

    for (i = 0; i < frame.Len(); ++i) {
	f = frame[i];	
	if(byteswap) {
	    endian_swap_float(&f, 1);
	}
	rc += fwrite(&f, sizeof(float), 1, stream);
    }
    ++framecount;
    return rc;
}

int OutputFile_HTK::WriteUttEnd(void) {
    // Mark that the features are finished
    // For HTK, attempt to rewrite header with true framecout
    if (fseek(stream, hdrPos, SEEK_SET) > -1) {
	_writeHdr();
	// Seek back to end of stream
	fseek(stream, 0, SEEK_END);
    }
    return 1;
}

