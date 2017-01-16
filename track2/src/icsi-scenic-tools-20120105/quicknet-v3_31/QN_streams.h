// -*- c++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_streams.h,v 1.20 2004/09/08 01:36:28 davidj Exp $

#ifndef QN_streams_h_INCLUDED
#define QN_streams_h_INCLUDED

#include <QN_config.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_types.h"
#include "QN_Logger.h"

// QN_OutFtrStream is a base class used to define an interface to a stream
// that accepts vectors of floating point features, with the stream being
// divided up into segments (typically sentences).  The stream can only be
// written sequentially.
// This is typically used to write out file from the forward pass of 
// an MLP, e.g. phoneme probabilities, although utilities could use the
// routines to process files in various ways.

class QN_OutFtrStream
{
public:
    virtual ~QN_OutFtrStream() { };

    // Find the width of the feature stream.
    virtual size_t num_ftrs() = 0;

    // Finish the current segment and get ready to start the next one.
    // The segment ID refers to the segment just finished.
    // Segment IDs are used to identify a given segment throughout
    // the quicknet library - they may be mapped to a segment label by an
    // output stream.
    virtual void doneseg(QN_SegID segid) = 0;

    // Write "cnt" vectors of feature data.
    virtual void write_ftrs(size_t cnt, const float* ftrs) = 0;
};


// QN_OutLabStream is a base class used to define an interface to a stream
// that accepts vectors of integer labels, with the stream being
// divided up into segments (typically sentences).  The stream can only be
// written sequentially.  The segments (sentences) are identified with a 
// sentence ID.

class QN_OutLabStream
{
public:
    virtual ~QN_OutLabStream() { };

    // Find the width of the stream.
    virtual size_t num_labs() = 0;

    // Finish the current segment and get ready to start the next one.
    // The segment ID refers to the segment just finished.
    // Segment IDs are used to identify a given segment throughout
    // the quicknet library - they may be mapped to a segment label by an
    // output stream.
    virtual void doneseg(QN_SegID segid) = 0;

    // Write "cnt" vectors of label data.
    virtual void write_labs(size_t cnt, const QNUInt32* labs) = 0;
};

// QN_OutFtrStream is a base class used to define an interface to a stream
// that accepts vectors of floating point features and vectors of integer
// labels. The stream can only be written sequentially.  This is typically
// used to write out combined feature/label files such as PFiles.

class QN_OutFtrLabStream : public QN_OutFtrStream, public QN_OutLabStream
{
public:
    virtual ~QN_OutFtrLabStream() { };

    // Find the number of features in one frame.
    virtual size_t num_ftrs() = 0;

    // Find the number of labels in one frame.
    virtual size_t num_labs() = 0;

    // Finish the current segment and get ready to start the next one.
    // The segment ID refers to the segment just finished.
    // Segment IDs are used to identify a given segment throughout
    // the quicknet library - they may be mapped to a segment label by an
    // output stream.
    virtual void doneseg(QN_SegID segid) = 0;

    // Write "cnt" vectors of feature data.
    virtual void write_ftrslabs(size_t cnt, const float* ftrs,
			       const QNUInt32* labs) = 0;
};

// QN_InStream is a base class for QN_InFtrStream and QN_InLabStream see
// these classes for more detail.
// It exists so details of the size of a stream can be found out without
// knowing exactly what type of stream it is.
class QN_InStream
{
public:
// The following functions may no be implemented - if this is so,
// they return an error value and DO NOT call QN_ERROR()

    // Return the total number of accesible segments in the file,
    // or QN_SIZET_BAD if not known.
    virtual size_t num_segs() = 0;

    // Return the number of frames in the given segment,
    // or the whole file if segno==QN_SIZET_BAD
    // Note that it is a segment number, not a segment ID, that is passed in.
    // The segment number starts at 0 for the first accessible segment
    // and increments by one for each sentence.
    // Returns QN_SIZET_BAD if this is not possible.
    virtual size_t num_frames(size_t segno = QN_ALL) = 0;
};


// QN_InFtrStream is a base class used to define an interface to a stream that
// returns vectors of floating point features, with the stream being divided
// up into segments (typically sentences).  The stream can be read
// sequentially and maybe also supports random access.  The segments
// (sentences) are arbitrarily numbered.  This is typically used to read
// training data for an MLP.

class QN_InFtrStream : public virtual QN_InStream
{
public:
    virtual ~QN_InFtrStream() { };

    // Find the width of feature stream
    virtual size_t num_ftrs() = 0;

    // Move on to the next sentence.
    // The segment ID return is used to identify a given segment throughout
    // the quicknet library - it may be mapped to a sentence number by the 
    // stream.  If nothing can be returned because e.g. we are at the
    // end of the file, QN_SEGID_BAD is returned.
    // Note that nextseg() must be called before reading the first sentence.
    virtual QN_SegID nextseg() = 0;

    // Read "cnt" vectors of feature data.
    // The return value is either the number of frames read or QN_SIZET_BAD.
    // If the return value is less than "cnt", we have reached the end of the
    // sentence.
    virtual size_t read_ftrs(size_t cnt, float* ftrs) = 0;

    // A strided version of the read_ftrs function.  Initially, not all
    // derived classes will implement this, so provide a default version.
    // If stride==QN_ALL, stride is same as num_ftrs().
    virtual size_t read_ftrs(size_t cnt, float* ftrs, size_t stride);


    // Move back to the start of the stream
    // Returns QN_BAD if not possible (i.e. stream not seekable - e.g. pipe)
    // After rewind(), nextseg() must be used to get to start of first segment.
    virtual int rewind() = 0;
    

// The following functions may no be implemented - if this is so,
// they return an error value and DO NOT call QN_ERROR()

    // Return the total number of accesible segments in the file,
    // or QN_SIZET_BAD if not known.
    virtual size_t num_segs() = 0;

    // Return the number of frames in the given segment,
    // or the whole file if segno==QN_SIZET_BAD
    // Note that it is a segment number, not a segment ID, that is passed in.
    // The segment number starts at 0 for the first accessible segment
    // and increments by one for each sentence.
    // Returns QN_SIZET_BAD if this is not possible.
    virtual size_t num_frames(size_t segno = QN_ALL) = 0;

    // Return the current segment and frame number (the next to be read).
    // Returns QN_BAD if not possible.  segno and/or frameno can be NULL.
    // *segno and *frameno are set to QN_SIZET_BAD if before first sentence.
    virtual int get_pos(size_t* segno, size_t* frameno) = 0;

    // Move to a given segment and frame number.
    // On failure returns QN_SEGID_BAD, otherwise the segment ID of the
    // sentence.
    // Note - set_pos(0,0) will fail on an unindexed file whereas
    // rewind() might work.
    virtual QN_SegID set_pos(size_t segno, size_t frameno) = 0;
};


// Default imlpementation of the strided read_ftrs function.  This can
// disappear when all derived functions implement this functionality.

inline size_t
QN_InFtrStream::read_ftrs(size_t cnt, float* ftrs, size_t stride)
{
    size_t ret = 0;

    if (stride==num_ftrs() || stride==QN_ALL)
    {
	ret = read_ftrs(cnt, ftrs);
    }
    else
    {
	//QN_ERROR("QN_InFtrStream", "tried to do unimplemented "
	//	 "strided feature read.");
	//ret = QN_SIZET_BAD;

	//log.warn("slow emulation of unimplemented strided feature read.");
	fprintf(stderr, "slow emulation of strided read\n");
	size_t i = 0;
	while (i < cnt) {
	    ret += read_ftrs(1, ftrs);
	    ftrs += stride;
	    ++i;
	}
    }
    return ret;
}

// QN_InLabStream is a base class used to define an interface to a stream that
// returns vectors of integer "labels", with the stream being divided
// up into segments (typically sentences).  The stream can be read
// sequentially and maybe also supports random access.  The segments
// (sentences) are arbitrarily numbered.  This is typically used to read
// phone labels for training an MLP.

class QN_InLabStream : public virtual QN_InStream
{
public:
    virtual ~QN_InLabStream() { };

    // Find the width of the label stream - often 1
    virtual size_t num_labs() = 0;

    // Move on to the next segment.  The segment ID returned corresponds to
    // the next sentence, is used to identify a given segment throughout the
    // quicknet library - it may be mapped to a segment number by the stream.
    // If nothing can be returned because e.g. we are at the end of the file,
    // QN_SEGID_BAD is returned.
    // Note that nextseg() must be called before reading the first sentence.
    virtual QN_SegID nextseg() = 0;

    // Read "cnt" vectors of label data.
    // The return value is either the number of frames read or QN_SIZET_BAD.
    // If the return value is less than "cnt", we have reached the end of the
    // segment.
    virtual size_t read_labs(size_t cnt, QNUInt32* labs) = 0;
    
     // A strided version of the read_labs function.  Initially, not all
    // derived classes will implement this, so provide a default version.
    // If stride==QN_ALL, stride is same as num_labs().
    virtual size_t read_labs(size_t cnt, QNUInt32* labs, size_t stride);

   // Move back to the start of the stream Returns QN_BAD if not
    // possible (i.e. stream not seekable - e.g. pipe), otherwise returns
    // the segment ID of the first sentence.
    // After rewind(), nextseg() must be used to get to start of first segment.
    virtual int rewind() = 0;

// The following functions may no be implemented - if this is so,
// they return an error value and DO NOT call QN_ERROR()

    // Return the total number of accesible segments in the file,
    // or QN_SIZET_BAD if not known.
    virtual size_t num_segs() = 0;

    // Return the number of frames in the given segment,
    // or the whole file if segno==QN_SIZET_BAD
    // Note that it is a segment number, not a segment ID, that is passed in.
    // The segment number starts at 0 for the first accessible segment
    // and increments by one for each segment.
    // Returns QN_SIZET_BAD if this is not possible.
    virtual size_t num_frames(size_t segno = QN_ALL) = 0;

    // Return the current segment and frame number (the next to be read).
    // Returns QN_BAD if not possible.  segno and/or frameno can be NULL.
    // *segno and *frameno are set to QN_SIZET_BAD if before first sentence.
    virtual int get_pos(size_t* segno, size_t* frameno) = 0;

    // Move to a given segment and frame number.
    // On failure returns QN_SEGID_BAD, otherwise the segment ID of the
    // sentence.
    // Note - set_pos(0,0) will fail on an unindexed file whereas
    // rewind() might work.
    virtual QN_SegID set_pos(size_t segno, size_t frameno) = 0;
};

/// Default imlpementation of the strided read_labs function.  This can
// disappear when all derived functions implement this functionality.

inline size_t
QN_InLabStream::read_labs(size_t cnt, QNUInt32* labs, size_t stride)
{
    size_t ret;

    if (stride==num_labs() || stride==QN_ALL)
    {
	ret = read_labs(cnt, labs);
    }
    else
    {
	QN_ERROR("QN_InLabStream", "tried to do unimplemented "
		 "strided label read.");
	ret = QN_SIZET_BAD;
    }
    return ret;
}

// QN_InFtrLabStream is a base class used to define an interface to a stream
// that returns vectors of floating point features _AND_ vectors of integer
// labels.  It is typically used to access files suchs as PFiles.  The stream
// can be read sequentially and maybe also supports random access.  The
// segments (sentences) are arbitrarily numbered.  This is typically used to
// read training data for an MLP.

class QN_InFtrLabStream : public QN_InFtrStream, public QN_InLabStream 
{
public:
    virtual ~QN_InFtrLabStream() { };

    // Find the number of features in one frame
    virtual size_t num_ftrs() = 0;

    // Find the number of labels in one frame
    virtual size_t num_labs() = 0;

    // Move on to the next segment.  The segment ID returned correspondes to
    // the next segment and is used to identify a given segment throughout the
    // quicknet library.
    // If nothing can be returned because e.g. we are at the end of the file,
    // QN_SEGID_BAD is returned.
    // Note that nextseg() must be called before reading the first sentence.
    virtual QN_SegID nextseg() = 0;

    // Read "cnt" vectors of feature and label data.
    // The return value is either the number of features read or QN_SIZET_BAD.
    // If the return value is less than "cnt", we have reached the end of the
    // segment.
    virtual size_t read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs) = 0;

    // These are needed to comply with the parent class
    virtual size_t read_ftrs(size_t cnt, float* ftrs) = 0;
    virtual size_t read_labs(size_t cnt, QNUInt32* labs) = 0;
    
    // Move back to the start of the stream
    // Returns QN_BAD if not possible (i.e. stream not seekable - e.g. pipe)
    // After rewind(), nextseg() must be used to get to start of first segment.
    virtual int rewind() = 0;

// The following functions may no be implemented - if this is so,
// they return an error value and DO NOT call QN_ERROR()

    // Return the total number of accesible segments in the file,
    // or QN_SIZET_BAD if not known.
    virtual size_t num_segs() = 0;

    // Return the number of frames in the given segment
    // or on the whole file if segno=QN_SIZET_BAD
    // Note that it is a segment number, not a segment ID, that is passed in.
    // The segment number starts at 0 for the first accessible segment
    // and increments by one for each segment.
    // Returns QN_SIZET_BAD if this is not possible.
    virtual size_t num_frames(size_t segno = QN_ALL) = 0;

    // Return the current segment and frame number (the next to be read).
    // Returns QN_BAD if not possible.  segno and/or frameno can be NULL.
    // *segno and *frameno are set to QN_SIZET_BAD if before first sentence.
    virtual int get_pos(size_t* segno, size_t* frameno) = 0;

    // Move to a given segment and frame number.
    // On failure returns QN_SEGID_BAD, otherwise the segment ID of the
    // sentence.
    // Note - set_pos(0,0) will fail on an unindexed file whereas
    // rewind() might work.
    virtual QN_SegID set_pos(size_t segno, size_t frameno) = 0;
};


#endif // #ifdef QN_streams_h_INCLUDED
