//
// OutputFile.H
//
// Definitions for OutputFile class
// which writes output features as pfiles or online.
//
// 1997jul25 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/OutputFile.H,v 1.9 2006/03/11 00:20:54 davidj Exp $
//

// Get the floatVec definition
//#include <floatArray/floatArray.H>
#include "floatVec.H"
#include "QuickNet.h"

// needed for FILE
#include <stdio.h>

class OutputFile {
    // Abstract class implements both pfile and online output
protected:
    int	open;		// is the file currently open?
    char *name;		// what is this file called?
    int nfeatures;	// how many features?
    int shouldClose;	// close the file when we're done? (we opened it?)
    int framecount;	// just track how many frames we write
public:
    OutputFile(char *name, int nfeats);
    virtual ~OutputFile();
    virtual int WriteUttStart(char *utid) { return 1; };
    virtual int WriteFrame(floatRef& frame) = 0;
    virtual int WriteUttEnd(void) = 0;
    virtual void Close(void) = 0;
};

class OutputFile_Online : public OutputFile {
    // Write the output file as a stream of online features
private:
    FILE *stream;
    floatVec last;
    int byteswap;
    int WriteLine(char flag, floatRef& vec);
public:
    OutputFile_Online(char *name, int nfeats, int swap = 0);
    virtual ~OutputFile_Online();
    virtual int WriteUttStart(char *utid);
    virtual int WriteFrame(floatRef& frame);
    virtual int WriteUttEnd(void);
    virtual void Close(void);
};

class OutputFile_Pfile : public OutputFile {
    // Output file that writes a Pfile
private:
    //pfileif *pfif;
    FILE *file;
    QN_OutFtrStream *opf;
    int current_utt;
    
public:
    OutputFile_Pfile(char *name, int nfeats);
    virtual ~OutputFile_Pfile();
    virtual int WriteUttStart(char *utid);
    virtual int WriteFrame(floatRef& frame);
    virtual int WriteUttEnd(void);
    virtual void Close(void);
};

class OutputFile_SRI : public OutputFile {
    // Output file that writes to an SRI feature file
private:
    FILE *file;
    QN_OutFtrStream *opf;
    int current_utt;
    
public:
    OutputFile_SRI(char *name, int nfeats);
    virtual ~OutputFile_SRI();
    virtual int WriteUttStart(char *utid);
    virtual int WriteFrame(floatRef& frame);
    virtual int WriteUttEnd(void);
    virtual void Close(void);
};

class OutputFile_Raw : public OutputFile {
    // Output file that writes raw binary to the output
private:
    FILE *stream;
    int byteswap;
public:
    OutputFile_Raw(char *name, int nfeats, int swapbytes = 0);
    virtual ~OutputFile_Raw();
    virtual int WriteUttStart(char *utid);
    virtual int WriteFrame(floatRef& frame);
    virtual int WriteUttEnd(void);
    virtual void Close(void);
};

class OutputFile_Ascii : public OutputFile {
    // Output file that writes ascii text (like rasta -A)
private:
    FILE *stream;
public:
    OutputFile_Ascii(char *name, int nfeats);
    virtual ~OutputFile_Ascii();
    virtual int WriteUttStart(char *utid);
    virtual int WriteFrame(floatRef& frame);
    virtual int WriteUttEnd(void);
    virtual void Close(void);
};

class OutputFile_HTK : public OutputFile {
    // Output file that writes HTK-style headers
private:
    FILE *stream;
    int byteswap;
    float stepTimeMs;	// analysis frame rate in milliseconds
    size_t hdrPos;	// where the last header was written, for rewrite

    int _writeHdr(void);

public:
    OutputFile_HTK(char *name, int nfeats, int swap, float step);
    virtual ~OutputFile_HTK();
    virtual int WriteUttStart(char *utid);
    virtual int WriteFrame(floatRef& frame);
    virtual int WriteUttEnd(void);
    virtual void Close(void);   
};
