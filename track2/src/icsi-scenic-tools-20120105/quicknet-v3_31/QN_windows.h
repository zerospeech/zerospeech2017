// $Header: /u/drspeech/repos/quicknet2/QN_windows.h,v 1.8 1996/07/23 23:48:25 davidj Exp $

#ifndef QN_windows_h_INCLUDED
#define QN_windows_h_INCLUDED

// This header contains "windows" for the QuickNet
// stream objects.  Windows take an input stream, and combine data
// from multiple adjacent frames to form one new frame.


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_seqgen.h"

// This is the basic windowing filter.  The input stream can only be scanned
// sequentially, and the resulting stream has one segment for evey
// segment of the input stream.  Each segment returned is shorter than
// the corresponding segment of the input stream, due to edge effects.
// The frames lost at the beginning and end are dependent on the top and
// bottom margins.  This allows for multiple synchronized windows of different
// sizes.


class QN_InFtrStream_SeqWindow : public QN_InFtrStream
{
public:
    // "a_debug" - level of debugging output.
    // "a_dbgname" - tag for debugging output.
    // "a_str" - input stream.
    // "a_win_len" - length of window in frames.
    // "a_top_margin" - number of frames discarded at start of segment.
    // "a_bot_margin" - number of frames discarded at bottom of segment.
    // "a_bunch_frames" - number of frames to read from input stream in one
    //                    read (QN_SIZET_BAD means use sensible default size).
    QN_InFtrStream_SeqWindow(int a_debug, const char* a_dbgname,
			     QN_InFtrStream& a_str,
			     size_t a_win_len, size_t a_top_margin,
			     size_t a_bot_margin,
			     size_t a_bunch_frames = QN_SIZET_BAD);
    ~QN_InFtrStream_SeqWindow();

    size_t num_ftrs();
    QN_SegID nextseg();
    size_t read_ftrs(size_t a_num_frames, float* a_ftrs);
    size_t read_ftrs(size_t a_num_frames, float* a_ftrs, size_t stride);
    int rewind();

// This stuff is basically not implemented.
    size_t num_segs();
    size_t num_frames(size_t a_segno = QN_ALL);
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Logging object.
    QN_ClassLogger log;
    // The stream we are windowing.
    QN_InFtrStream& str;
    // The number of frames in the window.
    const size_t win_len;
    // The number of frames we discard at the start of each segment.
    const size_t top_margin;
    // The number of frames we discard at the end of each segment.
    const size_t bot_margin;
    // The number of features in one frame of the input stream.
    const size_t in_width;
    // The number of features in one window full.
    const size_t out_width;
    // The number of frames we read from the input buffer in one bunch.
    const size_t bunch_frames;
    // The size of the window buffer in frames.
    const size_t max_buf_lines;
    // The buffer used for building up windows.
    float* buf;
    // The number of lines in the buffer.
    size_t buf_lines;
    // The current line (frame) we read from in the buffer;
    size_t cur_line;
    // Pointer to start of current line in buffer.
    float* cur_line_ptr;
    // Pointer to first line in buffer after top margin.
    float* first_line_ptr;
    
    // The number of the current segment (for debugging).
    long segno;
};


////////////////////////////////////////////////////////////////
// A window stream for label streams - much the same as above.

class QN_InLabStream_SeqWindow : public QN_InLabStream
{
public:
    // "a_debug" - level of debugging output.
    // "a_dbgname" - tag for debugging output.
    // "a_str" - input stream.
    // "a_win_len" - length of window in frames.
    // "a_top_margin" - number of frames discarded at start of segment.
    // "a_bot_margin" - number of frames discarded at bottom of segment.
    // "a_bunch_frames" - number of frames to read from input stream in one
    //                    read (QN_SIZET_BAD means use sensible default size).
    QN_InLabStream_SeqWindow(int a_debug,
			     const char* a_dbgname,
			     QN_InLabStream& a_str,
			     size_t a_win_len, size_t a_top_margin,
			     size_t a_bot_margin,
			     size_t a_bunch_frames = QN_SIZET_BAD);
    ~QN_InLabStream_SeqWindow();

    size_t num_labs();
    QN_SegID nextseg();
    size_t read_labs(size_t a_num_frames, QNUInt32* a_labs, size_t stride);
    size_t read_labs(size_t a_num_frames, QNUInt32* a_labs);
    int rewind();

// This stuff is basically not implemented.
    size_t num_segs();
    size_t num_frames(size_t a_segno = QN_ALL);
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Logging object.
    QN_ClassLogger log;
    // The stream we are windowing.
    QN_InLabStream& str;
    // The number of frames in the window.
    const size_t win_len;
    // The number of frames we discard at the start of each segment.
    const size_t top_margin;
    // The number of frames we discard at the end of each segment.
    const size_t bot_margin;
    // The number of labels in one frame of the input stream.
    const size_t in_width;
    // The number of labels in one window full.
    const size_t out_width;
    // The number of frames we read from the input buffer in one bunch.
    const size_t bunch_frames;
    // The size of the window buffer in frames.
    const size_t max_buf_lines;
    // The buffer used for building up windows.
    QNUInt32* buf;
    // The number of lines in the buffer.
    size_t buf_lines;
    // The current line (frame) we read from in the buffer;
    size_t cur_line;
    // Pointer to start of current line in buffer.
    QNUInt32* cur_line_ptr;
    // Pointer to first line in buffer after top margin.
    QNUInt32* first_line_ptr;
    
    // The number of the current segment (for debugging).
    long segno;

};


////////////////////////////////////////////////////////////////
// This is the database randomization and windowing class for MLP training.
//
// Segments are read from the input stream sequentially until the buffer
// is full.  Windows of data are then returned randomly or 
// sequentially from this buffer,
// with all the frames appearing to be in the same segment.  Edge conditions
// are taken care of - windows only take date from one segment.
//
// The exact pattern is controlled by the seed and the order.
// Note that the seed is _not_ reset to its initial value
// after a rewind, but rather to a defined but different value.
// Successive passes over the data will be
// done in different, but defined orders, with the order being independent
// of the state when a rewind was performed.


class QN_InLabStream_RandWindow : public QN_InLabStream
{
public:
    enum Order {
	SEQUENTIAL,
	RANDOM_NO_REPLACE
    };
    // "a_debug" - level of debugging output.
    // "a_dbgname" - tag for debugging output.
    // "a_str" - input stream.
    // "a_win_len" - length of window in frames.
    // "a_top_margin" - number of frames discarded at start of segment.
    // "a_bot_margin" - number of frames discarded at bottom of segment.
    // "a_buf_frames" - number of frames to store in the buffer.
    // "a_order" - the order we extract frames from the buffer.
    // "a_seed" - the random number seed.
    QN_InLabStream_RandWindow(int a_debug, const char* a_dbgname,
			      QN_InLabStream& a_str,
			      size_t a_win_len, size_t a_top_margin,
			      size_t a_bot_margin,
			      size_t a_buf_frames,
			      QNUInt32 a_seed,
			      Order a_order = RANDOM_NO_REPLACE
			      );
    ~QN_InLabStream_RandWindow();

    size_t num_labs();
    QN_SegID nextseg();
    size_t read_labs(size_t a_num_frames, QNUInt32* a_labs, size_t stride);
    size_t read_labs(size_t a_num_frames, QNUInt32* a_labs);
    int rewind();

    // The number of segments is NOT the number of segments in the input 
    // stream - it is the number of times the buffer is refilled for one pass
    // over the data.  This information can be useful for progress reporting.
    size_t num_segs();
    // This returns the number of presentations in one epoch.
    size_t num_frames(size_t a_segno = QN_ALL);

// These do not work.
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Logging object.
    QN_ClassLogger clog;
    // The stream we are windowing.
    QN_InLabStream& str;
    // The number of frames in the window.
    const size_t win_len;
    // The number of frames we discard at the start of each segment.
    const size_t top_margin;
    // The number of frames we discard at the end of each segment.
    const size_t bot_margin;
    // The order we extract presentations from our buffer of contiguous frames.
    enum Order order;
    // The seed from the constructor.
    QNUInt32 seed;
    // The number of features in one frame of the input stream.
    const size_t in_width;
    // The number of features in one window full.
    const size_t out_width;
    // The maximum number of frames we read from the input buffer at one time.
    const size_t buf_frames;
    // The number of segments in the output stream.
    size_t out_n_segs;
    // The number of frames in the output stream.
    size_t out_n_frames;
    // An index for mapping from output segment to input segments.
    size_t* out_seg_index;
    // The number of usable frames in the current buffer full.
    size_t usable_frames;
    // A vector of pointers to the start of all usable output frames
    // in the buffer.
    QNUInt32** buf_frame_index;
    // The buffer used for holding the data that will be presented as one
    // output segment.
    QNUInt32* buf;
    // The number of the epoch.
    size_t epoch;
    // The current output segment number.
    size_t out_segno;
    // The current output frame number.
    size_t out_frameno;
    // The sequence generator we use to select the next frame in the buffer.
    QN_SeqGen* seqgen;		
};

////////////////////////////////////////////////////////////////
// A randomizing windowing stream for feature streams - much the same as above.

class QN_InFtrStream_RandWindow : public QN_InFtrStream
{
public:
    enum Order {
	SEQUENTIAL,
	RANDOM_NO_REPLACE
    };
    // "a_debug" - level of debugging output.
    // "a_dbgname" - tag for debugging output.
    // "a_str" - input stream.
    // "a_win_len" - length of window in frames.
    // "a_top_margin" - number of frames discarded at start of segment.
    // "a_bot_margin" - number of frames discarded at bottom of segment.
    // "a_buf_frames" - number of frames to store in the buffer.
    // "a_order" - the order we extract frames from the buffer.
    // "a_seed" - the random number seed.
    QN_InFtrStream_RandWindow(int a_debug, const char* a_dbgname,
			      QN_InFtrStream& a_str,
			      size_t a_win_len, size_t a_top_margin,
			      size_t a_bot_margin,
			      size_t a_buf_frames,
			      QNUInt32 a_seed,
			      Order a_order = RANDOM_NO_REPLACE
			      );
    ~QN_InFtrStream_RandWindow();

    size_t num_ftrs();
    QN_SegID nextseg();
    size_t read_ftrs(size_t a_num_frames, float* a_ftrs, size_t stride);
    size_t read_ftrs(size_t a_num_frames, float* a_ftrs);
    int rewind();

    // The number of segments is NOT the number of segments in the input 
    // stream - it is the number of times the buffer is refilled for one pass
    // over the data.  This information can be useful for progress reporting.
    size_t num_segs();
    // This returns the number of presentations in one epoch.
    size_t num_frames(size_t a_segno = QN_ALL);

// These do not work.
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    // Logging object.
    QN_ClassLogger clog;
    // The stream we are windowing.
    QN_InFtrStream& str;
    // The number of frames in the window.
    const size_t win_len;
    // The number of frames we discard at the start of each segment.
    const size_t top_margin;
    // The number of frames we discard at the end of each segment.
    const size_t bot_margin;
    // The order we extract presentations from our buffer of contiguous frames.
    enum Order order;
    // The seed from the constructor.
    QNUInt32 seed;
    // The number of features in one frame of the input stream.
    const size_t in_width;
    // The number of features in one window full.
    const size_t out_width;
    // The maximum number of frames we read from the input buffer at one time.
    const size_t buf_frames;
    // The number of segments in the output stream.
    size_t out_n_segs;
    // The number of frames in the output stream.
    size_t out_n_frames;
    // An index for mapping from output segment to input segments.
    size_t* out_seg_index;
    // The number of usable frames in the current buffer full.
    size_t usable_frames;
    // A vector of pointers to the start of all usable output frames
    // in the buffer.
    float** buf_frame_index;
    // The buffer used for holding the data that will be presented as one
    // output segment.
    float* buf;
    // The number of the epoch.
    size_t epoch;
    // The current output segment number.
    size_t out_segno;
    // The current output frame number.
    size_t out_frameno;
    // The sequence generator we use to select the next frame in the buffer.
    QN_SeqGen* seqgen;		
};

#endif
