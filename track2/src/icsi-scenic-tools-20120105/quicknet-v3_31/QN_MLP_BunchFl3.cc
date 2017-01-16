const char* QN_MLP_BunchFl3_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_BunchFl3.cc,v 1.22 2011/03/03 00:32:59 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_MLP_BunchFl3.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"



QN_MLP_BunchFl3::QN_MLP_BunchFl3(int a_debug, const char* a_dbgname,
				 size_t a_input, size_t a_hidden,
				 size_t a_output,
				 enum QN_OutputLayerType a_outtype,
				 size_t a_size_bunch)
    : clog(a_debug, "QN_MLP_BunchFl3", a_dbgname),
      n_input(a_input),
      n_hidden(a_hidden),
      n_output(a_output),
      n_in2hid(a_input * a_hidden),
      n_hid2out(a_hidden * a_output),
      size_input(a_input * a_size_bunch),
      size_hidden(a_hidden * a_size_bunch),
      size_output(a_output * a_size_bunch),
      out_layer_type(a_outtype),
      size_bunch(a_size_bunch)
{
    // Maybe we do not support all output layer types
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	break;
    default:
	assert(0);		// Only the above output layer types are
				// supported.
    }


    in2hid = new float [n_in2hid];
    hid_bias = new float [size_hidden];
    hid_x = new float [size_hidden];
    hid_y = new float [size_hidden];
    
    hid2out = new float [n_hid2out];
    out_bias = new float [size_output];
    out_x = new float [size_output];

    out_dedx = new float [size_output];
    out_delta_bias = new float [n_output];
    // These intermediate values are only needed for sigmoid output layers
    // ...and only then when we are NOT doing cross entropy
    if (out_layer_type==QN_OUTPUT_SIGMOID)
    {
	out_dedy = new float [size_output];
	out_dydx = new float [size_output];
    }
    hid_dedy = new float [size_hidden];
    hid_dydx = new float [size_hidden];
    hid_dedx = new float [size_hidden];
    hid_delta_bias = new float [n_hidden];
    clog.log(QN_LOG_PER_RUN,"Created net, n_input=%lu n_hidden=%lu "
	     "n_output=%lu.", n_input, n_hidden, n_output);
    clog.log(QN_LOG_PER_RUN, "bunchsize=%lu size_input=%lu "
	     "size_hidden=%lu size_output=%lu.\n",
	     size_bunch, size_input, size_hidden, size_output);
}

QN_MLP_BunchFl3::~QN_MLP_BunchFl3()
{
    delete [] in2hid;
    delete [] hid_bias;
    delete [] hid_x;
    delete [] hid_y;

    delete [] hid2out;
    delete [] out_bias;
    delete [] out_x;

    delete [] out_dedx;
    delete [] out_delta_bias;
    // These intermediate values are only created for sigmoid output layers
    if (out_layer_type==QN_OUTPUT_SIGMOID)
    {
	delete [] out_dedy;
	delete [] out_dydx;
    }
    delete [] hid_dedy;
    delete [] hid_dydx;
    delete [] hid_dedx;
    delete [] hid_delta_bias;
}

size_t
QN_MLP_BunchFl3::size_layer(QN_LayerSelector layer) const
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
QN_MLP_BunchFl3::size_section(QN_SectionSelector section, size_t* output_p,
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
	assert(0);
    }
}

size_t
QN_MLP_BunchFl3::num_connections() const
{
    // Note how we take account of the biases separately
    return (n_input+n_output) * n_hidden + n_hidden + n_output;
}

void
QN_MLP_BunchFl3::forward(size_t n_frames, const float* in, float* out)
{
    size_t i;
    size_t frames_this_bunch;	// Number of frames to handle this bunch

    for (i=0; i<n_frames; i += size_bunch)
    {
	frames_this_bunch = qn_min_zz_z(size_bunch, n_frames - i);
	
	forward_bunch(frames_this_bunch, in, out);
	in += n_input * frames_this_bunch;
	out += n_output * frames_this_bunch;
    }
}

void
QN_MLP_BunchFl3::train(size_t n_frames, const float* in, const float* target,
		    float* out)
{
    size_t i;
    size_t frames_this_bunch;	// Number of frames to handle this bunch

    for (i=0; i<n_frames; i+= size_bunch)
    {
	frames_this_bunch = qn_min_zz_z(size_bunch, n_frames - i);
	train_bunch(frames_this_bunch, in, target, out);
	in += n_input * frames_this_bunch;
	out += n_output * frames_this_bunch;
	target += n_output * frames_this_bunch;
    }
}

void
QN_MLP_BunchFl3::forward_bunch(size_t n_frames, const float* in, float* out)
{
    const size_t cur_size_hidden = n_hidden * n_frames;
    const size_t cur_size_output = n_output * n_frames;

    // First layer
    qn_copy_vf_mf(n_frames, n_hidden, hid_bias, hid_x);
    qn_mulntacc_mfmf_mf(n_frames, n_input, n_hidden, in, in2hid, hid_x);
    qn_sigmoid_vf_vf(cur_size_hidden, hid_x, hid_y);

    // Second layer
    qn_copy_vf_mf(n_frames, n_output, out_bias, out_x);
    qn_mulntacc_mfmf_mf(n_frames, n_hidden, n_output, hid_y, hid2out, out_x);

    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
	qn_sigmoid_vf_vf(cur_size_output, out_x, out);
	break;
    case QN_OUTPUT_SOFTMAX:
    {
	size_t i;
	float* cur_out_x = out_x;
	float* cur_out = out;

	for (i=0; i<n_frames; i++)
	{
	    qn_softmax_vf_vf(n_output, cur_out_x, cur_out);
	    cur_out_x += n_output;
	    cur_out += n_output;
	}
	break;
    }
    case QN_OUTPUT_LINEAR:
	qn_copy_vf_vf(cur_size_output, out_x, out);
	break;
    default:
	assert(0);
    }
}

void
QN_MLP_BunchFl3::train_bunch(size_t n_frames, const float *in,
			     const float* target, float* out)
{
    int do_backprop;

// First move forward
    forward_bunch(n_frames, in, out);

// Find the error and propogate back through output layer in an appropriate
// manner 
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
	// For a sigmoid layer, de/dx = de/dy . dy/dx
	qn_sub_vfvf_vf(size_output, out, target, out_dedy);
	qn_dsigmoid_vf_vf(size_output, out, out_dydx);
	qn_mul_vfvf_vf(size_output, out_dydx, out_dedy, out_dedx);
	break;
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	// For these layers, dx = dy
	qn_sub_vfvf_vf(size_output, out, target, out_dedx);
	break;
    default:
	assert(0);
    }

    do_backprop = (neg_in2hid_learnrate!=0.0f || neg_hid_learnrate!=0.0f);
// Back propogate error through hidden to output weights.
    if (do_backprop)
    {
	qn_mul_mfmf_mf(n_frames, n_output, n_hidden, out_dedx, hid2out,
		       hid_dedy);
    }

// Update hid2out weights.
    if (neg_hid2out_learnrate!=0.0f)
    {
	qn_multnacc_fmfmf_mf(n_frames, n_output, n_hidden,
			     neg_hid2out_learnrate, out_dedx, hid_y, hid2out);
    }
// Update output biases.
    if (neg_out_learnrate!=0.0f)
    {
	qn_sumcol_mf_vf(n_frames, n_output, out_dedx, out_delta_bias);
	qn_mulacc_vff_vf(n_output, out_delta_bias, neg_out_learnrate,
			 out_bias);
    }

// Propogate error back through sigmoid
    if (do_backprop)
    {
	qn_dsigmoid_vf_vf(size_hidden, hid_y, hid_dydx);
	qn_mul_vfvf_vf(size_hidden, hid_dydx, hid_dedy, hid_dedx);
    }

// Update in2hid weights.
    if (neg_in2hid_learnrate!=0.0f)
    {
	qn_multnacc_fmfmf_mf(n_frames, n_hidden, n_input, neg_in2hid_learnrate,
			     hid_dedx, in, in2hid);
    }
// Update biases.
    if (neg_hid_learnrate!=0.0f)
    {
	qn_sumcol_mf_vf(n_frames, n_hidden, hid_dedx, hid_delta_bias);
	qn_mulacc_vff_vf(n_hidden, hid_delta_bias, neg_hid_learnrate, hid_bias);
    }
}



void
QN_MLP_BunchFl3::set_weights(enum QN_SectionSelector which,
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
    clog.log(QN_LOG_PER_SUBEPOCH,
	     "Set weights %s @ (%lu,%lu) size (%lu,%lu).",
	     nameweights(which), row, col, n_rows, n_cols);
    qn_copy_mf_smf(n_rows, n_cols, total_cols, from, start);
}

void
QN_MLP_BunchFl3::get_weights(enum QN_SectionSelector which,
			      size_t row, size_t col,
			      size_t n_rows, size_t n_cols,
			      float* to)
{
    float* start;		// The first element of the relveant weight
				// matrix that we want
    size_t total_cols;		// The total number of columns in the given
				// weight matrix
    start = findweights(which, row, col, n_rows, n_cols, &total_cols);
    clog.log(QN_LOG_PER_SUBEPOCH,
	     "Get weights %s @ (%lu,%lu) size (%lu,%lu).",
	     nameweights(which), row, col, n_rows, n_cols);
    qn_copy_smf_mf(n_rows, n_cols, total_cols, start, to);
}

void
QN_MLP_BunchFl3::set_learnrate(enum QN_SectionSelector which, float learnrate)
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
	assert(0);
	break;
    }
}

float 
QN_MLP_BunchFl3::get_learnrate(enum QN_SectionSelector which) const
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
	assert(0);
	break;
    }
    return res;
}

const char*
QN_MLP_BunchFl3::nameweights(QN_SectionSelector which)
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
QN_MLP_BunchFl3::findweights(QN_SectionSelector which,
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

size_t
QN_MLP_BunchFl3::get_bunchsize()
{
    return size_bunch;
}
