// $Header: /u/drspeech/repos/quicknet2/testsuite/MLPWeightFile_test.cc,v 1.9 2006/03/08 03:42:57 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Testfile for various weight file readers/writers

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "rtst.h"
#include "QuickNet.h"

QN_Logger* QN_logger;
int verbose = 10;

static void
test(const char* wfile_name, const char* tmpfile_name,
     size_t n_input, size_t n_hidden, size_t n_output)
{
    size_t count;		// Count of items read or written

    const size_t n_in2hid = n_input * n_hidden;
    const size_t n_hid2out = n_hidden * n_output;

    // Read in an existing weight file

    FILE* testfile = fopen(wfile_name, "r");
    assert(testfile!=NULL);
    float* weights = rtst_padvec_new_vf(n_in2hid + n_hid2out + n_hidden +
					n_output);
    float* const in2hid_weights = weights;
    float* const hid2out_weights = weights + n_in2hid;
    float* const hid_bias = weights + n_in2hid + n_hid2out;
    float* const out_bias = weights + n_in2hid + n_hid2out + n_hidden;

    QN_MLPWeightFile_RAP3 wfile_ref(verbose, testfile, QN_READ, wfile_name,
				    n_input, n_hidden, n_output);
    // Some consistency checks
    rtst_assert(wfile_ref.size_layer(QN_MLP3_INPUT)==n_input);
    rtst_assert(wfile_ref.size_layer(QN_MLP3_HIDDEN)==n_hidden);
    rtst_assert(wfile_ref.size_layer(QN_MLP3_OUTPUT)==n_output);
    rtst_assert(wfile_ref.get_filemode()==QN_READ);

    rtst_log("Reading weight file\n");
    int i;
    QN_WeightSelector which;
    // Read in the weight file
    for (i=0; i<4; i++)
    {
	which = wfile_ref.get_weighttype(i);
	switch(which)
	{
	case QN_MLP3_INPUT2HIDDEN:
	    count = wfile_ref.read(in2hid_weights, n_in2hid);
	    rtst_assert(count==n_in2hid);
	    break;
	case QN_MLP3_HIDDEN2OUTPUT:
	    count = wfile_ref.read(hid2out_weights, n_hid2out);
	    rtst_assert(count==n_hid2out);
	    break;
	case QN_MLP3_HIDDENBIAS:
	    count = wfile_ref.read(hid_bias, n_hidden);
	    rtst_assert(count==n_hidden);
	    break;
	case QN_MLP3_OUTPUTBIAS:
	    count = wfile_ref.read(out_bias, n_output);
	    rtst_assert(count==n_output);
	    break;
	default:
	    assert(0);
	}
    }

    // Test that reads past the end of file work
    float temp;
    count = wfile_ref.read(&temp, 1);
    rtst_assert(count==0);

// Now write a new weight file - ignore boundaries between weight sections
// to increase our chances of breaking stuff

    size_t len;
    float* cur_weight;

    rtst_log("Writing new weight file\n");
    FILE* outfile = fopen(tmpfile_name, "w");
    assert(outfile!=NULL);
    QN_MLPWeightFile_RAP3 wfile_out(verbose, outfile, QN_WRITE, tmpfile_name,
				    n_input, n_hidden, n_output);
    rtst_assert(wfile_out.get_filemode()==QN_WRITE);
    cur_weight = weights;
    do
    {
	len = rtst_urand_i32i32_i32(0, 10000);
	count = wfile_out.write(cur_weight, len);
	cur_weight += count;
    } while(count!=0);
    assert(fclose(outfile)==0);

// Finally we read in the new weight file

    float* new_weights = rtst_padvec_new_vf(n_in2hid + n_hid2out + n_hidden +
					    n_output);
    rtst_log("Reading new weight file\n");
    FILE* infile = fopen(tmpfile_name, "r");
    assert(infile!=NULL);
    QN_MLPWeightFile_RAP3 wfile_in(verbose, infile, QN_READ, tmpfile_name,
				   n_input, n_hidden, n_output);
    cur_weight = new_weights;
    do
    {
	len = rtst_urand_i32i32_i32(0, 10000);
	count = wfile_in.read(cur_weight, len);
	cur_weight += count;
    } while(count!=0);
    assert(fclose(outfile)==0);
    rtst_checknear_fvfvf(n_in2hid + n_hid2out + n_hidden + n_output, 0.0001,
			 new_weights, weights);
    
    rtst_padvec_del_vf(new_weights);
    rtst_padvec_del_vf(weights);
}



int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);
    assert(arg == argc-5);

    const char* weightfile = argv[arg++];
    const char* tmpfile = argv[arg++];
    size_t input = strtol(argv[arg++], NULL, 0);
    size_t hidden = strtol(argv[arg++], NULL, 0);
    size_t output = strtol(argv[arg++], NULL, 0);
    assert(input>0);
    assert(hidden>0);
    assert(output>0);

    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "MLPWeightFile_test");
    rtst_start("MLPWeightFile_RAP3");
    test(weightfile, tmpfile, input, hidden, output);
    rtst_passed();
}
