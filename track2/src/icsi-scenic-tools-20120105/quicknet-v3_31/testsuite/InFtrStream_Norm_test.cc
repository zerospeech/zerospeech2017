// $Header: /u/drspeech/repos/quicknet2/testsuite/InFtrStream_Norm_test.cc,v 1.5 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for QN_InFtrStream_Norm class

#include <assert.h>
#include <stdio.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

static void
test(int debug, const char* pfile_name)
{
    size_t i;
    int ec;

    // Open up the PFile.
    FILE* pfile_file = fopen(pfile_name, "r");
    assert(pfile_file!=NULL);
    QN_InFtrStream* pfile_str =
	new QN_InFtrLabStream_PFile(debug, "pfile", pfile_file, 1);


    // Generate some normalizaton data.
    size_t num_ftrs = pfile_str->num_ftrs();
    float* bias_vec = rtst_padvec_new_vf(num_ftrs);
    float* scale_vec = rtst_padvec_new_vf(num_ftrs);
    for (i=0; i<num_ftrs; i++)
    {
	bias_vec[i] = -0.01 * (float) i;
	scale_vec[i] = 0.01 * (float) i;
    }

    // Open up the PFile again and apply the norm filter.
    FILE* norm_pfile_file = fopen(pfile_name, "r");
    assert(norm_pfile_file!=NULL);
    QN_InFtrStream* norm_pfile_str =
	new QN_InFtrLabStream_PFile(debug, "norm", norm_pfile_file, 1);
    QN_InFtrStream* norm_str =
	new QN_InFtrStream_Norm(debug, "norm", *norm_pfile_str,
				bias_vec, scale_vec);

    // Check various bits and pieces.
    size_t pfile_num_segs = pfile_str->num_segs();
    size_t norm_num_segs = norm_str->num_segs();
    rtst_assert(pfile_num_segs = norm_num_segs);

    size_t pfile_num_frames = pfile_str->num_frames();
    size_t norm_num_frames = norm_str->num_frames();
    rtst_assert(pfile_num_frames==norm_num_frames);

    size_t norm_num_ftrs = norm_str->num_ftrs();
    rtst_assert(norm_num_ftrs==num_ftrs);

    assert(norm_num_segs>=2);
    size_t test_seg = norm_num_segs - 2;
    size_t pfile_test_frames = pfile_str->num_frames(test_seg);
    size_t norm_test_frames = norm_str->num_frames(test_seg);
    rtst_assert(norm_test_frames==pfile_test_frames);
    assert(norm_test_frames>=2);
    size_t test_frame = norm_test_frames - 2;

    // Make sure random access works.
    QN_SegID id = norm_str->set_pos(test_seg, test_frame);
    rtst_assert(id!=QN_SEGID_BAD);
    size_t seg, frame;
    ec = norm_str->get_pos(&seg, &frame);
    rtst_assert(ec==QN_OK);
    rtst_assert(seg==test_seg);
    rtst_assert(frame==test_frame);
    ec = norm_str->rewind();
    rtst_assert(ec==QN_OK);
    
    // Scan through both the PFile stream and the normed stream, ensuring
    // they are consistent.


    while(1)			// Iterate over all sentences.
    {
	QN_SegID p_id, n_id;

	p_id = pfile_str->nextseg();
	n_id = norm_str->nextseg();
	if (n_id==QN_SEGID_BAD)
	    break;
	while(1)		// Iterate over all bunches of frames.
	{
	    size_t num_frames;
	    size_t size_ftrs;
	    size_t p_count, n_count;
	    size_t i;

	    // Read a random number of frames.
	    num_frames = rtst_urand_i32i32_i32(1,200);
	    size_ftrs = num_frames * num_ftrs;
	    float* p_ftrbuf = rtst_padvec_new_vf(size_ftrs);
	    float* n_ftrbuf = rtst_padvec_new_vf(size_ftrs);

	    p_count = pfile_str->read_ftrs(num_frames, p_ftrbuf);
	    n_count = norm_str->read_ftrs(num_frames, n_ftrbuf);
	    rtst_assert(p_count==n_count);
	    for (i=0; i<n_count; i++)
	    {
		rtst_log("Checking line %lu\n", (unsigned long) i);
		qn_add_vfvf_vf(num_ftrs, &p_ftrbuf[num_ftrs*i],
			    bias_vec, &p_ftrbuf[num_ftrs*i]);
		qn_mul_vfvf_vf(num_ftrs, &p_ftrbuf[num_ftrs*i],
			    scale_vec, &p_ftrbuf[num_ftrs*i]);
		rtst_checknear_fvfvf(num_ftrs, 0.0000001,
				     &p_ftrbuf[num_ftrs*i],
				     &n_ftrbuf[num_ftrs*i]);
	    }
	    rtst_padvec_del_vf(n_ftrbuf);
	    rtst_padvec_del_vf(p_ftrbuf);
	    if (n_count<num_frames)
		break;
	}
    }
    delete norm_str;
    delete norm_pfile_str;
    delete pfile_str;
    ec = fclose(norm_pfile_file);
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
				     "InFtrStream_Norm_test");
    if (arg!=argc-1)
    {
	fprintf(stderr, "ERROR - Bad arguments.\n");
	exit(1);
    }

    const char* pfile = argv[arg++];

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start("InFtrStream_Norm_test");
    test(debug, pfile);
    rtst_passed();
}
