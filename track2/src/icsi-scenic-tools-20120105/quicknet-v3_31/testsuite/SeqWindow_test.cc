// $Header: /u/drspeech/repos/quicknet2/testsuite/SeqWindow_test.cc,v 1.6 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for sequential windows.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "QuickNet.h"
#include "rtst.h"

static void
test(int debug, const char* pfile_name, size_t win_len, size_t top_margin,
     size_t bot_margin, size_t buf_size)
{
    int ec;
    
    // Open up the PFile.
    FILE* pfile_file = fopen(pfile_name, "r");
    assert(pfile_file!=NULL);
    QN_InFtrStream* pfile_str =
	new QN_InFtrLabStream_PFile(debug, "pfile", pfile_file, 1);

    // Open up the PFile again and apply the window.
    FILE* win_pfile_file = fopen(pfile_name, "r");
    assert(win_pfile_file!=NULL);
    QN_InFtrStream* win_pfile_str =
	new QN_InFtrLabStream_PFile(debug, "win", win_pfile_file, 1);
    QN_InFtrStream* win_str =
	new QN_InFtrStream_SeqWindow(debug, "win", *win_pfile_str, win_len,
				     top_margin,
				     bot_margin, buf_size);

    // Check various bits and pieces.
    rtst_assert(win_str->num_segs()==QN_SIZET_BAD);
    rtst_assert(win_str->num_frames()==QN_SIZET_BAD);
    rtst_assert(win_str->num_frames(0)==QN_SIZET_BAD);

    // Make sure random access stuff fails.
    size_t dummy_seg, dummy_frame;
    rtst_assert(win_str->get_pos(&dummy_seg, &dummy_frame)==QN_BAD);

    // Scan through both the PFile stream and the windowed stream, ensuring
    // they are consistent.
    size_t pfile_num_ftrs = pfile_str->num_ftrs();
    size_t win_num_ftrs = win_str->num_ftrs();
    rtst_assert(pfile_num_ftrs * win_len == win_num_ftrs);
    size_t current_seg;
    current_seg = 0;
    while(1)			// Iterate over all sentences.
    {
	QN_SegID p_id, w_id;
	size_t pfile_segno;

	p_id = pfile_str->nextseg();
	w_id = win_str->nextseg();
	rtst_assert(p_id==w_id);
	if (w_id==QN_SEGID_BAD)
	    break;

	pfile_segno = top_margin;
	size_t current_frame;
	current_frame = 0;
	while(1)		// Iterate over all bunches of frames.
	{
	    size_t num_frames;
	    size_t p_count, w_count;
	    QN_SegID segid;
	    size_t size_win;

	    // Read a random number of frames.
	    num_frames = rtst_urand_i32i32_i32(1,200);
	    size_win = num_frames * win_num_ftrs;
	    float* p_ftrbuf = rtst_padvec_new_vf(size_win);
	    float* w_ftrbuf = rtst_padvec_new_vf(size_win);
	    rtst_log("Reading %lu frames.\n", (unsigned long) num_frames);

	    // Read in data from windowed stream.
	    w_count = win_str->read_ftrs(num_frames, w_ftrbuf);
	    // Read in equivalent data from PFile.
	    size_t frame;
	    for (frame=0; frame<w_count; frame++)
	    {
		segid = pfile_str->set_pos(current_seg,
					   current_frame+top_margin+frame);
		rtst_assert(segid!=QN_SEGID_BAD);
		p_count = pfile_str->read_ftrs(win_len,
					       &p_ftrbuf[frame*win_num_ftrs]);
		rtst_assert(p_count==win_len);
	    }
	    current_frame += w_count;
	    rtst_checkeq_vfvf(w_count*win_num_ftrs,
			       p_ftrbuf, w_ftrbuf);
	    rtst_padvec_del_vf(w_ftrbuf);
	    rtst_padvec_del_vf(p_ftrbuf);
	    if (w_count<num_frames)
		break;
	}
	// Check there were the right number of frames.
	rtst_assert((current_frame+top_margin+bot_margin+win_len-1)
		    ==pfile_str->num_frames(current_seg));
	current_seg++;
    }
    rtst_assert(current_seg==pfile_str->num_segs());

    delete win_str;
    delete win_pfile_str;
    delete pfile_str;

    ec = fclose(win_pfile_file);
    assert(ec==0);
    ec = fclose(pfile_file);
    assert(ec==0);
}


int
main(int argc, char* argv[])
{
    int arg;
    int debug = 0;
    
    arg = rtst_args(argc, argv);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "SeqWindow_test");
    assert(arg == argc-5);
    // Usage: SeqWindow_test <pfile> <win_len> <top_margin> <bot_margin> <buflen>
    const char* pfile = argv[arg++];
    size_t win_len = strtol(argv[arg++], NULL, 0);
    size_t top_margin = strtol(argv[arg++], NULL, 0);
    size_t bot_margin = strtol(argv[arg++], NULL, 0);
    size_t buf_size;
    if (strcmp(argv[arg], "d")==0)
	buf_size = QN_SIZET_BAD;
    else
	buf_size = strtol(argv[arg], NULL, 0);
    arg++;
    

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start("SeqWindow_test");
    test(debug, pfile, win_len, top_margin, bot_margin, buf_size);
    rtst_passed();
}
