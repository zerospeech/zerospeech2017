// $Header: /u/drspeech/repos/quicknet2/QN_HTKstream.h,v 1.4 2004/03/26 06:00:39 davidj Exp $

// QN_HTKstream.h
// Headers for reading/writing HTK-format data files
// 1999jun03 dpwe@icsi.berkeley.edu

// -*- C++ -*-

#ifndef QN_HTKstream_h_INCLUDED
#define QN_HTKstream_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include <stddef.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"


// Define HTK output types -- from the HTK Book Version 2.1,
//                            Section 5.7.1 
//
#define HTK_BASE_MASK	   0x0F // mask to pick out base bits

#define HTK_BASE_WAVEFORM  0x00 // sampled data
#define HTK_BASE_LPC       0x01 // LP coefficients
#define HTK_BASE_LPREFC    0x02 // LP reflection coefficients
#define HTK_BASE_LPCEPSTRA 0x03 // LP cepstral coefficients
#define HTK_BASE_LPDELCEP  0x04 // LP cepstral coefficients, plus deltas
#define HTK_BASE_IREFC     0x05 // 16b integer LP reflection coefficients
#define HTK_BASE_MFCC      0x06 // mel cepstral coefficients
#define HTK_BASE_FBANK     0x07 // log mel filterbank outputs
#define HTK_BASE_MELSPEC   0x08 // linear mel filterbank outputs
#define HTK_BASE_USER      0x09 // user-defined
#define HTK_BASE_DISCRETE  0x0A // vector-quantized data

// Define HTK output qualifiers
//
#define HTK_QUAL_E 0x0040       // Has energy term
#define HTK_QUAL_N 0x0080       // Absolute energy suppressed
#define HTK_QUAL_D 0x0100       // Has deltas
#define HTK_QUAL_A 0x0200       // Has delta-deltas (accelerations)
#define HTK_QUAL_C 0x0400       // Is compressed
#define HTK_QUAL_Z 0x0800       // Has zero-mean static coefficients
#define HTK_QUAL_K 0x1000       // Has CRC checksum
#define HTK_QUAL_O 0x2000       // Has 0-order cepstral coefficient

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_HTK - HTK format input
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


class QN_InFtrStream_HTK : public QN_InFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the tag used in debugging output.
    // "a_file" is the stream we read the features from.
    // "a_indexed" is non-zero if we want random access to the PFile.
    QN_InFtrStream_HTK(int a_debug, 
		       const char* a_dbgname, FILE* a_file,
		       int a_indexed);
    virtual ~QN_InFtrStream_HTK();

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
    FILE* const file;		// The stream for reading the PFile.
    long current_seg;		// Current segment number.
    long current_frame;		// Current frame within segment.
    size_t frames_this_seg;	// how many frames in the current seg

    size_t n_ftrs;		// The number of features per row

    size_t n_segs;		// The number of segments in the file -
				// can only be used if indexed.
    size_t n_rows;		// The number of frames in the file -
				// can only be used if indexed.

    size_t frame_bytes;		// bytes per frame in the file
    int    type_code;		// HTK type code
    int    period;		// HTK sampling period in 100ns units.
    
    int	   header_peeked;	// Flag that we already read the header
                                // (used to pre-read first header)

    const int indexed;		// 1 if indexed.
    QNUInt32* segind;		// Segment index vector.
    size_t segind_len;		// Current length of the segment index vector.

    // HTK data files can be 'compressed', meaning that parameter values 
    // are linearly mapped into 16-bit integer space.  To convert back 
    // to floats, we have xfloat = (xshort + B)*(1/A), where A and B 
    // are appended as extra header fields (see HTKBook sect 5.7.1)
    int    compressed;		// Flag that input values are HTK-compressed
    float* compScales;		// Per-el scale values to decompress (1/A)
    float* compBiases;		// Per-el bias values to decompress  (B)

    void build_index();		// Build a segment index.
    int read_header();		// read the per-utterance header
};


inline size_t
QN_InFtrStream_HTK::num_segs()
{
    size_t ret;

    if (indexed)
	ret = n_segs;
    else
	ret = QN_SIZET_BAD;
    return ret;
}


inline size_t
QN_InFtrStream_HTK::num_ftrs()
{
    return n_ftrs;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_HTK - output stream for HTK format ftrs
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


class QN_OutFtrStream_HTK : public QN_OutFtrStream
{
public:
    QN_OutFtrStream_HTK(int a_debug, const char* a_dbgname,
			FILE* a_file, size_t a_n_ftrs, 
			int a_type, int a_period);
    ~QN_OutFtrStream_HTK();

    size_t num_ftrs();

    void write_ftrs(size_t cnt, const float* ftrs);
    void doneseg(QN_SegID segid);
private:
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// File that output is written to.
    const size_t n_ftrs;	// Number of features in one frame.

    int type_code;		// type value to write to headers
    int period;			// HTK period in 100ns units (ms*10,000)

    size_t current_seg;		// which segment we're writing
    size_t frames_this_seg;	// how many frames we've written here

    size_t bytes_per_frame;	// HTK header value = sizeof(float)*n_ftrs

    int bos;			// flag that we are at beg-of-seg

    long header_pos;		// remember where to rewrite headers
};

inline size_t
QN_OutFtrStream_HTK::num_ftrs()
{
    return n_ftrs;
}

#endif // #ifdef QN_HTKstream_h_INCLUDED

