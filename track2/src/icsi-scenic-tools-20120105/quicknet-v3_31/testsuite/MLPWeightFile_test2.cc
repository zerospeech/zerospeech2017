// $Header: /u/drspeech/repos/quicknet2/testsuite/MLPWeightFile_test2.cc,v 1.5 2006/02/03 23:55:17 davidj Exp $
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
test_write(const char* wfile_name, size_t layers, size_t *units)
{
    size_t count;		// Count of items read or written
    size_t i, j, k;
    float *weights[QN_MLP_MAX_LAYERS-1];
    float *biases[QN_MLP_MAX_LAYERS-1];
    float *w, *b;
    size_t inputs, outputs;
    int ec;

    // Generate same weight and bias matrices.
    for (i = 0; i<layers-1; i++)
    {
	inputs = units[i];
	outputs = units[i+1];
	w = rtst_padvec_new_vf(inputs*outputs);
	weights[i] = w;
	b = rtst_padvec_new_vf(outputs);
	biases[i] = b;
	float jf, kf;
	for (j=0; j<inputs; j++)
	{
	    jf = (float) j;
	    for (k=0; k<outputs; k++)
	    {
		kf = ((float) k)/1000.0 +(float) j;
		*w++ = kf;
	    }
	}
	for (k=0; k<outputs; k++)
	{
	    kf = ((float) k)/1000.0;
	    *b++ = kf;
	}
    }
    // write a weight file

    FILE* wfile = QN_open(wfile_name, "w");
    rtst_assert(wfile!=NULL);

    QN_MLPWeightFile *wf = new QN_MLPWeightFile_Matlab(verbose, wfile_name,
						       wfile,
						       QN_WRITE,
						       layers, units);
   for (i=0; i<layers; i++)
	rtst_assert(wf->size_layer((QN_LayerSelector) i)==units[i]);
    for (i =0; i < (layers-1); i++)
    {
	wf->write(weights[i], units[i]*units[i+1]);
	wf->write(biases[i], units[i+1]);
    }
    rtst_assert(wf->get_filemode()==QN_WRITE);

    

    delete wf;
    QN_close(wfile);

    rtst_log("Writing weight file\n");
    rtst_log("Writing weight file done\n");

    for (i=0; i<layers-1; i++)
    {
	rtst_padvec_del_vf(weights[i]);
	rtst_padvec_del_vf(biases[i]);
    }
}

static void
test_read(const char* wfile_name, size_t layers, size_t *units)
{
    size_t count;		// Count of items read or written
    size_t i, j, k;
    float *weights[QN_MLP_MAX_LAYERS-1];
    float *biases[QN_MLP_MAX_LAYERS-1];
    float *w, *b;
    size_t inputs, outputs;
    size_t sections;
    int ec;

    // Generate same weight and bias matrices.
    for (i = 0; i<layers-1; i++)
    {
	inputs = units[i];
	outputs = units[i+1];
	rtst_assert(inputs!=0 && outputs!=0);
	w = rtst_padvec_new_vf(inputs*outputs);
	weights[i] = w;
	b = rtst_padvec_new_vf(outputs);
	biases[i] = b;
    }
    // read a weight file.

    rtst_log("Reading weight file\n");

    FILE* wfile = QN_open(wfile_name, "r");
    rtst_assert(wfile!=NULL);
    FILE* wfile2 = QN_open(wfile_name, "r");
    rtst_assert(wfile2!=NULL);

    QN_MLPWeightFile *wf = new QN_MLPWeightFile_Matlab(verbose, wfile_name,
						       wfile,
						       QN_READ,
						       layers, units);
    QN_MLPWeightFile *wf2 = new QN_MLPWeightFile_Matlab(verbose, wfile_name,
							wfile2,
							QN_READ,0, NULL);
    rtst_assert(wf->get_filemode()==QN_READ);
    rtst_assert(wf2->get_filemode()==QN_READ);
    rtst_assert(wf->num_layers()==layers);
    rtst_assert(wf2->num_layers()==layers);
    sections = wf->num_sections();
    rtst_assert(sections==(layers-1)*2);
    rtst_assert(wf->get_weightmaj()==QN_INPUTMAJOR);
    rtst_assert(wf2->get_weightmaj()==QN_INPUTMAJOR);
//
// Some consistency checks
   for (i=0; i<layers; i++)
	rtst_assert(wf->size_layer((QN_LayerSelector) i)==units[i]);
    for (i = 0; i<layers-1; i++)
    {
	inputs = units[i];
	outputs = units[i+1];
	w = rtst_padvec_new_vf(inputs*outputs);
	weights[i] = w;
	b = rtst_padvec_new_vf(outputs);
	biases[i] = b;
    }

    for (i =0; i < (layers-1); i++)
    {
	switch(wf->get_weighttype(i*2))
	{
	case QN_LAYER12_WEIGHTS:
	    rtst_assert(i==0);
	    break;
	case QN_LAYER23_WEIGHTS:
	    rtst_assert(i==1);
	    break;
	case QN_LAYER34_WEIGHTS:
	    rtst_assert(i==2);
	    break;
	case QN_LAYER45_WEIGHTS:
	    rtst_assert(i==3);
	    break;
	default:
	    rtst_assert(0);
	}
	switch(wf->get_weighttype(i*2+1))
	{
	case QN_LAYER2_BIAS:
	    rtst_assert(i==0);
	    break;
	case QN_LAYER3_BIAS:
	    rtst_assert(i==1);
	    break;
	case QN_LAYER4_BIAS:
	    rtst_assert(i==2);
	    break;
	case QN_LAYER5_BIAS:
	    rtst_assert(i==3);
	    break;
	default:
	    rtst_assert(0);
	}
	wf->read(weights[i], units[i]*units[i+1]);
	wf->read(biases[i], units[i+1]);
    }
	
    for (i =0; i < (layers-1); i++)
    {
	float jf, kf;
	float val;
	inputs = units[i];
	outputs = units[i+1];
	w = weights[i];
	b = biases[i];
	for (j=0; j<inputs; j++)
	{
	    jf = (float) j;
	    for (k=0; k<outputs; k++)
	    {
		kf = ((float) k)/1000.0 +(float) j;
		val = *w++;
		// printf("%e %e\n", (double) kf, (double) val);
		// Check for NaNs
		rtst_assert(val==val);
		rtst_assert(fabs(val - kf)<0.000001);
	    }
	}
	for (k=0; k<outputs; k++)
	{
	    kf = ((float) k)/1000.0;
	    val = *b++;
	    // Check for NaNs
	    rtst_assert(val==val);
	    rtst_assert(fabs(val - kf)<0.000001);
	}
    }


//     for (i =0; i < (layers-1); i++)
//     {
// 	wf->write(weights[i], units[i]*units[i+1]);
// 	wf->write(biases[i], units[i+1]);
//     }
// 	float jf, kf;
// 	for (j=0; j<inputs; j++)
// 	{
// 	    jf = (float) j;
// 	    for (k=0; k<outputs; k++)
// 	    {
// 		kf = ((float) k)/1000.0 +(float) j;
// 		*w++ = kf;
// 	    }
// 	}
// 	for (k=0; k<outputs; k++)
// 	{
// 	    kf = ((float) k)/1000.0;
// 	    *b++ = kf;
// 	}
    

    delete wf2;
    delete wf;
    QN_close(wfile2);
    QN_close(wfile);

    rtst_log("Reading weight file done\n");

    for (i=0; i<layers-1; i++)
    {
	rtst_padvec_del_vf(weights[i]);
	rtst_padvec_del_vf(biases[i]);
    }
}



int
main(int argc, char* argv[])
{
    int arg, args_left;
    size_t units[QN_MLP_MAX_LAYERS], layers;
    size_t i;

    for (i =0; i<QN_MLP_MAX_LAYERS; i++)
	units[i] = 0;

    arg = rtst_args(argc, argv);
    args_left = argc - arg;
    rtst_assert(args_left>=(2+QN_MLP_MIN_LAYERS) && args_left<=(2+QN_MLP_MAX_LAYERS));
    

    const char* weightfile = argv[arg++];
    layers = strtoul(argv[arg++], NULL, 0);
    units[0] = strtol(argv[arg++], NULL, 0);
    units[1] = strtol(argv[arg++], NULL, 0);
    for (i=2; i<QN_MLP_MAX_LAYERS; i++)
    {
	if (arg<argc)
	    units[i] = strtol(argv[arg++], NULL, 0);
    }
    rtst_assert(layers>=QN_MLP_MIN_LAYERS && layers<=QN_MLP_MAX_LAYERS);
    for (i=2; i<QN_MLP_MAX_LAYERS; i++)
    {
	if (layers>i)
	    rtst_assert(units[i] > 0);
	else
	    rtst_assert(units[i]==0);
    }

    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "MLPWeightFile_test2");
    rtst_start("MLPWeightFile_Matlab");
    test_write(weightfile, layers, units);
    test_read(weightfile, layers, units);
    rtst_passed();
}
