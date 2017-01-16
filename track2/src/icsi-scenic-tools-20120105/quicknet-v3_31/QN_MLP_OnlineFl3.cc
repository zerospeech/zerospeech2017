const char* QN_MLP3_OnlineFl3_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_OnlineFl3.cc,v 1.18 2011/03/03 00:32:59 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "QN_types.h"
#include "QN_MLP_OnlineFl3.h"
#include "QN_Logger.h"
#include "QN_fltvec.h"

QN_MLP_OnlineFl3::QN_MLP_OnlineFl3(int a_debug, const char* a_dbgname,
				   size_t input, size_t hidden, size_t output,
				   QN_OutputLayerType outtype)
    : clog(a_debug, "QN_MLP_OnlineFl3", a_dbgname),
      n_input(input),
      n_hidden(hidden),
      n_output(output),
      n_in2hid(input * hidden),
      n_hid2out(hidden * output),
      out_layer_type(outtype)
{
    const float init = 0.0f;	// Initialize weights and layers to this

    // Maybe we do not support all output layer types
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	break;
    default:
        // Only the output layer types enumerated above are supported.
	clog.error("unknown output layer type %i in constructor.",
		   (int) out_layer_type);
	break;
    }

    in2hid = new float [n_in2hid]; qn_copy_f_vf(n_in2hid, init, in2hid);
    hid_bias = new float [n_hidden]; qn_copy_f_vf(n_hidden, init, hid_bias);
    hid_x = new float [n_hidden]; qn_copy_f_vf(n_hidden, init, hid_x);
    hid_y = new float [n_hidden]; qn_copy_f_vf(n_hidden, init, hid_y);
    
    hid2out = new float [n_hid2out]; qn_copy_f_vf(n_hid2out, init, hid2out);
    out_bias = new float [n_output]; qn_copy_f_vf(n_output, init, out_bias);
    out_x = new float [n_output]; qn_copy_f_vf(n_output, init, out_x);

    out_dedx = new float [n_output]; qn_copy_f_vf(n_output, init, out_dedx);
    // These intermediate values are only needed for sigmoid output layers
    // ...and only then when we are NOT doing cross entropy
    if (out_layer_type==QN_OUTPUT_SIGMOID)
    {
	out_dedy = new float [n_output]; qn_copy_f_vf(n_output, init, out_dedy);
	out_dydx = new float [n_output]; qn_copy_f_vf(n_output, init, out_dydx);
    }
    hid_dedy = new float [n_hidden]; qn_copy_f_vf(n_hidden, init, hid_dedy);
    hid_dydx = new float [n_hidden]; qn_copy_f_vf(n_hidden, init, hid_dydx);
    hid_dedx = new float [n_hidden]; qn_copy_f_vf(n_hidden, init, hid_dedx);
    clog.log(QN_LOG_PER_RUN, "Created net inputs=%u hidden=%u output=%u.",
	     n_input, n_hidden, n_output);
}

QN_MLP_OnlineFl3::~QN_MLP_OnlineFl3()
{
    delete [] in2hid;
    delete [] hid_bias;
    delete [] hid_x;
    delete [] hid_y;

    delete [] hid2out;
    delete [] out_bias;
    delete [] out_x;

    delete [] out_dedx;
    // These intermediate values are only created for sigmoid output layers
    if (out_layer_type==QN_OUTPUT_SIGMOID)
    {
	delete [] out_dedy;
	delete [] out_dydx;
    }
    delete [] hid_dedy;
    delete [] hid_dydx;
    delete [] hid_dedx;
}

size_t
QN_MLP_OnlineFl3::size_layer(QN_LayerSelector layer) const
{
    size_t r;

    switch(layer)
    {
    case QN_MLP3_INPUT:
	r = n_input;
	break;
    case QN_MLP3_HIDDEN:
	r = n_hidden;
	break;
    case QN_MLP3_OUTPUT:
	r = n_output;
	break;
    default:
	r = 0;
	break;
    }
    return r;
}

void
QN_MLP_OnlineFl3::size_section(QN_SectionSelector section, size_t* output_p,
			       size_t* input_p) const
{
    switch(section)
    {
    case QN_MLP3_INPUT2HIDDEN:
	*output_p = n_hidden;
	*input_p = n_input;
	break;
    case QN_MLP3_HIDDENBIAS:
	*output_p = n_hidden;
	*input_p = 1;
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	*output_p = n_output;
	*input_p = n_hidden;
	break;
    case QN_MLP3_OUTPUTBIAS:
	*output_p = n_output;
	*input_p = 1;
	break;
    default:
	clog.error("unknown section %i in size_section().", (int) section);
    }
}

size_t
QN_MLP_OnlineFl3::num_connections() const
{
    // Note how we take account of the biases separately
    return (n_input+n_output) * n_hidden + n_hidden + n_output;
}

void
QN_MLP_OnlineFl3::forward(size_t n_frames, const float* in, float* out)
{
    size_t i;

    for (i=0; i<n_frames; i++)
    {
	forward1(in, out);
	in += n_input;
	out += n_output;
    }
}

void
QN_MLP_OnlineFl3::train(size_t n_frames, const float* in, const float* target,
	       float* out)
{
    size_t i;

    for (i=0; i<n_frames; i++)
    {
	train1(in, target, out);
	in += n_input;
	out += n_output;
	target += n_output;
    }
}

void
QN_MLP_OnlineFl3::forward1(const float* in, float* out)
{
    // First layer
    qn_mul_mfvf_vf(n_hidden, n_input, in2hid, in, hid_x);
    qn_acc_vf_vf(n_hidden, hid_bias, hid_x);
    qn_sigmoid_vf_vf(n_hidden, hid_x, hid_y);

    // Second layer
    qn_mul_mfvf_vf(n_output, n_hidden, hid2out, hid_y, out_x);
    qn_acc_vf_vf(n_output, out_bias, out_x);
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
	qn_sigmoid_vf_vf(n_output, out_x, out);
	break;
    case QN_OUTPUT_SOFTMAX:
	qn_softmax_vf_vf(n_output, out_x, out);
	break;
    case QN_OUTPUT_LINEAR:
	qn_copy_vf_vf(n_output, out_x, out);
	break;
    default:
	assert(0);
    }
}

void
QN_MLP_OnlineFl3::train1(const float *in, const float* target, float* out)
{
// First move forward
    forward1(in, out);

// Find the error and propogate back through output layer in an appropriate
// manner 
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
	// For a sigmoid layer, de/dx = de/dy . dy/dx
	qn_sub_vfvf_vf(n_output, out, target, out_dedy);
	qn_dsigmoid_vf_vf(n_output, out, out_dydx);
	qn_mul_vfvf_vf(n_output, out_dydx, out_dedy, out_dedx);
	break;
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	// For these layers, dx = dy
	qn_sub_vfvf_vf(n_output, out, target, out_dedx);
	break;
    default:
	assert(0);
    }

// Back propogate error through hidden to output weights
    qn_mul_vfmf_vf(n_output, n_hidden, out_dedx, hid2out, hid_dedy);

// Update hid2out weights and biases
    qn_mulacc_fvfvf_mf(n_output, n_hidden, neg_hid2out_learnrate, out_dedx, hid_y,
		    hid2out);
    qn_mulacc_vff_vf(n_output, out_dedx, neg_out_learnrate, out_bias);

// Propogate error back through sigmoid
    qn_dsigmoid_vf_vf(n_hidden, hid_y, hid_dydx);
    qn_mul_vfvf_vf(n_hidden, hid_dydx, hid_dedy, hid_dedx);

// Update in2hid weights and biases
    qn_mulacc_fvfvf_mf(n_hidden, n_input, neg_in2hid_learnrate, hid_dedx, in,
		    in2hid);
    qn_mulacc_vff_vf(n_hidden, hid_dedx, neg_hid_learnrate, hid_bias);
}


void
QN_MLP_OnlineFl3::set_weights(enum QN_SectionSelector which,
			      size_t row, size_t col,
			      size_t n_rows, size_t n_cols,
			      const float* from)
{
    float* start;		// The first element of the relveant weight
				// matrix that we want
    size_t total_cols;		// The total number of columns in the given
				// weight matrix

    start = findweights(which, row, col, n_rows, n_cols,
			&total_cols);
    clog.log(QN_LOG_PER_SUBEPOCH, "Set weights %s @ (%lu,%lu) size (%lu,%lu).",
	     nameweights(which), row, col, n_rows, n_cols);
    qn_copy_mf_smf(n_rows, n_cols, total_cols, from, start);
}

void
QN_MLP_OnlineFl3::get_weights(enum QN_SectionSelector which,
			      size_t row, size_t col,
			      size_t n_rows, size_t n_cols,
			      float* to)
{
    float* start;		// The first element of the relevant weight
				// matrix that we want.
    size_t total_cols;		// The total number of columns in the given
				// weight matrix.
    start = findweights(which, row, col, n_rows, n_cols, &total_cols);
    clog.log(QN_LOG_PER_SUBEPOCH, "Get weights %s @ (%lu,%lu) size (%lu,%lu).",
	     nameweights(which), row, col, n_rows, n_cols);
    qn_copy_smf_mf(n_rows, n_cols, total_cols, start, to);
}

void
QN_MLP_OnlineFl3::set_learnrate(enum QN_SectionSelector which, float learnrate)
{
    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
	neg_in2hid_learnrate = -learnrate;
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	neg_hid2out_learnrate = -learnrate;
	break;
    case QN_MLP3_HIDDENBIAS:
	neg_hid_learnrate = -learnrate;
	break;
    case QN_MLP3_OUTPUTBIAS:
	neg_out_learnrate = -learnrate;
	break;
    default:
	clog.error("unknown section %i in set_learnrate().", (int) which);
	break;
    }
}

float 
QN_MLP_OnlineFl3::get_learnrate(enum QN_SectionSelector which) const
{
    float res;			// Returned learning rate

    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
	res = -neg_in2hid_learnrate;
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	res = -neg_hid2out_learnrate;
	break;
    case QN_MLP3_HIDDENBIAS:
	res = -neg_hid_learnrate;
	break;
    case QN_MLP3_OUTPUTBIAS:
	res = -neg_out_learnrate;
	break;
    default:
	res = 0;
	clog.error("unknown section %i in get_learnrate().", (int) which);
	break;
    }
    return res;
}

const char*
QN_MLP_OnlineFl3::nameweights(QN_SectionSelector which)
{
    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
	return "input2hidden";
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	return "hidden2output";
	break;
    case QN_MLP3_HIDDENBIAS:
	return "hiddenbias";
	break;
    case QN_MLP3_OUTPUTBIAS:
	return "outputbias";
	break;
    default:
	assert(0);
	return "unknown";
	break;
    }
}

float*
QN_MLP_OnlineFl3::findweights(QN_SectionSelector which,
			      size_t row, size_t col,
			      size_t n_rows, size_t n_cols,
			      size_t* total_cols_p) const
{
// Note - for a weight matrix, inputs are across a row, outputs are
// ..down a column

    float *weights;		// Pointer to bit of weight matrix requested
    size_t total_rows;		// The number of rows in the selected matrix
    size_t total_cols;		// The number of cols in the selected matrix

    size_section(which, &total_rows, &total_cols);
    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
	weights = in2hid;
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	weights = hid2out;
	break;
    case QN_MLP3_HIDDENBIAS:
	weights = hid_bias;
	break;
    case QN_MLP3_OUTPUTBIAS:
	weights = out_bias;
	break;
    default:
	weights = NULL;
	assert(0);
    }
    assert(row<total_rows);
    assert(row+n_rows<=total_rows);
    assert(col<total_cols);
    assert(col+n_cols<=total_cols);
    weights += col + row * total_cols;
    *total_cols_p = total_cols;
    return(weights);
}
