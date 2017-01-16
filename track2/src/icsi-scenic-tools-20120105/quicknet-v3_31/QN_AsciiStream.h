// -*- c++ -*-
// QN_AsciiStream.h
// 
// Header definitions for ASCII FtrLab input/output formats
// 1999mar04 dpwe@icsi.berkeley.edu 
// $Header: /u/drspeech/repos/quicknet2/QN_AsciiStream.h,v 1.5 2010/10/29 02:20:37 davidj Exp $

#ifndef QN_AsciiStream_h_INCLUDED
#define QN_AsciiStream_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"

//
//  Stuff for token-based ascii streams
//
//  Need to be able to read & manipulate phoneset definitions
//

class QN_TokenMap {
    // object holds definition of tokens to convert into indices
    // (such as a phoneset)
private:
    int ntokens;
    char **tokenArray;
    QN_ClassLogger log;		// Logging object.

public:
    QN_TokenMap(int a_debug = 0) 
	: log(a_debug, "QN_TokenMap", "")
	{ntokens=0; tokenArray=NULL;}
    ~QN_TokenMap(void) {clear();}

    void clear(void);
    int readPhset(const char *filename);
    int writePhset(const char *filename);
    int lookup(const char *tok);	// returns -1 if token not found
    const char* index(int i) {char *s=NULL; if (i < ntokens) s = tokenArray[i];
                              return s;}
    int size(void) {return ntokens;}
};

//
// input stream
//

class QN_InFtrLabStream_Ascii : public QN_InFtrLabStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    // "a_indexed" is non-zero if we want random access to the File.
    // "a_tokmap" is an optional tokmap.  If present, read labels
    //            will be first looked up in this map (else atoi'd)
    QN_InFtrLabStream_Ascii(int a_debug, const char* a_dbgname,
			    FILE* a_file, 
			    size_t a_num_ftrs, size_t a_num_labs, 
			    int a_indexed, QN_TokenMap *a_tokmap = NULL);
    ~QN_InFtrLabStream_Ascii();

    // Return the number of labels in the file
    size_t num_ftrs();
    // Return the number of labels in the file
    size_t num_labs();
    // Read "cnt" vectors of feature and label data.
    // The return value is either the number of features read or QN_SIZET_BAD.
    // If the return value is less than "cnt", we have reached the end of the
    // segment.
    size_t read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs);

    // These are needed to comply with the parent class
    size_t read_ftrs(size_t cnt, float* ftrs);
    size_t read_labs(size_t cnt, QNUInt32* labs);
    
    // Return details of where we are (next frame to be read)
    // Always returns QN_OK.
    int get_pos(size_t* segnop, size_t* framenop);	

    // Move on to the next segment.
    // Returns ID of next segment if succeeds, else QN_SEGID_BAD if at end
    // of file.
    QN_SegID nextseg();

    // Move back to the beginning of the File.  This will work for
    // unindexed Files, but not streams.  If this fails, it returns QN_BAD.
    // After a rewind, nextseg() must be used to move to the start of the
    // first segment.
    int rewind();

// The following operations are not supported on ascii files since 
// they do not allow indexing.

    // Return the number of segments in the file
    size_t num_segs() {
	return QN_SIZET_BAD;
    }

    // Return the number of frames in the given segment.
    // (or the whole file if segno==QN_ALL)
    // Returns QN_SIZET_BAD if info not known
    size_t num_frames(size_t /* segno */) {
	return QN_SIZET_BAD;
    }

    // Move to a given segment and frame.  Returns QN_SEGID_BAD if not
    // possible, otherwise segment ID of the new segment.
    QN_SegID set_pos(size_t /* segno */, size_t /* frameno */) {
	return QN_SIZET_BAD;
    }

private:
    QN_ClassLogger log;		// Handles logging for us.
    FILE* const file;		// The stream for reading the file.
    const int indexed;		// 1 if indexed.

    const unsigned int num_ftr_cols; // Number of feature columns.
    const unsigned int num_lab_cols; // Number of label columns.

    long current_seg;		// Current segment number.
    long current_frame;		// Current frame number within segment.

    char *line;			// buffer for reading a line from file
    int linelen;		// number of bytes in linelen
    enum {
	QN_ASC_DEFAULT_LINELEN=256
    };

    QN_TokenMap *tokmap;	// symbol->code map, else symbols just nums

//// Private functions

    // Read one frame of File data.
    void read_frame();
    // like fgets, but silently reallocs the buffer
    char *my_fgets(char *s, int* pn, FILE *stream);
};

// Return the number of feature columns in the File.
inline size_t
QN_InFtrLabStream_Ascii::num_ftrs()
{
    return num_ftr_cols;
}

// Return the number of label columns in the File.
inline size_t
QN_InFtrLabStream_Ascii::num_labs()
{
    return num_lab_cols;
}

// Return just the feature values.
inline size_t
QN_InFtrLabStream_Ascii::read_ftrs(size_t frames, float* ftrs)
{
    return read_ftrslabs(frames, ftrs, NULL);
}

// Return just the label values.
inline size_t
QN_InFtrLabStream_Ascii::read_labs(size_t frames, QNUInt32* labs)
{
    return read_ftrslabs(frames, NULL, labs);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_Ascii - access a File for output
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_OutFtrLabStream_Ascii : public QN_OutFtrLabStream
{
public:
    // Constructor.
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the debugging output tag.
    // "a_file" is the stream we write the output data to.
    // "a_ftrs" is the number of features in the resulting File.
    // "a_labels" is the number of labels in the resulting File.
    // "a_indexed" is non-zero if we want an indexed File.
    // "a_tokmap" is an optional tokmap.  If present, emitted labels
    //            will be as symbols from this map.
    // "a_oldstyle" means use %f rather than %g for printf if true.
    QN_OutFtrLabStream_Ascii(int a_debug, const char* a_dbgname,
			     FILE* a_file, 
			     size_t a_num_ftrs, size_t a_num_labs, 
			     int a_indexed, QN_TokenMap *a_tokmap = NULL,
			     int a_oldstyle = 0);
    ~QN_OutFtrLabStream_Ascii();

    // Return the number of features in the File.
    size_t num_ftrs();
    // Return the number of labels in the File.
    size_t num_labs();

    // Write "cnt" vectors of feature data.
    void write_ftrslabs(size_t cnt, const float* ftrs,
			const QNUInt32* labs);

    // Wrt. just features - compatible with QN_OutFtrStream abstract interface.
    // NOTE - cannot use write_labs then write_ftrslabs - must use
    // write_ftrslabs if we need none-zero values for both.
    void write_ftrs(size_t frames, const float* ftrs);

    // Write just labels - compatible with QN_OutFtrLabStream abstract interface.
    // NOTE - cannot use write_ftrs then write_labs - must use
    // write_ftrslabs if we need non-zero values for both.
    void write_labs(size_t frames, const QNUInt32* labs);

    // Finish writing the current segment, identify the current segment
    // then move on to the next segment.
    void doneseg(QN_SegID segid);

private:
    QN_ClassLogger log;		// Logging object.
    FILE* const file;		// The stream for reading the File.
    const int indexed;		// 1 if indexed.

    const unsigned int num_ftr_cols; // Number of feature columns.
    const unsigned int num_lab_cols; // Number of label columns.

    long current_seg;		// The number of the current segment.
    long current_frame;		// The number of the current frame.

    QN_TokenMap *tokmap;	// map for symbolic output labels
    int oldstyle;		// 1 if %f rather than %g for printf

// Private member functions.
};

// Return the number of feature columns in the File.
inline size_t
QN_OutFtrLabStream_Ascii::num_ftrs()
{
    return num_ftr_cols;
}

// Return the number of label columns in the File.
inline size_t
QN_OutFtrLabStream_Ascii::num_labs()
{
    return num_lab_cols;
}

// Write just features to the file
inline void
QN_OutFtrLabStream_Ascii::write_ftrs(size_t frames, const float* ftrs)
{
    write_ftrslabs(frames, ftrs, NULL);
}

// Write just labels to the file
inline void
QN_OutFtrLabStream_Ascii::write_labs(size_t frames, const QNUInt32* labs)
{
    write_ftrslabs(frames, NULL, labs);
}

#endif /* QN_Ascii_h_INCLUDED */
