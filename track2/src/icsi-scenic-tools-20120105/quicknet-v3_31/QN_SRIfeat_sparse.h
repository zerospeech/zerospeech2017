// $Header: /u/drspeech/repos/quicknet2/QN_SRIfeat_sparse.h,v 1.1 2006/08/15 20:24:03 davidj Exp $
// -*- C++ -*-

#ifndef QN_SRIfeat_sparse_h_INCLUDED
#define QN_SRIfeat_sparse_h_INCLUDED

// This file contains routines for handling SRI sparse feature files.
// (this code is based on QN_SRIfeat.h)
//
// Note that SRI feature files can be either "cepfile"s, which do
// not have a feature name, or "featurefile" which has an
// associated featuer name.

#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_SRI_sparse - access an SRI sparse feature file for input
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_InFtrStream_SRI_sparse : public QN_InFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    QN_InFtrStream_SRI_sparse(int a_debug, const char* a_dbgname,
			 FILE* a_file);
    ~QN_InFtrStream_SRI_sparse();

    // Return the number of features
    size_t num_ftrs();
    // Return the number of segments (always 1)
    size_t num_segs();

    // Read just features from the next set of frames.
    // Returns the number of frames read, 0 if already at end of segment.
    size_t read_ftrs(size_t frames, float* ftrs);

    // Return details of where we are (next frame to be read)
    // Always returns QN_OK.
    int get_pos(size_t* segnop, size_t* framenop);

    // Move on to the next segment.
    // Returns ID of next segment if succeeds, else QN_SEGID_BAD if at end
    // of file.
    QN_SegID nextseg();

    // Move back to the beginning of the features.
    int rewind();

    // Return the number of frames in the given segment. This will only
    // be valid for segno==0 or segno==QN_ALL, which will both return
    // the same value since each file only contains one segment. Other
    // values will return QN_SIZET_BAD.
    size_t num_frames(size_t segno = QN_ALL);

    // Move to a given segment and frame. This is only valid for 
    // segno == 0. Returns QN_SEGID_BAD if segno != 0 or frameno is not
    // valid. Otherwise, returns 0 (the only valid segno).
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Some constants
    QN_ClassLogger log;		// Handles logging for us.
    FILE* const file;		// The stream for reading the features.
    unsigned int num_cols;	// Columns in data section.
    unsigned int total_frames;	// Frames in data section.
    const static unsigned int total_sents=1; // The number of segments in 
                                // the data section. Currently, always 1.

    unsigned int data_offset;	// Offset of data section from begin of file.
				// Should be 1024.

    size_t bytes_in_row;	// Bytes in one row (i.e. frame)

    long current_sent;		// Current segment number. (should always be 0)
    long current_frame;		// Current frame number within segment.

    float* buffer;		// A buffer to hold one frame of data.
    int* k_buffer;		// A buffer to hold one sparse frame keys
    float* v_buffer; 		// Holds sparse frame values

//// Private functions

    // Read the SRI/NIST header.
    void read_header();
    // Read one frame of data.
    void read_frame();
};

// Return the number of features per frame.
inline size_t
QN_InFtrStream_SRI_sparse::num_ftrs()
{
    return num_cols;
}

// Return the number of segments (utterances) in the file. This should
// always be 1.
inline size_t
QN_InFtrStream_SRI_sparse::num_segs()
{
    return total_sents;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_SRI_sparse - output stream for SRI format ftrs
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

// This is based on QN_OutFtrStream_SRI
class QN_OutFtrStream_SRI_sparse : public QN_OutFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    // "a_n_ftrs" is the number of features in the file
    // "a_ftr_name" is the name of the feature, or NULL if a cepfile
    QN_OutFtrStream_SRI_sparse(int a_debug, const char* a_dbgname,
                        FILE* a_file, size_t a_n_ftrs,
			const char* a_ftr_name = NULL); 
    ~QN_OutFtrStream_SRI_sparse();

    size_t num_ftrs();

    void write_ftrs(size_t cnt, const float* ftrs);
    void doneseg(QN_SegID segid);
private:
    QN_ClassLogger clog;        // Logging object.
    FILE* const file;           // File that output is written to.
    float* buffer;		// A buffer for endian conversion.
    int* k_buffer;		// A buffer to hold one sparse frame keys
    float* v_buffer; // Holds sparse frame values
    const size_t n_ftrs;        // Number of features in one frame.
    char* ftr_name;		// The name of the feature if we are
				// not writing a cepfile.
				// NULL if we are writing a cepfile.
    size_t current_seg;         // which segment we're writing
    size_t frames_this_seg;     // how many frames we've written here
    size_t bytes_per_frame;     // sizeof(float)*n_ftrs
    int bos;                    // flag that we are at beg-of-seg
    long header_pos;            // remember where to rewrite headers
    void write_hdr(char* xtra_info=NULL); // write the header
};

inline size_t
QN_OutFtrStream_SRI_sparse::num_ftrs()
{
    return n_ftrs;
}

#endif // #ifndef QN_SRIfeat_sparse_h_INCLUDED
