#if defined(FTRWIN)
const char* QN_ftrwindows_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_windows.cc,v 1.20 2006/02/22 23:10:26 davidj Exp $";
#else
const char* QN_labwindows_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_windows.cc,v 1.20 2006/02/22 23:10:26 davidj Exp $";
#endif

// This file contains "windows" for the QuickNet
// stream objects.  Windows take an input stream, and combine data
// from multiple adjacent frames to form one new frame.

#include <QN_config.h>
#include "QN_windows.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"


// This source file is use to build two sets of classes - one that works
// on QN_InFtrStreams and one that works on QN_InLabStreams.  A preprocessor
// symol is used to decide which is built, which in turn controls several
// macros that are used as types, function names, class names or debugging
// output.

// Note that in the associated header file, the label and feature classes are
// declared separately.  This both makes it simpler for users of the class and
// acts as a check that everything is defined correctly.

#if defined(FTRWIN)

#define VTYPE float
#define READ_VTYPE read_ftrs
#define NUM_VTYPE num_ftrs
#define QN_INSTREAM QN_InFtrStream
#define QN_INSTREAM_SEQWINDOW QN_InFtrStream_SeqWindow
#define QN_INSTREAM_RANDWINDOW QN_InFtrStream_RandWindow
#define QN_INSTREAM_SEQWINDOW_CLASSNAME "QN_InFtrStream_SeqWindow"
#define QN_INSTREAM_RANDWINDOW_CLASSNAME "QN_InFtrStream_RandWindow"
#define COPY_V_V(len, from, to) qn_copy_vf_vf((len), (from), (to))

#elif defined (LABWIN)

#define VTYPE QNUInt32
#define READ_VTYPE read_labs
#define NUM_VTYPE num_labs
#define QN_INSTREAM QN_InLabStream
#define QN_INSTREAM_SEQWINDOW QN_InLabStream_SeqWindow
#define QN_INSTREAM_RANDWINDOW QN_InLabStream_RandWindow
#define QN_INSTREAM_SEQWINDOW_CLASSNAME "QN_InLabStream_SeqWindow"
#define QN_INSTREAM_RANDWINDOW_CLASSNAME "QN_InLabStream_RandWindow"
#define COPY_V_V(len, from, to) qn_copy_vi32_vi32((len), (QNInt32*) (from), \
					          (QNInt32*) (to))

#else

#error "Must define LABWIN or FTRWIN"

#endif

QN_INSTREAM_SEQWINDOW::QN_INSTREAM_SEQWINDOW(int a_debug,
						   const char* a_dbgname,
						   QN_INSTREAM& a_str,
						   size_t a_win_len,
						   size_t a_top_margin,
						   size_t a_bot_margin,
						   size_t a_bunch_frames)
    : log(a_debug, QN_INSTREAM_SEQWINDOW_CLASSNAME, a_dbgname),
      str(a_str),
      win_len(a_win_len),
      top_margin(a_top_margin),
      bot_margin(a_bot_margin),
      in_width(str.NUM_VTYPE()),
      out_width(win_len * in_width),
      bunch_frames(a_bunch_frames==QN_SIZET_BAD
		 ? top_margin + win_len + bot_margin
		 : a_bunch_frames),
      max_buf_lines(bunch_frames + win_len + bot_margin-1),
      buf(new VTYPE[max_buf_lines*in_width]),
      first_line_ptr(&buf[top_margin*in_width]),
      segno(-1)
{
    if (bunch_frames<(top_margin+bot_margin+win_len))
	log.error("Insufficient bunch length for size of window.");
    log.log(QN_LOG_PER_RUN, "Windowing, window=%lu*%lu values, "
	    "top margin=%lu frames, bottom margin=%lu frames, "
	    "buffer size=%lu frames.", (unsigned long) in_width,
	    (unsigned long) win_len, (unsigned long) top_margin,
	    (unsigned long) bot_margin, (unsigned long) max_buf_lines);
}

QN_INSTREAM_SEQWINDOW::~QN_INSTREAM_SEQWINDOW()
{
    delete [] buf;
}

size_t
QN_INSTREAM_SEQWINDOW::NUM_VTYPE()
{
    return out_width;
}


// Move on to next segment.

QN_SegID
QN_INSTREAM_SEQWINDOW::nextseg()
{
    QN_SegID segid;		// Returned segment ID.

    segid = str.nextseg();
    if (segid!=QN_SEGID_BAD)
    {
	// Try and read in a buffer full of frames.
	buf_lines = str.READ_VTYPE(bunch_frames, buf);
	cur_line = top_margin;
	cur_line_ptr = first_line_ptr;
	segno++;
    }
    log.log(QN_LOG_PER_SENT, "Moved on to segment %li.", segno);
    return segid;
}

// For unstrided read, use strided read.
size_t
QN_INSTREAM_SEQWINDOW::READ_VTYPE(size_t a_count, VTYPE* a_vals)
{
    return READ_VTYPE(a_count, a_vals, QN_ALL);
}

size_t
QN_INSTREAM_SEQWINDOW::READ_VTYPE(size_t a_count, VTYPE* a_vals, size_t stride)
{
    if (segno==-1)
	log.error("Trying to read before start of first sentence.");

    size_t frame;		// Current frame being transferred.
    const size_t l_in_width = in_width; // Local copy of class variable.
    const size_t l_out_width = out_width; // Local copy of variable.
    size_t real_stride;		// Stride we use for output.
    VTYPE* vals = a_vals;	// Where to put windowed data.

    if (stride==QN_ALL)
	real_stride = l_out_width;
    else
	real_stride = stride;
    for (frame=0; frame<a_count; frame++)
    {
	// If at end of buffer, read in more lines.
	if (cur_line + win_len + bot_margin > buf_lines)
	{
	    // Move the remaining lines to the beginning of the buffer.
	    size_t line;	// Iterator over lines being moved.
	    size_t old_lines = win_len + bot_margin - 1; // Lines remaining.
	    VTYPE *from = cur_line_ptr;	// Where we get the line from.
	    VTYPE *to = &buf[0]; // Where we put the line.
	    for (line=0; line<old_lines; line++)
	    {
		COPY_V_V(l_in_width, from, to);
		from += l_in_width;
		to += l_in_width;
	    }
	    cur_line = 0;
	    cur_line_ptr = &buf[0];

	    // Try and fill up the rest of the buffer.
	    size_t count = str.READ_VTYPE(bunch_frames,
					  &buf[old_lines*l_in_width]);
	    buf_lines = old_lines + count;
	    // If nothing to read in, terminate read.
	    if (count==0)
		break;
					 
	}
	if (vals!=NULL)
	{
	    COPY_V_V(l_out_width, cur_line_ptr, vals);
	    vals += real_stride;
	}
	cur_line += 1;
	cur_line_ptr += l_in_width;
    }
    log.log(QN_LOG_PER_BUNCH, "Read %lu windows full.", (unsigned long) frame);
    return frame;
}

// Rewind works.

int
QN_INSTREAM_SEQWINDOW::rewind()
{
    int ec;

    ec = str.rewind();
    if (ec==QN_OK)
	segno = -1;
    return ec;
}

// Some streams functions that do not work for SeqWindows.

size_t
QN_INSTREAM_SEQWINDOW::num_segs()
{
    return QN_SIZET_BAD;
}

size_t
QN_INSTREAM_SEQWINDOW::num_frames(size_t)
{
    return QN_SIZET_BAD;
}

int
QN_INSTREAM_SEQWINDOW::get_pos(size_t*, size_t*)
{
    return QN_BAD;
}

QN_SegID
QN_INSTREAM_SEQWINDOW::set_pos(size_t, size_t)
{
    return QN_SEGID_BAD;
}

////////////////////////////////////////////////////////////////


QN_INSTREAM_RANDWINDOW::QN_INSTREAM_RANDWINDOW(int a_debug,
					       const char* a_dbgname,
					       QN_INSTREAM& a_str,
					       size_t a_win_len,
					       size_t a_top_margin,
					       size_t a_bot_margin,
					       size_t a_buf_frames,
					       QNUInt32 a_seed,
					       Order a_order)
    : clog(a_debug, QN_INSTREAM_RANDWINDOW_CLASSNAME, a_dbgname),
      str(a_str),
      win_len(a_win_len),
      top_margin(a_top_margin),
      bot_margin(a_bot_margin),
      order(a_order),
      seed(a_seed),
      in_width(str.NUM_VTYPE()),
      out_width(win_len * in_width),
      buf_frames(a_buf_frames),
      seqgen(NULL)
{
    size_t in_n_segs;		// No. of segments in input stream.

    // Make sure we can find out segment info about input stream.
    in_n_segs = str.num_segs();
    if (in_n_segs==QN_SIZET_BAD)
    {
	clog.error("Stream having a random cursor applied must be random "
		   "access.");
    }

// Use the length of all our input segments to work out how many
// output segments (e.g. batches of sentences) we will have.

    size_t frames_this_seg = 0;	// Frames we have seen so far in the 
				// current output segment.
    size_t in_seg;		// The number of the current input segment.
    size_t out_seg = 0;		// The number of the current output segment.
    size_t total_out_frames = 0; // The number of frames in the output.
    // The number of frames that are "unusable" in an input segment.
    const size_t lost_frames = (top_margin + bot_margin + win_len - 1);
    for (in_seg = 0; in_seg<in_n_segs; in_seg++)
    {
	size_t in_frames;	// Frames in the current input segment.

	in_frames = str.num_frames(in_seg);
	assert(in_frames!=QN_SIZET_BAD);
	if (in_frames<=lost_frames)
	{
	    clog.warning("Segment %lu has only %lu frames, which is too small"
			 " - no windows will be produced from this segment.",
			 in_seg, in_frames);
	}
	if (in_frames>buf_frames) // Warn for sentences that are too big.
	  {
	    // used to be an error, but too problematic for v.large BN segs
	    clog.warning("Segment %lu contains %lu frames, which is greater "
			 "than the %lu frames in the randomization buffer - "
			 "tail portion will be discarded.",
			 in_seg, in_frames, buf_frames);
	    // pretend it isn't quite so long
	    in_frames = buf_frames;
	  }
	if ((frames_this_seg+in_frames) > buf_frames)
	  {
	    // This input segment fills up the buffer and pushes us on to
	    // a new output segment.
	    out_seg++;
	    frames_this_seg = in_frames;
	  }
	else
	  {
	    // Still on same output segment.
	    frames_this_seg += in_frames;
	  }
	if (in_frames > lost_frames) 
	  total_out_frames += in_frames - lost_frames;
    }
    // Handle last segment.
    if (frames_this_seg!=0)
	out_seg++;
    out_n_segs = out_seg;	// Remember the number of output segments.
    out_n_frames = total_out_frames; // Remember the number of output frames.

// Allocate and initalize an index for output->input segment mapping.
    out_seg_index = new size_t[out_n_segs+1];
    frames_this_seg = 0;
    out_seg = 0;
    out_seg_index[0] = 0;
    size_t min_frames = buf_frames; // The minimum number of frames in a seg.
    size_t max_frames = 0;	// The maximum number of frames in a segment.
    for (in_seg = 0; in_seg<in_n_segs; in_seg++)
    {
	size_t in_frames;	// Frames in the current input segment.

	in_frames = str.num_frames(in_seg);

	// Include the 'too short' utterances in this calculation, 
	// since they will be read in, even if they're not used
	if (in_frames>buf_frames) {
	  in_frames = buf_frames;
	}
	if ((frames_this_seg+in_frames) > buf_frames)
	  {
	    // This input segment fills up the buffer and pushes us on to
	    // a new output segment.
	    if (frames_this_seg > max_frames)
	      max_frames = frames_this_seg;
	    if (frames_this_seg < min_frames)
	      min_frames = frames_this_seg;
	    out_seg++;
	    out_seg_index[out_seg]  = in_seg; 
	    frames_this_seg = in_frames;
	  }
	else
	  {
	    // Still on same output segment.
	    frames_this_seg += in_frames;
	  }
    }

    if (frames_this_seg > max_frames)
	max_frames = frames_this_seg;
    if (frames_this_seg < min_frames)
	min_frames = frames_this_seg;
    out_seg++;
    out_seg_index[out_seg]  = in_seg; 

    // Check that the buffer size is not gonna break 32 bit values.
    double memneeded = ((double) buf_frames) * ((double) in_width)
	* ((double) sizeof(VTYPE));
    double memsize = pow(2.0, ((double) sizeof(size_t)) * 8.0);
    if (memneeded > (memsize*0.5))
    {
	clog.error("Space needed for random window frame cache is "
		   "bigger than available memory.");
    }
    //  Allocate the buffer used to store the actual input data.
    buf = new VTYPE[buf_frames * in_width];
    // The buffer used to point to the start of all in-buffer presentations -
    // note that this is an over-allocation - there will almost certainly
    // be less than buf_frames usable frames.
    buf_frame_index = new VTYPE*[buf_frames];

    // Set up the starting position.
    epoch = 0;
    out_segno = QN_SIZET_BAD;
    out_frameno = QN_SIZET_BAD;
    clog.log(QN_LOG_PER_RUN,
	     "Created a random window stream with %lu output segments of "
	     "between %lu and %lu frames (%lu max).",
	     (unsigned long) out_n_segs,
	     (unsigned long) min_frames, (unsigned long) max_frames,
	     a_buf_frames);
}

QN_INSTREAM_RANDWINDOW::~QN_INSTREAM_RANDWINDOW()
{
    delete[] buf;
    delete[] buf_frame_index;
    delete[] out_seg_index;
    if (seqgen!=NULL)
	delete seqgen;
}

int
QN_INSTREAM_RANDWINDOW::rewind()
{
    out_segno = QN_SIZET_BAD;
    out_frameno = QN_SIZET_BAD;
    epoch++;
    clog.log(QN_LOG_PER_EPOCH, "Rewound to start of input stream for "
	     " epoch %lu.", (unsigned long) epoch);
    return QN_OK;
}

size_t
QN_INSTREAM_RANDWINDOW::NUM_VTYPE()
{
    return out_width;
}

size_t
QN_INSTREAM_RANDWINDOW::num_segs()
{
    return out_n_segs;
}

size_t
QN_INSTREAM_RANDWINDOW::num_frames(size_t segno)
{
    size_t ret;			// Returned value.

    if (segno==QN_SIZET_BAD)
    {
	ret = out_n_frames;
	clog.log(QN_LOG_PER_EPOCH, "%lu output frames (presentations) in "
		 "one epoch.", (unsigned long) ret);
    }
    else
    {
	if (segno>=out_n_segs)
	{
	    clog.error("Tried to find frames in non-existent segment no. %lu.",
		       (unsigned long) segno);
	    ret = 0;
	}
	else
	{
	    size_t in_seg;
	    size_t total_frames = 0; // The number of frames.
	    const size_t lost_frames = (top_margin + bot_margin + win_len - 1);
	    for (in_seg = out_seg_index[segno];
		 in_seg<out_seg_index[segno+1];
		 in_seg++)
	    {
		size_t frames;	// Frames in this segment.
		
		frames = str.num_frames(in_seg);
		assert(frames!=QN_SIZET_BAD);
		if (frames > lost_frames) 
		  total_frames += frames - lost_frames;
	    }
	    ret = total_frames;
	    clog.log(QN_LOG_PER_EPOCH, "%lu output frames (presentations) in "
		     "segment %lu.", (unsigned long) ret,
		     (unsigned long) segno);
	}
    }
    return ret;
}

QN_SegID
QN_INSTREAM_RANDWINDOW::nextseg()
{
    QN_SegID segid;		// Segment ID returned.
    
    if (out_segno==out_n_segs-1) // At end of stream.
	segid = QN_SEGID_BAD;
    else			// Not at end of stream.
    {
	if (out_segno==QN_SIZET_BAD) // At start of stream.
	    out_segno = 0;
	else
	    out_segno++;
	QN_SegID in_segid;	// Segment ID from input stream.
	// Move to start of segment in input.
	in_segid = str.set_pos(out_seg_index[out_segno], 0);
	assert(in_segid!=QN_SEGID_BAD);

	// Read in all the frames in all the input segments that correspond
	// with the current output segment.
	size_t in_segno;	// Current input segment number.
	size_t frames_read = 0;	// Number of frames read.
	VTYPE* buf_ptr = buf;	// Where to put data in buffer.
	VTYPE** frame_index_ptr = buf_frame_index; // Next entry in index.
	// The number of frames we do not use as start of presentations.
	const size_t lost_frames = (top_margin + bot_margin + win_len - 1);
	// The number of values at the start of segment we do not use.
	const size_t top_skip = top_margin * in_width;
	// The number of values at the bottom of segment that we skip.
	const size_t bot_skip = (bot_margin + win_len - 1) * in_width;
	// The number of output frames in this segment.
	size_t total_usable_frames = 0;
	for (in_segno = out_seg_index[out_segno];
	     in_segno < out_seg_index[out_segno+1];
	     in_segno++)
	{
	    size_t count;	// Frames read this read.
	    size_t usable_frames_this_seg; // Number of frames of output that
				// will be available from this read.

	    // Read all frames in input segment.
	    count = str.READ_VTYPE(buf_frames, buf_ptr);
	    assert(count + frames_read <= buf_frames);

	    str.nextseg();	// On to next input segment.
	    frames_read += count;

	    // Work out which frames will be at the start of presentations.
	    if (count > lost_frames) {
	      usable_frames_this_seg = count - lost_frames;
	      total_usable_frames += usable_frames_this_seg;
	      size_t frame;	// Frame enumerator.
	      buf_ptr += top_skip;
	      for (frame = 0; frame<usable_frames_this_seg; frame++)
		{
		  *frame_index_ptr = buf_ptr;
		  buf_ptr += in_width;
		  frame_index_ptr++;
		}
	      buf_ptr += bot_skip;
	    } else {
	      // This segment was completely useless, but leave it there.
	      buf_ptr += count * in_width;
	    }
	}
	usable_frames = total_usable_frames; // Remember how many
					     // presentations we can make.
	out_frameno = 0;
	if (seqgen!=NULL)
	    delete seqgen;
	switch(order)
	{
	case RANDOM_NO_REPLACE:
	    seqgen =
		new QN_SeqGen_RandomNoReplace(usable_frames,
					      out_segno+(12345*epoch)+seed);
	    break;
	case SEQUENTIAL:
	    seqgen = new QN_SeqGen_Sequential(usable_frames);
	    break;
	default:
	    assert(0);
	}
	clog.log(QN_LOG_PER_SENT, "Read input segments %lu to %lu containing "
		 "%lu usable frames (%lu total) for output as segment %lu.",
		 out_seg_index[out_segno], out_seg_index[out_segno+1]-1,
		 total_usable_frames, frames_read, out_segno);
	segid = QN_SEGID_UNKNOWN; // This really does not have a seg id.
    }
    return segid;
}

size_t
QN_INSTREAM_RANDWINDOW::READ_VTYPE(size_t a_count, VTYPE* a_vals)
{
    return READ_VTYPE(a_count, a_vals, QN_ALL);
}

size_t
QN_INSTREAM_RANDWINDOW::READ_VTYPE(size_t a_count, VTYPE* a_vals,
				   size_t stride)
{
    size_t count = 0;		// Count of frames we have output.
    VTYPE* vals = a_vals;	// Where we put the result.
    size_t real_stride;		// Actual stride we use.

    if (stride==QN_ALL)
	real_stride = out_width;
    else
	real_stride = stride;
    if (out_segno==QN_SIZET_BAD)
    {
	clog.error("Trying to read before first segment.");
    }
    assert(out_segno!=QN_SIZET_BAD);
    while(out_frameno<usable_frames && count<a_count)
    {
	size_t frame;		// The number of the frame we will return
				// in the buf_frame_index.
	frame = (size_t) seqgen->next();
	if (vals!=NULL)
	{
	    COPY_V_V(out_width, buf_frame_index[frame], vals);
	    vals += real_stride;
	}
	out_frameno++;
	count++;
    }
    clog.log(QN_LOG_PER_BUNCH, "Returned %lu frames (presentations) "
	     "out of requested %lu.",
	     (unsigned long) count, (unsigned long) a_count);
    return count;
}

// get_pos() not implemented for random window stream.
int
QN_INSTREAM_RANDWINDOW::get_pos(size_t*, size_t*)
{
    return QN_BAD;
}

// set_pos() not implemented for random window stream.
QN_SegID
QN_INSTREAM_RANDWINDOW::set_pos(size_t, size_t)
{
    return QN_SEGID_BAD;
}

