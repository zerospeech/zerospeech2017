// -*- c++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_fir.h,v 1.5 2007/06/13 18:34:44 davidj Exp $

#ifndef QN_fir_h_INCLUDED
#define QN_fir_h_INCLUDED

// This header defines the FIR filter class, use to add deltas to 
// feature streams 'on the fly'

// Formerly part of QN_filters.h

#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_args.h"
#include "QN_filters.h"

////////////////////////////////////////////////////////////////
// The Buffered stream maintains a local buffer around the current 
// point so that operations needing context or lookahead can have 
// it.  Used by FIR and OnlNorm (with lookahead)

class QN_InFtrStream_Buffered : public QN_InFtrStream
{
public:
    // Create a buffer for a feature stream.
    // "a_debug" is the level of debugging verbosity.
    // "a_dbgname" is the tag used for this instance in debugging output.
    // "a_str" is the stream we are buffering
    // "a_buf_frames" is the max no. of frames we handle in one go.
    QN_InFtrStream_Buffered(int a_debug, const char* a_dbgname,
		       QN_InFtrStream& a_str,
		       size_t a_buf_frames = 128
		       );
    ~QN_InFtrStream_Buffered();

    QN_SegID nextseg();
    int rewind();
    
    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);

    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

private:
    QN_ClassLogger log;		// Logging object.

protected:
    size_t fill_input_buffer(int start, int len);

    QN_InFtrStream& str;	// The stream we are filtering.
    size_t in_ftr_count;	// The no of ftrs in str

    // Tracking our effective pos
    size_t my_seg;
    size_t my_pos;
    size_t my_seg_len;
    // pos in input stream too, to skip unnecessary seeks
    size_t str_pos;

    // Defining the input cache
protected:
    size_t buf_frames;		// No. of frames we buffer.
private:
    int buf_base;		// frame index of first frame in buffer
    size_t buf_cont;		// how many valid frames in buf, from <base>
protected:
    size_t buf_maxpad;		// Up to how many frames are glued to end
    float *input_buffer;	// the buffer itself
};

////////////////////////////////////////////////////////////////
// The "FIR" filter applies an FIR filter to one or more channels of
// the input stream to produce additional channels.  This is typically
// used to add derivatives to feature streams.

class QN_InFtrStream_FIR : public QN_InFtrStream_Buffered
{
public:
    // Create a "FIR" filter.
    // Note that copies of the coeffs vector is taken so the
    // original vectors can be deleted.
    // "a_debug" is the level of debugging verbosity.
    // "a_dbgname" is the tag used for this instance in debugging output.
    // "a_str" is the stream we are filtering.
    // "a_filt_len" is the length of the filter.
    // "a_filt_coeffs" are the filter coefficients.
    // "a_ftr_start" is the first ftr channel we filter.
    // "a_ftr_cnt" is the number of ftr channels we filter.
    // "a_buf_frames" is the max no. of frames we handle in one go.
    //    It should be at least filt_len-1 larger than the buf_frames
    //     of the calling stream, so that those calls can be serviced
    //     with a single set of convolutions.
    QN_InFtrStream_FIR(int a_debug, const char* a_dbgname,
		       QN_InFtrStream& a_str,
		       size_t a_filt_len,
		       size_t a_filt_ctr,
		       const float* a_filt_coeffs,
		       size_t a_ftr_start = 0,
		       size_t a_ftr_cnt = QN_ALL,
		       size_t a_buf_frames = 128
		       );
    ~QN_InFtrStream_FIR();
    size_t num_ftrs();
    size_t read_ftrs(size_t cnt, float* ftrs);
    
    // Class statics to generate standard delta filter kernels
    // build a filter for delta calculation - write <len> pts into fv[]
    static int FillDeltaFilt(float *fv, size_t len);
    // build a filter for double-delta calculation - convol'n of 2 dfilts
    static int FillDoubleDeltaFilt(float *fv, size_t len);

private:
    QN_ClassLogger clog;		// Logging object.

    size_t out_ftr_count;	// total number of output ftrs
    size_t ftr_start;		// The first ftr we filter.
    size_t filter_len;		// Length of filter
    size_t filter_ctr;		// offset to 'center' point in filter
    float* filter;		// Filter coefficients.
  
    float *input_buffer_t;	// transpose of input features (NEW)
    float *output_buffer_t;	// transpose/scratch of output features (NEW)
};

#endif /* #define QN_fir_h_INCLUDED */
