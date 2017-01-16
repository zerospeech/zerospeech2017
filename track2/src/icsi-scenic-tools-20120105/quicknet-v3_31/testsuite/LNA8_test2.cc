// $Header: /u/drspeech/repos/quicknet2/testsuite/LNA8_test2.cc,v 1.5 2004/09/16 00:13:50 davidj Exp $
//
// Various tests for LNA8 classes
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
enum { num_ftrs = 16 };

// A function to convert a segment and frame number to a double
// feature value between 0 and 1.

double
val(size_t seg, size_t frame)
{
    double f;
    double segf = (double) seg;
    double framef = (double) frame;
    
    f = (segf / ((double) max_sents)) * 0.3
	+ (framef / ((double) max_frames_per_sent)) * 0.7;

    return f;
}

void
create_lnafile(int debug, const char* filename)
{

    size_t i;			// Counter
    FILE* lf;			// The LNA8 file
    float ftrs[num_ftrs*2];	// A buffer for features (two rows worth)

    lf = QN_open(filename, "w");
    QN_OutFtrLabStream_LNA8* ostrp =
	new QN_OutFtrLabStream_LNA8(debug, "lnafile", lf, num_ftrs);
    QN_OutFtrStream& ostr = *((QN_OutFtrStream*)ostrp);

    rtst_assert(ostr.num_ftrs()==num_ftrs);

    size_t num_sents = rtst_urand_i32i32_i32(1, max_sents);
    for (i=0; i<num_sents; i++)
    {
	size_t num_frames;
	size_t j;

	num_frames = rtst_urand_i32i32_i32(min_frames_per_sent,
					   max_frames_per_sent);
	rtst_log("Writing %lu frames in sentence %lu.\n",
		 (unsigned long) num_frames,
		 (unsigned long) i);
	for (j=0; j<num_frames; j+=2)
	{
	    // Put sentence number + frameno/1000 in feature data
	    qn_copy_f_vf(num_ftrs, val(i,j), &ftrs[0]);
	    qn_copy_f_vf(num_ftrs, val(i,j+1),  &ftrs[num_ftrs]);
	    if (j==num_frames-1)
		ostr.write_ftrs(1, ftrs);
	    else
		ostr.write_ftrs(2, ftrs);
	}
	rtst_log("End of sentence %i.\n", i);
	ostr.doneseg(0);
    }

    delete ostrp;
    QN_close(lf);
}

void
check_lnafile(int debug, const char* filename)
{
    size_t i;			// Counter
    FILE* lf;			// The LNA8 file
    float ftrs[num_ftrs];	// A buffer for features
    float correct_ftrs[num_ftrs]; // A buffer for features

    lf = QN_open(filename, "r");
    QN_InFtrLabStream_LNA8* istrp =
	new QN_InFtrLabStream_LNA8(debug, "lnafile", lf, num_ftrs, 1);
    QN_InFtrStream& istr = *((QN_InFtrStream*)istrp);

    rtst_assert(istr.num_ftrs()==num_ftrs);

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
	rtst_log("Found %lu frames in sentence %lu.\n",
		 (unsigned long) num_frames,
		 (unsigned long) i);
	rtst_assert(num_frames==correct_num_frames);
	for (j=0; j<num_frames; j++)
	{
	    istr.read_ftrs(1, ftrs);

	    qn_copy_f_vf(num_ftrs, val(i, j), &correct_ftrs[0]);
	    // Due to LNA format storing log values, we need a bigger
	    // tolerance than the 1/512 that you would expect with 8 bits.
	    rtst_checknear_fvfvf(num_ftrs, 0.012, ftrs, correct_ftrs);
	}
	// Check that we cannot read past end of sentence
	count = istr.read_ftrs(1, ftrs);
	rtst_assert(count==0);
	rtst_log("End of sentence %i.\n", i);
    }
    rtst_assert(istr.nextseg()==QN_SEGID_BAD);
    delete istrp;
    QN_close(lf);
}

void
LNA8_wr_test(int debug, const char* filename)
{
    unsigned short seed[3];

    rtst_start("LNA8_wr_test");
    // Save the random number state, so we can restore it later
    seed[0] = rtst_state48[0];
    seed[1] = rtst_state48[1];
    seed[2] = rtst_state48[2];
    create_lnafile(debug, filename);

    // Restore the random number state
    rtst_state48[0] = seed[0];
    rtst_state48[1] = seed[1];
    rtst_state48[2] = seed[2];
    check_lnafile(debug, filename);

    rtst_passed();
}

int
main(int argc, char* argv[])
{
    int arg;
    int debug = 0;		// Verbosity level

    arg = rtst_args(argc, argv);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "LNA8_test");

    assert(arg==argc-1);
    // Usage: LNA8_test2 <lnafile>
    const char* lnafile = argv[arg++];
    if (rtst_logfile!=NULL)
	debug = 99;
    LNA8_wr_test(debug, lnafile);

    rtst_exit();
}
