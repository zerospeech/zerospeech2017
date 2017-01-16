// $Header: /u/drspeech/repos/quicknet2/testsuite/PFile_test1.cc,v 1.11 2004/09/16 00:13:50 davidj Exp $
//
// Read tests for QN_InFtrLabStream_PFile class.
// davidj - 5th March 1996

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

void
PFile_indexed_test(int debug, const char* pfile, size_t num_sent,
		   size_t num_frames, size_t num_ftrs, size_t num_labs)
{
    size_t i;			// Counter

    rtst_start("PFile_indexed_test");
    FILE* testfile = QN_open(pfile, "r");

    QN_InFtrLabStream_PFile pf(debug, "pfile", testfile, 1);

    // Check the basic attributes of the PFile
    rtst_assert(pf.num_segs()==num_sent);
    rtst_assert(pf.num_frames()==num_frames);
    rtst_assert(pf.num_ftrs()==num_ftrs);
    rtst_assert(pf.num_labs()==num_labs);



    // Read through the whole PFile, checking lots of details Here we assume
    // the PFile holds normal speech data - that labels hold small positive
    // integers and features are small reals.  We also sum all the number of
    // frames in each sentence, and ensure the total is the sum as the number
    // of frames in the whole database
    {
	enum { FRAMES = 17 }; // Choose an obscure number of frames to read
	float* ftrs;
	QNUInt32* labs;
	size_t num_ftr_vals = FRAMES * num_ftrs;
	size_t num_lab_vals = FRAMES * num_labs;
	const float bad_ftr = 1e30;
	const QNUInt32 bad_lab = 0xffffffffu;
	const float min_ftr = -30.0f;
	const float max_ftr = 30.0f;
	const QNUInt32 min_lab = 0;
	const QNUInt32 max_lab = 100;
	QN_SegID sentid;
	size_t acc_frames = 0;

	ftrs = rtst_padvec_new_vf(num_ftr_vals);
	labs = (QNUInt32*) rtst_padvec_new_vi32(num_lab_vals);
	for (i=0; i<num_sent; i++)
	{
	    size_t sent_frames;
	    size_t frame;
	    size_t cnt = FRAMES;
	    
	    sentid = pf.nextseg();
	    rtst_assert(sentid!=QN_SEGID_BAD);
	    sent_frames = pf.num_frames(i);
	    assert(sent_frames!=QN_SIZET_BAD);
	    acc_frames += sent_frames;

	    for (frame=0; frame<sent_frames; frame+=cnt)
	    {

		// Clear the ftr and lab vectors before reading
		qn_copy_f_vf(num_ftr_vals, bad_ftr, ftrs);
		qn_copy_i32_vi32(num_lab_vals, bad_lab, (QNInt32*) labs);

		cnt = pf.read_ftrslabs(FRAMES, ftrs, labs);
		rtst_assert(cnt!=QN_SIZET_BAD);

		// Check that everything we read is sensible
		rtst_checkrange_ffvf(cnt*num_ftrs, min_ftr, max_ftr, ftrs);
		// This is a hack - really should use unsigned version
		rtst_checkrange_i32i32vi32(cnt*num_labs,
					   (rtst_int32) min_lab,
					   (rtst_int32) max_lab,
					   (rtst_int32 *) labs);

		// Check that stuff we did not change is not corrupted
		if (cnt!=FRAMES)
		{
		    rtst_checkrange_ffvf((FRAMES-cnt)*num_ftrs,
					 bad_ftr, bad_ftr,
					 &ftrs[num_ftrs*cnt]);
		    rtst_checkrange_i32i32vi32((FRAMES-cnt)*num_labs,
				       bad_lab, bad_lab,
				       (rtst_int32 *) &labs[num_labs*cnt]);
		}
	    }
	}
	sentid = pf.nextseg();
	rtst_assert(sentid==QN_SEGID_BAD);
	rtst_assert(acc_frames==num_frames);
	rtst_padvec_del_vi32(labs);
	rtst_padvec_del_vf(ftrs);
    }
    // Check seeking to start of PFile
    {
	int ec;
	size_t current_sent, current_frame;
	QN_SegID id;

	ec = pf.rewind();
	rtst_assert(ec==QN_OK);
	// Test that get_pos after rewind() works.
	ec = pf.get_pos(&current_sent, &current_frame);
	rtst_assert(ec==QN_OK);
	rtst_assert(current_sent==QN_SIZET_BAD);
	rtst_assert(current_frame==QN_SIZET_BAD);
	// Test that get_pos with NULL pointers works.
	ec = pf.get_pos(NULL, NULL);

	id = pf.nextseg();
	rtst_assert(id!=QN_SEGID_BAD);
	ec = pf.get_pos(&current_sent, &current_frame);
	rtst_assert(ec==QN_OK);
	rtst_assert(current_sent==0);
	rtst_assert(current_frame==0);
    }
    // Some storage for feature vectors
    float* last_frame_seek = new float [num_ftrs];
    float* last_frame_read = new float [num_ftrs];

    // Seek to end of PFile and return last frame
    {
	int ec;
	size_t current_sent, current_frame;

	// Goto end of PFile and check returned frame
	size_t last_sent  = num_sent - 1;
	size_t last_frame = pf.num_frames(last_sent) - 1;
	ec = pf.set_pos(last_sent, last_frame);
	rtst_assert(ec==QN_OK);
	ec = pf.get_pos(&current_sent, &current_frame);
	rtst_assert(ec==QN_OK);
	rtst_assert(current_sent==last_sent);
	rtst_assert(current_frame==last_frame);

	size_t cnt = pf.read_ftrs(1, last_frame_seek);
	rtst_assert(cnt==1);
    }
    // Scan through PFile and read last frame
    {
	size_t current_sent, current_frame;
	int id;

	id = pf.set_pos(0, 0);
	rtst_assert(id!=QN_SEGID_BAD);
	id = pf.get_pos(&current_sent, &current_frame);
	rtst_assert(id!=QN_SEGID_BAD);
	rtst_assert(current_sent==0);
	rtst_assert(current_frame==0);
	for (i=0; i<num_sent-1; i++)
	{
	    QN_SegID sentid;
	    sentid = pf.nextseg();
	    rtst_assert(sentid!=QN_SEGID_BAD);
	}
	for (i=0; i<pf.num_frames(num_sent-1)-1; i++)
	{
	    size_t count;
	    count = pf.read_labs(1, NULL);
	    rtst_assert(count==1);
	}
	size_t cnt = pf.read_ftrs(1, last_frame_read);
	rtst_assert(cnt==1);
	rtst_assert(pf.read_labs(1,NULL)==0);
	rtst_assert(pf.nextseg()==QN_SEGID_BAD);
    }
    rtst_checkeq_vfvf(num_ftrs, last_frame_seek, last_frame_read);
    delete [] last_frame_seek;
    delete [] last_frame_read;

    QN_close(testfile);

    rtst_passed();
}


int
main(int argc, char* argv[])
{
    int arg;
    int debug = 0;		// Verbosity level

    arg = rtst_args(argc, argv);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "PFile_test1");

    assert(arg==argc-5);
    const char* pfile = argv[arg++];
    long num_sents = strtol(argv[arg++], NULL, 0);
    long num_frames = strtol(argv[arg++], NULL, 0);
    long num_ftrs = strtol(argv[arg++], NULL, 0);
    long num_labels = strtol(argv[arg++], NULL, 0);
    if (rtst_logfile!=NULL)
	debug = 99;
    PFile_indexed_test(debug, pfile, num_sents, num_frames,
		       num_ftrs, num_labels);
    rtst_exit();
}
