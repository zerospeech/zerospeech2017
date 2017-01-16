const char* QN_MLP_BunchFlVar_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_BunchFlVar.cc,v 1.11 2006/05/24 01:06:26 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_MLP_BunchFlVar.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"


QN_MLP_BunchFlVar::QN_MLP_BunchFlVar(int a_debug, const char* a_dbgname,
				     size_t a_n_layers,
				     const size_t a_layer_units[QN_MLP_MAX_LAYERS],
				     enum QN_OutputLayerType a_outtype,
				     size_t a_size_bunch)
    : QN_MLP_BaseFl(a_debug, a_dbgname, "QN_MLP_BunchFlVar",
		    a_size_bunch, a_n_layers,
		    a_layer_units[0], a_layer_units[1],
		    a_layer_units[2], a_layer_units[3], a_layer_units[4]),
      out_layer_type(a_outtype)

{
    size_t i;
    float nan = qn_nan_f();

    // Some stuff so that when things go wrong it is more obvious.
    for (i=0; i<MAX_LAYERS; i++)
    {
	layer_x[i] = NULL;
	layer_y[i] = NULL;
	layer_dedy[i] = NULL;
	layer_dydx[i] = NULL;
	layer_dedx[i] = NULL;
	layer_delta_bias[i] = NULL;
    }
    // Maybe we do not support all output layer types
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_TANH:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	break;
    default:
	clog.error("Failed to create an MLP with an invalid"
		 " output layer type.");
    }
    if (size_bunch == 0)
	clog.error("Cannot use a 0 bunch size.");

    // Set up the per-layer data structures.
    for (i = 1; i<n_layers; i++)
    {
	size_t size = layer_size[i];
	size_t units = layer_units[i];

	layer_y[i] = new float [size];
	qn_copy_f_vf(size, nan, layer_y[i]);
	layer_x[i] = new float [size];
	qn_copy_f_vf(size, nan, layer_x[i]);
	layer_dedy[i] = new float [size];
	qn_copy_f_vf(size, nan, layer_dedy[i]);
	layer_dydx[i] = new float [size];
	qn_copy_f_vf(size, nan, layer_dydx[i]);
	layer_dedx[i] = new float [size];
	qn_copy_f_vf(size, nan, layer_dedx[i]);
	layer_delta_bias[i] = new float [units];
	qn_copy_f_vf(units, nan, layer_delta_bias[i]);
    }
    clog.log(QN_LOG_PER_RUN, "Created net with %lu layers, bunchsize %lu.",
	     n_layers, size_bunch);
    for (i=0; i<n_layers; i++)
    {
	clog.log(QN_LOG_PER_RUN, "Layer %lu has %lu units.",
		 i+1, layer_units[i]);
    }
}

QN_MLP_BunchFlVar::~QN_MLP_BunchFlVar()
{
    size_t i;

    for (i = 1; i<n_layers; i++)
    {
	delete [] layer_y[i];
	delete [] layer_delta_bias[i];
	delete [] layer_dedx[i];
	delete [] layer_dydx[i];
	delete [] layer_dedy[i];
	delete [] layer_x[i];
    }
}



void
QN_MLP_BunchFlVar::forward_bunch(size_t n_frames, const float* in, float* out)
{
    size_t cur_layer;		// The index of the current layer.
    size_t prev_layer;		// The index of the previous layer.
    size_t cur_weinum;		// The index of the current weight matrix.
    size_t cur_layer_units;	// The number of units in the current layer.
    size_t prev_layer_units;	// The number of units in the previous layer.
    size_t cur_layer_size;	// The size of the current layer.
    float* cur_layer_x;		// Input to the current layer non-linearity.
    float* cur_layer_y;		// Output from the current layer
				// non-linearity.
    const float* prev_layer_y;	// Output from the previous non-linearity.
    float* cur_layer_bias;	// Biases for the current layer.
    float* cur_weights;		// Weights inputing to the current layer.

    // Iterate over all of the layers except the input.  This is just one 
    // iteration for 2-layer MLPs.
    // Note that layer index starts at 0 for inputlayer, so we start at 1.
    for (cur_layer=1; cur_layer<n_layers; cur_layer++)
    {
	prev_layer = cur_layer - 1;
	cur_weinum = cur_layer - 1;
	cur_layer_units = layer_units[cur_layer];
	prev_layer_units = layer_units[prev_layer];
	cur_layer_size = cur_layer_units * n_frames;
	cur_layer_x = layer_x[cur_layer];
	cur_layer_y = layer_y[cur_layer];
	if (cur_layer==1)
	    prev_layer_y = in;
	else
	    prev_layer_y = layer_y[prev_layer];
	cur_layer_bias = layer_bias[cur_layer];
	cur_weights = weights[cur_weinum];

	qn_copy_vf_mf(n_frames, cur_layer_units, cur_layer_bias,
		      cur_layer_x);
	qn_mulntacc_mfmf_mf(n_frames, prev_layer_units, cur_layer_units,
			    prev_layer_y, cur_weights,
			    cur_layer_x); 
	
	// Check if we are doing things differently for the final layer.
	if (cur_layer!=n_layers - 1)
	{
	    // This is the intermediate layer non-linearity.
	    qn_sigmoid_vf_vf(cur_layer_size, cur_layer_x,
			     cur_layer_y);
	}
	else
	{
	    // This is the output layer non-linearity.
	    switch(out_layer_type)
	    {
	    case QN_OUTPUT_SIGMOID:
	    case QN_OUTPUT_SIGMOID_XENTROPY:
		qn_sigmoid_vf_vf(cur_layer_size, cur_layer_x, out);
		break;
	    case QN_OUTPUT_SOFTMAX:
	    {
		size_t i;
		float* layer_x_p = cur_layer_x;
		float* layer_y_p = out;

		for (i=0; i<n_frames; i++)
		{
		    qn_softmax_vf_vf(cur_layer_units, layer_x_p, layer_y_p);
		    layer_x_p += cur_layer_units;
		    layer_y_p += cur_layer_units;
		}
		break;
	    }
	    case QN_OUTPUT_LINEAR:
		qn_copy_vf_vf(cur_layer_size, cur_layer_x, out);
		break;
	    case QN_OUTPUT_TANH:
		qn_tanh_vf_vf(cur_layer_size, cur_layer_x, out);
		break;
	    default:
		assert(0);
	    }
	}
    }
    
}

void
QN_MLP_BunchFlVar::train_bunch(size_t n_frames, const float *in,
			       const float* target, float* out)
{
// First move forward
    forward_bunch(n_frames, in, out);



    size_t cur_layer;		// The index of the current layer.
    size_t prev_layer;		// The index of the previous layer.
    size_t cur_weinum;		// The index of the current weight matrix.
    size_t cur_layer_units;	// The number of units in the current layer.
    size_t prev_layer_units;	// The number of units in the previous layer.
    size_t cur_layer_size;	// The size of the current layer.
    size_t prev_layer_size;	// The size of the previous layer.
    float* cur_layer_x;		// Input to the current layer non-linearity.
    float* cur_layer_y;		// Output from the current layer
				// non-linearity.
    const float* prev_layer_y;	// Output from the previous non-linearity.
    float* cur_layer_dydx;	// dydx for the current layer.
    float* cur_layer_dedy;	// dedy for the current layer.
    float* prev_layer_dedy;	// dedy for the previous layer.
    float* cur_layer_dedx;	// dedx for the current layer.
    float* cur_layer_bias;	// Biases for the current layer.
    float* cur_layer_delta_bias; // Delta biases for the current layer.
    float* cur_weights;		// Weights inputing to the current layer.


    // Iterate back over all layers but the first.
    for (cur_layer=n_layers-1; cur_layer>0; cur_layer--)
    {
	prev_layer = cur_layer - 1;
	cur_weinum = cur_layer - 1;
	cur_layer_units = layer_units[cur_layer];
	prev_layer_units = layer_units[prev_layer];
	cur_layer_size = cur_layer_units * n_frames;
	prev_layer_size = prev_layer_units * n_frames;
	cur_layer_x = layer_x[cur_layer];
	cur_layer_y = layer_y[cur_layer];
	if (cur_layer==1)
	    prev_layer_y = in;
	else
	    prev_layer_y = layer_y[prev_layer];
	cur_layer_dydx = layer_dydx[cur_layer];
	cur_layer_dedy = layer_dedy[cur_layer];
	prev_layer_dedy = layer_dedy[prev_layer];
	cur_layer_dedx = layer_dedx[cur_layer];
	cur_layer_bias = layer_bias[cur_layer];
	cur_layer_delta_bias = layer_delta_bias[cur_layer];
	cur_weights = weights[cur_weinum];

	float cur_neg_weight_learnrate = neg_weight_learnrate[cur_weinum];
	float cur_neg_bias_learnrate = neg_bias_learnrate[cur_layer];

	if (cur_layer!=n_layers - 1 && backprop_weights[cur_weinum+1])
	{
 	    // Propogate error back through sigmoid
 	    qn_dsigmoid_vf_vf(cur_layer_size, cur_layer_y, cur_layer_dydx);
 	    qn_mul_vfvf_vf(cur_layer_size, cur_layer_dydx, cur_layer_dedy,
			   cur_layer_dedx);
	}
	else
	{
	    // Going back through the output layer.
	    switch(out_layer_type)
	    {
	    case QN_OUTPUT_SIGMOID:
		// For a sigmoid layer, de/dx = de/dy . dy/dx
		qn_sub_vfvf_vf(cur_layer_size, out, target, cur_layer_dedy);
		qn_dsigmoid_vf_vf(cur_layer_size, out, cur_layer_dydx);
		qn_mul_vfvf_vf(cur_layer_size,
			       cur_layer_dydx, cur_layer_dedy, cur_layer_dedx);
		break;
	    case QN_OUTPUT_TANH:
		// tanh output layer very similar to sigmoid
		qn_sub_vfvf_vf(cur_layer_size, out, target, cur_layer_dedy);
		qn_dtanh_vf_vf(cur_layer_size, out, cur_layer_dydx);
		qn_mul_vfvf_vf(cur_layer_size,
			       cur_layer_dydx, cur_layer_dedy, cur_layer_dedx);
		break;
	    case QN_OUTPUT_SIGMOID_XENTROPY:
	    case QN_OUTPUT_SOFTMAX:
	    case QN_OUTPUT_LINEAR:
		// For these layers, dx = dy
		qn_sub_vfvf_vf(cur_layer_size, out, target, cur_layer_dedx);
		break;
	    default:
		assert(0);
	    } // End of output layer type switch.
	} // End of special output layer treatment.

	// Back propogate error through this layer.
	if (cur_layer!=1 && backprop_weights[cur_weinum])
	{
	    qn_mul_mfmf_mf(n_frames, cur_layer_units, prev_layer_units,
			   cur_layer_dedx, cur_weights, prev_layer_dedy);
	}
	// Update weights.
	if (cur_neg_weight_learnrate!=0.0f)
	{
	    qn_multnacc_fmfmf_mf(n_frames, cur_layer_units, prev_layer_units,
				 cur_neg_weight_learnrate, cur_layer_dedx,
				 prev_layer_y, cur_weights);
	}
	// Update biases.
	if (cur_neg_bias_learnrate!=0.0f)
	{
	    qn_sumcol_mf_vf(n_frames, cur_layer_units, cur_layer_dedx,
			    cur_layer_delta_bias); 
	    qn_mulacc_vff_vf(cur_layer_units, cur_layer_delta_bias,
			     cur_neg_bias_learnrate, cur_layer_bias);
	}
    } // End of iteration over all layers.
}

