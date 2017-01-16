#if defined(FTRCUT)
const char* QN_ftrcut_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_cut.cc,v 1.7 2004/04/08 02:57:42 davidj Exp $";
#elif defined(LABCUT)
const char* QN_labcut_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_cut.cc,v 1.7 2004/04/08 02:57:42 davidj Exp $";
#endif

// Some classes for cutting one stream into two maybe-overlapping copies.

#include <QN_config.h>
#include <assert.h>
#include "QN_split.h"
#include "QN_cut.h"


// This source file is use to build two sets of classes - one that works
// on QN_InFtrStreams and one that works on QN_InLabStreams.  A preprocessor
// symol is used to decide which is built, which in turn controls several
// macros that are used as types, function names, class names or debugging
// output.

// Note that in the associated header file, the label and feature classes are
// declared separately.  This both makes it simpler for users of the class and
// acts as a check that everything is defined correctly.

#if defined(FTRCUT)

#define VTYPE float
#define READ_VTYPE read_ftrs
#define NUM_VTYPE num_ftrs
#define QN_INSTREAM QN_InFtrStream
#define QN_INSTREAM_CUT QN_InFtrStream_Cut
#define QN_INSTREAM_CUTRANGE QN_InFtrStream_CutRange
#define CLASSNAME "QN_InFtrStream_Cut"

#elif defined(LABCUT)

#define VTYPE QNUInt32
#define READ_VTYPE read_labs
#define NUM_VTYPE num_labs
#define QN_INSTREAM QN_InLabStream
#define QN_INSTREAM_CUT QN_InLabStream_Cut
#define QN_INSTREAM_CUTRANGE QN_InLabStream_CutRange
#define CLASSNAME "QN_InLabStream_Cut"

#else

#error "Must define LABCUT or FTRCUT"

#endif



////////////////////////////////////////////////////////////////

QN_INSTREAM_CUT::
QN_INSTREAM_CUT(int a_debug, const char* a_dbgname,
		   QN_INSTREAM& a_instr,
		   size_t a_first_seg1, size_t a_num_segs1, 
		   size_t a_first_seg2, size_t a_num_segs2)
  : clog(a_debug, CLASSNAME, a_dbgname),
    instr(a_instr)
{
    QN_SegID segid;
    int ec;

    // Check the stream is seekable.
    segid = instr.set_pos(0,0);
    ec = instr.rewind();
    if ((ec!=QN_OK) || (segid==QN_SEGID_BAD))
	clog.error("Cannot cut a non-indexed stream.");

    // Check that the input stream is big enough.
    const size_t instr_num_segs = instr.num_segs();
    assert (instr_num_segs!=QN_SIZET_BAD);
    assert(a_num_segs1!=0 && a_num_segs2!=0);
    if (a_num_segs1==QN_ALL)
	a_num_segs1 = instr_num_segs - a_first_seg1;
    if (a_num_segs2==QN_ALL)
	a_num_segs2 = instr_num_segs - a_first_seg2;
    if (a_first_seg1+a_num_segs1>instr_num_segs)
	clog.error("Cut 1 is bigger than input stream");
    if (a_first_seg2+a_num_segs2>instr_num_segs)
	clog.error("Cut 2 is bigger than input stream");

    // Initialize the individual cuts status.
    size_t segno;
    cut1_info.first_seg = a_first_seg1;
    cut1_info.num_segs = a_num_segs1;
    // Before start of first sentence - segno==QN_SIZET_BAD.
    cut1_info.segno = QN_SIZET_BAD;
    cut1_info.frameno = QN_SIZET_BAD;
    cut1_info.frames = 0;
    for (segno = a_first_seg1; segno<(a_first_seg1+a_num_segs1); segno++)
	cut1_info.frames += instr.num_frames(segno);
    cut2_info.first_seg = a_first_seg2;
    cut2_info.num_segs = a_num_segs2;
    // Before start of first sentence - segno==QN_SIZET_BAD.
    cut2_info.segno = QN_SIZET_BAD;
    cut2_info.frameno = QN_SIZET_BAD;
    cut2_info.frames = 0;
    for (segno = a_first_seg2; segno<(a_first_seg2+a_num_segs2); segno++)
	cut2_info.frames += instr.num_frames(segno);

    // Assume cut1 was accessed last.
    current_cut = CUT1;
    cut_infop = &cut1_info;
    clog.log(QN_LOG_PER_EPOCH, "Created two streams, "
	    "first stream segs %lu to %lu with %lu frames, "
	    "second stream segs %lu to %lu with %lu frames.",
	    (unsigned long) a_first_seg1,
	    (unsigned long) (a_first_seg1+a_num_segs1-1),
	    (unsigned long) cut1_info.frames,
	    (unsigned long) a_first_seg2,
	    (unsigned long) (a_first_seg2+a_num_segs2-1),
	    (unsigned long) cut2_info.frames);
}
						 
QN_INSTREAM_CUT::
~QN_INSTREAM_CUT()
{
}

size_t
QN_INSTREAM_CUT::NUM_VTYPE()
{
    return instr.NUM_VTYPE();
}

size_t
QN_INSTREAM_CUT::num_segs()
{
    return cut1_info.num_segs;
}

size_t
QN_INSTREAM_CUT::num_segs2()
{
    return cut2_info.num_segs;
}

size_t
QN_INSTREAM_CUT::num_frames(size_t segno)
{
    size_t ret;			// The return value.
    
    if(segno==QN_SIZET_BAD)
    {
	ret = cut1_info.frames;
	clog.log(QN_LOG_PER_EPOCH,
		 "%lu frames in cut 1.", (unsigned long) ret);
    }
    else
    {
	assert(segno<cut1_info.num_segs);
	// ret = instr.num_frames(segno + cut1_info.first_seg);
	ret = instr.num_frames(segnum_map(CUT1, segno));
	clog.log(QN_LOG_PER_SENT,
		 "%lu frame in cut 1, segment %lu.",
		 (unsigned long) ret, (unsigned long) segno);
    }
    return ret;
}

size_t
QN_INSTREAM_CUT::num_frames2(size_t segno)
{
    size_t ret;			// The return value.
     
    if(segno==QN_SIZET_BAD)
    {
	ret = cut2_info.frames;
	clog.log(QN_LOG_PER_EPOCH,
		 "%lu frames in cut 2.", (unsigned long) ret);
    }
    else
    {
	assert(segno<cut2_info.num_segs);
	//ret = instr.num_frames(segno + cut2_info.first_seg);
	ret = instr.num_frames(segnum_map(CUT2, segno));
	clog.log(QN_LOG_PER_SENT,
		 "%lu frame in cut 2, segment %lu.",
		 (unsigned long) ret, (unsigned long) segno);
    }
    return ret;
}

void
QN_INSTREAM_CUT::select_cut(CutSelector new_cut)
{
    assert(new_cut==CUT1 || new_cut==CUT2);
    if (current_cut!=new_cut)
    {
	if (new_cut==CUT1)
	    cut_infop = &cut1_info;
	else
	    cut_infop = &cut2_info;
	current_cut = new_cut;
	// Here we position the input stream to reflect the new active cut.
	size_t& segno = cut_infop->segno;
	size_t& frameno = cut_infop->frameno;
	if (segno==QN_SIZET_BAD)
	{
	    // Currently positioned before first sentence.
	    assert(frameno==QN_SIZET_BAD);
	    instr.rewind();
	}
	else
	{
	    // size_t& first_seg = cut_infop->first_seg;
	    // instr.set_pos(segno+first_seg, frameno);
	    instr.set_pos(segnum_map(new_cut, segno), frameno);
	}
    }
}

// Here we do the actual work for the get_pos() functions.

int
QN_INSTREAM_CUT::get_pos(CutSelector cut,
			    size_t* segnop, size_t* framenop)
{
    CutDetails* infop;

    assert(cut==CUT1 || cut==CUT2);
    // Do not need to actually seek to the current position for the cut
    // required.
    if (cut==CUT1)
	infop = &cut1_info;
    else
	infop = &cut2_info;
    *segnop = infop->segno;
    *framenop = infop->frameno;
    return QN_OK;
}

// Here we do the actual work for the set_pos() functions.

QN_SegID
QN_INSTREAM_CUT::set_pos(CutSelector cut,
			 size_t a_segno, size_t a_frameno)
{
    QN_SegID segid;		// The returned segment ID.

    // Make the required cut the current one.
    select_cut(cut);

    // Set up some aliases to variables in the current cut.
    //    size_t& first_seg = cut_infop->first_seg;
    size_t& num_segs = cut_infop->num_segs;
    size_t& segno = cut_infop->segno;
    size_t& frameno = cut_infop->frameno;
    if (a_segno>num_segs)
    {
	clog.error("Seek past end of last segment on cut %i, "
		   "requested segment=%lu, "
		   "number of segments=%lu.", (cut==CUT1) ? 1 : 2,
		   (unsigned long) a_segno,
		   (unsigned long) num_segs);
	segid = QN_SEGID_BAD;
    }
    else
    {
	segno = a_segno;
	frameno = a_frameno;
	// segid = instr.set_pos(segno+first_seg, frameno);
	segid = instr.set_pos(segnum_map(cut, segno), frameno);
	clog.log(QN_LOG_PER_SUBEPOCH, "Seeked to segno=%lu frameno=%lu in "
		 "cut %i.", (unsigned long) segno, (unsigned long) frameno, 
		 (cut==CUT1) ? 1 : 2);
    }

    return segid;
}


// Here we do the actual work for the nextseg() functions.
QN_SegID
QN_INSTREAM_CUT::nextseg(CutSelector cut)
{
    QN_SegID segid;		// The returned segment ID.

    // Make the required cut the current one.
    select_cut(cut);

    // Set up some aliases to variables in the current cut.
    size_t& first_seg = cut_infop->first_seg;
    size_t& num_segs = cut_infop->num_segs;
    size_t& segno = cut_infop->segno;
    size_t& frameno = cut_infop->frameno;

    if (segno==(num_segs-1))
	// At end of stream.
	segid = QN_SEGID_BAD;
    else if (segno==QN_SIZET_BAD)
    {
	// Before start of first sentence.
	segid = instr.set_pos(first_seg, 0);
	assert(segid!=QN_SEGID_BAD); // End of stream checked above, so
				// this should never happen.
	segno = 0;
	frameno = 0;
    }
    else
    {
	// The normal case.
	segid = instr.nextseg();
	segno++;
	frameno = 0;
    }
    return segid;
}

// Here we do the actual work for the rewind() functions.
int
QN_INSTREAM_CUT::rewind(CutSelector cut)
{
    int ec;			// The returned error code.

    // Make the required cut the current one.
    select_cut(cut);
    ec = instr.rewind();
    if(ec==QN_OK)
    {
	cut_infop->segno = QN_SIZET_BAD;
	cut_infop->frameno = QN_SIZET_BAD;
	clog.log(QN_LOG_PER_EPOCH, "Cut %i rewound.",
		 (cut==CUT1) ? 1: 2);
    }
    return ec;
}

// Here we do the actual work for the READ_VTYPE() functions.
size_t
QN_INSTREAM_CUT::READ_VTYPE(CutSelector cut, size_t count, VTYPE* ftrs)
{
    size_t frames;		// The number of frames returned.

    select_cut(cut);
    frames = instr.READ_VTYPE(count, ftrs);
    clog.log(QN_LOG_PER_BUNCH, "Read %lu frames of %lu requested, "
	     "starting at segno=%lu frameno=%lu cut=%i.",
	     (unsigned long) frames, (unsigned long) count,
	     (unsigned long) cut_infop->segno,
	     (unsigned long) cut_infop->frameno,
	     (cut==CUT1) ? 1 : 2);
    cut_infop->frameno += frames;
    return(frames);
}


/////////////////////////////////////////////////
// CutRange fns
/////////////////////////////////////////////////

QN_INSTREAM_CUTRANGE::QN_INSTREAM_CUTRANGE(int a_debug, 
					   const char* a_dbgname,
					   QN_INSTREAM& a_instr,
					   const char *range1 /*=0*/,
					   const char *range2 /*=0*/) 
    : QN_INSTREAM_CUT(a_debug, a_dbgname, a_instr)
{
    int maxsegs = instr.num_segs();
    cut1range = new QN_Range(range1, 0, maxsegs);
    cut2range = new QN_Range(range2, 0, maxsegs);
    cut1itt = cut1range->begin();
    cut2itt = cut2range->begin();
    cut1_info.first_seg = cut1range->first();
    cut2_info.first_seg = cut2range->first();
    cut1_info.num_segs = cut1range->length();
    cut2_info.num_segs = cut2range->length();

    // initialize point to cut1
    iterator = &cut1itt;

    // Figure frames in each cut
    cut1_info.frames = 0;
    while (!cut1itt.at_end()) {
	cut1_info.frames += instr.num_frames(*cut1itt);
	cut1itt++;
    }
    cut1itt.reset();

    cut2_info.frames = 0;
    while (!cut2itt.at_end()) {
	cut2_info.frames += instr.num_frames(*cut2itt);
	cut2itt++;
    }
    cut2itt.reset();

    clog.log(QN_LOG_PER_EPOCH, "Created two ranged cuts, "
	    "first stream from seg %lu with %lu segs and %lu frames, "
	    "second stream from seg %lu with %lu segs and %lu frames.", 
	    (unsigned long) cut1_info.first_seg, 
	    (unsigned long) cut1_info.num_segs, 
	    (unsigned long) cut1_info.frames,
	    (unsigned long) cut2_info.first_seg, 
	    (unsigned long) cut2_info.num_segs, 
	    (unsigned long) cut2_info.frames);
}

QN_INSTREAM_CUTRANGE::~QN_INSTREAM_CUTRANGE(void) {
    delete cut1range;
    delete cut2range;
}

QN_SegID QN_INSTREAM_CUTRANGE::nextseg(CutSelector cut) {
    size_t lastseg, nextseg;
    QN_SegID segid;

    // Make the required cut the current one.
    select_cut(cut);

    if (cut_infop->segno==(cut_infop->num_segs-1)) {
	// At end of stream.
	segid = QN_SEGID_BAD;
    } else {
	if (cut_infop->segno == QN_SIZET_BAD) {
	    lastseg = QN_SIZET_BAD;
	} else {
	    lastseg = *iterator;
	    ++(*iterator);
	}
	nextseg = *iterator;
	if (nextseg == lastseg+1 \
	    || (nextseg == 0 && lastseg == QN_SIZET_BAD)) {
	    // A regular one-step forward
	    segid = instr.nextseg();
	} else {
	    // We have to seek
	    segid = instr.set_pos(nextseg, 0);
	}
	if (segid != QN_SEGID_BAD) {
	    if (cut_infop->segno == QN_SIZET_BAD) {
		cut_infop->segno = 0;
	    } else {
		++cut_infop->segno;
	    }
	    cut_infop->frameno = 0;
	} else {
	    assert(segid!=QN_SEGID_BAD); // End of stream checked above, so
				         // this should never happen.
	}
    }
    return segid;
}

int QN_INSTREAM_CUTRANGE::rewind(CutSelector cut) {
    // Actually do the work of the rewind() calls.
    if (cut == CUT1) {
	cut1itt.reset();
    } else {
	cut2itt.reset();
    }
    return QN_INSTREAM_CUT::rewind(cut);
}

QN_SegID QN_INSTREAM_CUTRANGE::set_pos(CutSelector cut, size_t segno, size_t frameno) {
    // Actually do the work of the set_pos() calls.
    // Which segment does this correspond to?
    QN_SegID segid;

    // Make the required cut the current one.
    select_cut(cut);

    if (segno < cut_infop->segno) {
	iterator->reset();
	cut_infop->segno = 0;
    }

    iterator->step_by(segno - cut_infop->segno);

    if (iterator->at_end()) {
	clog.error("Seek past end of last segment on cut %i, "
		   "requested segment=%lu, "
		   "number of segments=%lu.", (cut==CUT1) ? 1 : 2,
		   (unsigned long) segno,
		   (unsigned long) cut_infop->num_segs);
	segid = QN_SEGID_BAD;
    } else {
	cut_infop->segno = segno;
	cut_infop->frameno = frameno;
	segid = instr.set_pos(*iterator, frameno);
	clog.log(QN_LOG_PER_SUBEPOCH, "Seeked to segno=%lu frameno=%lu in "
		 "cut %i.", (unsigned long) segno, (unsigned long) frameno, 
		 (cut==CUT1) ? 1 : 2);
    }
    return segid;
}


// Make a given cut the current one.
void
QN_INSTREAM_CUTRANGE::select_cut(CutSelector new_cut)
{
    QN_INSTREAM_CUT::select_cut(new_cut);
    if (new_cut==CUT1)
	iterator = &cut1itt;
    else
	iterator = &cut2itt;
}
