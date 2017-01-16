// $Header: /u/drspeech/repos/quicknet2/QN_RapAct.h,v 1.12 1998/06/16 02:52:26 dpwe Exp $

#ifndef QN_RapAct_h_INCLUDED
#define QN_RapAct_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"

// This file contains classes for handling RAP style activiation files.
// There are three sub-formats.  The most simple stores the activiation
// values as ASCII floating point numbers.  An optimization on this is
// to use a hex representation of the IEEE single precision binary float.
// Finally, the third version stores all numbers in big-endian binary.
//
// For more details, see `man 5 rapact'.


// QN_InFtrStream_Rapact is an input stream that reads RAP style
// activation files.

class QN_InFtrStream_Rapact : public QN_InFtrStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    // "a_indexed" is one if we need indexed access.
    QN_InFtrStream_Rapact(int a_debug, const char* a_dbgname,
			  FILE* a_file, QN_StreamType format, int a_indexed);
    ~QN_InFtrStream_Rapact();

    // Return the number of features in the file.
    size_t num_ftrs();

    // Return the number of segments in the file - QN_SIZET_BAD if this
    // is not known.
    size_t num_segs();

    // Read the next set of frames, up until the end of segment.
    // Returns the number of frames read, 0 if already at end of segment.
    size_t read_ftrs(size_t frames, float* ftrs);

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

// The following operations are not available if the file is not indexed.

    // Return the number of frames in the given segment
    // (or the whole file if segno==QN_ALL).
    // Returns QN_SIZET_BAD if info not known.
    size_t num_frames(size_t segno = QN_ALL);

    // Move to a given segment and frame.  Returns QN_SEGID_BAD if not
    // possible, otherwise segment ID of the new segment.
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    QN_ClassLogger clog;	// Handles logging for us.
    FILE* const file;		// The stream for reading the PFile.
    QN_StreamType format;	// Specific format.

    const int indexed;		// 1 if indexed.
    size_t width;		// Number of features in one frame.

    long current_frameno;	// Current frame number.
    long current_segno;		// Current segment number.
    float* buf;			// Buffer for one frames worth of values.

    int atEOS;			// last access took us to end of segment
    int atEOF;			// last access took us to end of file

    int read1frame(void);	// Read next frame into buf; set flags.

};

// QN_OutFtrStream_Rapact is an output stream that produces RAP style
// activiation files.  These are often used to store phoneme probabilities,
// although may be used for other data.

class QN_OutFtrStream_Rapact : public QN_OutFtrStream
{
public:
    // Constructor.
    // "debug" controls the level of status message output.
    // "file" is the stream we write the output data to.
    // "width" is the number of features in the output file.
    // "format" is the specific format we want, one of:
    //   QN_STREAM_RAPACT_ASCII
    //   QN_STREAM_RAPACT_HEX
    //   QN_STREAM_RAPACT_BIN
    // "online" is non-zero if we are working online and need to
    //   flush at the end of each sentence.
    QN_OutFtrStream_Rapact(int debug, const char* dbgname,
			   FILE* file, size_t width,
			   QN_StreamType format, int online);
    ~QN_OutFtrStream_Rapact();

    // Return the width of the stream.
    size_t num_ftrs();

    // Write out multiple frames.
    //  "frames" is number of frames.
    //  "ftrs" points to the first value of the first frame.
    void write_ftrs(size_t frames, const float* ftrs);

    // Move on the the next sentence.
    //  "sentid" is the internal number of the just finished sentence.
    //  It is ignored for this format.
    void doneseg(QN_SegID sentid);
private:
    QN_ClassLogger clog;	// Handles logging for us.
    FILE* const file;		// Output stream.
    const size_t width;		// Width (in features) of the output stream.
    const QN_StreamType format;	// Format (ascii, hex, binary).
    int online;			// 1 if flush at the end of each sentence.

    float* buf;			// Buffer for endian swapping of
				// floating point data.
    size_t current_frameno;	// Number of next frame to output.
};

#endif // #ifndef QN_RapAct_h_INCLUDED

