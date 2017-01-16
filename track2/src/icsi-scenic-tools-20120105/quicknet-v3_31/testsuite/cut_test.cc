// $Header: /u/drspeech/repos/quicknet2/testsuite/cut_test.cc,v 1.3 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for QN_InFtrStream_SplitFtrLab and InLabStream_SplitFtrLab

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

#define TEST_NAME "cut_test"


// The details of the specific cuts.
enum { CUT1_START = 2, CUT1_LEN = 8, CUT2_START = 3, CUT2_LEN = QN_ALL };

// Routine to check that a stream is at a given position.
void
check_str(QN_InFtrStream* str, size_t a_segno, size_t a_frameno)
{
    int ec;
    size_t segno, frameno;

    ec = str->get_pos(&segno, &frameno);
    rtst_assert(ec==QN_OK);
    rtst_assert(segno==a_segno);
    rtst_assert(frameno==a_frameno);
}

// Perform a random operation on a stream.

void
random_streamop(QN_InFtrStream* str)
{
    int op = (int) rtst_urand_i32i32_i32(0, 10);

    switch(op)
    {
    case 0:
	str->rewind();
	break;
    case 1:
    {
	size_t segno = (size_t) rtst_urand_i32i32_i32(0, str->num_segs()-1);
	size_t frameno = (size_t) str->num_frames(segno);
	QN_SegID segid = str->set_pos(segno, frameno);
	rtst_assert(segid!=QN_SEGID_BAD);
	break;
    }
    case 2:
	str->nextseg();
	break;
    case 3:
    {
	size_t count = (size_t) rtst_urand_i32i32_i32(0, 200);
	str->read_ftrs(count, NULL);
	break;
    }
    default:
	// Do nothing.
	break;
    }
}

static void
test(int debug, const char* pfile_name)
{
    // Open up the PFile.
    FILE* pfile_file = fopen(pfile_name, "r");
    assert(pfile_file!=NULL);
    QN_InFtrStream* pfile_str =
	new QN_InFtrLabStream_PFile(debug, "pfile", pfile_file, 1);

    // Open up the PFile again and apply the cut operator.
    FILE* cut_pfile_file = fopen(pfile_name, "r");
    assert(cut_pfile_file!=NULL);
    QN_InFtrStream* cut_pfile_str =
	new QN_InFtrLabStream_PFile(debug, "cut", cut_pfile_file, 1);
    size_t pfile_num_segs = cut_pfile_str->num_segs();
    assert(pfile_num_segs>CUT1_START);
    assert((CUT1_LEN!=QN_ALL) ? CUT1_START+CUT1_LEN<=pfile_num_segs : 1);
    assert(pfile_num_segs>CUT2_START);
    assert((CUT2_LEN!=QN_ALL) ? CUT2_START+CUT2_LEN<=pfile_num_segs : 1);

    QN_InFtrStream_Cut* cut_str =
	new QN_InFtrStream_Cut(debug, "cut", *cut_pfile_str, CUT1_START,
			       CUT1_LEN, CUT2_START, CUT2_LEN);
    QN_InFtrStream* cut1_str = cut_str;
    QN_InFtrStream* cut2_str =
	new QN_InFtrStream_Cut2(*cut_str);

    // Check number of features.
    {
	size_t cut1_num_ftrs = cut1_str->num_ftrs();
	size_t cut2_num_ftrs = cut2_str->num_ftrs();
	size_t pfile_num_ftrs = pfile_str->num_ftrs();
	rtst_assert(cut1_num_ftrs==pfile_num_ftrs);
	rtst_assert(cut2_num_ftrs==pfile_num_ftrs);
    }

    // A check of the number of segments.
    {
	size_t cut1_num_segs = cut1_str->num_segs();
	size_t cut2_num_segs = cut2_str->num_segs();
	size_t pfile_num_segs = pfile_str->num_segs();
	if (CUT1_LEN==QN_ALL)
	{
	    // Note - some versions of "rtst" are broken - need to put
	    // this line in its own block.
	    rtst_assert((cut1_num_segs == pfile_num_segs - CUT1_START));
	}
	else
	{
	    rtst_assert(cut1_num_segs == CUT1_LEN);
	}
	if (CUT2_LEN==QN_ALL)
	{
	    rtst_assert((cut2_num_segs == pfile_num_segs - CUT2_START));
	}
	else
	{
	    rtst_assert(cut2_num_segs == CUT2_LEN);
	}
    }

    // A crude check of the total number of frames.
    {
	size_t cut1_num_frames = cut1_str->num_frames();
	size_t cut2_num_frames = cut2_str->num_frames();
	size_t pfile_num_frames = pfile_str->num_frames();
	size_t cut1_num_segs = cut1_str->num_segs();
	size_t cut2_num_segs = cut2_str->num_segs();
	rtst_assert(cut1_num_frames<=pfile_num_frames);
	rtst_assert(cut2_num_frames<=pfile_num_frames);
	rtst_assert(cut1_num_frames>=cut1_num_segs);
	rtst_assert(cut2_num_frames>=cut2_num_segs);
    }

    // Check the number of frames in specific sentences.
    {
	size_t cut1_sent0_num_frames = cut1_str->num_frames(0);
	size_t cut2_sent0_num_frames = cut2_str->num_frames(0);
	rtst_assert(cut1_sent0_num_frames==pfile_str->num_frames(CUT1_START));
	rtst_assert(cut2_sent0_num_frames==pfile_str->num_frames(CUT2_START));
    }

    // Check seeking around.
    {
	enum {CUT1_SEG = 1, CUT1_FRAME = 5, CUT2_SEG = 2, CUT2_FRAME = 6 };
	assert(cut1_str->num_segs()>CUT1_SEG+1);
	assert(cut1_str->num_frames(CUT1_SEG)>CUT1_FRAME+2);
	assert(cut2_str->num_segs()>CUT2_SEG+1);
	assert(cut2_str->num_frames(CUT2_SEG)>CUT2_FRAME+2);
	QN_SegID segid1, segid2;
	segid1 = cut1_str->set_pos(CUT1_SEG, CUT1_FRAME);
	segid2 = cut2_str->set_pos(CUT2_SEG, CUT2_FRAME);
	rtst_assert(segid1!=QN_SEGID_BAD);
	rtst_assert(segid2!=QN_SEGID_BAD);
	check_str(cut1_str, CUT1_SEG, CUT1_FRAME);
	check_str(cut2_str, CUT2_SEG, CUT2_FRAME);

	// Move cut2 forward two frames, checking
	// things are still okay.
	size_t count;
	count = cut2_str->read_ftrs(2, NULL);
	rtst_assert(count==2);
	check_str(cut1_str, CUT1_SEG, CUT1_FRAME);
	check_str(cut2_str, CUT2_SEG, CUT2_FRAME+2);
	
	// Move cut1 forward two frames, checking
	// things are still okay.
	segid1 = cut1_str->nextseg();
	rtst_assert(segid1!=QN_SEGID_BAD);
	check_str(cut1_str, CUT1_SEG+1, 0);
	check_str(cut2_str, CUT2_SEG, CUT2_FRAME+2);

	// Check rewinding each stream.
	int ec1, ec2;
	ec2 = cut2_str->rewind();
	rtst_assert(ec2==QN_OK);
	check_str(cut1_str, CUT1_SEG+1, 0);
	check_str(cut2_str, QN_SIZET_BAD, QN_SIZET_BAD);
	ec1 = cut1_str->rewind();
	rtst_assert(ec1==QN_OK);
	check_str(cut1_str, QN_SIZET_BAD, QN_SIZET_BAD);
	check_str(cut2_str, QN_SIZET_BAD, QN_SIZET_BAD);
    }

    // Check scanning the whole of one cut stream with random stuff
    // happening on other.
    size_t segno;		// Current segment number;
    size_t n_ftrs = cut1_str->num_ftrs();
    QN_SegID pfile_segid;
    pfile_segid = pfile_str->set_pos(CUT1_START-1, 0);
    assert(pfile_segid!=QN_SEGID_BAD);
    segno = 0;
    while(1)			// Iterate over segments.
    {
	QN_SegID pfile_id, cut1_id;

	// Move on to next sentence.
	pfile_id = pfile_str->nextseg();
	cut1_id = cut1_str->nextseg();
	if (cut1_id==QN_SEGID_BAD)
	{
	    rtst_assert(segno==CUT1_LEN);
	    break;
	}
	else
	{
	    rtst_assert(cut1_id==pfile_id);
	}

	// Read a sentence and check it is the same as from the PFile.
	size_t n_frames;
	float *pfile_buf, *cut1_buf;
	float *pfile_ptr, *cut1_ptr;
	n_frames = cut1_str->num_frames(segno);
	pfile_buf = rtst_padvec_new_vf(n_ftrs * n_frames);
	pfile_ptr = pfile_buf;
	cut1_buf = rtst_padvec_new_vf(n_ftrs * n_frames);
	cut1_ptr = cut1_buf;
	const size_t bunch_size = 13; // Unlucky for some!
	while(1)
	{
	    size_t pfile_count;
	    size_t cut1_count;

	    pfile_count = pfile_str->read_ftrs(bunch_size, pfile_ptr);
	    cut1_count = cut1_str->read_ftrs(bunch_size, cut1_ptr);
	    rtst_assert(pfile_count==cut1_count);
	    pfile_ptr += pfile_count*n_ftrs;
	    cut1_ptr += cut1_count*n_ftrs;
	    if (pfile_count < bunch_size)
		break;
	    random_streamop(cut2_str);
	}
	rtst_checkeq_vfvf(n_frames * n_ftrs, pfile_buf, cut1_buf);
	rtst_padvec_del_vf(cut1_buf);
	rtst_padvec_del_vf(pfile_buf);

	// Do something random on the other stream to try and break things.
	random_streamop(cut2_str);
	segno++;
    }
	

// ......................

    delete cut2_str;
    delete cut1_str;
    delete cut_pfile_str;
    delete pfile_str;
    int ec;
    ec = fclose(cut_pfile_file);
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
				     TEST_NAME);
    if (arg!=argc-1)
    {
	fprintf(stderr, "ERROR - Bad arguments.\n");
	exit(1);
    }
    // Usage: cut_test <pfile>
    const char* pfile = argv[arg++];

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start(TEST_NAME);
    test(debug, pfile);
    rtst_passed();
}
