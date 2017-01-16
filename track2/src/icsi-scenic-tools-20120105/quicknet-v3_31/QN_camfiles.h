// $Header: /u/drspeech/repos/quicknet2/QN_camfiles.h,v 1.16 2004/09/16 01:12:54 davidj Exp $

// This file contains contains classes for reading and writing a variety of
// similar file formats.  The common features are that all the formats are 
// binary with fixed length frames.  Also each frame is preceded by a single
// byte flag with the most significant bit of the flag being used to
// differentiate between the last frame of each segment (bit set) and all
// other frames.

#ifndef QN_camfiles_h_INCLUDED
#define QN_camfiles_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include <stddef.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"



////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_CamFile - generic Cambridge format input class
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

// This is a base class where we do most of the work for reading "pre", LNA
// and "online feature" format input.  All details of segmentation are handled
// here.  The derived classes override the member functions handling the
// details of data extraction.

class QN_InFtrLabStream_CamFile : public QN_InFtrLabStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_classname" is the class name used in debugging output.
    // "a_dbgname" is the tag used in debugging output.
    // "a_file" is the stream we read the features and labels from.
    // "a_width" is the length, in bytes, of one frame.
    // "a_indexed" is non-zero if we want random access to the PFile.
    QN_InFtrLabStream_CamFile(int a_debug, const char* a_classname,
			      const char* a_dbgname, FILE* a_file,
			      size_t a_width, int a_indexed);
    virtual ~QN_InFtrLabStream_CamFile();

// Things to do with accessing features which may be overridden by the LNA and
// PreFile classes.
    virtual size_t num_ftrs();
    virtual size_t num_labs();
    virtual size_t read_ftrslabs(size_t count, float* ftrs, QNUInt32* labs);
    virtual size_t read_ftrs(size_t count, float* ftrs);
    virtual size_t read_labs(size_t count, QNUInt32* labs);

// Things to do with segments cannot be overridden.
    int rewind();
    QN_SegID nextseg();

// The following functions may fail due to lack of index - if this is happens,
// they return an error value and DO NOT call QN_ERROR().

    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

protected:
    QN_ClassLogger log;		// Send debugging output through here.
    FILE* const file;		// The stream for reading the PFile.
    size_t width;		// The width of the frame in bytes.
    long current_seg;		// Current segment number.
    long current_frame;		// Current frame within segment.
    int eos;			// 1 if at end of sentence.

    QNUInt8* frame_buf;         // Buffer for one frame.
    float* frame_buf_float;	// The unsuitable aligned memory used for
				// the frame buffer.

    enum {
	EOS_MASK = 0x80,	// The MSB of the first byte in each frame
				// holds a flag used to mark end of segment.
	LABEL_MASK = 0x7f,	// The lower 7 bits of the first byte in each
				// frame can hold a label.
	DEFAULT_INDEX_LEN = 16 // Default length of the index vector.
				// (short to aid testing).
    };

    size_t n_segs;		// The number of segments in the file -
				// can only be used if indexed.
    size_t n_rows;		// The number of frames in the file -
				// can only be used if indexed.
    const int indexed;		// 1 if indexed.
    QNUInt32* segind;		// Segment index vector.
    size_t segind_len;		// Current length of the segment index vector.

    void build_index();		// Build a segment index.
    void next_frame();		// Read next frame from disk.
};


inline size_t
QN_InFtrLabStream_CamFile::num_segs()
{
    size_t ret;

    if (indexed)
	ret = n_segs;
    else
	ret = QN_SIZET_BAD;
    return ret;
}


inline size_t
QN_InFtrLabStream_CamFile::num_ftrs()
{
    return width - 1;
}

inline size_t
QN_InFtrLabStream_CamFile::num_labs()
{
    return 1;
}

inline size_t
QN_InFtrLabStream_CamFile::read_ftrs(size_t cnt, float* ftrs)
{
    return read_ftrslabs(cnt, ftrs, NULL);
}

inline size_t
QN_InFtrLabStream_CamFile::read_labs(size_t cnt, QNUInt32* labs)
{
    return read_ftrslabs(cnt, NULL, labs);
}

////////////////////////////////////////////////////////////////
// Pre-files are a format invented by the Cambridge speech group for storing
// speech features with associated phonetic labels.  The frame contains bytes,
// each of which contains a feature value compressed using a non-linear error
// function.  The phonetic label is stored in the lower 7 bits of the flag
// byte, and for historical reasons the pre-file frames are padded to an
// integer number of 4-byte words i.e. there may be up to 3 unused bytes in
// each frame.

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_PreFile - Input a cambrdge "pre" feature file
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_InFtrLabStream_PreFile : public QN_InFtrLabStream_CamFile
{
public:
    // "a_debug" - level of debug output.
    // "a_dbgname" - tag for debugging output.
    // "a_file" - file being read.
    // "a_num_ftrs" - number of reatures in file.
    // "a_indexed" - 1 if indexed stream required, else 0.
    QN_InFtrLabStream_PreFile(int a_debug, const char* a_dbgname,
			      FILE* a_file,
			      size_t a_num_ftrs,
			      int a_indexed
			      );

    // Return the number of features in the file
    size_t num_ftrs();

    // Actually read the features and labels
    size_t read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs);

private:
    const size_t n_pre_ftrs;	// Number of features.
				// Note that this may be up to 3 less than the
				// width - for historical reasons, one frame
				// is always an integer number of frames
    virtual const char* classname(); // The name of the class.
};

inline size_t
QN_InFtrLabStream_PreFile::num_ftrs()
{
    return n_pre_ftrs;
}


inline const char*
QN_InFtrLabStream_PreFile::classname()
{
    return "QN_InFtrLabStream_PreFile";
}

////////////////////////////////////////////////////////////////
//  LNA files are used by the Cambridge speech group and others to store
//  phoneme probabilities.  The frames are byte-aligned. A log compression
//  function is used to reduce each probability to one byte.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_LNA8 - Input a cambrdge "LNA" probability stream
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_InFtrLabStream_LNA8 : public QN_InFtrLabStream_CamFile
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the tag for debug output.
    // "a_file" is the stream we read the features and labels from.
    // "a_num_ftrs" is the number of "featuers" in one frame.
    // "a_indexed" is non-zero if we want random access to the PFile.
    QN_InFtrLabStream_LNA8(int a_debug, const char* a_dbgname,
			   FILE* a_file, size_t a_num_ftrs,
			   int a_indexed);
    ~QN_InFtrLabStream_LNA8();

    // Read the actual features
    size_t read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs);
    size_t read_ftrs(size_t cnt, float* ftrs);
    size_t read_labs(size_t cnt, QNUInt32* labs);
    size_t num_labs();
};

inline size_t
QN_InFtrLabStream_LNA8::num_labs()
{
    return 0;
}

inline size_t
QN_InFtrLabStream_LNA8::read_ftrs(size_t cnt, float* ftrs)
{
    return read_ftrslabs(cnt, ftrs, NULL);
}

inline size_t
QN_InFtrLabStream_LNA8::read_labs(size_t cnt, QNUInt32* labs)
{
    return read_ftrslabs(cnt, NULL, labs);
}

//////////////////////////////////////////////////////////////
// The online feature format was developed at ICSI for transmitting phoneme
// probabilities or similar data in streamed recogniton systems.  The
// contents of each frame is multiple IEEE single-precision big-endian words.
// Note - this is not really a Cambridge file format, but it is so
// similar to LNA and Pre files that we can reuse much of the implementation.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_OnlFtr - Input an ICSI "Online Feature"  stream
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_InFtrLabStream_OnlFtr : public QN_InFtrLabStream_CamFile
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the tag for debugging output.
    // "a_file" is the stream we read the features and labels from.
    // "a_num_ftrs" is the number of "featuers" in one frame.
    // "a_indexed" is non-zero if we want random access to the PFile.
    QN_InFtrLabStream_OnlFtr(int a_debug, const char* a_dbgname,
			     FILE* a_file, size_t a_num_ftrs, int a_indexed);
    ~QN_InFtrLabStream_OnlFtr();

    // Return the number of labels or ftrs in the file
    size_t num_labs();
    size_t num_ftrs();

    // Read the actual features
    size_t read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs);
    size_t read_ftrs(size_t cnt, float* ftrs);
    size_t read_labs(size_t cnt, QNUInt32* labs);
private:
    size_t n_ftrs;		// Actual number of features
};

// Cannot have labels in online feature files

inline size_t
QN_InFtrLabStream_OnlFtr::num_labs()
{
    return 0;
}

// Cannot have labels in online feature files

inline size_t
QN_InFtrLabStream_OnlFtr::num_ftrs()
{
    return n_ftrs;
}

inline size_t
QN_InFtrLabStream_OnlFtr::read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs)
{
    assert(labs==NULL);
    return read_ftrs(cnt, ftrs);
}

inline size_t
QN_InFtrLabStream_OnlFtr::read_labs(size_t cnt, QNUInt32* labs)
{
    assert(labs==NULL);
    return read_ftrs(cnt, NULL);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_LNA8 - output stream for Cambridge format probs.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


class QN_OutFtrLabStream_LNA8 : public QN_OutFtrLabStream
{
public:
    QN_OutFtrLabStream_LNA8(int a_debug, const char* a_dbgname,
			    FILE* a_file, size_t a_n_ftrs, int onl = 0);
    ~QN_OutFtrLabStream_LNA8();

    size_t num_ftrs();
    size_t num_labs();

    void write_ftrs(size_t cnt, const float* ftrs);
    void write_labs(size_t cnt, const QNUInt32* labs);
    void write_ftrslabs(size_t cnt, const float* ftrs, const QNUInt32* labs);
    void doneseg(QN_SegID segid);
private:
    enum { EOS = 0x80 };
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// File that output is written to.
    
    const size_t n_ftrs;	// Number of features in one frame.
    QNInt8* frame_buf;		// Buffer for one frame - needed so we
				// can add end of sentence marker.
    int buf_full;		// 1 if frame_buf holds useful data.
    int online;			// should we flush after each write?
};

inline size_t
QN_OutFtrLabStream_LNA8::num_ftrs()
{
    return n_ftrs;
}

inline size_t
QN_OutFtrLabStream_LNA8::num_labs()
{
    return 0;
}

inline void
QN_OutFtrLabStream_LNA8::write_ftrs(size_t cnt, const float* ftrs)
{
    write_ftrslabs(cnt, ftrs, NULL);
}

inline void
QN_OutFtrLabStream_LNA8::write_labs(size_t cnt, const QNUInt32* labs)
{
    write_ftrslabs(cnt, NULL, labs);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_OnlFtr - output stream for online feature files.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


class QN_OutFtrStream_OnlFtr : public QN_OutFtrStream
{
public:
    QN_OutFtrStream_OnlFtr(int a_debug, const char* a_dbgname,
			   FILE* a_file, size_t a_n_ftrs);
    ~QN_OutFtrStream_OnlFtr();

    size_t num_ftrs();

    void write_ftrs(size_t cnt, const float* ftrs);
    void doneseg(QN_SegID segid);
private:
    enum { EOS = 0x80 };
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// File that output is written to.
    
    const size_t n_ftrs;	// Number of features in one frame.
    float* frame_buf;		// Buffer for one frame - needed so we
				// can add end of sentence marker.
    int buf_full;		// 1 if frame_buf holds useful data.
};

inline size_t
QN_OutFtrStream_OnlFtr::num_ftrs()
{
    return n_ftrs;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_PreFile - output stream for Cambridge format probs.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// Note that each frame has a multiple of four bytes in it.


class QN_OutFtrLabStream_PreFile : public QN_OutFtrLabStream
{
public:
    QN_OutFtrLabStream_PreFile(int a_debug, const char* a_dbgname,
			       FILE* a_file, size_t a_n_ftrs);
    ~QN_OutFtrLabStream_PreFile();

    size_t num_ftrs();
    size_t num_labs();

    void write_ftrs(size_t cnt, const float* ftrs);
    void write_labs(size_t cnt, const QNUInt32* labs);
    void write_ftrslabs(size_t cnt, const float* ftrs, const QNUInt32* labs);
    void doneseg(QN_SegID segid);
private:
    enum { EOS = 0x80 };
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// File that output is written to.
    
    const size_t n_ftrs;	// Number of features in one frame.
    const size_t width;		// Width of one frame - multiple of 4 bytes.
    QNInt8* frame_buf;		// Buffer for one frame - needed so we
				// can add end of sentence marker.
    int buf_full;		// 1 if frame_buf holds useful data.
};

inline size_t
QN_OutFtrLabStream_PreFile::num_ftrs()
{
    return n_ftrs;
}

inline size_t
QN_OutFtrLabStream_PreFile::num_labs()
{
    return 1;
}

inline void
QN_OutFtrLabStream_PreFile::write_ftrs(size_t cnt, const float* ftrs)
{
    write_ftrslabs(cnt, ftrs, NULL);
}

inline void
QN_OutFtrLabStream_PreFile::write_labs(size_t cnt, const QNUInt32* labs)
{
    write_ftrslabs(cnt, NULL, labs);
}


#endif // #ifdef QN_camfiles_h_INCLUDED
