// $Header: /u/drspeech/repos/quicknet2/testsuite/PFile_test2.cc,v 1.9 2004/09/16 00:13:50 davidj Exp $
//
// Various tests for QN_PFile classes
// davidj - 5th March 1996

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "QuickNet.h"
#include "rtst.h"

const size_t max_sents = 100;
const size_t max_frames_per_sent = 300;
const size_t min_frames_per_sent = 1;
enum { num_ftrs = 16, num_labs = 2 };

void
create_pfile(int debug, const char* filename, int out_indexed)
{

    size_t i;			// Counter
    FILE* pf;			// The PFile file
    float ftrs[num_ftrs*2];	// A buffer for features (two rows worth)
    QNUInt32 labs[num_labs*2];	// A buffer for labels (two rows worth)

    pf = QN_open(filename, "w");
    QN_OutFtrLabStream_PFile* ostrp =
	new QN_OutFtrLabStream_PFile(debug, "pfile", pf, num_ftrs, num_labs,
				     out_indexed);
    QN_OutFtrLabStream& ostr = *((QN_OutFtrLabStream*)ostrp);

    rtst_assert(ostr.num_ftrs()==num_ftrs);
    rtst_assert(ostr.num_labs()==num_labs);

    size_t num_sents = rtst_urand_i32i32_i32(1, max_sents);
    for (i=0; i<num_sents; i++)
    {
	size_t num_frames;
	size_t j;

	num_frames = rtst_urand_i32i32_i32(min_frames_per_sent,
					   max_frames_per_sent);
	// Put Sentence number and frame number in label data
	labs[0] = i;
	labs[2] = i;
	for (j=0; j<num_frames; j+=2)
	{
	    labs[1] = j;
	    labs[3] = j+1;

	    // Put sentence number + frameno/1000 in feature data
	    qn_copy_f_vf(num_ftrs, (float) i + (float)j/1000.0, &ftrs[0]);
	    qn_copy_f_vf(num_ftrs, (float) i + (float)(j+1)/1000.0,
			 &ftrs[num_ftrs]);
	    if (j==num_frames-1)
		ostr.write_ftrslabs(1, ftrs, labs);
	    else
		ostr.write_ftrslabs(2, ftrs, labs);
	}
	rtst_log("End of sentence %i.\n", i);
	ostr.doneseg(0);
    }

    delete ostrp;
    QN_close(pf);
}

void
check_pfile(int debug, const char* filename)
{
    size_t i;			// Counter
    FILE* pf;			// The PFile file
    float ftrs[num_ftrs];	// A buffer for features
    float correct_ftrs[num_ftrs]; // A buffer for features
    QNUInt32 labs[num_labs];	// A buffer for labels

    pf = QN_open(filename, "r");
    QN_InFtrLabStream_PFile* istrp =
	new QN_InFtrLabStream_PFile(debug, "pfile", pf, 1);
    QN_InFtrLabStream& istr = *((QN_InFtrLabStream*)istrp);

    rtst_assert(istr.num_ftrs()==num_ftrs);
    rtst_assert(istr.num_labs()==num_labs);

    size_t correct_num_sents = rtst_urand_i32i32_i32(1, max_sents);
    size_t num_sents = istr.num_segs();
    rtst_assert(num_sents==correct_num_sents);
    for (i=0; i<num_sents; i++)
    {
	size_t j;
	size_t count;

	istr.nextseg();
	size_t correct_num_frames = rtst_urand_i32i32_i32(min_frames_per_sent,
						   max_frames_per_sent);
	size_t num_frames = istr.num_frames(i);
	rtst_assert(num_frames==correct_num_frames);
	for (j=0; j<num_frames; j++)
	{
	    istr.read_ftrslabs(1, ftrs, labs);
	    rtst_assert(labs[0]==i);
	    rtst_assert(labs[1]==j);

	    qn_copy_f_vf(num_ftrs, (float) i + (float)j/1000.0,
			 &correct_ftrs[0]); 
	    rtst_checkeq_vfvf(num_ftrs, ftrs, correct_ftrs);
	}
	// Check that we cannot read past end of sentence
	count = istr.read_ftrslabs(1, ftrs, labs);
	rtst_assert(count==0);
	rtst_log("End of sentence %i.\n", i);
    }
    rtst_assert(istr.nextseg()==QN_SEGID_BAD);
    delete istrp;
    QN_close(pf);
}

void
PFile_wr_test(int debug, const char* filename, int out_indexed)
{
    unsigned short seed[3];

    rtst_start("PFile_wr_test");
    // Save the random number state, so we can restore it later
    seed[0] = rtst_state48[0];
    seed[1] = rtst_state48[1];
    seed[2] = rtst_state48[2];
    create_pfile(debug, filename, out_indexed);

    // Restore the random number state
    rtst_state48[0] = seed[0];
    rtst_state48[1] = seed[1];
    rtst_state48[2] = seed[2];
    check_pfile(debug, filename);

    rtst_passed();
}

int
main(int argc, char* argv[])
{
    int arg;
    int debug = 0;		// Verbosity level

    arg = rtst_args(argc, argv);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "PFile_test");

    assert(arg==argc-2);
    // Usage: <pfile> <out_indexed>
    const char* pfile = argv[arg++];
    int out_indexed = (int) strtol(argv[arg++], NULL, 0);
    if (rtst_logfile!=NULL)
	debug = 99;
    PFile_wr_test(debug, pfile, out_indexed);

    rtst_exit();
}
