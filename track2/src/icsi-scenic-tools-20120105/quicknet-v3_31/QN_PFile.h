// $Header: /u/drspeech/repos/quicknet2/QN_PFile.h,v 1.19 2005/06/13 02:17:02 davidj Exp $

#ifndef QN_PFile_h_INCLUDED
#define QN_PFile_h_INCLUDED

// This file contains some miscellaneous routines for handling PFiles

// IMPORTANT NOTE - some places in this file refer to "sentence" - this
// is synonymous with "segment", the later being a generalization of the
// former with no functional difference.

#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"

// The standard PFile 32 bit data type

typedef union
{
    QNInt32 l; float f;
} QN_PFile_Val;

// Some stuff to deal with large file support
//  First make sure things still work if we do not have fseeko/ftello
#ifdef QN_HAVE_FSEEKO
#define qn_fseek(a,b,c) fseeko(a,b,c)
#define qn_ftell(a) ftello(a)
typedef off_t qn_off_t;
#else
#define qn_fseek(a,b,c) fseek(a,b,c)
#define qn_ftell(a) ftell(a)
typedef long qn_off_t;
#endif

// Set up long long types if we have them, along with approriate format
// string segments
#if defined(QN_HAVE_LONG_LONG)
typedef long long qn_longlong_t;
typedef unsigned long long qn_ulonglong_t;
#define QN_PF_LLU "%llu"
#define QN_PF_LLD "%lld"
#else
typedef long qn_longlong_t;
typedef unsigned long qn_ulonglong_t;
#define QN_PF_LLU "%lu"
#define QN_PF_LLD "%ld"
#endif

// Define QN_PFILE_LARGE if we support large PFiles
#if (defined(QN_HAVE_FSEEKO) && defined(QN_HAVE_LONG_LONG) && (_FILE_OFFSET_BITS==64))
#define QN_PFILE_LARGEFILES 1
#else
#define QN_PFILE_LARGEFILES 0
#endif

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_PFile - access a PFile for input
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

// This is the lowest level access to a PFile, returning feature and
// label data simultaneously.  For general use, the "FtrStream" and
// "LabStream" based interfaces are preferable.

class QN_InFtrLabStream_PFile : public QN_InFtrLabStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    // "a_indexed" is non-zero if we want random access to the PFile.
    QN_InFtrLabStream_PFile(int a_debug, const char* a_dbgname,
			    FILE* a_file, int a_indexed);
    ~QN_InFtrLabStream_PFile();

    // Return the number of labels in the PFile
    size_t num_labs();
    // Return the number of features in the PFile
    size_t num_ftrs();
    // Return the number of segments in the PFile
    size_t num_segs();

    // Read the next set of frames, up until the end of segment.
    // Returns the number of frames read, 0 if already at end of sentence
    size_t read_ftrslabs(size_t frames, float* ftrs, QNUInt32* labs);

    // Read just features from the next set of frames.
    // Returns the number of frames read, 0 if already at end of segment.
    // NOTE - once read_ftrs has been called, the label values for that
    // frame have been lost.  Use read_ftrslabs() if you want both features
    // and labels
    size_t read_ftrs(size_t frames, float* ftrs);

    // Read just labels from the next set of frames.
    // Returns the number of frames read, 0 if already at end of segment.
    // NOTE - once read_labs has been called, the feature values for that
    // frame have been lost.  Use read_ftrslabs() if you want both features
    // and labels
    size_t read_labs(size_t frames, QNUInt32* labs);

    // Return details of where we are (next frame to be read)
    // Always returns QN_OK.
    int get_pos(size_t* segnop, size_t* framenop);

    // Move on to the next segment.
    // Returns ID of next segment if succeeds, else QN_SEGID_BAD if at end
    // of file.
    QN_SegID nextseg();

    // Move back to the beginning of the PFile.  This will work for
    // unindexed PFiles, but not streams.  If this fails, it returns QN_BAD.
    // After a rewind, nextseg() must be used to move to the start of the
    // first segment.
    int rewind();

// The following operations are only available if we selected indexing in
// the constructor

    // Return the number of frames in the given segment.
    // (or the whole file if segno==QN_ALL)
    // Returns QN_SIZET_BAD if info not known
    size_t num_frames(size_t segno = QN_ALL);

    // Move to a given segment and frame.  Returns QN_SEGID_BAD if not
    // possible, otherwise segment ID of the new segment.
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Some constants for pfiles
    enum
    {
	SENT_EOF = -1		// A suitable segment no. to mark EOF
    };

    QN_ClassLogger log;		// Handles logging for us.
    FILE* const file;		// The stream for reading the PFile.
    const int indexed;		// 1 if indexed.
    unsigned int num_cols;	// Columns in data section.
    unsigned int total_frames;	// Frames in data section.
    unsigned int total_sents;	// The number of segments in the data section.
    unsigned int first_ftr_col;	// First column of features.
    unsigned int num_ftr_cols;	// Number of feature columns.
    unsigned int first_lab_col;	// First column of labels.
    unsigned int num_lab_cols;	// Number of label columns.
// Do not support targets yet
//    unsigned int first_target_col; // First column of targets.
//    unsigned int num_target_cols; // Number of target columns.

    qn_longlong_t data_offset;	// Offset of data section from pfile header.
    qn_longlong_t sentind_offset; // The offset of the segment index section
				// ..in the pfile.
    size_t bytes_in_row;		// Bytes in one row of the PFile.

    long current_sent;		// Current segment number.
    long current_frame;		// Current frame number within segment.
    long current_row;		// Current row within pfile.

    long pfile_sent;		// Segment number read from PFile.
    long pfile_frame;		// Frame number read from PFile.

    QN_PFile_Val* buffer;	// A buffer for one frame.
    QNUInt32* sentind;		// The segment start index.

//// Private functions

    // Read the PFile header.
    void read_header();
    // Read one frame of PFile data.
    void read_frame();
    // Two diffent implementations for "build_index", depending on whether
    // there is already a segment index section in the pfile.
    void build_index_from_sentind_sect();
    void build_index_from_data_sect();
};

// Return the number of label columns in the PFile.
inline size_t
QN_InFtrLabStream_PFile::num_labs()
{
    return num_lab_cols;
}

// Return the number of feature columns in the PFile.
inline size_t
QN_InFtrLabStream_PFile::num_ftrs()
{
    return num_ftr_cols;
}

// Return the number of segments in the PFile.
inline size_t
QN_InFtrLabStream_PFile::num_segs()
{
    return total_sents;
}

// Return just the feature values.
inline size_t
QN_InFtrLabStream_PFile::read_ftrs(size_t frames, float* ftrs)
{
    return read_ftrslabs(frames, ftrs, NULL);
}

// Return just the label values.
inline size_t
QN_InFtrLabStream_PFile::read_labs(size_t frames, QNUInt32* labs)
{
    return read_ftrslabs(frames, NULL, labs);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_PFile - access a PFile for output
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

// This is the lowest level access to a PFile, requiring feature and
// label data simultaneously.  For general use, the "OutFtrStream" and
// "OutLabStream" based interfaces are preferable.

class QN_OutFtrLabStream_PFile : public QN_OutFtrLabStream
{
public:
    // Constructor.
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the debugging output tag.
    // "a_file" is the stream we write the output data to.
    // "a_ftrs" is the number of features in the resulting PFile.
    // "a_labels" is the number of labels in the resulting PFile.
    // "a_indexed" is non-zero if we want an indexed PFile.
    QN_OutFtrLabStream_PFile(int a_debug, const char* a_dbgname,
			     FILE* a_file, size_t a_ftrs, size_t a_labs,
			     int a_indexed);
    ~QN_OutFtrLabStream_PFile();

    // Return the number of labels in the PFile.
    size_t num_labs();
    // Return the number of features in the PFile.
    size_t num_ftrs();

    // Write the next set of frames, keeping them part of the current segment.
    void write_ftrslabs(size_t frames, const float* ftrs,
		       const QNUInt32* labs);

    // Wrt. just features - compatible with QN_OutFtrStream abstract interface.
    // NOTE - cannot use write_labs then write_ftrslabs - must use
    // write_ftrslabs if we need none-zero values for both.
    void write_ftrs(size_t frames, const float* ftrs);

    // Write just labels - compatible with QN_OutLabStream abstract interface.
    // NOTE - cannot use write_ftrs then write_labs - must use
    // write_ftrslabs if we need non-zero values for both.
    void write_labs(size_t frames, const QNUInt32* labs);

    // Finish writing the current segment, identify the current segment
    // then move on to the next segment.
    void doneseg(QN_SegID segid);

private:
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// The stream for reading the PFile.
    const int indexed;		// 1 if indexed.

    const unsigned int num_ftr_cols; // Number of feature columns.
    const unsigned int num_lab_cols; // Number of label columns.

    long current_sent;		// The number of the current segment.
    long current_frame;		// The number of the current frame.
    long current_row;		// The number of the current row.

    QN_PFile_Val* buffer;	// A buffer for one frame of PFile data.
    char* header;		// Space for the header data.
    // The default length of the sentence index - it grows by doubling in
    // size.
    // Note that this is small so index enlargement can be tested with small
    // test files.
    enum { DEFAULT_INDEX_SIZE = 32 }; 
    QNUInt32* index;		// Sentence index (if building indexed file).
    size_t index_len;		// The length of the sentence index that has
				// been allocated (in words).

// Private member functions.
    // Write the header information at the start of the file.
    void write_header();
    // Write the sentence index information at the end of the file.
    void write_index();
};

// Return the number of labels in the PFile
inline size_t
QN_OutFtrLabStream_PFile::num_labs()
{
    return num_lab_cols;
}

// Return the number of features in the PFile
inline size_t
QN_OutFtrLabStream_PFile::num_ftrs()
{
    return num_ftr_cols;
}

// Write just features to the file
inline void
QN_OutFtrLabStream_PFile::write_ftrs(size_t frames, const float* ftrs)
{
    write_ftrslabs(frames, ftrs, NULL);
}

// Write just labels to the file
inline void
QN_OutFtrLabStream_PFile::write_labs(size_t frames, const QNUInt32* labs)
{
    write_ftrslabs(frames, NULL, labs);
}

#endif // #ifndef QN_PFile_h_INCLUDED
