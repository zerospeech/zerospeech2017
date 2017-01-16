const char* QN_MLP3_OnlineFl3Diag_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_OnlineFl3Diag.cc,v 1.3 2004/04/08 02:57:41 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "QN_types.h"
#include "QN_MLP_OnlineFl3Diag.h"
#include "QN_Logger.h"

QN_MLP_OnlineFl3Diag::QN_MLP_OnlineFl3Diag(int a_debug, const char* a_dbgname,
					   size_t a_input, size_t a_hidden,
					   size_t a_output,
					   QN_OutputLayerType a_outtype)
    : QN_MLP_OnlineFl3(a_debug, a_dbgname, a_input, a_hidden, a_output,
			   a_outtype),
      clog(a_debug, "QN_MLP_OnlineFl3Diag", a_dbgname),
      forward_count(0),
      train_count(0)
{
}

QN_MLP_OnlineFl3Diag::~QN_MLP_OnlineFl3Diag()
{
}


void
QN_MLP_OnlineFl3Diag::forward1(const float* in, float* out)
{
    QN_MLP_OnlineFl3::forward1(in, out);
    if (1)
    {
	dump("in_y", 1, n_input, in);
	dump("in2hid", n_hidden, n_input, in2hid);
	dump("hid_bias", 1, n_hidden, hid_bias);
	dump("hid_x", 1, n_hidden, hid_x);
	dump("hid_y", 1, n_hidden, hid_y);
	dump("hid2out", n_output, n_hidden, hid2out);
	dump("out_bias", 1, n_output, out_bias);
	dump("out_x", 1, n_output, out_x);
	dump("out_y", 1, n_output, out);
    }
    forward_count++;
}

void
QN_MLP_OnlineFl3Diag::train1(const float *in, const float* target, float* out)
{
    QN_MLP_OnlineFl3::train1(in, target, out);
    if (1)
    {
	dump("neg_hid2out_learnrate", 1, 1, &neg_hid2out_learnrate);
	dump("neg_out_bias_learnrate", 1, 1, &neg_out_learnrate);
	dump("neg_in2hid_learnrate", 1, 1, &neg_in2hid_learnrate);
	dump("neg_hid_bias_learnrate", 1, 1, &neg_hid_learnrate);
	dump("target", 1, n_output, target);
	if (out_layer_type==QN_OUTPUT_SIGMOID)
	{
	    dump("out_dedy", 1, n_output, out_dedy);
	}
	dump("out_dedx", 1, n_output, out_dedx);
	dump("hid_dedy", 1, n_hidden, hid_dedy);
	dump("hid_dydx", 1, n_hidden, hid_dydx);
	dump("hid_dedx", 1, n_hidden, hid_dedx);
    }
    train_count++;
}

void
QN_MLP_OnlineFl3Diag::dump(const char* /*matname*/, size_t /*rows*/,
			   size_t /*cols*/, const float* /*mat*/)
{
    enum { MAXMATNAMELEN = 32 };
}
