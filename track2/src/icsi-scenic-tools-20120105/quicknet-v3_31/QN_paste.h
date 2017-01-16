// $Header: /u/drspeech/repos/quicknet2/QN_paste.h,v 1.1 1999/05/06 02:52:03 dpwe Exp $

#ifndef QN_paste_h_INCLUDED
#define QN_paste_h_INCLUDED


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_filters.h"

////////////////////////////////////////////////////////////////
// The "Paste" class takes two (several?) feature streams of the 
// same width and glues them end to end, to make one large effective 
// utterance sequence, formed by taking the utterances of each 
// stream in sequence.  It is used to get around the 2G filesize 
// limit


class QN_InFtrStream_Paste : public QN_InFtrStream
{
public:
    // "a_debug" is the level of debugging verbosity.
    // "a_dbgname" is the name of the instance to use in log messages.
    // "a_str1" is the first of the two streams to be concatenated.
    // "a_str2" is the second of the two streams to be concatenated.
    QN_InFtrStream_Paste(int a_debug,
			 const char* a_dbgname,
			 QN_InFtrStream& a_str1,
			 QN_InFtrStream& a_str2);
    ~QN_InFtrStream_Paste();

    size_t num_ftrs();
    QN_SegID nextseg();
    size_t read_ftrs(size_t a_cnt, float* a_ftrs);
    size_t read_ftrs(size_t a_cnt, float* a_ftrs, size_t a_stride);
    int rewind();
    
    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
    int get_pos(size_t* segno, size_t* frameno);
    QN_SegID set_pos(size_t segno, size_t frameno);

protected:
    QN_ClassLogger clog;		// Logging object.
    QN_InFtrStream& str1;	// The first stream.
    QN_InFtrStream& str2;	// The second stream.
    QN_InFtrStream* cur_str;	// The current stream
    size_t str1_n_segs;		// No. of segs in the first stream.
    size_t str2_n_segs;		// No. of segs in the second stream.
    size_t n_ftrs;				  
    size_t current_seg;
    size_t seg_base;
};

inline size_t
QN_InFtrStream_Paste::num_ftrs()
{
    return n_ftrs;
}

inline size_t
QN_InFtrStream_Paste::num_segs()
{
    return str1_n_segs + str2_n_segs;
}

// This is just a call of the strided version.
inline size_t
QN_InFtrStream_Paste::read_ftrs(size_t cnt, float* ftrs)
{
    return read_ftrs(cnt, ftrs, QN_ALL);
}

#endif
