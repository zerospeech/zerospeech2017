// $Header: /u/drspeech/repos/quicknet2/QN_Strut.h,v 1.1 2001/04/11 17:20:38 dpwe Exp $
// -*- C++ -*-

#ifndef QN_Strut_h_INCLUDED
#define QN_Strut_h_INCLUDED

// This file contains some miscellaneous routines for handling Struts

// IMPORTANT NOTE - some places in this file refer to "sentence" - this
// is synonymous with "segment", the later being a generalization of the
// former with no functional difference.

#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_Strut - access a Strut for input
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_InFtrStream_Strut : public QN_InFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    QN_InFtrStream_Strut(int a_debug, const char* a_dbgname,
			 FILE* a_file);
    ~QN_InFtrStream_Strut();

    // Return the number of features in the Strut
    size_t num_ftrs();
    // Return the number of segments in the Strut
    size_t num_segs();

    // Read just features from the next set of frames.
    // Returns the number of frames read, 0 if already at end of segment.
    // NOTE - once read_ftrs has been called, the label values for that
    // frame have been lost.  Use read_ftrslabs() if you want both features
    // and labels
    size_t read_ftrs(size_t frames, float* ftrs);

    // Return details of where we are (next frame to be read)
    // Always returns QN_OK.
    int get_pos(size_t* segnop, size_t* framenop);

    // Move on to the next segment.
    // Returns ID of next segment if succeeds, else QN_SEGID_BAD if at end
    // of file.
    QN_SegID nextseg();

    // Move back to the beginning of the Strut.  
    int rewind();

    // Return the number of frames in the given segment.
    // (or the whole file if segno==QN_ALL)
    // Returns QN_SIZET_BAD if info not known
    size_t num_frames(size_t segno = QN_ALL);

    // Move to a given segment and frame.  Returns QN_SEGID_BAD if not
    // possible, otherwise segment ID of the new segment.
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Some constants for struts
    enum
    {
	SENT_EOF = -1		// A suitable segment no. to mark EOF
    };

    QN_ClassLogger log;		// Handles logging for us.
    FILE* const file;		// The stream for reading the Strut.
    unsigned int num_cols;	// Columns in data section.
    unsigned int total_frames;	// Frames in data section.
    unsigned int total_sents;	// The number of segments in the data section.

    long data_offset;		// Offset of data section from strut header.
    long sentind_offset;	// The offset of the segment index section
				// ..in the strut.
    size_t bytes_in_row;		// Bytes in one row of the Strut.

    long current_sent;		// Current segment number.
    long current_frame;		// Current frame number within segment.
    long current_row;		// Current row within strut.

    float* buffer;	// A buffer for one frame.
    QNUInt32* sentind;		// The segment start index.

//// Private functions

    // Read the Strut header.
    void read_header();
    // Read the Strut index
    void read_index();
    // Read one frame of Strut data.
    void read_frame();
};

// Return the number of feature columns in the Strut.
inline size_t
QN_InFtrStream_Strut::num_ftrs()
{
    return num_cols;
}

// Return the number of segments in the Strut.
inline size_t
QN_InFtrStream_Strut::num_segs()
{
    return total_sents;
}

#endif // #ifndef QN_Strut_h_INCLUDED
