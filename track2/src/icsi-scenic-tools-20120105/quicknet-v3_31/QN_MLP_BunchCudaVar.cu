const char* QN_MLP_BunchCudaVar_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_BunchCudaVar.cu,v 1.5 2011/05/24 02:03:14 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_CudaUtils.h"
#include "QN_MLP_BunchCudaVar.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"
#include "QN_cuvec.h"

#include <cuda.h>
#include <cublas.h>


// These appear later but we do not want them in the header file
// __global__ void QN_BunchCudaVar_forward_bunch(QN_BunchCudaVar_Workspace *ws,
// 					      int n_frames);
// __global__ void QN_BunchCudaVar_train_bunch(QN_BunchCudaVar_Workspace *ws,
// 					    int n_frames);
// __device__ void QN_BunchCudaVar_forward_bunch_do(QN_BunchCudaVar_Workspace *ws,
//					      int n_frames);


QN_MLP_BunchCudaVar::QN_MLP_BunchCudaVar(int a_debug,
					 const char* a_dbgname,
					 size_t a_n_layers,
					 const size_t a_layer_units[QN_MLP_MAX_LAYERS],
					 enum QN_OutputLayerType a_outtype,
					 size_t a_size_bunch)
    : QN_MLP_BaseFl(a_debug, a_dbgname, "QN_MLP_BunchCudaVar",
		    a_size_bunch, a_n_layers,
		    a_layer_units[0], a_layer_units[1],
		    a_layer_units[2], a_layer_units[3], a_layer_units[4]),
      out_layer_type(a_outtype)
{
    size_t i;

    // Initialize CUDA if it has not happened already

    QN_cuda_init();
    
    // Some stuff so that when things go wrong it is more obvious.
    // for (i=0; i<MAX_LAYERS; i++)
    // {
    // 	layer_x[i] = NULL;
    // 	layer_y[i] = NULL;
    // 	layer_dedy[i] = NULL;
    // 	layer_dydx[i] = NULL;
    // 	layer_dedx[i] = NULL;
    // 	layer_delta_bias[i] = NULL;
    // }
    // Maybe we do not support all output layer types
    switch(out_layer_type)
    {
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_LINEAR:
    case QN_OUTPUT_TANH:
	break;
    default:
	clog.error("Failed to create an MLP with an invalid"
		   " output layer type.");
    }
    if (size_bunch == 0)
	clog.error("Cannot use a 0 bunch size.");


    // Allocate device data structures

    size_t in_size = layer_size[0];
    size_t out_size = layer_size[n_layers-1];

    devnew_vf("in", in_size, &(dev.in));
    devnew_vf("out", out_size, &(dev.out));
    devnew_vf("targ", out_size, &(dev.targ));

    for (i = 1; i<n_layers; i++)
    {
	size_t size = layer_size[i];
	size_t units = layer_units[i];

	devnew_vf("layer_bias", size, &(dev.layer_bias[i]));
	devnew_vf("layer_y", size, &(dev.layer_y[i]));
	devnew_vf("layer_x", size, &(dev.layer_x[i]));
	devnew_vf("layer_dedy", size, &(dev.layer_dedy[i]));
	devnew_vf("layer_dydx", size, &(dev.layer_dydx[i]));
	devnew_vf("layer_dedx", size, &(dev.layer_dedx[i]));
	devnew_vf("layer_delta_bias", units, &(dev.layer_delta_bias[i]));
    }
    // Set up the per-weight-matrix data structures.
    for (i = 0; i<n_weightmats; i++)
    {
	// Note the host weights are alloacted by QN_MLP_BaseFl 
	size_t n_weights = weights_size[i];

	// Allocate device data structures
	devnew_vf("weights", n_weights, &dev.weights[i]);
    }

    clog.log(QN_LOG_PER_RUN, "Created net with %lu layers, bunchsize %lu.",
	     n_layers, size_bunch);
    for (i=0; i<n_layers; i++)
    {
	clog.log(QN_LOG_PER_RUN, "Layer %lu has %lu units.",
		 i+1, layer_units[i]);
    }
    dev_weights_stale = QN_TRUE;
    host_weights_stale = QN_FALSE;

}

QN_MLP_BunchCudaVar::~QN_MLP_BunchCudaVar()
{
    size_t i;

    QN_cuda_check();
    // Wind down the per-weight-matrix data structures.
    for (i = 0; i<n_weightmats; i++)
    {
	// Deallocate device data structures
	devfree_vf("weights", dev.weights[i]);
	// Note the host weights are deallocated by QN_MLP_BaseFl 
    }
    // Wind down the per-layer data structures.
    for (i = 1; i<n_layers; i++)
    {
	// delete [] layer_y[i];
	// delete [] layer_delta_bias[i];
	// delete [] layer_dedx[i];
	// delete [] layer_dydx[i];
	// delete [] layer_dedy[i];
	// delete [] layer_x[i];
	// Note the host biases are deallocated by QN_MLP_BaseFl 

	devfree_vf("layer_delta_bias", dev.layer_delta_bias[i]);
	devfree_vf("layer_dedx", dev.layer_dedx[i]);
	devfree_vf("layer_dydx", dev.layer_dydx[i]);
	devfree_vf("layer_dedy", dev.layer_dedy[i]);
	devfree_vf("layer_x", dev.layer_x[i]);
	devfree_vf("layer_y", dev.layer_y[i]);
	devfree_vf("layer_bias", dev.layer_bias[i]);
    }
    devfree_vf("targ", dev.targ);
    devfree_vf("out", dev.out);
    devfree_vf("in", dev.in);
}



void
QN_MLP_BunchCudaVar::forward_bunch(size_t n_frames, const float* in, float* out)
{
//    printf("in=%x, out=%x\n", in, out);

    // Copy the data across to the device
    int in_size = n_frames * layer_units[0];
    int out_size = n_frames * layer_units[n_layers-1];
    todev_vf_vf("forward_bunch().in", in_size, in, dev.in);

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
	cur_layer_x = dev.layer_x[cur_layer];
	cur_layer_y = dev.layer_y[cur_layer];
	if (cur_layer==1)
	    prev_layer_y = dev.in;
	else
	    prev_layer_y = dev.layer_y[prev_layer];
	cur_layer_bias = dev.layer_bias[cur_layer];
	cur_weights = dev.weights[cur_weinum];

	if (checking)
	    devcheck("forward_bunch #1");
	qn_dev_copy_vf_mf(n_frames, cur_layer_units, cur_layer_bias,
			    cur_layer_x);
	if (checking)
	    devcheck("forward_bunch #2");
	qn_dev_mulntacc_mfmf_mf(n_frames, prev_layer_units, cur_layer_units,
				prev_layer_y, cur_weights,
				cur_layer_x); 
	if (checking)
	    devcheck("forward_bunch #3");
	
	// Check if we are doing things differently for the final layer.
	if (cur_layer!=n_layers - 1)
	{
	    // This is the intermediate layer non-linearity.
	    qn_dev_sigmoid_vf_vf(cur_layer_size, cur_layer_x,
				 cur_layer_y);
	    if (checking)
		devcheck("forward_bunch #4");
	}
	else
	{
	    // This is the output layer non-linearity.
	    switch(out_layer_type)
	    {
	    case QN_OUTPUT_SIGMOID:
	    case QN_OUTPUT_SIGMOID_XENTROPY:
		qn_dev_sigmoid_vf_vf(cur_layer_size, cur_layer_x, dev.out);
		if (checking)
		    devcheck("forward_bunch #5");
		break;
	    case QN_OUTPUT_SOFTMAX:
		qn_dev_multisoftmax_mf_mf(n_frames, cur_layer_units,
					  cur_layer_x, dev.out);
		if (checking)
		    devcheck("forward_bunch #6");
		break;
	    case QN_OUTPUT_LINEAR:
		qn_dev_copy_vf_vf(cur_layer_size, cur_layer_x, dev.out);
		if (checking)
		    devcheck("forward_bunch #7");
		break;
	    case QN_OUTPUT_TANH:
		qn_dev_tanh_vf_vf(cur_layer_size, cur_layer_x, dev.out);
		if (checking)
		    devcheck("forward_bunch #8");
		break;
	    default:
		assert(0);
	    }
	}
    }
    // Copy the data back from the device
    fromdev_vf_vf("forward_bunch().out", out_size, dev.out, out);
    if (checking)
	devcheck("forward_bunch #9");

}

void
QN_MLP_BunchCudaVar::train_bunch(size_t n_frames, const float *in,
				 const float* target, float* out)
{
// First move forward, which copies over in and out
    forward_bunch(n_frames, in, out);
    if (checking)
	devcheck("train_bunch #0");

// So we stil have to copy across targ
    int out_size = n_frames * layer_units[n_layers-1];
    todev_vf_vf("train_bunch().targ", out_size, target, dev.targ);
    if (checking)
	devcheck("train_bunch #1");

    size_t cur_layer;		// The index of the current layer.
    size_t prev_layer;		// The index of the previous layer.
    size_t cur_weinum;		// The index of the current weight matrix.
    size_t cur_layer_units;	// The number of units in the current layer.
    size_t prev_layer_units;	// The number of units in the previous layer.
    size_t cur_layer_size;	// The size of the current layer.
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
	cur_layer_y = dev.layer_y[cur_layer];
	if (cur_layer==1)
	    prev_layer_y = dev.in;
	else
	    prev_layer_y = dev.layer_y[prev_layer];
	cur_layer_dydx = dev.layer_dydx[cur_layer];
	cur_layer_dedy = dev.layer_dedy[cur_layer];
	prev_layer_dedy = dev.layer_dedy[prev_layer];
	cur_layer_dedx = dev.layer_dedx[cur_layer];
	cur_layer_bias = dev.layer_bias[cur_layer];
	cur_layer_delta_bias = dev.layer_delta_bias[cur_layer];
	cur_weights = dev.weights[cur_weinum];

	float cur_neg_weight_learnrate = neg_weight_learnrate[cur_weinum];
	float cur_neg_bias_learnrate = neg_bias_learnrate[cur_layer];

	if (cur_layer!=n_layers - 1 && backprop_weights[cur_weinum+1])
	{
 	    // Propogate error back through sigmoid
 	    qn_dev_dsigmoid_vf_vf(cur_layer_size, cur_layer_y, cur_layer_dydx);
	    if (checking)
		devcheck("train_bunch #3");
 	    qn_dev_mul_vfvf_vf(cur_layer_size, cur_layer_dydx, cur_layer_dedy,
			       cur_layer_dedx);
	    if (checking)
		devcheck("train_bunch #4");
	}
	else
	{
	    // Going back through the output layer.
	    switch(out_layer_type)
	    {
	    case QN_OUTPUT_SIGMOID:
		// For a sigmoid layer, de/dx = de/dy . dy/dx
		qn_dev_sub_vfvf_vf(cur_layer_size, dev.out, dev.targ,
				   cur_layer_dedy);
		if (checking)
		    devcheck("train_bunch #5");
		qn_dev_dsigmoid_vf_vf(cur_layer_size, dev.out, cur_layer_dydx);
		if (checking)
		    devcheck("train_bunch #6");
		qn_dev_mul_vfvf_vf(cur_layer_size, cur_layer_dydx,
				   cur_layer_dedy, cur_layer_dedx);
		if (checking)
		    devcheck("train_bunch #7");
		break;
	    case QN_OUTPUT_TANH:
		// tanh output layer very similar to sigmoid
		qn_dev_sub_vfvf_vf(cur_layer_size, dev.out, dev.targ,
				   cur_layer_dedy);
		if (checking)
		    devcheck("train_bunch #8");
		qn_dev_dtanh_vf_vf(cur_layer_size, dev.out, cur_layer_dydx);
		if (checking)
		    devcheck("train_bunch #9");
		qn_dev_mul_vfvf_vf(cur_layer_size,
			       cur_layer_dydx, cur_layer_dedy, cur_layer_dedx);
		if (checking)
		    devcheck("train_bunch #10");
		break;
	    case QN_OUTPUT_SIGMOID_XENTROPY:
	    case QN_OUTPUT_SOFTMAX:
	    case QN_OUTPUT_LINEAR:
		// For these layers, dx = dy
		qn_dev_sub_vfvf_vf(cur_layer_size, dev.out, dev.targ,
				   cur_layer_dedx);
		if (checking)
		    devcheck("train_bunch #11");
		break;
	    default:
		assert(0);
	    } // End of output layer type switch.
	} // End of special output layer treatment.

	// Back propogate error through this layer.
	if (cur_layer!=1 && backprop_weights[cur_weinum])
	{
	    qn_dev_mul_mfmf_mf(n_frames, cur_layer_units, prev_layer_units,
			   cur_layer_dedx, cur_weights, prev_layer_dedy);
	    if (checking)
		devcheck("train_bunch #12");
	}
	// Update weights.
	if (cur_neg_weight_learnrate!=0.0f)
	{
	    qn_dev_multnacc_fmfmf_mf(n_frames, cur_layer_units, prev_layer_units,
				 cur_neg_weight_learnrate, cur_layer_dedx,
				 prev_layer_y, cur_weights);
	    if (checking)
		devcheck("train_bunch #13");
	}
	// Update biases.
	if (cur_neg_bias_learnrate!=0.0f)
	{
	    qn_dev_sumcol_mf_vf(n_frames, cur_layer_units, cur_layer_dedx,
				cur_layer_delta_bias); 
	    if (checking)
		devcheck("train_bunch #14");
	    qn_dev_mulacc_vff_vf(cur_layer_units, cur_layer_delta_bias,
				 cur_neg_bias_learnrate, cur_layer_bias);
	    if (checking)
		devcheck("train_bunch #15");
	}
    } // End of iteration over all layers.


    // Copy the data back from the device
    fromdev_vf_vf("train_bunch().out", out_size, dev.out, out);
    if (checking)
	devcheck("train_bunch #16");

}

void
QN_MLP_BunchCudaVar::forward(size_t n_frames, const float* in, float* out)
{
    refresh_dev_weights();
    QN_MLP_BaseFl::forward(n_frames, in, out);
}

void
QN_MLP_BunchCudaVar::train(size_t n_frames, const float* in,
			   const float* target, float* out)
{
    refresh_dev_weights();
    QN_MLP_BaseFl::train(n_frames, in, target, out);
    host_weights_stale = QN_TRUE;
}

void
QN_MLP_BunchCudaVar::set_weights(enum QN_SectionSelector which,
				 size_t row, size_t col,
				 size_t n_rows, size_t n_cols,
				 const float* weights)
{
    refresh_host_weights();
    QN_MLP_BaseFl::set_weights(which, row, col, n_rows, n_cols, weights);
    dev_weights_stale = QN_TRUE;
}


void
QN_MLP_BunchCudaVar::get_weights(enum QN_SectionSelector which,
				 size_t row, size_t col,
				 size_t n_rows, size_t n_cols,
				 float* weights)
{
    refresh_host_weights();
    QN_MLP_BaseFl::get_weights(which, row, col, n_rows, n_cols, weights);
}

void
QN_MLP_BunchCudaVar::refresh_dev_weights(void)
{
    if (dev_weights_stale)
    {
	dev_weights_stale = QN_FALSE;

	size_t i;

	for (i = 0; i<n_weightmats; i++)
	{
	    size_t n_weights;

	    n_weights = weights_size[i]; 
	    todev_vf_vf("refresh_dev_weights().weights",
			n_weights, weights[i], dev.weights[i]);
	}

	for (i = 1; i<n_layers; i++)
	{
	    size_t n_biases;

	    n_biases = layer_size[i];
	    todev_vf_vf("refresh_dev_weights().layer_bias",
			n_biases, layer_bias[i], dev.layer_bias[i]);
	}
    }
}

void
QN_MLP_BunchCudaVar::refresh_host_weights(void)
{
    if (host_weights_stale)
    {
	host_weights_stale = QN_FALSE;

	size_t i;

	for (i = 0; i<n_weightmats; i++)
	{
	    size_t n_weights;

	    n_weights = weights_size[i]; 
	    fromdev_vf_vf("refresh_host_weights.weights)",
			  n_weights, dev.weights[i], weights[i]);
	}

	for (i = 1; i<n_layers; i++)
	{
	    size_t n_biases;

	    n_biases = layer_size[i];
	    fromdev_vf_vf("freresh_host_weights().layer_bias", 
			   n_biases, dev.layer_bias[i], layer_bias[i]);
	}
    }
}

void
QN_MLP_BunchCudaVar::devnew_vf(const char* varname, int n, float **devptr)
{
    cublasStatus e;

    e = cublasAlloc(n, sizeof(float), (void **) devptr);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas device new_vf error variable %s - %s.",
		   varname, QN_cublas_error_string(e));
    }
    clog.log(QN_LOG_PER_EPOCH, "Created CUDA float vec \"%s\" size %i at %.8x\n", varname, n, (unsigned long) *devptr);
}

void
QN_MLP_BunchCudaVar::devnew_vi(const char* varname, int n, int **devptr)
{
    cublasStatus e;

    e = cublasAlloc(n, sizeof(int), (void **) devptr);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas device new_vi error variable %s - %s.",
		   varname, QN_cublas_error_string(e));
    }
    clog.log(QN_LOG_PER_EPOCH, "Created CUDA int vec \"%s\" size %i at %.8x\n", varname, n, (unsigned long) *devptr);

}


void 
QN_MLP_BunchCudaVar::devcheck(const char* location)
{
    cudaError_t e;

    e = cudaThreadSynchronize();
    if (e!=cudaSuccess)
    {
	clog.error("asynchronous CUDA error at %s - %s.",
		   location, cudaGetErrorString(e));
    }
    
    cublasStatus eb;

    eb = cublasGetError();
    if (eb!=CUBLAS_STATUS_SUCCESS)
	QN_ERROR("QN_cuda_check", "accumulated cublas error detected");
}

void
QN_MLP_BunchCudaVar::devnew(const char* varname, int n, int size,
			    void **devptr)
{
    cublasStatus e;

    e = cublasAlloc(n, size, devptr);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blasw device free error variable %s - %s.",
		   varname, QN_cublas_error_string(e));
    }

}

void
QN_MLP_BunchCudaVar::devfree(const char* varname, const void* devptr)
{
    cublasStatus e;
    e = cublasFree(devptr);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas device free error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
}

void
QN_MLP_BunchCudaVar::devfree_vf(const char* varname, const float* devptr)
{
    cublasStatus e;
    e = cublasFree((void *) devptr);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas device free_vf error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
}

void
QN_MLP_BunchCudaVar::devfree_vi(const char* varname, const int* devptr)
{
    cublasStatus e;
    e = cublasFree((void *) devptr);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas device free_vf error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
}

void
QN_MLP_BunchCudaVar::todev_vf_vf(const char* varname, int n, const float* from,
				 float* devto)
{
    cublasStatus e;

    e = cublasSetVector(n, sizeof(float), from, 1, devto, 1);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas todev_vf_vf error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
    clog.log(QN_LOG_PER_BUNCH, "Copied %i floats to device variable \"%s\" at address %.8x\n", n, varname, devto);
}

void
QN_MLP_BunchCudaVar::fromdev_vf_vf(const char* varname, int n,
				   const float* devfrom, float* to)
{
    cublasStatus e;

    e = cublasGetVector(n, sizeof(float), devfrom, 1, to, 1);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas fromdev_vf_vf error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
    clog.log(QN_LOG_PER_BUNCH, "Copied %i floats from device variable \"%s\" at address %.8x\n", n, varname, devfrom);
}

void
QN_MLP_BunchCudaVar::todev_vi_vi(const char* varname, int n,
				 const int* from, int* devto)
{
    cublasStatus e;

    e = cublasSetVector(n, sizeof(int), from, 1, devto, 1);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas todev_vi_vi error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
    clog.log(QN_LOG_PER_BUNCH, "Copied %i ints to device variable \"%s\" at address %.8x\n", n, varname, devto);
}

void
QN_MLP_BunchCudaVar::fromdev_vi_vi(const char* varname, int n,
				   const int* devfrom, int* to)
{
    cublasStatus e;

    e = cublasGetVector(n, sizeof(int), devfrom, 1, to, 1);
    if (e != CUBLAS_STATUS_SUCCESS)
    {
	clog.error("cuda blas fromdev_vi_vi error variable %s - %s.",
		   varname, QN_cublas_error_string(e)); 
    }
    clog.log(QN_LOG_PER_BUNCH, "Copied %i ints from device variable \"%s\" at address %.8x\n", n, varname, devfrom);
}




