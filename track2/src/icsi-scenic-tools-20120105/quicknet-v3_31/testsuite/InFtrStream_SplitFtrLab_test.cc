// $Header: /u/drspeech/repos/quicknet2/testsuite/InFtrStream_SplitFtrLab_test.cc,v 1.2 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for QN_InFtrStream_Narrow class

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

static void
test(int debug, const char* pfile_name, size_t buf_size)
{
    int ec;
    
    // Open up the PFile.
    FILE* pfile_file = fopen(pfile_name, "r");
    assert(pfile_file!=NULL);
    QN_InFtrLabStream* pfile_str =
	new QN_InFtrLabStream_PFile(debug, "pfile", pfile_file, 1);

    // Open up the PFile again and split it.
    FILE* split_pfile_file = fopen(pfile_name, "r");
    assert(split_pfile_file!=NULL);
    QN_InFtrLabStream* split_pfile_str =
	new QN_InFtrLabStream_PFile(debug, "split", split_pfile_file, 1);
    QN_InFtrStream_SplitFtrLab* split_ftrlab_str =
	new QN_InFtrStream_SplitFtrLab(debug, "split", *split_pfile_str,
				       buf_size);
    QN_InFtrStream* split_ftr_str = split_ftrlab_str;
    QN_InLabStream* split_lab_str =
	new QN_InLabStream_SplitFtrLab(*split_ftrlab_str);
				       
    // Check various bits and pieces.
    size_t pfile_num_segs = pfile_str->num_segs();
    size_t ftr_num_segs = split_ftr_str->num_segs();
    size_t lab_num_segs = split_lab_str->num_segs();
    rtst_assert(pfile_num_segs == ftr_num_segs);
    rtst_assert(pfile_num_segs == lab_num_segs);

    size_t pfile_num_frames = pfile_str->num_frames();
    size_t ftr_num_frames = split_ftr_str->num_frames();
    size_t lab_num_frames = split_lab_str->num_frames();
    rtst_assert(pfile_num_frames == ftr_num_frames);
    rtst_assert(pfile_num_frames == lab_num_frames);

    size_t pfile_num_ftrs = pfile_str->num_ftrs();
    size_t ftr_num_ftrs = split_ftr_str->num_ftrs();
    rtst_assert(pfile_num_ftrs==ftr_num_ftrs);
    size_t pfile_num_labs = pfile_str->num_labs();
    size_t lab_num_labs = split_lab_str->num_labs();
    rtst_assert(pfile_num_labs==lab_num_labs);

    size_t seg;
    size_t pfile_seg_frames, ftr_seg_frames, lab_seg_frames, max_seg_frames;
    max_seg_frames = 0;
    for (seg=0; seg<pfile_num_segs; seg++)
    {
	pfile_seg_frames = pfile_str->num_frames(seg);
	if (pfile_seg_frames>max_seg_frames)
	    max_seg_frames = pfile_seg_frames;
	ftr_seg_frames = split_ftr_str->num_frames(seg);
	lab_seg_frames = split_lab_str->num_frames(seg);
	rtst_assert(pfile_seg_frames==lab_seg_frames);
	rtst_assert(pfile_seg_frames==ftr_seg_frames);
    }
    rtst_log("Max frames in seg=%lu.\n", (unsigned long) max_seg_frames);

    // Make sure random access works.
    QN_SegID segid;
    size_t test_ftr_seg = pfile_num_segs - 2;
    size_t test_lab_seg = 1;
    assert(test_ftr_seg!=test_lab_seg);
    size_t test_ftr_frame = pfile_str->num_frames(test_ftr_seg) - 3;
    assert(test_ftr_frame<split_ftr_str->num_frames(test_ftr_seg));
    size_t test_lab_frame = 3;
    assert(test_lab_frame<split_lab_str->num_frames(test_lab_seg));
    // Test that streams can be positioned independently.
    segid = split_ftr_str->set_pos(test_ftr_seg, test_ftr_frame);
    rtst_assert(segid!=QN_SEGID_BAD);
    segid = split_lab_str->set_pos(test_lab_seg, test_lab_frame);
    rtst_assert(segid!=QN_SEGID_BAD);
    size_t ftr_seg, ftr_frame, lab_seg, lab_frame;
    ec = split_ftr_str->get_pos(&ftr_seg, &ftr_frame);
    rtst_assert(ec==QN_OK);
    ec = split_lab_str->get_pos(&lab_seg, &lab_frame);
    rtst_assert(ec==QN_OK);
    rtst_assert(test_ftr_seg==ftr_seg);
    rtst_assert(test_ftr_frame==ftr_frame);
    rtst_assert(test_lab_seg==lab_seg);
    rtst_assert(test_lab_frame==lab_frame);
    // Test that rewinding one stream does not break the other.
    ec = split_ftr_str->rewind();
    rtst_assert(ec==QN_OK);
    ec = split_lab_str->get_pos(&lab_seg, &lab_frame);
    rtst_assert(ec==QN_OK);
    rtst_assert(test_lab_seg==lab_seg);
    rtst_assert(test_lab_frame==lab_frame);
    // Rewind the label stream.
    ec = split_lab_str->rewind();
    rtst_assert(ec==QN_OK);

    // Read the whole stream, randomly alternating between ftr and lab reads.
    seg = 0;
    while(1)
    {
	QN_SegID ftr_id, lab_id, pfile_id;

	ftr_id = split_ftr_str->nextseg();
	lab_id = split_lab_str->nextseg();
	pfile_id = pfile_str->nextseg();
	rtst_assert(ftr_id==pfile_id);
	rtst_assert(lab_id==pfile_id);
	if (pfile_id==QN_SEGID_BAD)
	    break;

	size_t ftr_count, lab_count, pfile_count;
	size_t ftr_frames = 0;
	size_t lab_frames = 0;
	size_t pfile_frames = 0;
	int lab_eos = 0;
	int ftr_eos = 0;
	pfile_frames = pfile_str->num_frames(seg);
	float* pfile_ftr_buf = rtst_padvec_new_vf(pfile_num_ftrs*pfile_frames);
	QNUInt32* pfile_lab_buf =
	    (QNUInt32*) rtst_padvec_new_vi32(pfile_num_labs*pfile_frames);
	float* ftr_buf = rtst_padvec_new_vf(pfile_num_ftrs*pfile_frames);
	QNUInt32* lab_buf =
	    (QNUInt32*) rtst_padvec_new_vi32(pfile_num_labs*pfile_frames);

	// Read in segment from lbl and ftr streams in a "random" way.
	do
	{
	    int which;
	    size_t count;

	    which = rtst_urand_i32i32_i32(0, 1);
	    count = rtst_urand_i32i32_i32(0, 100);

	    if (which==0)
	    {
		// Read features.
		rtst_log("Reading %lu features.\n", (unsigned long) count);
		float* ftr_ptr = &ftr_buf[ftr_frames*pfile_num_ftrs];
		ftr_count = split_ftr_str->read_ftrs(count, ftr_ptr);
		ftr_eos = (ftr_count<count);
		ftr_frames += ftr_count;
	    }
	    else
	    {
		// Read labels.
		rtst_log("Reading %lu labels.\n", (unsigned long) count);
		QNUInt32* lab_ptr = &lab_buf[lab_frames*pfile_num_labs];
		lab_count = split_lab_str->read_labs(count, lab_ptr);
		lab_eos = (lab_count<count);
		lab_frames += lab_count;
	    }
	} while(!(lab_eos && ftr_eos));		// While in current segment.

	// Read in segment from pfile for comparison.
	pfile_count = pfile_str->read_ftrslabs(pfile_frames, pfile_ftr_buf,
					       pfile_lab_buf);
	assert(pfile_count==pfile_frames);

	// Check segments are the same size.
	rtst_assert(pfile_frames==lab_frames);
	rtst_assert(pfile_frames==ftr_frames);

	// Compare pfile data with separate ftr and lbl data.
	rtst_checkeq_vfvf(ftr_frames*pfile_num_ftrs, pfile_ftr_buf, ftr_buf);
	rtst_checkeq_vi32vi32(lab_frames*pfile_num_labs,
			      (rtst_int32*) pfile_lab_buf,
			      (rtst_int32*) lab_buf);

	rtst_padvec_del_vi32(lab_buf);
	rtst_padvec_del_vf(ftr_buf);
	rtst_padvec_del_vi32(pfile_lab_buf);
	rtst_padvec_del_vf(pfile_ftr_buf);

	seg++;
    }

    delete split_lab_str;
    delete split_ftr_str;
    delete split_pfile_str;
    delete pfile_str;
    ec = fclose(split_pfile_file);
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
				     "InFtrStream_SplitFtrLab_test");
    assert(arg == argc-2);
    // Usage: InFtrStream_Narrow_test <pfile> <bufsize>
    const char* pfile = argv[arg++];
    size_t buf_size = strtol(argv[arg++], NULL, 0);

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start("InFtrStream_SplitFtrLab_test");
    test(debug, pfile, buf_size);
    rtst_passed();
}
