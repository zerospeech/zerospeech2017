/* Must include the config.h file first */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "QuickNet.h"

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#endif

// MLP3_perf.cc
//
// This is a test program used to evaluate the performance of QuickNet
// MLPs.  It is not intended for regular use.

enum QN_OutputLayerType OUT_TYPE = QN_OUTPUT_SOFTMAX;


void
fill_vf(size_t len, float* vec, float min, float max)
{
    float range = max - min;

    size_t i;

    for (i=0; i<len; i++)
	(*vec++) = ((float) drand48()) * range + min;
}

static int
dblcompare(const void* p1, const void* p2)
{
    double f1 = *((double*) p1);
    double f2 = *((double*) p2);

    if (f1 > f2)
	return(1);
    else if (f2 > f1)
	return(-1);
    else
	return(0);
}

int
main(int argc, char* const* argv)
{
    size_t n_input = 0;
    size_t n_hidden = 0;
    size_t n_output = 0;
    size_t bunch_size = 512;
    size_t threads = 0;
    size_t bunches = 0;
    size_t repeats = 5;		// Number of repeats

    size_t buf_frames;
    float* input_buf;
    float* output_buf;
    float* target_buf;

    int bunch_mode = 1;		// Whether to use bunch mode
    int forward_pass = 0;	// Whether to do just a forward pass
    int cuda = 0; 		// Whether to do cuda


    // Usage: MLP3_perf [-o] [-f] [-b] [-p] <n_in> <n_hid> <n_out>
    //   [<bunchsize>] [<threads>] [<bunches>] [<repeats>]
    // -o - online
    // -f - forward pass only
    // -b - no blas
    // -p - no pp routines

    int c;
    int error = 0;
    // By default, enable blas and "pp" routines
#ifdef QN_HAVE_LIBBLAS
    qn_math = QN_MATH_BL | QN_MATH_PP;
#else
    qn_math = QN_MATH_PP;
#endif

    while ((c = getopt(argc, argv, "bcfpo")) != -1)
    {
	switch (c) {
	case 'b':
	    qn_math &= ~QN_MATH_BL;
	    break;
	case 'c':
	    cuda = 1;
	    break;
	case 'f':
	    forward_pass = 1;
	    break;
	case 'p':
	    qn_math &= ~QN_MATH_PP;
	    break;
	case 'o':
	    bunch_mode = 0;
	    break;
	case '?':
	    error = 1;
	    break;
	}
    }
    argc -= optind;
    if (error || (argc<3) || (argc>7))
    {
	fprintf(stderr, "usage: MLP3_perf [-b] [-f] [-p] [-o]"
		" in hid out [bunch] [threads] [bunches] [repeats]\n");
	exit(EXIT_FAILURE);
    }
    n_input = strtol(argv[optind], NULL, 0);
    n_hidden = strtol(argv[optind+1], NULL, 0);
    n_output = strtol(argv[optind+2], NULL, 0);
    if (argc > 3)
	bunch_size = strtol(argv[optind+3], NULL, 0);
    if (argc > 4)
	threads = strtol(argv[optind+4], NULL, 0);
    if (argc > 5)
	bunches = strtol(argv[optind+5], NULL, 0);
    if (argc > 6)
	repeats = strtol(argv[optind+6], NULL, 0);

    assert(n_input>0 && n_hidden>0 && n_output>0);
    assert(bunch_size>=1);

    buf_frames = bunch_size;
    input_buf = new float[n_input * buf_frames];
    output_buf = new float[n_output * buf_frames];
    target_buf = new float[n_output * buf_frames];

    // Need to do this in double else overflows for big nets
    double n_conns = (double) (n_input + n_output)
	* (double) n_hidden * (double) buf_frames;
    // Assuming 1 GCUP, work out how many bunches of work takes 10
    // seconds. Use that as the default no of bunches.
    double suggested_bunches = 1e9 * 10.0 / n_conns;
    if (bunches == 0)
	bunches = (int) suggested_bunches;
    if (suggested_bunches < 2)
    {
	printf("WARNING - problem too big to guess number of bunches so "
	       "only running 2 times\n");
	suggested_bunches = 2;
    }
    double* elapsed_time = new double[repeats];

    QN_MLP* net;
    size_t layer_units[QN_MLP_MAX_LAYERS];
    layer_units[0] = n_input;
    layer_units[1] = n_hidden;
    layer_units[2] = n_output;
    if (cuda)
    {
	if (threads!=1 && threads!=0)
	    printf("WARNING - cannot use multiple threads on cuda\n");
	if (bunch_mode==0)
	    printf("WARNING - ignoring online uption and using bunch mode\n");
	// Threaded/bunch floating point.
	net = new QN_MLP_BunchCudaVar(0, "net", 3, layer_units,
				      OUT_TYPE, bunch_size);
    }
    else if (bunch_mode && threads>0)
    {
	// Threaded/bunch floating point.
	net = new QN_MLP_ThreadFlVar(0, "net", 3, layer_units,
				  OUT_TYPE, bunch_size, threads);
    }
    else if (bunch_mode && threads==0)
    {
	// Threaded/bunch floating point.
	net = new QN_MLP_BunchFlVar(0, "net", 3, layer_units,
				    OUT_TYPE, bunch_size);
    }
    else
    {
	// Online floating point.
	net = new QN_MLP_OnlineFl3(0, "net", n_input, n_hidden, n_output,
				   OUT_TYPE);
    }

    net->set_learnrate(QN_MLP3_INPUT2HIDDEN, 0.001);

    net->set_learnrate(QN_MLP3_HIDDEN2OUTPUT, 0.001);
    net->set_learnrate(QN_MLP3_HIDDENBIAS, 0.001);
    net->set_learnrate(QN_MLP3_OUTPUTBIAS, 0.001);
    int seed = 1;
    size_t i, j;

    for (i=0; i< repeats; i++)
    {
	double start_time, end_time;

	fill_vf(n_input*buf_frames, input_buf, 0.0f, 0.1f);
	fill_vf(n_output*buf_frames, target_buf, 0.0f, 0.99f);
	QN_randomize_weights(0, seed, *net,
			     -0.1, 0.1,       // weights
			     -4.1, -3.9);      // biases


	start_time = QN_time();

	for (j=0; j<bunches; j++)
	{
	    if (forward_pass)
	    {
		net->forward(buf_frames, input_buf, output_buf);
	    }
	    else
	    {
		net->train(buf_frames, input_buf, target_buf, output_buf);
	    }
	}
	end_time = QN_time();
	elapsed_time[i] = end_time - start_time;
    }
    // Sort the elapsed_times so we can pick the shortest ones
    qsort(elapsed_time, repeats, sizeof(elapsed_time[0]), dblcompare);
//     for (i=0; i<repeats; i++)
//     {
// 	printf("elapsed_time[%d] = %f\n", i, elapsed_time[i]);
//     }
    double mcps = (double) bunches * n_conns / 1e6/ elapsed_time[0];

    printf("%d %d %d ", n_input, n_hidden, n_output);
    if (bunch_mode)
	printf("bunch %d ", bunch_size);
    else
	printf("online 1 ");
    char* math;
    if (cuda)
    {
	math = "cuda";
    }
    else
    {
	switch(qn_math)
	{
	case 0:
	    math = "basic"; break;
	case QN_MATH_BL:
	    math = "blas"; break;
	case QN_MATH_PP: 
	    math = "pp"; break;
	case QN_MATH_PP|QN_MATH_BL:
	    math = "blas+pp"; break;
	default:
	    assert(0);
	}
    }
    printf("%s ", math);
    printf("threads %d ", threads);
    printf("%.2f ", mcps);
    printf("%s", forward_pass ? "MCPS" : "MCUPS");
    printf("\n");

    delete net;
    delete [] elapsed_time;

    delete [] target_buf;
    delete [] output_buf;
    delete [] input_buf;

    exit(EXIT_SUCCESS);
}
