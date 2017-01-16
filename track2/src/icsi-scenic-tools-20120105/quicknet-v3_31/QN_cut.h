// -*- C++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_cut.h,v 1.5 1999/11/04 05:53:02 dpwe Exp $

#ifndef QN_cut_h_INCLUDED
#define QN_cut_h_INCLUDED


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_Range.h"

////////////////////////////////////////////////////////////////
// The "Cut" stream takes and existing stream and "cuts" it into two.
// The two new streams can contain any contiguous range of segments,
// and may be overlapped (or even identical).  The two streams operate
// independently, although rapid switching between streams may be inefficient.
// The "Cut" stream is typically used to split an existing stream into
// two streams for training and cross validation.
//
// The "Cut2" helper stream is used to access the second stream.

class QN_InFtrStream_Cut : public QN_InFtrStream
{
    friend class QN_InFtrStream_Cut2;
public:
    // "a_debug" is the level of debugging.
    // "a_dbgname" is the instance name used in the debugging output."
    // "a_instr" is the feature stream that we are cutting in two.
    // "a_firstseg1" is the segment from the input stream that becomes the
    //     first segment in cut 1.
    // "a_numsegs1" is the number of segments in the first cut. "QN_ALL"
    //     means all until the end of the stream.
    // "a_firstseg2" is the segment from the input stream that becomes the
    //     first segment in cut 2.
    // "a_numsegs2" is the number of segments in the second cut. "QN_ALL"
    //     means all until the end of the stream.
    QN_InFtrStream_Cut(int a_debug, const char* a_dbgname,
		       QN_InFtrStream& a_instr,
		       size_t a_firstseg1 = 0, size_t a_numsegs1 = QN_ALL,
		       size_t a_firstseg2 = 0, size_t a_numsegs2 = QN_ALL);
    virtual ~QN_InFtrStream_Cut();

    // Member functions shared by both streams.
    size_t num_ftrs();

    // Member functions for the first cut.
    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
    size_t read_ftrs(size_t cnt, float* ftrs);
    QN_SegID nextseg();
    int rewind();
    int get_pos(size_t* segnop, size_t* framenop);
    QN_SegID set_pos(size_t segno, size_t frameno);

    // Member functions for the second cut.
    size_t num_segs2();
    size_t num_frames2(size_t segno = QN_ALL);

protected:
    // A structure for holding the status information for a given cut.
    struct CutDetails
    {
	size_t first_seg;	// The segment that is considered seg. zero.
	size_t num_segs;	// The number of segments in the cut.
	size_t frames;		// Total number of frames.
	size_t segno;		// The current segment number
				// QN_SIZET_BAD implies before segment 0.
	size_t frameno;		// The current frame number within segment.
    };
    // An enum for remembering which cut was the last we accessed.
    enum CutSelector { CUT1, CUT2 };

    QN_ClassLogger clog;	// Our logging object.
    QN_InFtrStream& instr;	// The stream we are cutting.
    
    CutDetails cut1_info;	// The status information for cut 1.
    CutDetails cut2_info;	// The status information for cut 2.
    CutDetails* cut_infop;	// A pointer to the current status structure.
    CutSelector current_cut;	// Which cut was accessed last.

// A virtual function to actually figure the cut mapping
    virtual int segnum_map(CutSelector cut, unsigned int index) {
    // Return the instr segno mapped from a particular cut and index
	int ix = -1;
	if (cut == CUT1) {
	    assert(index < cut1_info.num_segs);
	    ix = cut1_info.first_seg + index;
	} else { /* CUT2 */
	    assert(index < cut2_info.num_segs);
	    ix = cut2_info.first_seg + index;
	}
	return ix;
    }

// Some private functions.
    // Actually do the work of the nextseg() calls.
    virtual QN_SegID nextseg(CutSelector cut);
    // Actually do the work of the rewind() calls.
    virtual int rewind(CutSelector cut);
    // Actually do the work of the get_pos() calls.
    int get_pos(CutSelector cut, size_t* segnop, size_t* framenop);
    // Actually do the work of the set_pos() calls.
    virtual QN_SegID set_pos(CutSelector cut, size_t segno, size_t frameno);
    // Actually do the work of the read_ftrs() calls.
    size_t read_ftrs(CutSelector cut, size_t count, float* ftrs);
    // Make a given cut the current one.
    virtual void select_cut(CutSelector new_cut);
};

inline QN_SegID
QN_InFtrStream_Cut::nextseg()
{
    return nextseg(CUT1);
}

inline int
QN_InFtrStream_Cut::get_pos(size_t* segnop, size_t* framenop)
{
    return get_pos(CUT1, segnop, framenop);
}

inline QN_SegID
QN_InFtrStream_Cut::set_pos(size_t segno, size_t frameno)
{
    return set_pos(CUT1, segno, frameno);
}

inline int
QN_InFtrStream_Cut::rewind()
{
    return rewind(CUT1);
}

inline size_t
QN_InFtrStream_Cut::read_ftrs(size_t count, float* ftrs)
{
    return read_ftrs(CUT1, count, ftrs);
}


////////////////////////////////////////////////////////////////
// The helper class that aids QN_InFtrStream_Cut by providing
// the functions for the secund cut.

class QN_InFtrStream_Cut2 : public QN_InFtrStream
{
public:
    // "a_cutstr" - the already created stream that does that actual
    //                work.
    QN_InFtrStream_Cut2(QN_InFtrStream_Cut& a_cutstr)
	: cutstr(a_cutstr)
    {   };

    ~QN_InFtrStream_Cut2() {};

    size_t num_ftrs() { return cutstr.num_ftrs(); };

    size_t num_segs() { return cutstr.num_segs2(); };
    size_t num_frames(size_t segno = QN_ALL)
         { return cutstr.num_frames2(segno); };
    size_t read_ftrs(size_t cnt, float* ftrs)
        { return cutstr.read_ftrs(QN_InFtrStream_Cut::CUT2, cnt, ftrs); };
    QN_SegID nextseg() { return cutstr.nextseg(QN_InFtrStream_Cut::CUT2); };
    int rewind() { return cutstr.rewind(QN_InFtrStream_Cut::CUT2); };
    int get_pos(size_t* segno, size_t* frameno)
        { return cutstr.get_pos(QN_InFtrStream_Cut::CUT2, segno, frameno); };
    QN_SegID set_pos(size_t segno, size_t frameno)
        { return cutstr.set_pos(QN_InFtrStream_Cut::CUT2, segno, frameno); };

private:
    QN_InFtrStream_Cut& cutstr;
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// The InLabStream cutting class.

class QN_InLabStream_Cut : public QN_InLabStream
{
    friend class QN_InLabStream_Cut2;
public:
    // "a_debug" is the level of debugging.
    // "a_dbgname" is the instance name used in the debugging output."
    // "a_instr" is the label stream that we are cutting in two.
    // "a_firstseg1" is the segment from the input stream that becomes the
    //     first segment in cut 1.
    // "a_numsegs1" is the number of segments in the first cut. "QN_ALL"
    //     means all until the end of the stream.
    // "a_firstseg2" is the segment from the input stream that becomes the
    //     first segment in cut 2.
    // "a_numsegs2" is the number of segments in the second cut. "QN_ALL"
    //     means all until the end of the stream.
    QN_InLabStream_Cut(int a_debug, const char* a_dbgname,
		       QN_InLabStream& a_instr,
		       size_t a_firstseg1 = 0, size_t a_numsegs1 = QN_ALL,
		       size_t a_firstseg2 = 0, size_t a_numsegs2 = QN_ALL);
    virtual ~QN_InLabStream_Cut();

    // Member functions shared by both streams.
    size_t num_labs();

    // Member functions for the first cut.
    size_t num_segs();
    size_t num_frames(size_t segno = QN_ALL);
    size_t read_labs(size_t cnt, QNUInt32* labs);
    QN_SegID nextseg();
    int rewind();
    int get_pos(size_t* segnop, size_t* framenop);
    QN_SegID set_pos(size_t segno, size_t frameno);

    // Member functions for the second cut.
    size_t num_segs2();
    size_t num_frames2(size_t segno = QN_ALL);

protected:
    // A structure for holding the status information for a given cut.
    struct CutDetails
    {
	size_t first_seg;	// The segment that is considered seg. zero.
	size_t num_segs;	// The number of segments in the cut.
	size_t frames;		// Total number of frames.
	size_t segno;		// The current segment number
				// QN_SIZET_BAD implies before segment 0.
	size_t frameno;		// The current frame number within segment.
    };
    // An enum for remembering which cut was the last we accessed.
    enum CutSelector { CUT1, CUT2 };

    QN_ClassLogger clog;	// Our logging object.
    QN_InLabStream& instr;	// The stream we are cutting.
    
    CutDetails cut1_info;	// The status information for cut 1.
    CutDetails cut2_info;	// The status information for cut 2.
    CutDetails* cut_infop;	// A pointer to the current status structure.
    CutSelector current_cut;	// Which cut was accessed last.

// A virtual function to actually figure the cut mapping
    virtual int segnum_map(CutSelector cut, unsigned int index) {
    // Return the instr segno mapped from a particular cut and index
	int ix = -1;
	if (cut == CUT1) {
	    assert(index < cut1_info.num_segs);
	    ix = cut1_info.first_seg + index;
	} else { /* CUT2 */
	    assert(index < cut2_info.num_segs);
	    ix = cut2_info.first_seg + index;
	}
	return ix;
    }

// Some private functions.
    // Actually do the work of the nextseg() calls.
    virtual QN_SegID nextseg(CutSelector cut);
    // Actually do the work of the rewind() calls.
    virtual int rewind(CutSelector cut);
    // Actually do the work of the get_pos() calls.
    int get_pos(CutSelector cut, size_t* segnop, size_t* framenop);
    // Actually do the work of the set_pos() calls.
    virtual QN_SegID set_pos(CutSelector cut, size_t segno, size_t frameno);
    // Actually do the work of the read_labs() calls.
    size_t read_labs(CutSelector cut, size_t count, QNUInt32* labs);
    // Make a given cut the current one.
    virtual void select_cut(CutSelector new_cut);
};

inline QN_SegID
QN_InLabStream_Cut::nextseg()
{
    return nextseg(CUT1);
}

inline int
QN_InLabStream_Cut::get_pos(size_t* segnop, size_t* framenop)
{
    return get_pos(CUT1, segnop, framenop);
}

inline QN_SegID
QN_InLabStream_Cut::set_pos(size_t segno, size_t frameno)
{
    return set_pos(CUT1, segno, frameno);
}

inline int
QN_InLabStream_Cut::rewind()
{
    return rewind(CUT1);
}

inline size_t
QN_InLabStream_Cut::read_labs(size_t count, QNUInt32* labs)
{
    return read_labs(CUT1, count, labs);
}


////////////////////////////////////////////////////////////////
// The helper class that aids QN_InLabStream_Cut by providing
// the functions for the secund cut.

class QN_InLabStream_Cut2 : public QN_InLabStream
{
public:
    // "a_cutstr" - the already created stream that does that actual
    //                work.
    QN_InLabStream_Cut2(QN_InLabStream_Cut& a_cutstr)
	: cutstr(a_cutstr)
    {   };

    ~QN_InLabStream_Cut2() {};

    size_t num_labs() { return cutstr.num_labs(); };

    size_t num_segs() { return cutstr.num_segs2(); };
    size_t num_frames(size_t segno = QN_ALL)
         { return cutstr.num_frames2(segno); };
    size_t read_labs(size_t cnt, QNUInt32* labs)
        { return cutstr.read_labs(QN_InLabStream_Cut::CUT2, cnt, labs); };
    QN_SegID nextseg() { return cutstr.nextseg(QN_InLabStream_Cut::CUT2); };
    int rewind() { return cutstr.rewind(QN_InLabStream_Cut::CUT2); };
    int get_pos(size_t* segno, size_t* frameno)
        { return cutstr.get_pos(QN_InLabStream_Cut::CUT2, segno, frameno); };
    QN_SegID set_pos(size_t segno, size_t frameno)
        { return cutstr.set_pos(QN_InLabStream_Cut::CUT2, segno, frameno); };

private:
    QN_InLabStream_Cut& cutstr;
};


////////////////////////////////////////////////////////////
// QN_InFtrStream_CutRange
// Convert a stream into one or two arbitrarily-ordered 
// utterance ranges based on definition strings for the "Range.C" 
// class.

class QN_InFtrStream_CutRange : QN_InFtrStream_Cut
{
    friend class QN_InFtrStream_Cut2;
public:
    // "a_debug" is the level of debugging.
    // "a_dbgname" is the instance name used in the debugging output."
    // "a_instr" is the feature stream that we are cutting in two.
    // "a_range1" is the string defining the first range
    // "a_range2" is the string defining the second range (if any)
    QN_InFtrStream_CutRange(int a_debug, const char* a_dbgname,
			    QN_InFtrStream& a_instr,
			    const char *range1 = 0, const char *range2 = 0);
    ~QN_InFtrStream_CutRange();

private:

// private structures
    QN_Range *cut1range;
    QN_Range *cut2range;
    QN_Range::iterator cut1itt;
    QN_Range::iterator cut2itt;
    QN_Range::iterator *iterator;

// A virtual function to actually figure the cut mapping
    virtual int segnum_map(CutSelector cut, unsigned int index) {
    // Return the instr segno mapped from a particular cut and index
	int ix = -1;
	if (cut == CUT1) {
	    assert(index < cut1_info.num_segs);
	    ix = cut1range->index(index);
	} else { /* CUT2 */
	    assert(index < cut2_info.num_segs);
	    ix = cut2range->index(index);
	}
	return ix;
    }

// Some private functions.
    // Actually do the work of the nextseg() calls.
    virtual QN_SegID nextseg(CutSelector cut);
    // Actually do the work of the rewind() calls.
    virtual int rewind(CutSelector cut);
    // Actually do the work of the set_pos() calls.
    virtual QN_SegID set_pos(CutSelector cut, size_t segno, size_t frameno);
    // Make a given cut the current one.
    virtual void select_cut(CutSelector new_cut);
};


class QN_InLabStream_CutRange : QN_InLabStream_Cut
{
    friend class QN_InLabStream_Cut2;
public:
    // "a_debug" is the level of debugging.
    // "a_dbgname" is the instance name used in the debugging output."
    // "a_instr" is the feature stream that we are cutting in two.
    // "a_range1" is the string defining the first range
    // "a_range2" is the string defining the second range (if any)
    QN_InLabStream_CutRange(int a_debug, const char* a_dbgname,
			    QN_InLabStream& a_instr,
			    const char *range1 = 0, const char *range2 = 0);
    ~QN_InLabStream_CutRange();

private:

// private structures
    QN_Range *cut1range;
    QN_Range *cut2range;
    QN_Range::iterator cut1itt;
    QN_Range::iterator cut2itt;
    QN_Range::iterator *iterator;

// A virtual function to actually figure the cut mapping
    virtual int segnum_map(CutSelector cut, unsigned int index) {
    // Return the instr segno mapped from a particular cut and index
	int ix = -1;
	if (cut == CUT1) {
	    assert(index < cut1_info.num_segs);
	    ix = cut1range->index(index);
	} else { /* CUT2 */
	    assert(index < cut2_info.num_segs);
	    ix = cut2range->index(index);
	}
	return ix;
    }

// Some private functions.
    // Actually do the work of the nextseg() calls.
    virtual QN_SegID nextseg(CutSelector cut);
    // Actually do the work of the rewind() calls.
    virtual int rewind(CutSelector cut);
    // Actually do the work of the set_pos() calls.
    virtual QN_SegID set_pos(CutSelector cut, size_t segno, size_t frameno);
    // Make a given cut the current one.
    virtual void select_cut(CutSelector new_cut);
};

#endif
