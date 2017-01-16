// $Header: /u/drspeech/repos/quicknet2/testsuite/utils_test.cc,v 1.6 2004/09/16 00:13:50 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Various tests for MLP3 classes

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "QuickNet.h"

#include "rtst.h"


enum
{
    INPUT = 134,
    HIDDEN = 297,		// Note INPUT*HIDDEN>5000 (buffer size)
    OUTPUT = 36
};

int verbose = 10;
int seed = 1;

size_t
max(size_t a, size_t b)
{
    if (a>=b)
	return a;
    else
	return b;
}

void
readwrite_test(QN_MLP& mlp, const char* filename)
{
    size_t num_input = mlp.size_layer(QN_MLP3_INPUT);
    size_t num_hidden = mlp.size_layer(QN_MLP3_HIDDEN);
    size_t num_output = mlp.size_layer(QN_MLP3_OUTPUT);
    // A new MLP, identical to the old one
    QN_MLP_OnlineFl3 nmlp(verbose, "mlp", num_input, num_hidden, num_output,
			  QN_OUTPUT_SOFTMAX);
    float min_weight, max_weight;
    float nmin_weight, nmax_weight;
    QN_MLPWeightFile* wfile;

    // Intialize the weights
    QN_randomize_weights(verbose, seed, mlp, -0.01, 0.01, -3.9, -4.1);

    // Write the weights out to a file
    FILE* outfile = fopen(filename, "w");
    assert(outfile!=NULL);
    wfile = new QN_MLPWeightFile_RAP3(verbose, outfile, QN_WRITE, filename,
				      num_input, num_hidden, num_output);
    QN_write_weights(*wfile, mlp, &min_weight, &max_weight, verbose);
    delete wfile;
    assert(fclose(outfile)==0);

    // Read the weights back into a new MLP
    FILE* infile = fopen(filename, "r");
    assert(infile!=NULL);
    wfile = new QN_MLPWeightFile_RAP3(verbose, infile, QN_READ, filename,
				      num_input, num_hidden, num_output);
    QN_read_weights(*wfile, nmlp, &nmin_weight, &nmax_weight, verbose);
    delete wfile;
    assert(fclose(infile)==0);

    rtst_assert(fabs(nmin_weight-min_weight) < 0.00001);
    rtst_assert(fabs(nmax_weight-max_weight) < 0.00001);

    size_t biggest_layer = max(num_input, num_output);
    float* weights = new float [num_hidden * biggest_layer];
    float* nweights = new float [num_hidden * biggest_layer];

    size_t section;
    for (section=0; section<mlp.num_sections(); section++)
    {
		size_t rows, cols;

		mlp.size_section((QN_WeightSelector) section, &rows, &cols);
		mlp.get_weights((QN_WeightSelector) section,0, 0, rows, cols, weights);
		nmlp.get_weights((QN_WeightSelector) section, 0, 0, rows, cols,
						 nweights);
		rtst_checknear_fvfvf(rows*cols, 0.00001f, weights, nweights);
	}
	delete [] weights;
	delete [] nweights;
}

void
rand_test(QN_MLP& mlp)
{
    size_t i, j;
    size_t num_input = mlp.size_layer(QN_MLP3_INPUT);
    size_t num_hidden = mlp.size_layer(QN_MLP3_HIDDEN);
    size_t num_output = mlp.size_layer(QN_MLP3_OUTPUT);
    size_t biggest_layer = max(num_input, num_output);

    float* weights = new float [biggest_layer * num_hidden];
    qn_copy_f_vf(biggest_layer*num_hidden, -10.0, weights);

    // Intialiaze the weights to a constant
    for (i=0; i<mlp.num_sections(); i++)
    {
	size_t rows, cols;
	mlp.size_section((QN_WeightSelector) i, &rows, &cols);
	mlp.set_weights((QN_WeightSelector) i, 0, 0, rows, cols, weights);
    }

    // Randomize the weights
    QN_randomize_weights(verbose, seed, mlp, -0.1, 0.2, -3.5, -4.5);

    // Check that all the weights are randomized
    for (i=0; i<mlp.num_sections(); i++)
    {
	size_t rows, cols;
	mlp.size_section((QN_WeightSelector) i, &rows, &cols);
	mlp.get_weights((QN_WeightSelector) i, 0, 0, rows, cols, weights);
	for (j=0; j<rows*cols; j++)
	{
	    if (i==QN_MLP3_INPUT2HIDDEN || i==QN_MLP3_HIDDEN2OUTPUT)
	    {
			rtst_assert(weights[j]>=-0.1);
			rtst_assert(weights[j]<=0.2);
	    }
	    else
	    {
			rtst_assert(weights[j]>=-4.5);
			rtst_assert(weights[j]<=-3.5);
	    }
	}
    }
    delete [] weights;
}

void
test(const char* weightfile)
{
    rtst_start("QN_randomize_weights");
    rtst_log("normal layer\n");
    QN_MLP* mlp = new QN_MLP_OnlineFl3(verbose, "mlp", INPUT, HIDDEN, OUTPUT,
				       QN_OUTPUT_SOFTMAX);
    rand_test(*mlp);
    delete mlp;
    // Need to try something with a layer >5000 (the default buffer size)
    rtst_log("big layer\n");
    QN_MLP* mlp2 = new QN_MLP_OnlineFl3(verbose, "mlp", 2, 6000, 3,
					QN_OUTPUT_SOFTMAX);
    rand_test(*mlp2);
    delete mlp2;
    rtst_passed();

    // Need to try something with a layer >5000 (the default buffer size)
    rtst_start("QN_write_weights/QN_read_weights");
    QN_MLP* mlp3 = new QN_MLP_OnlineFl3(verbose, "mlp", 10, 20, 15,
					QN_OUTPUT_SOFTMAX);
    readwrite_test(*mlp3, weightfile);
    delete mlp3;
    QN_MLP* mlp4 = new QN_MLP_OnlineFl3(verbose, "mlp", 100, 101, 1,
					QN_OUTPUT_SOFTMAX);
    readwrite_test(*mlp4, weightfile);
    delete mlp4;
    rtst_passed();
}

// Test weightfile log name generation

void
logfile_template_test()
{
    rtst_start("logfile_template");
    const char *good_tmpl = "/tmp/test%e_%p%%";
    const char *bad_tmpl = "/tmp/test%Z";
    char buf[100];
    rtst_assert(QN_logfile_template_check(good_tmpl)==QN_OK);
    rtst_assert(QN_logfile_template_check(bad_tmpl)==QN_BAD);
    rtst_assert(QN_logfile_template_map(good_tmpl, buf, 100,
					123,4567)==QN_OK);
    rtst_assert(strcmp(buf, "/tmp/test123_4567%")==0);
    rtst_passed();
}

int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);
    assert(arg==argc-1);

    const char* weightfile = argv[arg++];
    assert(arg == argc);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "utils_test");
    test(weightfile);
    logfile_template_test();
    rtst_exit();
}

