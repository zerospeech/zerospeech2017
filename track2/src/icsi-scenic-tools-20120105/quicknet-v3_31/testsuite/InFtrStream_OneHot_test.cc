// $Header: /u/drspeech/repos/quicknet2/testsuite/InFtrStream_OneHot_test.cc,v 1.3 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for QN_InFtrStream_OneHot class

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

static void
test(int debug, const char* pfile_name, size_t size, size_t buf_size)
{
    int ec;
    const float high = 1.0f;
    const float low = 0.0f;
    
    // Open up the PFile.
    FILE* pfile_file = fopen(pfile_name, "r");
    assert(pfile_file!=NULL);
    QN_InLabStream* pfile_str =
	new QN_InFtrLabStream_PFile(debug, "pfile", pfile_file, 1);
    assert(pfile_str->num_labs()==1);

    // Open up the PFile again and make the OneHot stream.
    FILE* onehot_pfile_file = fopen(pfile_name, "r");
    assert(onehot_pfile_file!=NULL);
    QN_InLabStream* onehot_pfile_str =
	new QN_InFtrLabStream_PFile(debug, "onehot", onehot_pfile_file, 1);
    QN_InFtrStream* onehot_str =
	new QN_InFtrStream_OneHot(debug, "onehot", *onehot_pfile_str,
				  size, buf_size, high, low);

    // Check various bits and pieces.
    size_t pfile_num_segs = pfile_str->num_segs();
    size_t onehot_num_segs = onehot_str->num_segs();
    rtst_assert(pfile_num_segs = onehot_num_segs);

    size_t pfile_num_frames = pfile_str->num_frames();
    size_t onehot_num_frames = onehot_str->num_frames();
    rtst_assert(pfile_num_frames==onehot_num_frames);

    
    size_t onehot_num_ftrs = onehot_str->num_ftrs();
    rtst_assert(onehot_num_ftrs==size);

    assert(onehot_num_segs>=2);
    size_t test_seg = onehot_num_segs - 2;
    size_t pfile_test_frames = pfile_str->num_frames(test_seg);
    size_t onehot_test_frames = onehot_str->num_frames(test_seg);
    rtst_assert(onehot_test_frames==pfile_test_frames);
    assert(onehot_test_frames>=2);
    size_t test_frame = onehot_test_frames - 2;

    // Make sure random access works.
    QN_SegID id = onehot_str->set_pos(test_seg, test_frame);
    rtst_assert(id!=QN_SEGID_BAD);
    size_t seg, frame;
    ec = onehot_str->get_pos(&seg, &frame);
    rtst_assert(ec==QN_OK);
    rtst_assert(seg==test_seg);
    rtst_assert(frame==test_frame);
    ec = onehot_str->rewind();
    rtst_assert(ec==QN_OK);

    // Scan through both the PFile stream and the onehot stream, ensuring
    // they are consistent.
    while(1)			// Iterate over all sentences.
    {
	QN_SegID p_id, n_id;

	p_id = pfile_str->nextseg();
	n_id = onehot_str->nextseg();
	rtst_assert(p_id==n_id);
	if (n_id==QN_SEGID_BAD)
	    break;
	while(1)		// Iterate over all bunches of frames.
	{
	    size_t num_frames;
	    size_t size_ftrs;
	    size_t size_labs;
	    size_t p_count, n_count;
	    size_t i;
	    QNUInt32* labbuf;
	    float* ftrbuf;
	    float* tmpbuf;

	    // Read a random number of frames.
	    num_frames = rtst_urand_i32i32_i32(1,200);
	    size_labs = num_frames;
	    size_ftrs = num_frames * onehot_num_ftrs;
	    labbuf  = (QNUInt32*) rtst_padvec_new_vi32(size_labs);
	    ftrbuf  = rtst_padvec_new_vf(size_ftrs);
	    tmpbuf = rtst_padvec_new_vf(onehot_num_ftrs);

	    p_count = pfile_str->read_labs(num_frames, labbuf);
	    n_count = onehot_str->read_ftrs(num_frames, ftrbuf);
	    rtst_assert(p_count==n_count);
	    for (i=0; i<n_count; i++)
	    {
		qn_copy_f_vf(onehot_num_ftrs, low, tmpbuf);
		rtst_assert(labbuf[i]<onehot_num_ftrs);
		tmpbuf[labbuf[i]] = high;
		rtst_log("Checking line %lu\n", (unsigned long) i);
		rtst_checkeq_vfvf(onehot_num_ftrs,
				  &ftrbuf[onehot_num_ftrs*i],
				  tmpbuf);
	    }
	    rtst_padvec_del_vf(tmpbuf);
	    rtst_padvec_del_vf(ftrbuf);
	    rtst_padvec_del_vf(labbuf);
	    if (n_count<num_frames)
		break;
	}
    }

    delete onehot_str;
    delete onehot_pfile_str;
    delete pfile_str;
    ec = fclose(onehot_pfile_file);
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
				     "InFtrStream_OneHot_test");
    if (arg!=argc-3)
    {
	fprintf(stderr, "ERROR - Bad arguments.\n");
	exit(1);
    }
    // Usage: InFtrStream_OneHot_test <pfile> <ftr_size> <buf_size>
    const char* pfile = argv[arg++];
    size_t size = strtol(argv[arg++], NULL, 0);
    size_t buf_size = strtol(argv[arg++], NULL, 0);
    

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start("InFtrStream_OneHot_test");
    test(debug, pfile, size, buf_size);
    rtst_passed();
}
