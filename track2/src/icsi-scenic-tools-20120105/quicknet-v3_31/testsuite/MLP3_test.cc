// $Header: /u/drspeech/repos/quicknet2/testsuite/MLP3_test.cc,v 1.22 2006/03/09 05:18:16 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Various tests for MLP3 classes

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "QuickNet.h"
#include "rtst.h"

void
MLP_3_size_test(QN_MLP& mlp, size_t a_input, size_t a_hidden, size_t a_output)
{
    size_t input, hidden, output, dummy;

    rtst_log("Testing size_layer etc....\n");
    rtst_assert(mlp.num_layers()==3);
    input = mlp.size_layer(QN_MLP3_INPUT);
    hidden = mlp.size_layer(QN_MLP3_HIDDEN);
    output = mlp.size_layer(QN_MLP3_OUTPUT);
    dummy = mlp.size_layer(QN_LAYER4);
    rtst_assert(input==a_input);
    rtst_assert(hidden==a_hidden);
    rtst_assert(output==a_output);
    rtst_assert(dummy==0);
}

void
MLP_3_learnrate_test(QN_MLP& mlp, const float tolerance)
{
    const float lri[4] = { 0.008, 0.006, 0.004, 0.002 };
    float lro[4];

    rtst_log("Testing learnrates...\n");
    mlp.set_learnrate(QN_MLP3_INPUT2HIDDEN, lri[0]);
    mlp.set_learnrate(QN_MLP3_HIDDENBIAS, lri[1]);
    mlp.set_learnrate(QN_MLP3_HIDDEN2OUTPUT, lri[2]);
    mlp.set_learnrate(QN_MLP3_OUTPUTBIAS, lri[3]);
    lro[0] = mlp.get_learnrate(QN_MLP3_INPUT2HIDDEN);
    lro[1] = mlp.get_learnrate(QN_MLP3_HIDDENBIAS);
    lro[2] = mlp.get_learnrate(QN_MLP3_HIDDEN2OUTPUT);
    lro[3] = mlp.get_learnrate(QN_MLP3_OUTPUTBIAS);
    rtst_checknear_fvfvf(4, tolerance, lri, lro);

}

// Check everything is the same regardless of how many presentations we
// pass into the net

void
MLP_3_batch_test(QN_MLP& mlp, float tol)
{
    size_t n_input, n_hidden, n_output;
    size_t batch_size = 4;

    rtst_log("Testing batching...\n");
    n_input = mlp.size_layer(QN_LAYER1);
    n_hidden = mlp.size_layer(QN_LAYER2);
    n_output = mlp.size_layer(QN_LAYER3);
    float* in2hid = new float[n_input * n_hidden];
    float* hid = new float [n_hidden];
    float* hid2out = new float [n_hidden * n_output];
    float* out = new float [n_output];

    rtst_urand_ff_vf(n_input * n_hidden, -0.05, 0.05, in2hid);
    rtst_urand_ff_vf(n_hidden * n_output, -0.05, 0.05, hid2out);
    rtst_urand_ff_vf(n_hidden, -1.0, 1.0, hid);
    rtst_urand_ff_vf(n_output, -1.0, 1.0, out);

    mlp.set_weights(QN_MLP3_INPUT2HIDDEN, 0, 0, n_hidden, n_input, in2hid);
    mlp.set_weights(QN_MLP3_HIDDEN2OUTPUT, 0, 0, n_output, n_hidden, hid2out);
    mlp.set_weights(QN_MLP3_HIDDENBIAS, 0, 0, n_hidden, 1, hid);
    mlp.set_weights(QN_MLP3_OUTPUTBIAS, 0, 0, n_output, 1, out);

    float* in = new float [n_input * batch_size];
    rtst_urand_ff_vf(n_input * batch_size, -1.0, 1.0, in);
    float* out1 = new float [n_output * batch_size];
    float* outn = new float [n_output * batch_size];
    mlp.forward(batch_size, in, outn);
    size_t i;
    float* out1_ptr = out1;
    float* in_ptr = in;
    for (i=0; i<batch_size; i++)
    {
	mlp.forward(1, in_ptr, out1_ptr);
	in_ptr += n_input;
	out1_ptr += n_output;
    }
    rtst_checknear_fvfvf(n_output * batch_size, tol, out1, outn);
    delete [] outn;
    delete [] out1;
    delete [] in;
    delete [] out;
    delete [] hid2out;
    delete [] hid;
    delete [] in2hid;
}

// Check reading and writing of weights

void
MLP_3_weight_test(QN_MLP& mlp, float tol)
{
    size_t n_in, n_hid, n_out;
    size_t n_in2hid, n_hid2out;
    size_t i;

    rtst_log("Testing weights\n");
    n_in = mlp.size_layer(QN_MLP3_INPUT);
    n_hid = mlp.size_layer(QN_MLP3_HIDDEN);
    n_out = mlp.size_layer(QN_MLP3_OUTPUT);
    n_in2hid = n_in * n_hid;
    n_hid2out = n_hid * n_out;

    float* in2hid_in = new float [n_in2hid];
    float* hid_in = new float [n_hid];
    float* hid2out_in = new float [n_hid2out];
    float* out_in = new float [n_out];

    for (i=0; i<n_in2hid; i++)
	in2hid_in[i] = i/100.0;
    for (i=0; i<n_hid; i++)
	hid_in[i] = i/100.0;
    for (i=0; i<n_hid2out; i++)
	hid2out_in[i] = i/100.0;
    for (i=0; i<n_out; i++)
	out_in[i] = i/100.0;

    // Test a horizontal cut in INPUT2HIDDEN
    mlp.set_weights(QN_MLP3_INPUT2HIDDEN, 0, 0, 2, n_in, in2hid_in);
    mlp.set_weights(QN_MLP3_INPUT2HIDDEN, 2, 0, n_hid-2, n_in,
		    in2hid_in+2*n_in);
    // Test a cut in the HIDDEN bias
    mlp.set_weights(QN_MLP3_HIDDENBIAS, 0, 0, 4, 1, hid_in);
    mlp.set_weights(QN_MLP3_HIDDENBIAS, 4, 0, n_hid-4, 1, hid_in+4);
    mlp.set_weights(QN_MLP3_HIDDEN2OUTPUT, 0, 0, n_out, n_hid,
		    hid2out_in);
    mlp.set_weights(QN_MLP3_OUTPUTBIAS, 0, 0, n_out, 1, out_in);

    float* in2hid_res = rtst_padvec_new_vf(n_in2hid);
    float* hid_res = rtst_padvec_new_vf(n_hid);
    float* hid2out_res = rtst_padvec_new_vf(n_hid2out);
    float* out_res = rtst_padvec_new_vf(n_out);
    float* temp1_res = rtst_padvec_new_vf(1);
    float* temp2_res = rtst_padvec_new_vf(2*3);

    mlp.get_weights(QN_MLP3_INPUT2HIDDEN, 0, 0, n_hid, n_in, in2hid_res);
    // Get an individual element and check that is okay
    mlp.get_weights(QN_MLP3_INPUT2HIDDEN, 4, 2, 1, 1, temp1_res);
    rtst_assert(in2hid_res[4*n_in+2] == *temp1_res);
    mlp.get_weights(QN_MLP3_HIDDENBIAS, 0, 0, 1, 1, hid_res);
    mlp.get_weights(QN_MLP3_HIDDENBIAS, 1, 0, n_hid-1, 1, hid_res+1);
    mlp.get_weights(QN_MLP3_HIDDEN2OUTPUT, 0, 0, n_out, n_hid,
		    hid2out_res);
    // Get a block from the middle of the hidden-to-output weights
    mlp.get_weights(QN_MLP3_HIDDEN2OUTPUT, 1, 2, 2, 3, temp2_res);
    qn_copy_mf_smf(2, 3, n_hid, temp2_res, &hid2out_res[1*n_hid+2]);
    mlp.get_weights(QN_MLP3_OUTPUTBIAS, 0, 0, n_out, 1, out_res);

    rtst_checknear_fvfvf(n_in2hid, tol, in2hid_in, in2hid_res);
    rtst_checknear_fvfvf(n_hid, tol, hid_in, hid_res);
    rtst_checknear_fvfvf(n_hid2out, tol, hid2out_in, hid2out_res);
    rtst_checknear_fvfvf(n_out, tol, out_in, out_res);

    rtst_padvec_del_vf(in2hid_res);
    rtst_padvec_del_vf(hid_res);
    rtst_padvec_del_vf(hid2out_res);
    rtst_padvec_del_vf(out_res);
    rtst_padvec_del_vf(temp1_res);
    rtst_padvec_del_vf(temp2_res);
    delete [] out_in;
    delete [] hid2out_in;
    delete [] hid_in;
    delete [] in2hid_in;
}



enum
{
    INPUT = 5,
    HIDDEN = 7,
    OUTPUT = 3
};

void
OnlineFl3_test()
{
    const char* name = "MLP_OnlineFl3";

    rtst_start(name);
    QN_MLP_OnlineFl3 mlp(10, "mlp", INPUT, HIDDEN, OUTPUT,
			 QN_OUTPUT_SIGMOID_XENTROPY);
    MLP_3_size_test(mlp, INPUT, HIDDEN, OUTPUT);
    MLP_3_learnrate_test(mlp, 0.0);
    MLP_3_batch_test(mlp, 0.00001);
    MLP_3_weight_test(mlp, 0.0);
    rtst_passed();
}

#ifdef HAVE_LIBFXLIB

void
OnlineFx3_test()
{
    const char* name = "MLP_OnlineFx3";

    rtst_start(name);
    QN_MLP_OnlineFx3 mlp(10, "mlp", INPUT, HIDDEN, OUTPUT,
			 QN_OUTPUT_SIGMOID_XENTROPY);
    MLP_3_size_test(mlp, INPUT, HIDDEN, OUTPUT);
    MLP_3_learnrate_test(mlp, 0.00001);
    MLP_3_batch_test(mlp, 0.00001);
    MLP_3_weight_test(mlp, 0.00005);
    rtst_passed();
}

void
Online1632Fx3_test()
{
    const char* name = "MLP_Online1632Fx3";

    rtst_start(name);
    QN_MLP_Online1632Fx3 mlp(10, "mlp", INPUT, HIDDEN, OUTPUT,
			     QN_OUTPUT_SIGMOID_XENTROPY);
    MLP_3_size_test(mlp, INPUT, HIDDEN, OUTPUT);
    MLP_3_learnrate_test(mlp, 0.00001);
    MLP_3_batch_test(mlp, 0.00001);
    MLP_3_weight_test(mlp, 0.00005);
    rtst_passed();
}

#endif

void
BunchFl3_test()
{
    const char* name = "MLP_BunchFl3";
    enum { BUNCH_SIZE = 16 };

    rtst_start(name);
    QN_MLP_BunchFl3 mlp(10, "mlp", INPUT, HIDDEN, OUTPUT,
			 QN_OUTPUT_SIGMOID_XENTROPY, BUNCH_SIZE);
    MLP_3_size_test(mlp, INPUT, HIDDEN, OUTPUT);
    MLP_3_learnrate_test(mlp, 0.0);
    MLP_3_batch_test(mlp, 0.00001);
    MLP_3_weight_test(mlp, 0.0);
    rtst_assert(mlp.get_bunchsize()== BUNCH_SIZE);
    rtst_passed();
}

void
BunchFl3_test2()
{
    const char* name = "MLP_BunchFl3_v2";
    enum { BUNCH_SIZE = 16 };
    size_t threads;

    rtst_start(name);
    for (threads=1; threads<7; threads++)
    {
	QN_MLP_BunchFl3* mlpt = new QN_MLP_BunchFl3(10, "mlp", INPUT, HIDDEN,
						     OUTPUT,
						     QN_OUTPUT_SIGMOID_XENTROPY,
						     BUNCH_SIZE);

	QN_MLP* mlp = (QN_MLP*) mlpt;
	MLP_3_size_test(*mlp, INPUT, HIDDEN, OUTPUT);
	MLP_3_learnrate_test(*mlp, 0.0);
	MLP_3_batch_test(*mlp, 0.00001);
	MLP_3_weight_test(*mlp, 0.0);
	rtst_assert(mlpt->get_bunchsize()== BUNCH_SIZE);
	delete mlp;
    }
    rtst_passed();
}

void
BunchFlVar_test()
{
    const char* name = "MLP_BunchFlVar";
    enum { BUNCH_SIZE = 16 };
    size_t layers[3] = { INPUT, HIDDEN, OUTPUT};

    rtst_start(name);
    QN_MLP_BunchFlVar mlp(10, "mlp", 3,
			  layers,
			  QN_OUTPUT_SIGMOID_XENTROPY, BUNCH_SIZE);
    MLP_3_size_test(mlp, INPUT, HIDDEN, OUTPUT);
    MLP_3_learnrate_test(mlp, 0.0);
    MLP_3_batch_test(mlp, 0.00001);
    MLP_3_weight_test(mlp, 0.0);
    rtst_assert(mlp.get_bunchsize()== BUNCH_SIZE);
    rtst_passed();
}



void
ThreadFl3_test()
{
    const char* name = "MLP_ThreadFl3";
    enum { BUNCH_SIZE = 16 };
    size_t threads;

    rtst_start(name);
    for (threads=1; threads<7; threads++)
    {
	QN_MLP_ThreadFl3* mlpt = new QN_MLP_ThreadFl3(10, "mlp", INPUT, HIDDEN,
						     OUTPUT,
						     QN_OUTPUT_SIGMOID_XENTROPY,
						     BUNCH_SIZE, threads);

	QN_MLP* mlp = (QN_MLP*) mlpt;
	MLP_3_size_test(*mlp, INPUT, HIDDEN, OUTPUT);
	MLP_3_learnrate_test(*mlp, 0.0);
	MLP_3_batch_test(*mlp, 0.00001);
	MLP_3_weight_test(*mlp, 0.0);
	rtst_assert(mlpt->get_bunchsize()== BUNCH_SIZE);
	delete mlp;
    }
    rtst_passed();
}

void
ThreadFlVar_test()
{
    const char* name = "MLP_ThreadFlVar";
    enum { BUNCH_SIZE = 16 };
    size_t threads;
    size_t layers[3] = { INPUT, HIDDEN, OUTPUT};

    rtst_start(name);
    for (threads=1; threads<7; threads++)
    {
	QN_MLP* mlp;
	QN_MLP_ThreadFlVar* mlpt;

	mlpt = new QN_MLP_ThreadFlVar(10, "mlp", 3,
				      layers,
				      QN_OUTPUT_SIGMOID_XENTROPY, BUNCH_SIZE,
				      threads);
	mlp = (QN_MLP*) mlpt;
	MLP_3_size_test(*mlp, INPUT, HIDDEN, OUTPUT);
	MLP_3_learnrate_test(*mlp, 0.0);
	MLP_3_batch_test(*mlp, 0.00001);
	MLP_3_weight_test(*mlp, 0.0);
	rtst_assert(mlpt->get_bunchsize()== BUNCH_SIZE);
    }
    rtst_passed();
}


int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);
    assert(arg == argc);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "MLP3_test");
    OnlineFl3_test();
    BunchFl3_test();
    BunchFl3_test2();
    BunchFlVar_test();
    ThreadFl3_test();
    ThreadFlVar_test();
#ifdef HAVE_LIBFXLIB
    OnlineFx3_test();
    Online1632Fx3_test();
#endif
    rtst_exit();
}

