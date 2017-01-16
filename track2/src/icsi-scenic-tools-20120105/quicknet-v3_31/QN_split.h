// $Header: /u/drspeech/repos/quicknet2/QN_split.h,v 1.1 1996/05/24 00:13:26 davidj Exp $

#ifndef QN_split_h_INCLUDED
#define QN_split_h_INCLUDED


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"

////////////////////////////////////////////////////////////////
// The "SplitFtrLab" filter splits an InFtrLabStream into separate independent
// InFtrStreams and InLabStreams.  The features are returned by the main class
// and the labels can be accessed using the helper class
// "QN_InLabStream_SplitFtrLab".  This class does lots of clever buffering in
// an attempt to minimize the number if repeated reads from the input stream.

class QN_InFtrStream_SplitFtrLab : public QN_InFtrStream
{
public:
    // "a_debug" is the level of debugging.
    // "a_dbgname" is the instance name used in the debugging output."
    // "a_instr" is the feature+label stream that we are splitting into
    //           a separate, asynchronous feature and label streams.
    // "a_buf_frames" is the size of the feature and/or label buffer in
    //                frames.  The setting of this is important - see above
    //                for more details.
    QN_InFtrStream_SplitFtrLab(int a_debug, const char* a_dbgname,
			       QN_InFtrLabStream& a_instr,size_t a_buf_frames);
    ~QN_InFtrStream_SplitFtrLab();

    // Access functions for the feature stream.
    size_t num_ftrs();
    size_t read_ftrs(size_t cnt, float* ftrs);
    QN_SegID nextseg();
    int rewind();
    int get_pos(size_t* segnop, size_t* framenop);
    QN_SegID set_pos(size_t segno, size_t frameno);

    // Access functions for the label stream.
    size_t num_labs();
    size_t read_labs(size_t cnt, QNUInt32* labs);
    QN_SegID lab_nextseg();
    int lab_rewind();
    int lab_get_pos(size_t* segno, size_t* frameno);
    QN_SegID lab_set_pos(size_t segno, size_t frameno);

    // These functions are shared by both streams.
    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
private:
    QN_ClassLogger log;		// Our logging object.
    QN_InFtrLabStream& instr;	// The stream we are splitting.
    size_t buf_frames;		// The maximum number of frames to buffer.
    size_t n_ftrs;		// The number of features in the input stream.
    size_t n_labs;		// The number of labels in the input stream.

    size_t instr_segno;		// A local copy of the segment no. for the
				// stream being read.
    size_t instr_frameno;	// A local copy of the frame no. for the
				// stream being read.
    QN_SegID instr_segid;	// A copy of the segment ID for the stream
				// being read.

    size_t ftr_segno;		// The current feature segment number.
    size_t ftr_frameno;		// The current feature frame number.
    float* ftr_buf;		// A buffer for read features.
    size_t ftr_buf_len;		// The length of the feature buffer.
    size_t ftr_buf_start;	// The no. of the frame in the feature buffer
				// containing the next feature vector to read.
    size_t ftr_buf_end;		// The no. of the frame in the feature buffer
				// where the next read frame should be put.
    size_t ftr_buf_segno;	// The segment number of the first frame in
				// the feature buffer.
    size_t ftr_buf_frameno;	// The frame number of the first frame in
				// the feature buffer.

    size_t lab_segno;		// The current label segment number.
    size_t lab_frameno;		// The current label frame number.
    QNUInt32* lab_buf;		// A buffer for read labels.
    size_t lab_buf_len;		// The length of the label buffer.
    size_t lab_buf_start;	// The no. of the frame in the label buffer
				// containing the next feature vector to read.
    size_t lab_buf_end;		// The no. of the frame in the label buffer
				// where the next read frame should be put.
    size_t lab_buf_segno;	// The segment number of the first frame in
				// the label buffer.
    size_t lab_buf_frameno;	// The frame number of the first frame in
				// the label buffer.

// Misc. private member functions.

    // Seek to "segno" and "frameno" if we are not there already.
    void seek_if_necessary(size_t segno, size_t frameno);
    // Make space in the feature buffer for "frames" more frames if possible,
    // returning QN_OK on success else QN_BAD.
    int ftr_freespace(size_t frames);
    // Make space in the label buffer for "frames" more frames if possible,
    // returning QN_OK on success else QN_BAD.
    int lab_freespace(size_t frames);
};

////////////////////////////////////////////////////////////////
// The helper function that aids QN_InFtrStream_SplitFtrLab by providing
// the label reading functions in a form that compiles with the QN_InLabStream
// interface.

class QN_InLabStream_SplitFtrLab : public QN_InLabStream
{
public:
    // "a_splitstr" - the already created stream that does that actual
    //                work.
    QN_InLabStream_SplitFtrLab(QN_InFtrStream_SplitFtrLab& a_splitstr)
	: splitstr(a_splitstr)
    {   };

    ~QN_InLabStream_SplitFtrLab() {};

    size_t num_labs() { return splitstr.num_labs(); };
    size_t read_labs(size_t cnt, QNUInt32* labs)
        { return splitstr.read_labs(cnt, labs); };
    QN_SegID nextseg() { return splitstr.lab_nextseg(); };
    int rewind() { return splitstr.lab_rewind(); };
    int get_pos(size_t* segno, size_t* frameno)
        { return splitstr.lab_get_pos(segno, frameno); };
    QN_SegID set_pos(size_t segno, size_t frameno)
        { return splitstr.lab_set_pos(segno, frameno); };


    // These functions are the same for the label and feature stream.
    size_t num_segs() { return splitstr.num_segs(); };
    size_t num_frames(size_t segno = QN_ALL)
        { return splitstr.num_frames(segno); };
    
private:
    QN_InFtrStream_SplitFtrLab& splitstr;
};

#endif
