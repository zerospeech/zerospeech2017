// -*- c++ -*-
// QN_ILab.h
// 
// Header definitions for ICSI binary label file format
// 1999mar04 dpwe@icsi.berkeley.edu 
// $Header: /u/drspeech/repos/quicknet2/QN_ILab.h,v 1.1 1999/03/06 00:16:52 dpwe Exp $

#ifndef QN_ILab_h_INCLUDED
#define QN_ILab_h_INCLUDED

// This file contains some miscellaneous routines for handling PFiles

// IMPORTANT NOTE - some places in this file refer to "sentence" - this
// is synonymous with "segment", the later being a generalization of the
// former with no functional difference.

#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Logger.h"

#define QN_BITS_PER_BYTE (8)


class QN_InLabStream_ILab : public QN_InLabStream
{
public:
    // Constructor
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is appended to status messages.
    // "a_file" is the stream we read the features and labels from.
    // "a_indexed" is non-zero if we want random access to the Ilabfile.
    QN_InLabStream_ILab(int a_debug, const char* a_dbgname,
			FILE* a_file, int a_indexed);
    ~QN_InLabStream_ILab();

    // Return the number of labels in the Ilabfile
    size_t num_labs();
    // Return the number of segments in the Ilabfile
    size_t num_segs();

    // Read the next set of frames, up until the end of segment.
    // Returns the number of frames read, 0 if already at end of sentence
    size_t read_labs(size_t frames, QNUInt32* labs);

    // Return details of where we are (next frame to be read)
    // Always returns QN_OK.
    int get_pos(size_t* segnop, size_t* framenop);

    // Move on to the next segment.
    // Returns ID of next segment if succeeds, else QN_SEGID_BAD if at end
    // of file.
    QN_SegID nextseg();

    // Move back to the beginning of the Ilabfile.  This will work for
    // unindexed Ilabfiles, but not streams.  If this fails, it returns QN_BAD.
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
    QN_ClassLogger log;		// Handles logging for us.
    FILE* const file;		// The stream for reading the Ilabfile.
    const int indexed;		// 1 if indexed.
    unsigned int total_frames;	// Frames in data section.
    unsigned int total_segs;	// The number of segments in the data section.

    long data_offset;		// Offset of data section from ilabfile header.
    long sentind_offset;	// The offset of the segment index section
				// ..in the ilabfile.
    size_t bytes_per_label;		// Bytes in one label (typ. 1 or 2)

    long current_seg;		// Current segment number.
    long current_frame;		// Current frame number within segment.

    long ilabfile_sent;		// Segment number read from Ilabfile.
    long ilabfile_frame;		// Frame number read from Ilabfile.

    QNUInt32* segpos;		// The segment seek pos index.
    QNUInt32* seglens;		// The record of frames per segment

    // Data of current block
    long	block_start_frame;	// address of first count in this block
    long	block_frame_count;	// number of frames in this block
    long	block_label;		// label value for this block

//// Private functions

    // Read the Ilabfile header.
    void read_header();
    // Read one frame of Ilabfile data.
    void read_frame();
    // Two diffent implementations for "build_index", depending on whether
    // there is already a segment index section in the ilabfile.
    void build_index_from_sentind_sect();
    void build_index_from_data_sect();
    // Read the next block from the current pos, return bytecount consumed
    int read_next_block(void);
};

// Return the number of label columns in the Ilabfile.
inline size_t
QN_InLabStream_ILab::num_labs()
{
    return 1;
}

inline size_t
QN_InLabStream_ILab::num_segs()
{
    return total_segs;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutLabStream_ILab - access a Ilabfile for output
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

class QN_OutLabStream_ILab : public QN_OutLabStream
{
public:
    // Constructor.
    // "a_debug" controls the level of status message output.
    // "a_dbgname" is the debugging output tag.
    // "a_file" is the stream we write the output data to.
    // "a_ftrs" is the number of features in the resulting Ilabfile.
    // "a_labels" is the number of labels in the resulting Ilabfile.
    // "a_indexed" is non-zero if we want an indexed Ilabfile.
    QN_OutLabStream_ILab(int a_debug, const char* a_dbgname,
			 FILE* a_file, 
			 int maxlabel, int a_indexed);
    ~QN_OutLabStream_ILab();

    // Return the number of labels in the Ilabfile.
    size_t num_labs();

    // Write the next set of frames, keeping them part of the current segment.
    void write_labs(size_t frames, const QNUInt32* labs);

    // Finish writing the current segment, identify the current segment
    // then move on to the next segment.
    void doneseg(QN_SegID segid);

private:
    QN_ClassLogger clog;	// Logging object.
    FILE* const file;		// The stream for reading the Ilabfile.
    const int indexed;		// 1 if indexed.

    int bytes_per_label;	// length of each label

    long current_seg;		// The number of the current segment.
    long current_frame;		// The number of the current frame.
    long current_pos;		// our current byte position

    long total_frames;		// all frames in all segments

    // Details of the current block
    long	block_start_frame;	// address of first count in this block
    long	block_frame_count;	// number of frames in this block
    long	block_label;		// label value for this block
    
    // The default length of the sentence index - it grows by doubling in
    // size.
    // Note that this is small so index enlargement can be tested with small
    // test files.
    enum { DEFAULT_INDEX_SIZE = 32 }; 
    QNUInt32* segpos;		// Index of file pos for each seg
    QNUInt32* seglens;		// Index of frame count for each seg
    size_t index_len;		// The length of the sentence indices that have
				// been allocated (in words).


// Private member functions.
    // Write the header information at the start of the file.
    void write_header();
    // Write the sentence index information at the end of the file.
    void write_index();
    // Write out a block at the current pos - return number of bytes
    int write_next_block(QNUInt32 count, QNUInt32 label);
};

// Return the number of labels in the Ilabfile
inline size_t
QN_OutLabStream_ILab::num_labs()
{
    return 1;
}


#endif /* QN_ILab_h_INCLUDED */
