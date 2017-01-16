// $Header: /u/drspeech/repos/quicknet2/testsuite/RandWindow_test.cc,v 1.2 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for random window classes.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

enum {
    MIN_SEGS = 10,
    MAX_SEGS = 100,
    MAX_FRAMES = 100
};

void
seq_test(int debug, size_t win_len, size_t top_margin, size_t bot_margin,
	 size_t buf_frames)
{
    // The random windowing class can also act sequentially.  This can
    // be used to test most of the functionality.
    rtst_log("Comparing with sequential window...\n");

    // Generate a couple of label streams of random sizes containing known
    // data. 
    int seed = rtst_urand_i32i32_i32(-0x7fffffff-1, 0x7fffffff);
    size_t min_frames = top_margin + bot_margin + win_len;
    QN_InLabStream* finfo_str1 =
	new QN_InLabStream_FrameInfo(debug, "finfo_seq", MIN_SEGS, MAX_SEGS,
				     min_frames, MAX_FRAMES, seed);
    QN_InLabStream* finfo_str2 =
	new QN_InLabStream_FrameInfo(debug, "finfo_rand", MIN_SEGS, MAX_SEGS, 
				     min_frames, MAX_FRAMES, seed);

    // Create sequential and random (but with sequential ordering) windows
    // on these streams.
    QN_InLabStream* seq_str =
	new QN_InLabStream_SeqWindow(debug, "seq", *finfo_str1,
				     win_len, top_margin,
				     bot_margin, 10);
    QN_InLabStream* rand_str =
	new QN_InLabStream_RandWindow(debug, "rand", *finfo_str2,
				      win_len, top_margin,
				      bot_margin, buf_frames, seed,
				      QN_InLabStream_RandWindow::SEQUENTIAL);
    // Check various bits and pieces.
    rtst_assert(rand_str->num_segs()<MAX_SEGS);
    size_t n_frames = rand_str->num_frames();
    rtst_assert(n_frames<MAX_SEGS*MAX_FRAMES);
    rtst_assert(rand_str->num_frames(0)<buf_frames);
    size_t n_labs = rand_str->num_labs();
    size_t seq_n_labs = seq_str->num_labs();
    rtst_assert(n_labs==seq_n_labs);

    // Make sure random access stuff fails.
    size_t dummy_seg, dummy_frame;
    rtst_assert(rand_str->get_pos(&dummy_seg, &dummy_frame)==QN_BAD);

    // Allocate space for all of both streams.
    size_t buf_size = n_frames * n_labs;
    QNUInt32* rand_buf;
    QNUInt32* seq_buf;
    rand_buf = (QNUInt32*) rtst_padvec_new_vi32(buf_size);
    seq_buf = (QNUInt32*) rtst_padvec_new_vi32(buf_size);

    // Read the sequential stream into a buffer.
    QNUInt32* seq_buf_ptr = seq_buf;
    size_t seq_actual_frames = 0;
    size_t seq_actual_segs = 0;
    while(1)			// Iterate over all segments.
    {
	QN_SegID id;

	id = seq_str->nextseg();
	if (id==QN_SEGID_BAD)
	    break;
	seq_actual_segs++;
	while(1)		// Iterate over all frames in segment.
	{
	    size_t frames, count;
	    frames = rtst_urand_i32i32_i32(1,200);
	    count = seq_str->read_labs(frames, seq_buf_ptr);
	    seq_buf_ptr += count*n_labs;
	    seq_actual_frames += count;
	    if (count<frames)
		break;
	}
    }

    // Read the random stream into a buffer.
    QNUInt32* rand_buf_ptr = rand_buf;
    size_t rand_actual_frames = 0;
    size_t rand_actual_segs = 0;
    while(1)			// Iterate over all segments.
    {
	QN_SegID id;

	id = rand_str->nextseg();
	if (id==QN_SEGID_BAD)
	    break;
	rand_actual_segs++;
	while(1)		// Iterate over all frames in segment.
	{
	    size_t frames, count;
	    frames = rtst_urand_i32i32_i32(1,200);
	    count = rand_str->read_labs(frames, rand_buf_ptr);
	    rand_buf_ptr += count*n_labs;
	    rand_actual_frames += count;
	    if (count<frames)
		break;
	}
    }
    rtst_assert(seq_actual_frames==rand_actual_frames);
    rtst_assert(rand_actual_frames==n_frames);

    // Check that the contents of the two buffers are the same.
    rtst_checkeq_vi32vi32(buf_size, (rtst_int32*) seq_buf,
			  (rtst_int32*) rand_buf);

    rtst_padvec_del_vi32((rtst_int32) seq_buf);
    rtst_padvec_del_vi32((rtst_int32) rand_buf);
    delete rand_str;
    delete seq_str;
    delete finfo_str2;
    delete finfo_str1;
}

void
rand_test(int debug, size_t win_len, size_t top_margin, size_t bot_margin,
	 size_t buf_frames)
{
    size_t i;
    size_t j;

    // Test true random access.  With the frame and segment number in the
    // data, we can keep a count of how often each frame occurs, checking
    // that it is 0 in the margins and 1 elsewhere.
    // be used to test most of the functionality.
    rtst_log("Checking true random access...\n");

    // Generate a label stream of random size, but with the data indicating
    // where in the input stream it comes from.
    int seed = rtst_urand_i32i32_i32(-0x7fffffff-1, 0x7fffffff);
    size_t min_frames = top_margin + bot_margin + win_len;
    QN_InLabStream* finfo_str =
	new QN_InLabStream_FrameInfo(debug, "finfo_seq", MIN_SEGS, MAX_SEGS,
				     min_frames, MAX_FRAMES, seed);

    // Create a C-style matrix holding the observation counts for each frame.
    size_t n_segs = finfo_str->num_segs();
    size_t** segptrs = new size_t*[n_segs];
    for (i=0; i<n_segs; i++)
    {
	size_t n_frames;
	size_t* counts;

	n_frames = finfo_str->num_frames(i);
	counts = new size_t[n_frames];
	for (j=0; j<n_frames; j++)
	    counts[j] = 0;
	segptrs[i] = counts;
    }

    // Create the random stream.
    QN_InLabStream* rand_str =
	new QN_InLabStream_RandWindow(debug, "rand", *finfo_str,
				      win_len, top_margin,
				      bot_margin, buf_frames, seed,
				      QN_InLabStream_RandWindow::RANDOM_NO_REPLACE);
    size_t in_n_labs = finfo_str->num_labs();
    size_t out_n_labs = rand_str->num_labs();
    rtst_assert(out_n_labs==in_n_labs*win_len);

    // Read the random stream, counting the number of times each frame occurs.
    enum { MAX_FRAMES = 50 };	// Maximum frames in one read.
    size_t frames, count;
    QNUInt32* buf = (QNUInt32*) rtst_padvec_new_vi32(MAX_FRAMES*out_n_labs);
    QN_SegID id;
    QNUInt32* in_frame;
    id = rand_str->nextseg();
    rtst_assert(id!=QN_SEGID_BAD);
    while(1)			// Iterate over all data.
    {

	frames = rtst_urand_i32i32_i32(1,MAX_FRAMES);
	count = rand_str->read_labs(frames, buf);
	in_frame = buf;
	// Iterate over all frames in the window.
	for (i=0; i<count; i++)
	{
	    size_t segno = in_frame[1];
	    size_t frameno = in_frame[2];
	    rtst_assert(segno<n_segs);
	    rtst_assert(frameno<finfo_str->num_frames(segno));
	    segptrs[segno][frameno]++; // Count how many times this frame
				       // occurs.
	    in_frame+=out_n_labs;
	}
	if (count<frames)
	{
	    id = rand_str->nextseg();
	    if (id==QN_SEGID_BAD)
		break;
	}
    }

    // Check the values in the count matrix.
    for (i=0; i<n_segs; i++)
    {
	size_t n_frames = finfo_str->num_frames(i);
	for (j=0; j<n_frames; j++)
	{
	    if (j<top_margin || j>=(n_frames-(bot_margin+win_len-1)))
	    {
		rtst_assert(segptrs[i][j]==0);
	    }
	    else
	    {
		rtst_assert(segptrs[i][j]==1);
	    }
	}
    }
    
    // Delete the count matrix.
    for (i=0; i<n_segs; i++)
	delete[] segptrs[i];
    delete[] segptrs;
    rtst_padvec_del_vi32((rtst_int32) buf);

    delete rand_str;
    delete finfo_str;
}

int
main(int argc, char* argv[])
{
    int arg;
    int debug = 0;
    
    arg = rtst_args(argc, argv);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "RandWindow_test");
    if (arg!=argc-4)
    {
	fprintf(stderr, "Usage: RandWindow_test <win_len> <top_margin> "
		"<bot_margin> <bufframes>\n");
	exit (1);
    }
    size_t win_len = strtol(argv[arg++], NULL, 0);
    size_t top_margin = strtol(argv[arg++], NULL, 0);
    size_t bot_margin = strtol(argv[arg++], NULL, 0);
    size_t buf_size = strtol(argv[arg], NULL, 0);
    arg++;
    

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start("RandWindow_test");
    seq_test(debug, win_len, top_margin, bot_margin, buf_size);
    rand_test(debug, win_len, top_margin, bot_margin, buf_size);
    rtst_passed();
}
