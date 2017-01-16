// $Header: /u/drspeech/repos/quicknet2/testsuite/camfiles_test1.cc,v 1.7 2004/09/16 00:13:50 davidj Exp $
//
// Read tests for QN_InFtrLabStream_{LNA8,PreFile,OnlFtr} classes.
// davidj - 5th March 1996

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

// This routines runs tests on a supplied stream
void
test(QN_InFtrLabStream& instr, size_t num_segs, size_t num_frames,
	     size_t num_ftrs, size_t num_labs, int indexed)
{
    ///////////////////////
    // Check the basic attributes of the file
    rtst_log("Testing basic attributes.\n");
    rtst_assert(instr.num_ftrs()==num_ftrs);
    rtst_assert(instr.num_labs()==num_labs);
    // Some tests that only work for an indexed file
    if (indexed)
    {
	rtst_assert(instr.num_segs()==num_segs);
	rtst_assert(instr.num_frames()==num_frames);
    }
    else
    {
	rtst_assert(instr.num_segs()==QN_SIZET_BAD);
	rtst_assert(instr.num_frames()==QN_SIZET_BAD);
    }

    ///////////////////////
    // Read through the whole file, checking lots of details Here we assume
    // the file holds normal speech data - that labels hold small positive
    // integers and features are small reals.  We also sum all the number of
    // frames in each sentence, and ensure the total is the sum as the number
    // of frames in the whole database.
    rtst_log("Testing scan through all of file.\n");
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
	size_t current_seg;

	ftrs = rtst_padvec_new_vf(num_ftr_vals);
	if (num_labs!=0)
	    labs = (QNUInt32*) rtst_padvec_new_vi32(num_lab_vals);
	else
	    labs = NULL;
	
	current_seg = 0;
	while(1)
	{
	    size_t sent_frames;
	    size_t cnt = FRAMES;

	    sent_frames = 0;
	    sentid = instr.nextseg();
	    if (sentid==QN_SEGID_BAD)
		break;
	    do
	    {

		// Clear the ftr and lab vectors before reading
		qn_copy_f_vf(num_ftr_vals, bad_ftr, ftrs);
		if (num_labs!=0)
		    qn_copy_i32_vi32(num_lab_vals, bad_lab, (QNInt32*) labs);

		cnt = instr.read_ftrslabs(FRAMES, ftrs, labs);
		rtst_assert(cnt!=QN_SIZET_BAD);

		// Check that everything we read is sensible
		rtst_checkrange_ffvf(cnt*num_ftrs, min_ftr, max_ftr, ftrs);
		// This is a hack - really should use unsigned version
		if (num_labs!=0)
		{
		    rtst_checkrange_i32i32vi32(cnt,
					       (rtst_int32) min_lab,
					       (rtst_int32) max_lab,
					       (rtst_int32 *) labs);
		}

		// Check that stuff we did not change is not corrupted
		if (cnt!=FRAMES)
		{
		    rtst_checkrange_ffvf((FRAMES-cnt)*num_ftrs,
					 bad_ftr, bad_ftr,
					 &ftrs[num_ftrs*cnt]);
		    if (num_labs!=0)
		    {
			rtst_checkrange_i32i32vi32((FRAMES-cnt),
						   bad_lab, bad_lab,
						   (rtst_int32 *) &labs[cnt]);
		    }
		}
		sent_frames += cnt;
	    } while (cnt!=0);
	    acc_frames += sent_frames;
	    if (indexed)
	    {
		size_t ind_sent_frames = instr.num_frames(current_seg);
		rtst_assert(ind_sent_frames==sent_frames);
	    }
	    else
	    {
		size_t noind_sent_frames = instr.num_frames(current_seg);
		rtst_assert(noind_sent_frames==QN_SIZET_BAD);
	    }
	    current_seg++;
	}
	rtst_assert(acc_frames==num_frames);
	rtst_assert(current_seg==num_segs);
	if (num_labs!=0)
	    rtst_padvec_del_vi32(labs);
	rtst_padvec_del_vf(ftrs);
    }

    //////////////////
    // Check seeking to start of PFile
    rtst_log("Testing rewind.\n");
    {
	int ec;
	size_t current_sent, current_frame;
	QN_SegID id;

	ec = instr.rewind();
	rtst_assert(ec==QN_OK);
	if (indexed)
	{
	    // Test that get_pos after rewind() works.
	    ec = instr.get_pos(&current_sent, &current_frame);
	    rtst_assert(ec==QN_OK);
	    rtst_assert(current_sent==QN_SIZET_BAD);
	    rtst_assert(current_frame==QN_SIZET_BAD);
	    // Test that get_pos with NULL pointers works.
	    ec = instr.get_pos(NULL, NULL);
	}

	id = instr.nextseg();
	rtst_assert(id!=QN_SEGID_BAD);
	if (indexed)
	{
	    ec = instr.get_pos(&current_sent, &current_frame);
	    rtst_assert(ec==QN_OK);
	    rtst_assert(current_sent==0);
	    rtst_assert(current_frame==0);
	}
    }
    ////////////////
    // Check that seeking to end of segment works.
    if (indexed)
    {
	rtst_log("Testing seek to end of segment.\n");
	{
	    size_t count;
	    QN_SegID segid;

	    size_t frame = instr.num_frames(0);
	    segid = instr.set_pos(0, frame);
	    rtst_assert(segid!=QN_SEGID_BAD);
	    count = instr.read_ftrslabs(1, NULL, NULL);
	    rtst_assert(count==0);
	}
    }

    ///////////////////////
    // Check last frame read with seek to end is same as last
    // frame read with read of whole file.
    if (indexed)
    {
	size_t i;
	int ec;
	size_t current_seg, current_frame;
	int id;
	size_t cnt;
	// Some storage for feature vectors.
	float* last_frame_seek = new float[num_ftrs];
	float* last_frame_read = new float[num_ftrs];

	rtst_log("Testing seeks to end of file.\n");

	// Seek to end of file and return last frame.
	size_t last_seg  = num_segs - 1;
	size_t last_frame =instr.num_frames(last_seg) - 1;
	ec = instr.set_pos(last_seg, last_frame);
	rtst_assert(ec==QN_OK);
	ec = instr.get_pos(&current_seg, &current_frame);
	rtst_assert(ec==QN_OK);
	rtst_assert(current_seg==last_seg);
	rtst_assert(current_frame==last_frame);

	// Try to read three frames, should get just one.
	cnt = instr.read_ftrs(3, last_frame_seek);
	rtst_assert(cnt==1);

	// Scan through PFile and read last frame

	id = instr.set_pos(0, 0);
	rtst_assert(id!=QN_SEGID_BAD);
	id = instr.get_pos(&current_seg, &current_frame);
	rtst_assert(id!=QN_SEGID_BAD);
	rtst_assert(current_seg==0);
	rtst_assert(current_frame==0);
	for (i=0; i<num_segs-1; i++)
	{
	    QN_SegID segid;
	    segid = instr.nextseg();
	    rtst_assert(segid!=QN_SEGID_BAD);
	}
	for (i=0; i<instr.num_frames(num_segs-1)-1; i++)
	{
	    size_t count;
	    count = instr.read_labs(1, NULL);
	    rtst_assert(count==1);
	}
	cnt = instr.read_ftrs(2, last_frame_read);
	rtst_assert(cnt==1);
	// Just an extra read that should fail
	rtst_assert(instr.read_labs(1,NULL)==0);
	rtst_assert(instr.nextseg()==QN_SEGID_BAD);
	
	rtst_checkeq_vfvf(num_ftrs, last_frame_seek, last_frame_read);
	delete [] last_frame_seek;
	delete [] last_frame_read;
    }
}

int
main(int argc, char* argv[])
{
    int arg;
    int debug = 0;		// Verbosity level

    arg = rtst_args(argc, argv);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "camfiles_test1");
    
// Usage: camfiles_test1.exe <file> <type: o|l|p> <segs> <frames> <ftrs> \
//                           <labs> <indexed>
    assert(arg==argc-7);
    const char* filename = argv[arg++];
    const char filetype = *argv[arg++];
    long num_segs = strtol(argv[arg++], NULL, 0);
    long num_frames = strtol(argv[arg++], NULL, 0);
    long num_ftrs = strtol(argv[arg++], NULL, 0);
    long num_labs = strtol(argv[arg++], NULL, 0);
    int indexed = (int) strtol(argv[arg++], NULL, 0);
    if (rtst_logfile!=NULL)
	debug = 99;

    FILE* file = QN_open(filename, "r");
    QN_InFtrLabStream* instr;
    switch(filetype)
    {
    case 'l':
	rtst_start("LNA8_test");
	if (num_labs!=0)
	    QN_ERROR("LNA8_test", "num_labs must be 0 for LNA test.");
	instr = new QN_InFtrLabStream_LNA8(debug, "lna8",
					   file, num_ftrs, indexed);
	break;
    case 'o':
	rtst_start("OnlFtrs_test");
	if (num_labs!=0)
	    QN_ERROR("OnlFtrs_test", "num_labs must be 0 for OnlFtrs test.");
	instr = new QN_InFtrLabStream_OnlFtr(debug, "onlftr",
					     file, num_ftrs, indexed);
	break;
    case 'p':
	rtst_start("PreFile_test");
	if (num_labs!=1)
	    QN_ERROR("PreFile_test", "num_labs must be 1 for PreFile test.");
	instr = new QN_InFtrLabStream_PreFile(debug, "prefile",
					      file, num_ftrs, indexed);
	break;
    default:
	instr = NULL;
	QN_ERROR(NULL,"Filetype must be one of l,o,p.");
	break;
    }
    test(*instr, num_segs, num_frames, num_ftrs, num_labs, indexed);
    rtst_passed();

    delete instr;
    QN_close(file);
    rtst_exit();
}
