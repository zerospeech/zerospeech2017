// $Header: /u/drspeech/repos/quicknet2/testsuite/InFtrStream_Narrow_test.cc,v 1.5 2004/09/16 00:13:50 davidj Exp $
//
// Testfile for QN_InFtrStream_Narrow class

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "QuickNet.h"
#include "rtst.h"

static void
test(int debug, const char* pfile_name, size_t first_ftr, size_t num_ftrs,
     size_t buf_size)
{
    int ec;
    
    // Open up the PFile.
    FILE* pfile_file = fopen(pfile_name, "r");
    assert(pfile_file!=NULL);
    QN_InFtrStream* pfile_str =
	new QN_InFtrLabStream_PFile(debug, "pfile", pfile_file, 1);
    assert(pfile_str->num_ftrs()>=(first_ftr + num_ftrs));

    // Open up the PFile again and apply the narrow filter.
    FILE* narrow_pfile_file = fopen(pfile_name, "r");
    assert(narrow_pfile_file!=NULL);
    QN_InFtrStream* narrow_pfile_str =
	new QN_InFtrLabStream_PFile(debug, "narrow", narrow_pfile_file, 1);
    QN_InFtrStream* narrow_str =
	new QN_InFtrStream_Narrow(debug, "narow", *narrow_pfile_str,
				  first_ftr, num_ftrs,
				  buf_size);

    // Check various bits and pieces.
    size_t pfile_num_segs = pfile_str->num_segs();
    size_t narrow_num_segs = narrow_str->num_segs();
    rtst_assert(pfile_num_segs = narrow_num_segs);

    size_t pfile_num_frames = pfile_str->num_frames();
    size_t narrow_num_frames = narrow_str->num_frames();
    rtst_assert(pfile_num_frames==narrow_num_frames);

    
    size_t pfile_num_ftrs = pfile_str->num_ftrs();
    size_t narrow_num_ftrs = narrow_str->num_ftrs();
    if (num_ftrs!=QN_ALL)
	rtst_assert(narrow_num_ftrs==num_ftrs);

    assert(narrow_num_segs>=2);
    size_t test_seg = narrow_num_segs - 2;
    size_t pfile_test_frames = pfile_str->num_frames(test_seg);
    size_t narrow_test_frames = narrow_str->num_frames(test_seg);
    rtst_assert(narrow_test_frames==pfile_test_frames);
    assert(narrow_test_frames>=2);
    size_t test_frame = narrow_test_frames - 2;

    // Make sure random access works.
    QN_SegID id = narrow_str->set_pos(test_seg, test_frame);
    rtst_assert(id!=QN_SEGID_BAD);
    size_t seg, frame;
    ec = narrow_str->get_pos(&seg, &frame);
    rtst_assert(ec==QN_OK);
    rtst_assert(seg==test_seg);
    rtst_assert(frame==test_frame);
    ec = narrow_str->rewind();
    rtst_assert(ec==QN_OK);
    
    // Scan through both the PFile stream and the narrowed stream, ensuring
    // they are consistent.


    while(1)			// Iterate over all sentences.
    {
	QN_SegID p_id, n_id;

	p_id = pfile_str->nextseg();
	n_id = narrow_str->nextseg();
	rtst_assert(p_id==n_id);
	if (n_id==QN_SEGID_BAD)
	    break;
	while(1)		// Iterate over all bunches of frames.
	{
	    size_t num_frames;
	    size_t p_size_ftrs;
	    size_t n_size_ftrs;
	    size_t p_count, n_count;
	    size_t i;

	    // Read a random number of frames.
	    num_frames = rtst_urand_i32i32_i32(1,200);
	    p_size_ftrs = num_frames * pfile_num_ftrs;
	    n_size_ftrs = num_frames * narrow_num_ftrs;
	    float* p_ftrbuf = rtst_padvec_new_vf(p_size_ftrs);
	    float* n_ftrbuf = rtst_padvec_new_vf(n_size_ftrs);

	    p_count = pfile_str->read_ftrs(num_frames, p_ftrbuf);
	    n_count = narrow_str->read_ftrs(num_frames, n_ftrbuf);
	    rtst_assert(p_count==n_count);
	    for (i=0; i<n_count; i++)
	    {
		rtst_log("Checking line %lu\n", (unsigned long) i);
		rtst_checkeq_vfvf(narrow_num_ftrs,
				  &p_ftrbuf[pfile_num_ftrs*i+first_ftr],
				  &n_ftrbuf[narrow_num_ftrs*i]);
	    }
	    rtst_padvec_del_vf(n_ftrbuf);
	    rtst_padvec_del_vf(p_ftrbuf);
	    if (n_count<num_frames)
		break;
	}
    }
    delete narrow_str;
    delete narrow_pfile_str;
    delete pfile_str;
    ec = fclose(narrow_pfile_file);
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
				     "InFtrStream_Narrow_test");
    if (arg!=argc-4)
    {
	fprintf(stderr, "ERROR - Bad arguments.\n");
	exit(1);
    }
    // Usage: InFtrStream_Narrow_test <pfile> <first_ftrs> <num_ftrs> <bufsize>
    const char* pfile = argv[arg++];
    size_t first_ftr = strtol(argv[arg++], NULL, 0);
    size_t num_ftrs;
    if (strcmp(argv[arg], "a")==0)
	num_ftrs = QN_ALL;
    else
	num_ftrs = strtol(argv[arg], NULL, 0);
    arg++;
    size_t buf_size = strtol(argv[arg++], NULL, 0);
    

    if (rtst_logfile!=NULL)
	debug = 99;
    rtst_start("InFtrStream_Narrow_test");
    test(debug, pfile, first_ftr, num_ftrs, buf_size);
    rtst_passed();
}
