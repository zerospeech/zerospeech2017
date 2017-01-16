const char* QN_MLP_BaseFl_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_BaseFl.cc,v 1.2 2011/03/03 00:32:58 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_MLP_BaseFl.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"


// A simple minimum routine

QN_MLP_BaseFl::QN_MLP_BaseFl(int a_debug, const char* a_dbgname,
			     const char* a_classname,
			     size_t a_size_bunch,
			     size_t a_n_layers,
			     size_t a_layer1_units,
			     size_t a_layer2_units,
			     size_t a_layer3_units,
			     size_t a_layer4_units,
			     size_t a_layer5_units)
    : clog(a_debug, a_classname, a_dbgname),
      n_layers(a_n_layers),
      n_weightmats(a_n_layers-1),
      n_sections((n_layers-1) + n_weightmats),
      size_bunch(a_size_bunch)
{
    size_t i;
    float nan = qn_nan_f();

    assert (MAX_LAYERS == MAX_WEIGHTMATS + 1);
   
    layer_units[0] = a_layer1_units;
    layer_units[1] = a_layer2_units;
    layer_units[2] = a_layer3_units;
    layer_units[3] = a_layer4_units;
    layer_units[4] = a_layer5_units;
    for (i=0; i<MAX_LAYERS; i++)
    {
	layer_size[i] = 0;
	layer_bias[i] = NULL;
	neg_bias_learnrate[i] = nan;
    }
    for (i=0; i<MAX_WEIGHTMATS; i++)
    {
	weights_size[i] = 0;
	weights[i] = NULL;
	neg_weight_learnrate[i] = nan;
	if (i>0)
	    backprop_weights[i] = 1;
	else
	    backprop_weights[i] = 0;
    }
    if (n_layers<2 || n_layers > MAX_LAYERS)
    {
	clog.error("Cannot create an MLP with <2 or >%lu layers.",
		 (unsigned long) MAX_LAYERS);
    }
    if (size_bunch == 0)
	clog.error("Cannot use a 0 bunch size.");

    // Set up the per-layer data structures.
    for (i = 0; i<n_layers; i++)
    {
	size_t units = layer_units[i];
	size_t size = units * size_bunch;
	if (units==0)
	    clog.error("Failed trying to create an MLP with a 0 unit layer.");
	layer_size[i] = size;

	if (i>0)
	{
	    layer_bias[i] = new float [size];
	    qn_copy_f_vf(size, nan, layer_bias[i]); // Fill with NaNs for
						    // safety
	}
    }
    // Set up the per-weight-matrix data structures.
    for (i = 0; i<n_weightmats; i++)
    {
	size_t n_weights;

	n_weights = layer_units[i] * layer_units[i+1];
	weights_size[i] = n_weights;
	weights[i] = new float[n_weights];
	qn_copy_f_vf(n_weights, nan, weights[i]);
    }

    clog.log(QN_LOG_PER_RUN,"Created net, n_layers=%lu bunchsize=%lu.",
	     (unsigned long) n_layers, (unsigned long) size_bunch);
    for (i=0; i<n_layers; i++)
    {
	clog.log(QN_LOG_PER_RUN, "layer_units[%lu]=%lu layer_size[%lu]=%lu.",
		 i, (unsigned long) layer_units[i],
		 i, (unsigned long) layer_size[i]);
    }
}

QN_MLP_BaseFl::~QN_MLP_BaseFl()
{
    size_t i;

    for (i = 0; i<n_layers; i++)
    {
	delete [] layer_bias[i];
    }
    for (i = 0; i<n_weightmats; i++)
    {
	delete [] weights[i];
    }
}

size_t
QN_MLP_BaseFl::size_layer(QN_LayerSelector layer) const
{
    size_t r = 0;

    switch(layer)
    {
    case QN_LAYER1:
	r = layer_units[0];
	break;
    case QN_LAYER2:
	r = layer_units[1];
	break;
    case QN_LAYER3:
	if (n_layers>2)
	    r = layer_units[2];
	break;
    case QN_LAYER4:
	if (n_layers>3)
	    r = layer_units[3];
	break;
    case QN_LAYER5:
	if (n_layers>4)
	    r = layer_units[4];
	break;
    default:
	break;
    }
    return r;
}




void
QN_MLP_BaseFl::size_section(QN_SectionSelector section, size_t* output_p,
			      size_t* input_p) const
{
    // Default to 0 for unavailable sections.
    size_t input = 0;
    size_t output = 0;
    switch(section)
    {
    case QN_LAYER12_WEIGHTS:
	output = layer_units[1];
	input = layer_units[0];
	break;
    case QN_LAYER2_BIAS:
	output = layer_units[1];
	input = 1;
	break;
    case QN_LAYER23_WEIGHTS:
	if (n_layers>2)
	{
	    output = layer_units[2];
	    input = layer_units[1];
	}
	break;
    case QN_LAYER3_BIAS:
	if (n_layers>2)
	{
	    output = layer_units[2];
	    input = 1;
	}
	break;
    case QN_LAYER34_WEIGHTS:
	if (n_layers>3)
	{
	    output = layer_units[3];
	    input = layer_units[2];
	}
	break;
    case QN_LAYER4_BIAS:
	if (n_layers>3)
	{
	    output = layer_units[3];
	    input = 1;
	}
	break;
    case QN_LAYER45_WEIGHTS:
	if (n_layers>4)
	{
	    output = layer_units[4];
	    input = layer_units[3];
	}
	break;
    case QN_LAYER5_BIAS:
	if (n_layers>4)
	{
	    output = layer_units[4];
	    input = 1;
	}
	break;
    default:
	assert(0);
    }
    *input_p = input;
    *output_p = output;
}

size_t
QN_MLP_BaseFl::num_connections() const
{
    size_t i;
    size_t cons = 0;

    // For some reason our number of connections includes biases.
    // This is the same as the other MLP classes.
    // Note the biases are not used in the first layer.
    for (i = 1; i<n_layers; i++)
	cons += layer_units[i];
    for (i=0; i<n_weightmats; i++)
	cons += layer_units[i] * layer_units[i+1];

    return cons;
}


size_t
QN_MLP_BaseFl::get_bunchsize() const
{
    return size_bunch;
}

void
QN_MLP_BaseFl::forward(size_t n_frames, const float* in, float* out)
{
    size_t i;
    size_t frames_this_bunch;	// Number of frames to handle this bunch
    size_t n_input = layer_units[0];
    size_t n_output = layer_units[n_layers - 1];

    for (i=0; i<n_frames; i += size_bunch)
    {
	frames_this_bunch = qn_min_zz_z(size_bunch, n_frames - i);
	
	forward_bunch(frames_this_bunch, in, out);
	in += n_input * frames_this_bunch;
	out += n_output * frames_this_bunch;
    }
}

void
QN_MLP_BaseFl::train(size_t n_frames, const float* in, const float* target,
		    float* out)
{
    size_t i;
    size_t frames_this_bunch;	// Number of frames to handle this bunch
    size_t n_input = layer_units[0];
    size_t n_output = layer_units[n_layers - 1];

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
QN_MLP_BaseFl::set_learnrate(enum QN_SectionSelector which, float learnrate)
{
    size_t i;

    switch(which)
    {
    case QN_LAYER12_WEIGHTS:
	neg_weight_learnrate[0] = -learnrate;
	break;
    case QN_LAYER23_WEIGHTS:
	assert(n_layers>2);
	neg_weight_learnrate[1] = -learnrate;
	break;
    case QN_LAYER34_WEIGHTS:
	assert(n_layers>3);
	neg_weight_learnrate[2] = -learnrate;
	break;
    case QN_LAYER45_WEIGHTS:
	assert(n_layers>4);
	neg_weight_learnrate[3] = -learnrate;
	break;
    case QN_LAYER2_BIAS:
	neg_bias_learnrate[1] = -learnrate;
	break;
    case QN_LAYER3_BIAS:
	assert(n_layers>2);
	neg_bias_learnrate[2] = -learnrate;
	break;
    case QN_LAYER4_BIAS:
	assert(n_layers>3);
	neg_bias_learnrate[3] = -learnrate;
	break;
    case QN_LAYER5_BIAS:
	assert(n_layers>4);
	neg_bias_learnrate[4] = -learnrate;
	break;
    default:
	assert(0);
	break;
    }
    // Work out if we have whole weight matrices that do not need to
    // be updated due to 0 learning rates.
    for (i=1; i<n_weightmats; i++)
	backprop_weights[i] = 1;
    for (i=1; i<n_weightmats; i++)
    {
	if ((neg_weight_learnrate[i-1]==0.0f) && (neg_bias_learnrate[i]==0.0f))
	    backprop_weights[i] = 0;
	else
	    // If any non-zero learnrates, all following layers need backprop.
	    break;
    }
	
}

float 
QN_MLP_BaseFl::get_learnrate(enum QN_SectionSelector which) const
{
    float res;			// Returned learning rate

    switch(which)
    {
    case QN_LAYER12_WEIGHTS:
	res = -neg_weight_learnrate[0];
	break;
    case QN_LAYER23_WEIGHTS:
	assert(n_layers>2);
	res = -neg_weight_learnrate[1];
	break;
    case QN_LAYER34_WEIGHTS:
	assert(n_layers>3);
	res = -neg_weight_learnrate[2];
	break;
    case QN_LAYER45_WEIGHTS:
	assert(n_layers>4);
	res = -neg_weight_learnrate[3];
	break;
    case QN_LAYER2_BIAS:
	res = -neg_bias_learnrate[1];
	break;
    case QN_LAYER3_BIAS:
	assert(n_layers>2);
	res = -neg_bias_learnrate[2];
	break;
    case QN_LAYER4_BIAS:
	assert(n_layers>3);
	res = -neg_bias_learnrate[3];
	break;
    case QN_LAYER5_BIAS:
	assert(n_layers>4);
	res = -neg_bias_learnrate[4];
	break;
    default:
	res = 0;
	assert(0);
	break;
    }
    return res;
}



void
QN_MLP_BaseFl::set_weights(enum QN_SectionSelector which,
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
QN_MLP_BaseFl::get_weights(enum QN_SectionSelector which,
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


const char*
QN_MLP_BaseFl::nameweights(QN_SectionSelector which)
{
    switch(which)
    {
    case QN_LAYER12_WEIGHTS:
	return "layer12_weights";
	break;
    case QN_LAYER23_WEIGHTS:
	return "layer23_weights";
	break;
    case QN_LAYER34_WEIGHTS:
	return "layer34_weights";
	break;
    case QN_LAYER45_WEIGHTS:
	return "layer45_weights";
	break;
    case QN_LAYER2_BIAS:
	return "layer2_bias";
	break;
    case QN_LAYER3_BIAS:
	return "layer3_bias";
	break;
    case QN_LAYER4_BIAS:
	return "layer4_bias";
	break;
    case QN_LAYER5_BIAS:
	return "layer5_bias";
	break;
    default:
	assert(0);
	return "unknown";
	break;
    }
}


float*
QN_MLP_BaseFl::findweights(QN_SectionSelector which,
			     size_t row, size_t col,
			     size_t n_rows, size_t n_cols,
			     size_t* total_cols_p) const
{
// Note - for a weight matrix, inputs are across a row, outputs are
// ..down a column

    float *wp;			// Pointer to bit of weight matrix requested
    size_t total_rows;		// The number of rows in the selected matrix
    size_t total_cols;		// The number of cols in the selected matrix

    size_section(which, &total_rows, &total_cols);
    switch(which)
    {
    case QN_LAYER12_WEIGHTS:
	wp = weights[0];
	break;
    case QN_LAYER23_WEIGHTS:
	assert(n_weightmats>1);
	wp = weights[1];
	break;
    case QN_LAYER34_WEIGHTS:
	assert(n_weightmats>2);
	wp = weights[2];
	break;
    case QN_LAYER45_WEIGHTS:
	assert(n_weightmats>3);
	wp = weights[3];
	break;
    case QN_LAYER2_BIAS:
	wp = layer_bias[1];
	break;
    case QN_LAYER3_BIAS:
	assert(n_layers>2);
	wp = layer_bias[2];
	break;
    case QN_LAYER4_BIAS:
	assert(n_layers>3);
	wp = layer_bias[3];
	break;
    case QN_LAYER5_BIAS:
	assert(n_layers>4);
	wp = layer_bias[4];
	break;
    default:
	wp = NULL;
	assert(0);
    }
    assert(row<total_rows);
    assert(row+n_rows<=total_rows);
    assert(col<total_cols);
    assert(col+n_cols<=total_cols);
    wp += col + row * total_cols;
    *total_cols_p = total_cols;
    return(wp);
}



