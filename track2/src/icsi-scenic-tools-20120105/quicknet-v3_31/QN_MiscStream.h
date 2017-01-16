// -*- C++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_MiscStream.h,v 1.5 2004/03/26 06:00:40 davidj Exp $

// QN_MiscStream.h
// Headers for reading/writing various miscellaneous feature stream formats
// (based on QN_HTKstream)
// 2000-03-14 dpwe@icsi.berkeley.edu

#ifndef QN_MiscStream_h_INCLUDED
#define QN_MiscStream_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include <stddef.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"


////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_UNIIO - DaimlerChrysler UNIIO format input
////////////////////////////////////////////////////////////////


#define UNIO_TESTSEQUENCE       0x03020100L                   
#define UNIO_TESTSEQUENCE2      0x03220100L                   
#define UNIO_REVTESTSEQUENCE    0x00010203L
#define UNIO_REVTESTSEQUENCE2   0x00012203L
#define UNIO_FILEHEADINFOLEN    64

#define UNIO_HDR_BYTES		84

// Flags for PeekMagic
#define UNIO_ALLOW_HDR		(1)
#define UNIO_ALLOW_COUNT	(2)
#define UNIO_ALLOW_ANY		(255)

// Special values for the first column
#define UNIO_DUMMY_BOS		(4)
#define UNIO_DUMMY_NORMAL	(6)

class QN_InFtrStream_UNIIO : public QN_InFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the tag used in debugging output.
    // "a_file" is the stream we read the features from.
    // "a_indexed" is non-zero if we want random access to the PFile.
    // "a_n_dummy" is the number of dummy feature els to strip from the 
    //           front of each vector (typically 2)
    QN_InFtrStream_UNIIO(int a_debug, 
			 const char* a_dbgname, FILE* a_file,
			 int a_indexed, int a_n_dummy);
    virtual ~QN_InFtrStream_UNIIO();

    virtual size_t num_ftrs();
    virtual size_t read_ftrs(size_t count, float* ftrs);

    int rewind();
    QN_SegID nextseg();

// The following functions may fail due to lack of index - if this is happens,
// they return an error value and DO NOT call QN_ERROR().

    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

protected:
    enum {
	DEFAULT_INDEX_LEN = 16 	// Default length of the index vector.
				// (short to aid testing).
    };

    QN_ClassLogger log;		// Send debugging output through here.
    FILE* const file;		// The stream for reading the file.
    long current_seg;		// Current segment number.
    long current_frame;		// Current frame within segment.

    size_t n_ftrs;		// The number of features per row

    size_t n_segs;		// The number of segments in the file -
				// can only be used if indexed.
    size_t n_rows;		// The number of frames in the file -
				// can only be used if indexed.

    int    byteswap;		// Flag that bytes must be swapped re: disk
    QNUInt32 cepAnz;		// Special DC flag
    int	   n_dummy;		// Dummy feature els prefixing each vector

    const int indexed;		// 1 if indexed.
    QNUInt32* segind;		// Segment index vector.
    size_t segind_len;		// Current length of the segment index vector.

    int	header_peeked;		// Flag that header has been read already
    int magic_peeked;		// flag that framecount has been peeked 

    int uniio_version;		// version code for this file

    void build_index();		// Build a segment index.

    int read_header();		// read the per-utterance header

    int SkipRemainingExtensions(void); 	// skip extra info in header
    int peek_magic(int allow = UNIO_ALLOW_ANY, 
		   QNUInt32 *pval = 0);   // check magic number
    size_t read_to_eos(void);	// read thru to EOS, return # frames

};


inline size_t
QN_InFtrStream_UNIIO::num_segs()
{
    size_t ret;

    if (indexed)
	ret = n_segs;
    else
	ret = QN_SIZET_BAD;
    return ret;
}


inline size_t
QN_InFtrStream_UNIIO::num_ftrs()
{
    return n_ftrs;
}

////////////////////////////////////////////////////////////////
// QN_OutFtrStream_UNIIO - output stream for DaimlerChrysler UNIIO format ftrs
////////////////////////////////////////////////////////////////


class QN_OutFtrStream_UNIIO : public QN_OutFtrStream
{
public:
    QN_OutFtrStream_UNIIO(int a_debug, const char* a_dbgname,
			  FILE* a_file, size_t a_n_ftrs, size_t a_n_dummy);
    ~QN_OutFtrStream_UNIIO();

    size_t num_ftrs();

    void write_ftrs(size_t cnt, const float* ftrs);
    void doneseg(QN_SegID segid);
private:
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// File that output is written to.
    const size_t n_ftrs;	// Number of features in one frame.

    size_t current_seg;		// which segment we're writing
    size_t frames_this_seg;	// how many frames we've written here

    int bos;			// flag that we are at beg-of-seg

    int    byteswap;		// Flag that bytes must be swapped re: disk
    QNUInt32 cepAnz;		// Special DC flag
    int	   n_dummy;		// Dummy feature els prefixing each vector

    int uniio_version;		// version code for this file

};

inline size_t
QN_OutFtrStream_UNIIO::num_ftrs()
{
    return n_ftrs;
}

////////////////////////////////////////////////////////////////
// QN_InFtrStream_raw - input of raw binary floats
////////////////////////////////////////////////////////////////

// Since there is no way to define sentence lengths for
// raw data files, they are assumed to always constitute a 
// single sentence.  Then the list structure might be 
// something you can layer on top.

// Values for format
#define QNRAW_FLOAT32 (0)

// Values for byteorder
#define QNRAW_LITTLEENDIAN (0)
#define QNRAW_BIGENDIAN (1)


class QN_InFtrStream_raw : public QN_InFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the tag used in debugging output.
    // "a_file" is the stream we read the features from.
    // "a_format" is the data format (0 = 32 bit float, others are int)
    // "a_byteorder" is the byteorder (0=littleendian, 1=bigendian)
    // "a_width" is the number of values in each frame
    // "a_indexed" is non-zero if we want random access to the PFile.
    QN_InFtrStream_raw(int a_debug, 
		       const char* a_dbgname, FILE* a_file,
		       int a_format, int a_byteorder, 
		       size_t a_width, int a_indexed);
    virtual ~QN_InFtrStream_raw();

    virtual size_t num_ftrs();
    virtual size_t read_ftrs(size_t count, float* ftrs);

    int rewind();
    QN_SegID nextseg();

// The following functions may fail due to lack of index - if this is happens,
// they return an error value and DO NOT call QN_ERROR().

    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

protected:

    void next_frame(void);

    enum {
	DEFAULT_INDEX_LEN = 16 	// Default length of the index vector.
				// (short to aid testing).
    };

    QN_ClassLogger log;		// Send debugging output through here.
    FILE* const file;		// The stream for reading the PFile.
    long current_seg;		// Current segment number.
    long current_frame;		// Current frame within segment.
    size_t frames_this_seg;	// how many frames in the current seg

    int format;			// 0 = 32 bit float, 1 = ??
    int byteorder;		// 0 = little endian, 1 = bigendian

    size_t n_ftrs;		// The number of features per row

    size_t n_segs;		// The number of segments in the file -
				// can only be used if indexed.
    size_t n_rows;		// The number of frames in the file -
				// can only be used if indexed.

    size_t frame_bytes;		// bytes per frame in the file

    const int indexed;		// 1 if indexed.
    int eos;			// at the end of a segment?

    float *frame_buf;		// 1-frame read-ahead

};


inline size_t
QN_InFtrStream_raw::num_segs()
{
    // Raw files only have one segment

    return 1;
}


inline size_t
QN_InFtrStream_raw::num_ftrs()
{
    return n_ftrs;
}

////////////////////////////////////////////////////////////////
// QN_OutFtrStream_raw - write raw binary output
////////////////////////////////////////////////////////////////

class QN_OutFtrStream_raw : public QN_OutFtrStream
{
public:
    // Constructor.
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the debugging output tag.
    // "a_file" is the stream we write the output data to.
    // "a_ftrs" is the number of features in the resulting File.
    // "a_format" is the data format (0 = 32 bit float, others are int)
    // "a_byteorder" is the byteorder (0=littleendian, 1=bigendian)
    // "a_indexed" is non-zero if we want an indexed File.
    QN_OutFtrStream_raw(int a_debug, const char* a_dbgname,
			FILE* a_file, 
			int a_format, int a_byteorder, 
			size_t a_num_ftrs, int a_indexed);
    ~QN_OutFtrStream_raw();

    // Return the number of features in the File.
    size_t num_ftrs();

    // Write "cnt" vectors of feature data.
    void write_ftrs(size_t cnt, const float* ftrs);

    // Finish writing the current segment, identify the current segment
    // then move on to the next segment.
    void doneseg(QN_SegID segid);

private:
    QN_ClassLogger log;		// Logging object.
    FILE* const file;		// The stream for reading the File.
    const int indexed;		// 1 if indexed.

    int format;			// 0 = 32 bit float, 1 = ??
    int byteorder;		// 0 = little endian, 1 = bigendian

    const unsigned int num_ftr_cols; // Number of feature columns.

    long current_seg;		// The number of the current segment.
    long current_frame;		// The number of the current frame.

    float *frame_buf;		// 1-frame scratchpad

// Private member functions.
};

// Return the number of feature columns in the File.
inline size_t
QN_OutFtrStream_raw::num_ftrs()
{
    return num_ftr_cols;
}

#endif // #ifdef QN_MiscStream_h_INCLUDED
