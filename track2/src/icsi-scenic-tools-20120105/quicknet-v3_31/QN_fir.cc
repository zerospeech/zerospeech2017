//
// QN_fir.cc
// 
// Quicknet routines for the FIR-filtering stream filter
// i.e. for adding deltas to a stream on the fly.
//
// 1998aug10 dpwe@icsi.berkeley.edu
//
const char* QN_fir_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_fir.cc,v 1.14 2010/10/29 02:20:38 davidj Exp $";

// This file contains various input stream filters.

#include <QN_config.h>
#include "QN_fir.h"
#include "QN_fltvec.h"
#include <stdlib.h>
#include <string.h>	// for memset()

//
// QN_InFtrStream_Buffered
//

inline int max(int a, int b) { return (a>b)?a:b; }
inline int min(int a, int b) { return (a<b)?a:b; }

size_t QN_InFtrStream_Buffered::fill_input_buffer(int start, int len) {
    // Fill our internal buffer of input frames from the feeding
    // feature stream.  Arrange for it to contain (at least) <len>
    // contiguous frames, starting from index <start>.  Return the 
    // number of frames available in the buffer, <= len.
    
    // Buffer member variables:
    //   input_buffer - allocated space
    //   in_ftr_count - elements per input frame
    //   buf_frames - max size of buffer, in frames
    //   buf_base - index of frame currently at base of buffer
    //   buf_cont - number of valid frames following base currently in buf
    //   buf_maxpad - how many frames are faked at end

    int load_base, load_len, prepad = 0;
    
    // Truncate requests to read more than 1 buffer
    if (len > (int)buf_frames) {
    	len = (int)buf_frames;
    }
    
    // Copy back any overlap currently in the buffer
    int overlap_min = max(start, buf_base);
    int overlap_max = min(start+len, buf_base+buf_cont);
    if (overlap_min < overlap_max) {
    	// have overlap
	qn_copy_vf_vf((overlap_max - overlap_min)*in_ftr_count, 
		   (const float *)input_buffer + (overlap_min - buf_base)*in_ftr_count,
		   input_buffer + (overlap_min - start)*in_ftr_count);
    	// figure remainder to load - either at start or at end
    	if (overlap_min == start) {
    	    load_base = overlap_max;
    	    load_len = len - (overlap_max - start);
    	} else {
    	    // overlap was at *end* of buffer
    	    load_base = start;
    	    load_len = overlap_min - start;
    	}
    } else {
    	// No overlap - load whole lot
    	load_base = start;
    	load_len = len;
    	overlap_min = overlap_max = start;
    }
    
    // Don't try to read -ve frames!  Mark them for padding later
    if (load_base < 0) {
    	prepad = -load_base;
    	load_len -= prepad;
    	load_base = 0;
    }
    
    // Load the remainder from upstream
    int got = 0;
    if (my_seg_len == QN_SIZET_BAD || load_base < (int)my_seg_len) {
	// Don't attempt to seek to pos except if it's within the 
	// known end-of-seg, or end-of-seg hasn't been seen.
	if (load_base != (int)str_pos) {
	    log.log(QN_LOG_PER_SENT, "qn_fir:fill_ip: str.set_pos(%d,%d) from %d", my_seg, load_base, str_pos);
	    if (str.set_pos(my_seg, load_base) == QN_SEGID_BAD) {
		log.error("unable to set_pos(%d,%d)", my_seg, load_base);
	    }
	    str_pos = load_base;
	}
	got = str.read_ftrs(load_len, 
			    input_buffer 
			      + (load_base - start)*in_ftr_count);
	str_pos += got;
    }

    log.log(QN_LOG_PER_BUNCH, 
	    "fill_IP(%d:%d+%d): buf=%d+%d olap=%d..%d load %d+%d got %d", 
	    my_seg, start, len, buf_base, buf_cont, overlap_min, overlap_max, 
	    load_base, load_len, got);

    // if we saw the end-of-seg, record it
    if (got < load_len) {
    	if (my_seg_len == QN_SIZET_BAD) {
    	    my_seg_len = load_base + got;
    	}
    }
    				   	
    // if we were filling-in below a shifted bit, insist on success
    if (load_base < overlap_min) {
    	assert( (load_base + got) == overlap_min);
    }
    
    // do pre-padding, if required
    if (prepad!=0) {
	qn_copy_vf_mf(prepad, in_ftr_count, 
    	           (const float *)input_buffer + prepad*in_ftr_count,
		   input_buffer);
    }

    // Update description of buffer contents
    buf_base = start;
    buf_cont = max(overlap_max, load_base+got) - buf_base;
    
    // If hit end, maybe to postpad too
    if (buf_cont < (size_t)len) {
    	// (not if we have nothing!)
    	assert(buf_cont > 0);
    	// else, copy last valid frame over rest of request, up to 
    	// buf_maxpad beyond last frame
    	assert(my_seg_len != QN_SIZET_BAD);
    	int howfar = min(buf_base + len, my_seg_len + buf_maxpad);
	qn_copy_vf_mf(howfar - buf_base - buf_cont, in_ftr_count, 
    	           (const float *)input_buffer + (buf_cont-1)*in_ftr_count,
		   input_buffer+buf_cont*in_ftr_count);
	buf_cont = howfar - buf_base;
    }
    
    // Return total available frames, <= len (may load extras later)
    //    fprintf(stderr, "QN_fir::fill_buf(%d:%d+%d): got %d\n", 
    //            my_seg, start, len, buf_cont);
    return min(len, buf_cont);
}

////////////////////////////////////////////////////////////////

QN_InFtrStream_Buffered::QN_InFtrStream_Buffered(int a_debug,
						 const char* a_dbgname,
						 QN_InFtrStream& a_str,
						 size_t a_buf_frames)
  : log(a_debug, "QN_InFtrStream_Buffered", a_dbgname),
    str(a_str),
    in_ftr_count(a_str.num_ftrs()),
    buf_frames(a_buf_frames)
{
    // No seekpos at start
    my_seg = QN_SIZET_BAD;
    my_seg_len = QN_SIZET_BAD;
    my_pos = 0;
    str_pos = 0;
    
    // Set up our buffer
    int ip_buf_pts = in_ftr_count*buf_frames;
    input_buffer = new float[ip_buf_pts];

    buf_base = 0;
    buf_cont = 0;
    // Let derived classes set buf_maxpad
    buf_maxpad = 0;

    log.log(QN_LOG_PER_RUN, "qn_buffered(in_ftr_c=%d, buf_fr=%d)", 
	    in_ftr_count, buf_frames);
}

QN_InFtrStream_Buffered::~QN_InFtrStream_Buffered()
{
    delete[] input_buffer;
}

int
QN_InFtrStream_Buffered::rewind()
{
    my_seg = QN_SIZET_BAD;
    my_seg_len = QN_SIZET_BAD;
    my_pos = 0;
    str_pos = 0;

    // mark the buffer empty
    buf_base = 0;
    buf_cont = 0;
    
    log.log(QN_LOG_PER_EPOCH, "qn_Buffered::rewind");

    return str.rewind();
}

QN_SegID 
QN_InFtrStream_Buffered::nextseg()
{
    QN_SegID segid = str.nextseg();
    
    // mark the buffer empty
    buf_base = 0;
    buf_cont = 0;
    
    if (segid == QN_SEGID_BAD) {
    	my_seg = QN_SIZET_BAD;
    } else {
    	// After rewind, nextseg gives seg 0
    	if (my_seg == QN_SIZET_BAD) {
    	    my_seg = 0;
    	} else {
    	    ++my_seg;
    	}
    }
    // pos is reset in new seg
    my_pos = 0;
    str_pos = 0;
    // don't know seglen (haven't seen EOF), so set to zero
    my_seg_len = QN_SIZET_BAD;
    
    log.log(QN_LOG_PER_EPOCH, "qn_Buffered::nextseg->%d", my_seg);

    return segid;
}

size_t
QN_InFtrStream_Buffered::num_segs()
{
    return str.num_segs();
}

size_t
QN_InFtrStream_Buffered::num_frames(size_t segno)
{
    return str.num_frames(segno);
}

int
QN_InFtrStream_Buffered::get_pos(size_t* segno, size_t* frameno)
{
    int ret = str.get_pos(segno, frameno); // Call substream get_pos

    if (ret != QN_BAD) {
    	if (segno) {
    	    *segno = my_seg;
    	}
    	if (frameno) {
    	    *frameno = my_pos;
    	}
    }
    
    return ret;
}

QN_SegID
QN_InFtrStream_Buffered::set_pos(size_t segno, size_t frameno)
{
    // We set the pos of the upstream stream just to check the validity
    // of these numbers.  Of course, when we come to read from that 
    // stream, we'll need to seek again to get the pre-context (unless 
    // we're very lucky with what we have in our buffer).  But if we 
    // just did the seek to the pre-context pos right now, we could end 
    // up giving errors in the wrong places, and no errors in other places.
    QN_SegID segid = str.set_pos(segno, frameno);
    log.log(QN_LOG_PER_SENT, "qn_Buffered:set_pos: str.set_pos(%d,%d) from %d", segno, frameno, str_pos);

    if(segno != my_seg) {
	my_seg = segno;
	// changed seg, so don't know length.
	my_seg_len = QN_SIZET_BAD;
	// uh, I guess the current buffer contents would be invalid too
	buf_base = 0;
	buf_cont = 0;
    }
    my_pos = frameno;
    str_pos = frameno;
    
    return segid;
}

////////////////////////////////////////////////////////////////

QN_InFtrStream_FIR::QN_InFtrStream_FIR(int a_debug,
				       const char* a_dbgname,
				       QN_InFtrStream& a_str,
				       size_t a_filt_len,
				       size_t a_filt_ctr, 
				       const float* a_filt_coeffs,
				       size_t a_ftr_start,
				       size_t a_ftr_count,
				       size_t a_buf_frames)
  : QN_InFtrStream_Buffered(a_debug, a_dbgname, a_str, a_buf_frames), 
    clog(a_debug, "QN_InFtrStream_FIR", a_dbgname),
    ftr_start(a_ftr_start),
    filter_len(a_filt_len), 
    filter_ctr(a_filt_ctr)
{
    int new_ftr_count = (a_ftr_count==QN_ALL) ? in_ftr_count - ftr_start 
                                                : a_ftr_count;
    out_ftr_count = in_ftr_count + new_ftr_count;
    
    filter = new float[filter_len];
    qn_copy_vf_vf(filter_len, a_filt_coeffs, filter);

    // Set up the buffers
    int ip_buf_pts = in_ftr_count*buf_frames;
    // tmp output buffer must hold all end-ringing from convolution too
    int op_buf_pts = out_ftr_count*(buf_frames + filter_len-1);
    // input_buffer_t also used as target for transposing out_buf_t, so
    // make large enough
    input_buffer_t = new float[max(ip_buf_pts, op_buf_pts)];
    output_buffer_t = new float[op_buf_pts];

    // Allow the ends of frames to be post-padded by up to 
    // filter_postlen points
    buf_maxpad = filter_len - filter_ctr - 1;

    clog.log(QN_LOG_PER_RUN, "qn_fir(in_ftr=%d add_ftr=%d+%d filt=%d@%d)", 
	    in_ftr_count, ftr_start, new_ftr_count, filter_len, filter_ctr);
}

QN_InFtrStream_FIR::~QN_InFtrStream_FIR()
{
    delete[] output_buffer_t;
    delete[] input_buffer_t;
    delete[] filter;
}

size_t
QN_InFtrStream_FIR::num_ftrs()
{
    return out_ftr_count;
}

//
// QN_InFtrStream_FIR
//

size_t do_convolution(float *filt, int filt_len, 
                      float *ip, int istep, int ilen, 
                      float *op, int ostep) {
    // Do a convolution with the impulse response <filt> of <filt_len> 
    // and <ilen> input points, starting at <ip> with <istep> between 
    // each point, writing the results at <op> stepped by <ostep>.
    // Return the number of points written, which is only the fully-
    // calculable ones i.e. (ilen - filt_len + 1) of them.
    int irem = ilen;
    int j;
    int ndone = 0;
    while (irem >= filt_len) {
    	double a = 0.0;
    	for (j = 0; j < filt_len; ++j) {
    	    a += filt[filt_len - 1 - j] * *(ip + j*istep);
    	}
    	*op = a;
    	op += ostep;
    	ip += istep;
    	--irem;
    	++ndone;
    }
    return ndone;
}

size_t QN_InFtrStream_FIR::read_ftrs(size_t cnt, float* output) {
    // Read some feature frames, including adding deltas    
    int op_frames = -1;
    int remain = cnt;
    size_t n_done = 0;
    int ftr;
    int add_ftr_count = out_ftr_count - in_ftr_count;

    int past_context = filter_ctr;
    int future_context = filter_len - filter_ctr - 1;
    int my_pos_entry = my_pos;	// to report to logger at exit
    
    if (my_seg == QN_SIZET_BAD || my_pos == QN_SIZET_BAD) {
    	// not at a valid pt - can't read
    	n_done = QN_SIZET_BAD;
    } else {
        int req_len, net_frames, earliest_frame;
	int total_frames = 0, got_frames = 0;
	while (remain > 0 && /* op_frames != 0 */ got_frames == total_frames) {
	    // Setup for next iteration
	    req_len = min(remain, buf_frames-past_context-future_context);
            total_frames = req_len + past_context + future_context;
	    earliest_frame = ((int)my_pos) - past_context;
	    got_frames = fill_input_buffer(earliest_frame, total_frames);
	    net_frames = got_frames - past_context - future_context;
#define OLD 1
#ifdef OLD
	    for (ftr = 0; ftr < add_ftr_count; ++ftr) {
		op_frames = do_convolution(filter, filter_len, 
					   input_buffer+ftr, in_ftr_count, 
					   got_frames, 
					   output + in_ftr_count + ftr
					       + n_done*out_ftr_count, 
					   out_ftr_count);
	    }
	    // Also copy the direct features to the output, 
	    // number of frames to match output of convolution
	    int frm;
	    for (frm = 0; frm < op_frames; ++frm) {
		memcpy(output + out_ftr_count*(n_done + frm), 
		       (const float *)input_buffer 
		           + in_ftr_count*(past_context + frm), 
		       in_ftr_count*sizeof(float));
	    }
#else /* NEW */
	    // Buffer for transposed result is last bit of output buffer
	    //float *output_tmp = output + out_ftr_count*cnt 
	    //                     - input_ftr_count*got_frames
	    // Total number of output frames from full convoluation
	    int fullop_frames = got_frames + filter_len - 1;

	    op_frames = net_frames;	    
	    if (op_frames) {
	        qn_trans_mf_mf(got_frames, in_ftr_count, input_buffer, input_buffer_t);
		for (ftr = 0; ftr < add_ftr_count; ++ftr) {
		    qn_convol_vfvf_vf(filter_len, got_frames, filter, 
				   (const float*)input_buffer_t+got_frames*ftr, 
				   output_buffer_t + fullop_frames*ftr);
		}
		qn_trans_mf_mf(add_ftr_count, fullop_frames, output_buffer_t, 
			    input_buffer_t);
		qn_copy_mf_smf(op_frames, add_ftr_count, out_ftr_count, 
			   (const float *)input_buffer_t 
			         + add_ftr_count*(filter_len - 1), 
			   output + in_ftr_count + out_ftr_count*n_done);
	    // Also copy the direct features to the output, 
	    // number of frames to match output of convolution
		qn_copy_mf_smf(op_frames, in_ftr_count, out_ftr_count, 
			   (const float *)input_buffer + in_ftr_count*past_context,
			   output + out_ftr_count*n_done);
	    }

	    // Other way to do it would be to use mulsum_vfvf_f instead 
	    // of convol

#endif /* OLD/NEW */

	    n_done += op_frames;
	    remain -= op_frames;
	    my_pos += op_frames;
	}
    }
    
    clog.log(QN_LOG_PER_BUNCH, 
	    "qnfir:read_ftrs(%d)@%d:%d got %d", cnt, my_seg, my_pos_entry, n_done);

    assert(my_seg != QN_SIZET_BAD);

    return n_done;
}

/* Routines to set up filters, adapted from rasta/deltas_c.c */

int QN_InFtrStream_FIR::FillDeltaFilt(float *fv, size_t len)
{   /* build a filter for delta calculation */
    size_t i;
    size_t mp = (len-1)/2;

    fv[mp] = 0;
    for(i = 1; i <= mp; ++i) {
	fv[mp+i] = - (float) i;
	fv[mp-i] = (float) i;
    }
    /* Note: de facto standard of not normalizing delta filter for consistency
       with other tools */
    return len;
}

int QN_InFtrStream_FIR::FillDoubleDeltaFilt(float *fv, size_t len)
{   /* build a filter for double-delta calculation - convol'n of 2 dfilts */

    assert(len!=0);
    size_t dlen = (len+1)/2;
    size_t blen = 3*dlen-2; 
    float *buf = new float[blen];
    qn_copy_f_vf(blen, 0.0f, buf);
    FillDeltaFilt(buf+dlen-1, dlen);
    qn_convol_vfvf_vf(dlen, blen, buf+dlen-1, buf, fv);
    /* no need to flip sign?? */
    delete [] buf;
    return len;
}

