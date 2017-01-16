const char* QN_MLP_ThreadFl3_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_ThreadFl3.cc,v 1.11 2006/03/09 05:18:47 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_MLP_ThreadFl3.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"



#ifdef QN_HAVE_LIBPTHREAD

// The actual thread worker routine, defined below
extern "C" {
	static void* worker_wrapper(void*);
};

QN_MLP_ThreadFl3::QN_MLP_ThreadFl3(int a_debug,
				   const char* a_dbgname,
				   size_t a_input, size_t a_hidden,
				   size_t a_output,
				   enum QN_OutputLayerType a_outtype,
				   size_t a_size_bunch,
				   size_t a_threads)
    : QN_MLP_BaseFl(a_debug, a_dbgname, "QN_MLP_ThreadFl3",
		    a_size_bunch, 3, a_input, a_hidden, a_output, 0, 0),
      n_input(layer_units[0]),
      n_hidden(layer_units[1]),
      n_output(layer_units[2]),
      n_in2hid(weights_size[0]),
      n_hid2out(weights_size[1]),
      size_input(layer_size[0]),
      size_hidden(layer_size[1]),
      size_output(layer_size[2]),
      do_backprop(backprop_weights[1]),
      out_layer_type(a_outtype),
      in2hid(weights[0]),
      hid_bias(layer_bias[1]),
      hid2out(weights[1]),
      out_bias(layer_bias[2]),
      neg_in2hid_learnrate(neg_weight_learnrate[0]),
      neg_hid2out_learnrate(neg_weight_learnrate[1]),
      neg_hid_learnrate(neg_bias_learnrate[1]),
      neg_out_learnrate(neg_bias_learnrate[2]),
      num_threads(a_threads)
{
    int ec;
    size_t i;

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
    assert(num_threads>0);

    // Create arrays of pointers to thread-specific weights and biases
    per_thread = new PerThread[num_threads];

    threadp = new float* [num_threads];
    for (i = 0; i<num_threads; i++)
	threadp[i] = NULL;

    // Some temp work space.
    // Cannot have more threads than bunch size.
    assert(num_threads<=size_bunch);
    threads = new pthread_t[num_threads];

    // Set up the "action" variable/mutex/cv worker thread command channel
    action_seq = 0;
    ec = pthread_mutex_init(&action_mutex, NULL);
    if (ec)
        clog.error("failed to init action_mutex");
    ec = pthread_cond_init(&action_cv, NULL);
    if (ec)
        clog.error("failed to init action_cv");
    done_count = 0;
    ec = pthread_mutex_init(&done_mutex, NULL);
    if (ec)
        clog.error("failed to init done_mutex");
    ec = pthread_cond_init(&done_cv, NULL);
    if (ec)
        clog.error("failed to init done_cv");

    // Create threads, make them joinable
    pthread_attr_t attr;
    ec = pthread_attr_init(&attr);
    assert(ec==0);
    ec = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    assert(ec==0);
    worker_args = new QN_MLP_ThreadFl3_WorkerArg[num_threads];
    for (i=0; i<num_threads; i++)
    {
	worker_args[i].threadno = i;
	worker_args[i].mlp = this;
	clog.log(QN_LOG_PER_RUN,"Creating thread %lu.", i);
	ec = pthread_create(&threads[i], &attr, worker_wrapper,
			    (void*) &worker_args[i]);
	if (ec)
	{
	    clog.error("failed to create thread number %lu - %s.",
		       i, strerror(ec));
	}
    }

    clog.log(QN_LOG_PER_RUN,"Created net, n_input=%lu n_hidden=%lu "
	     "n_output=%lu.", n_input, n_hidden, n_output);
    clog.log(QN_LOG_PER_RUN, "bunchsize=%lu size_input=%lu "
	     "size_hidden=%lu size_output=%lu.\n",
	     size_bunch, size_input, size_hidden, size_output);
    clog.log(QN_LOG_PER_RUN, "num_threads=%lu.", num_threads);
}

QN_MLP_ThreadFl3::~QN_MLP_ThreadFl3()
{
    int ec;

    clog.log(QN_LOG_PER_RUN,"Terminating threads.");
    tell_workers("destructor", ACTION_EXIT);
    wait_on_workers("destructor");

    // Wait for all of the worker threads to die
    size_t i;
    for (i = 0; i<num_threads; i++)
    {
	clog.log(QN_LOG_PER_RUN,"Waiting for end of thread %lu.", i);
        ec = pthread_join(threads[i], NULL);
	if (ec)
	    clog.error("failed to join thread %lu - %s.", i, strerror(ec));
    }
    delete[] worker_args;

    // Kill thead-related variables
    ec = pthread_cond_destroy(&done_cv);
    if (ec)
        clog.error("failed to destroy done_cv.");
    ec = pthread_mutex_destroy(&done_mutex);
    if (ec)
        clog.error("failed to destroy done_mutex.");
    ec = pthread_cond_destroy(&action_cv);
    if (ec)
        clog.error("failed to destroy action_cv.");
    ec = pthread_mutex_destroy(&action_mutex);
    if (ec)
        clog.error("failed to destroy action_mutex.");

    delete [] threads;
    delete [] threadp;
    delete [] per_thread;
}


void
QN_MLP_ThreadFl3::forward_bunch(size_t n_frames, const float* in, float* out)
{
    clog.log(QN_LOG_PER_BUNCH, "forward_bunch sending ACTION_FORWARD.");
    action_n_frames = n_frames;
    action_in = in;
    action_out = out;
    tell_workers("forward_bunch", ACTION_FORWARD);
    // ....workers work...
    wait_on_workers("forward_bunch");
    clog.log(QN_LOG_PER_BUNCH, "forward_bunch workers claim they are done.");
}

void
QN_MLP_ThreadFl3::train_bunch(size_t n_frames, const float *in,
			     const float* target, float* out)
{

    clog.log(QN_LOG_PER_BUNCH, "train_bunch sending ACTION_TRAIN.");
    action_n_frames = n_frames;
    action_in = in;
    action_out = out;
    action_target = target;
    tell_workers("train_bunch", ACTION_TRAIN);
    // ....workers work...
    wait_on_workers("train_bunch");
    clog.log(QN_LOG_PER_BUNCH, "train_bunch workers claim they are done.");

    // He we merge all of the deltas.
    // NOTE: The loop ordering is optimized for cache efficiency.
    size_t thread;
    size_t i;
    float* accp;
    float sum;

// Below we do the optimized version of:
//     for (thread = 0; thread<num_threads; thread++)
//     {
// 	qn_mulacc_vff_vf(n_output, out_bias_deltas[thread], neg_out_learnrate,
// 			 out_bias);
//     }
    if (neg_out_learnrate!=0.0f)
    {
	for (thread = 0 ; thread<num_threads; thread++)
	    threadp[thread] = per_thread[thread].out_bias_deltas;
	accp = out_bias;
	for (i = 0; i<n_output; i++)
	{
	    
	    sum = *threadp[0];
	    threadp[0] += 1;
	    for (thread = 1; thread<num_threads; thread++)
	    {
		sum += *threadp[thread];
		threadp[thread] += 1;
	    }
	    (*accp++) += sum * neg_out_learnrate;
	}
    }
// Below we do the optimized version of:
//     for (thread = 0; thread<num_threads; thread++)
//     {
// 	qn_acc_vf_vf(n_hid2out, hid2out_deltas[thread], hid2out);
//     }
    if (neg_hid2out_learnrate!=0.0f)
    {
	for (i =0; i<num_threads; i++)
	    threadp[i] = per_thread[i].hid2out_deltas;
	accp = hid2out;
	for (i = 0; i<n_hid2out; i++)
	{
	    sum = *threadp[0];
	    (threadp[0])++;
	    for (thread = 1; thread<num_threads; thread++)
	    {
		sum += *threadp[thread];
		threadp[thread]++;
	    }
	    (*accp++) += sum;
	}
    }
    
//     for (thread = 0; thread<num_threads; thread++)
//     {
// 	qn_mulacc_vff_vf(n_hidden, hid_bias_deltas[thread], neg_hid_learnrate,
// 			 hid_bias);
//     }
    if (neg_hid_learnrate!=0.0f)
    {
	for (thread = 0 ; thread<num_threads; thread++)
	    threadp[thread] = per_thread[thread].hid_bias_deltas;
	accp = hid_bias;
	for (i = 0; i<n_hidden; i++)
	{

	    sum = *threadp[0];
	    threadp[0] += 1;
	    for (thread = 1; thread<num_threads; thread++)
	    {
		sum += *threadp[thread];
		threadp[thread] += 1;
	    }
	    (*accp++) += sum * neg_hid_learnrate;
	}
    }
//     for (thread = 0; thread<num_threads; thread++)
//     {
// 	qn_acc_vf_vf(n_in2hid, in2hid_deltas[thread], in2hid);
//     }
    if (neg_in2hid_learnrate!=0.0f)
    {
	for (i =0; i<num_threads; i++)
	    threadp[i] = per_thread[i].in2hid_deltas;
	accp = in2hid;
	for (i = 0; i<n_in2hid; i++)
	{
	    sum = *threadp[0];
	    (threadp[0])++;
	    for (thread = 1; thread<num_threads; thread++)
	    {
		sum += *threadp[thread];
		threadp[thread]++;
	    }
	    (*accp++) += sum;
	}
    }
}


void
QN_MLP_ThreadFl3::tell_workers(const char* logstr, enum Action action)
{
    int ec;

    // Send an action to all of the worker threads
    ec = pthread_mutex_lock(&action_mutex);
    if (ec)
    {
        clog.error("%s - failed to lock action_mutex in tell_workers - %s",
		   logstr, strerror(ec));
    }
    action_command = action;
    action_seq++;		// Increment sequence to indicate new command
    ec = pthread_cond_broadcast(&action_cv);
    if (ec)
    {
        clog.error("%s - failed to unlock action_mutex in tell_workers - %s",
		   logstr, strerror(ec));
    }
    ec = pthread_mutex_unlock(&action_mutex);
    if (ec)
    {
        clog.error("%s - failed to unlock action_mutex in tell_workers - %s",
		   logstr, strerror(ec));
    }
}

void
QN_MLP_ThreadFl3::wait_on_workers(const char* logstr)
{
    int ec;

    // Send an action to all of the worker threads
    ec = pthread_mutex_lock(&done_mutex);
    if (ec)
    {
        clog.error("%s - failed to lock done_mutex in wait_on_workers - %s",
		   logstr, strerror(ec));
    }
    while (done_count < num_threads)
    {
	ec = pthread_cond_wait(&done_cv, &done_mutex);
	if (ec)
	{
	    clog.error("%s - failed wait on done_cv in wait_on_workers - %s",
		       logstr, strerror(ec));
	}

    }
    // Reset count for next time around whilst we have the mutex
    done_count = 0;
    ec = pthread_mutex_unlock(&done_mutex);
    if (ec)
    {
        clog.error("%s - failed to unlock done_mutex in wait_on_workers - %s",
		   logstr, strerror(ec));
    }
}

void
QN_MLP_ThreadFl3::worker(size_t threadno)
{
    int ec;			// Error code
    enum Action action;		// Our copy of the action we are to do
    unsigned int last_action_seq = 0; // Last action sequence no.
    size_t i;			// Counter
    int exiting = 0;		// Set to true when exiting.
    float nan = qn_nan_f();

    clog.log(QN_LOG_PER_RUN, "Thread %d up and self-aware", threadno);


    // Sized of our part of bunch.
    const size_t w_num_threads = num_threads;
    const size_t w_size_bunch = \
	(size_bunch + w_num_threads - 1)/ w_num_threads;
    const size_t w_first_frame = threadno * w_size_bunch;
    // Our copies of important net params
    const size_t w_n_input = n_input;
    const size_t w_n_hidden = n_hidden;
    const size_t w_n_output = n_output;
    const size_t w_n_in2hid = n_in2hid;
    const size_t w_n_hid2out = n_hid2out;
    // The size of our local matrices
    const size_t w_size_input = w_n_input * w_size_bunch;
    const size_t w_size_hidden = w_n_hidden * w_size_bunch;
    const size_t w_size_output = w_n_output * w_size_bunch;
    // Offsets for reading inputs and writing results
    const size_t w_input_offset = w_size_input * threadno;
    const size_t w_output_offset = w_size_output * threadno;
    // Other state
    const enum QN_OutputLayerType w_out_layer_type = out_layer_type;

    // Allocate our local state
    float* w_in2hid_delta = new float [w_n_in2hid + CACHE_PAD];
    qn_copy_f_vf(w_n_in2hid+CACHE_PAD, nan, w_in2hid_delta);
    per_thread[threadno].in2hid_deltas = w_in2hid_delta;
    float* w_hid_x = new float [w_size_hidden + CACHE_PAD];
    qn_copy_f_vf(w_size_hidden + CACHE_PAD, nan, w_hid_x);
    float* w_hid_y = new float [w_size_hidden + CACHE_PAD];
    qn_copy_f_vf(w_size_hidden + CACHE_PAD, nan, w_hid_y);
    float* w_hid2out_delta = new float [w_n_hid2out + CACHE_PAD];
    qn_copy_f_vf(w_n_hid2out + CACHE_PAD, nan, w_hid2out_delta);
    per_thread[threadno].hid2out_deltas = w_hid2out_delta;
    float* w_out_x = new float [w_size_output + CACHE_PAD];
    qn_copy_f_vf(w_size_output + CACHE_PAD, nan, w_out_x);

    float* w_out_dedx = new float [w_size_output + CACHE_PAD];
    qn_copy_f_vf(w_size_output + CACHE_PAD, nan, w_out_dedx);
    float* w_out_bias_delta = new float [w_n_output + CACHE_PAD];
    qn_copy_f_vf(w_n_output + CACHE_PAD, nan, w_out_bias_delta);
    per_thread[threadno].out_bias_deltas = w_out_bias_delta;

    // These intermediate values are only needed for sigmoid output layers
    // ...and only then when we are NOT doing cross entropy
    float* w_out_dedy = NULL;
    float* w_out_dydx = NULL;
    if (w_out_layer_type==QN_OUTPUT_SIGMOID)
    {
	w_out_dedy = new float [w_size_output + CACHE_PAD];
	qn_copy_f_vf(w_size_output + CACHE_PAD, nan, w_out_dedy);
	w_out_dydx = new float [w_size_output + CACHE_PAD];
	qn_copy_f_vf(w_size_output + CACHE_PAD, nan, w_out_dydx);
    }
    float* w_hid_dedy = new float [w_size_hidden + CACHE_PAD];
    qn_copy_f_vf(w_size_hidden + CACHE_PAD, nan, w_hid_dedy);
    float* w_hid_dydx = new float [w_size_hidden + CACHE_PAD];
    qn_copy_f_vf(w_size_hidden + CACHE_PAD, nan, w_hid_dydx);
    float* w_hid_dedx = new float [w_size_hidden + CACHE_PAD];
    qn_copy_f_vf(w_size_hidden + CACHE_PAD, nan, w_hid_dedx);
    float* w_hid_bias_delta = new float [w_n_hidden + CACHE_PAD];
    qn_copy_f_vf(w_n_hidden + CACHE_PAD, nan, w_hid_bias_delta);
    per_thread[threadno].hid_bias_deltas = w_hid_bias_delta;

    size_t w_cur_frames;	// Number of frames this time around.
    const float* w_in;		// Input frame pointer
    float* w_out;		// Output frame pointer
    const float* w_target;	// Target frame pointer
    size_t w_cur_size_hidden;	// Hidden size this time around
    size_t w_cur_size_output;	// Output size this time around
    float* w_out_x_p;		// Pointer into w_out_x
    float* w_out_p;		// Pointer into w_out

    // The main worker loop
    // Exited by a call to pthread_exit()
    while(!exiting)
    {
	//  Wait to be told what to do by sitting on the action
	// ...condition variable waiting for action_seq to change
	ec = pthread_mutex_lock(&action_mutex);
	if (ec)
	    clog.error("failed to lock action_mutex in worker");
	clog.log(QN_LOG_PER_BUNCH, "Thread %d waiting.", threadno); //??
	while (action_seq == last_action_seq)
	{
	    ec = pthread_cond_wait(&action_cv, &action_mutex);
	    if (ec)
		clog.error("failed to wait on action_cv in worker");
	}
	action = action_command;
	last_action_seq = action_seq;
	ec = pthread_mutex_unlock(&action_mutex);
	if (ec)
	    clog.error("failed to unlock action_mutex in worker");

	switch(action)
	{
	case ACTION_EXIT:
	    exiting = 1;
	    break;
	case ACTION_FORWARD:
	    // Work out how many frames we deal with.
	    if (action_n_frames>w_first_frame)
	    {
		w_cur_frames = \
		    qn_min_zz_z(action_n_frames - w_first_frame, w_size_bunch);
	    }
	    else
		w_cur_frames = 0;
	    clog.log(QN_LOG_PER_BUNCH, "Thread %d forward bunch %lu frames.",
		     threadno, w_cur_frames);
	    if (w_cur_frames>0)
	    {
		// Note that for forward we use non-thread weights in-place

		w_cur_size_hidden = w_n_hidden * w_cur_frames;
		w_cur_size_output = w_n_output * w_cur_frames;
		w_in = action_in + w_input_offset;
		w_out = action_out + w_output_offset;

		// First layer
		qn_copy_vf_mf(w_cur_frames, w_n_hidden, hid_bias, w_hid_x);
		qn_mulntacc_mfmf_mf(w_cur_frames, w_n_input, w_n_hidden,
				    w_in, in2hid, w_hid_x);
		qn_sigmoid_vf_vf(w_cur_size_hidden, w_hid_x, w_hid_y);

		// Second layer
		qn_copy_vf_mf(w_cur_frames, w_n_output, out_bias, w_out_x);
		qn_mulntacc_mfmf_mf(w_cur_frames, w_n_hidden, w_n_output,
				    w_hid_y, hid2out, w_out_x);
		switch(w_out_layer_type)
		{
		case QN_OUTPUT_SIGMOID:
		case QN_OUTPUT_SIGMOID_XENTROPY:
		    qn_sigmoid_vf_vf(w_cur_size_output, w_out_x, w_out);
		    break;
		case QN_OUTPUT_SOFTMAX:
		    w_out_x_p = w_out_x;
		    w_out_p = w_out;
		    
		    for (i=0; i<w_cur_frames; i++)
		    {
			qn_softmax_vf_vf(w_n_output, w_out_x_p, w_out_p);
			w_out_x_p += w_n_output;
			w_out_p += w_n_output;
		    }
		    break;
		case QN_OUTPUT_LINEAR:
		    qn_copy_vf_vf(w_cur_size_output, w_out_x, w_out);
		    break;
		default:
		    assert(0);
		}
	    }
	    break;
	case ACTION_TRAIN:
	    // Work out how many frames we deal with.
	    if (action_n_frames>w_first_frame)
	    {
		w_cur_frames = \
		    qn_min_zz_z(action_n_frames - w_first_frame, w_size_bunch);
	    }
	    else
		w_cur_frames = 0;
	    clog.log(QN_LOG_PER_BUNCH, "Thread %d train bunch %lu frames.",
		     threadno, w_cur_frames);
	    if (w_cur_frames>0)
	    {
		w_cur_size_hidden = w_n_hidden * w_cur_frames;
		w_cur_size_output = w_n_output * w_cur_frames;

		w_in = action_in + w_input_offset;
		w_out = action_out + w_output_offset;
		w_target = action_target + w_output_offset;

		// First layer
		qn_copy_vf_mf(w_cur_frames, w_n_hidden, hid_bias, w_hid_x);

		qn_mulntacc_mfmf_mf(w_cur_frames, w_n_input, w_n_hidden,
				    w_in, in2hid, w_hid_x);
		qn_sigmoid_vf_vf(w_cur_size_hidden, w_hid_x, w_hid_y);

		// Second layer
		qn_copy_vf_mf(w_cur_frames, w_n_output, out_bias, w_out_x);

		qn_mulntacc_mfmf_mf(w_cur_frames, w_n_hidden, w_n_output,
				    w_hid_y, hid2out, w_out_x);
		switch(w_out_layer_type)
		{
		case QN_OUTPUT_SIGMOID:
		    // Forward part
		    qn_sigmoid_vf_vf(w_cur_size_output, w_out_x, w_out);
		    // Backward part
		    // For a sigmoid layer, de/dx = de/dy . dy/dx
		    qn_sub_vfvf_vf(w_cur_size_output, w_out, w_target,
				   w_out_dedy);
		    qn_dsigmoid_vf_vf(w_cur_size_output, w_out, w_out_dydx);
		    qn_mul_vfvf_vf(w_cur_size_output, w_out_dydx, w_out_dedy,
				   w_out_dedx);
		    break;
		case QN_OUTPUT_SIGMOID_XENTROPY:
		    // Forward part
		    qn_sigmoid_vf_vf(w_cur_size_output, w_out_x, w_out);
		    // Backward part
		    // For cross entropy sigmoid, dx = dy
		    qn_sub_vfvf_vf(w_cur_size_output, w_out, w_target,
				   w_out_dedx); 
		    break;
		case QN_OUTPUT_SOFTMAX:
		    // Forward part - need to do each frame individually
		    w_out_x_p = w_out_x;
		    w_out_p = w_out;
		    for (i=0; i<w_cur_frames; i++)
		    {
			qn_softmax_vf_vf(w_n_output, w_out_x_p, w_out_p);
			w_out_x_p += w_n_output;
			w_out_p += w_n_output;
		    }
		    // Backward part
		    // For cross entropy softmax, dx = dy
		    qn_sub_vfvf_vf(w_cur_size_output, w_out, w_target,
				   w_out_dedx);
		    break;
		case QN_OUTPUT_LINEAR:
		    // Forward part
		    qn_copy_vf_vf(w_cur_size_output, w_out_x, w_out);
		    // Backward part
		    // For linear, dx = dy
		    qn_sub_vfvf_vf(w_cur_size_output, w_out, w_target,
				   w_out_dedx);
		    break;
		default:
		    // Unknown layer type
		    assert(0);
		} // End of output unit switch

		// Back propagate error through hidden to output weights.
		if (do_backprop)
		{
		    qn_mul_mfmf_mf(w_cur_frames, w_n_output, w_n_hidden,
				   w_out_dedx, hid2out, w_hid_dedy); 
		}

		// Update hid2out weight deltas.
		// Note learning rate is applied later.
		if (neg_hid2out_learnrate!=0.0f)
		{
		    qn_copy_f_vf(w_n_hid2out, 0.0f, w_hid2out_delta);
		    qn_multnacc_fmfmf_mf(w_cur_frames, w_n_output, w_n_hidden,
					 neg_hid2out_learnrate, w_out_dedx,
					 w_hid_y, w_hid2out_delta);
		}
		
		// Calculate output biases delta.
		if (neg_out_learnrate!=0.0f)
		{
		    qn_sumcol_mf_vf(w_cur_frames, w_n_output, w_out_dedx,
				    w_out_bias_delta);
		}

		// Propogate error back through sigmoid.
		if (do_backprop)
		{
		    qn_dsigmoid_vf_vf(w_cur_size_hidden, w_hid_y, w_hid_dydx);
		    qn_mul_vfvf_vf(w_cur_size_hidden, w_hid_dydx, w_hid_dedy,
				   w_hid_dedx);
		}

		// Calculate in2hid weight deltas.
		if (neg_in2hid_learnrate!=0.0f)
		{
		    qn_copy_f_vf(w_n_in2hid, 0.0f, w_in2hid_delta);
		    qn_multnacc_fmfmf_mf(w_cur_frames, w_n_hidden, w_n_input,
					 neg_in2hid_learnrate , w_hid_dedx, 
					 w_in, w_in2hid_delta);
		}

		// Calculate bias deltas.
		if (neg_hid_learnrate!=0.0f)
		{
		    qn_sumcol_mf_vf(w_cur_frames, w_n_hidden, w_hid_dedx,
				    w_hid_bias_delta);
		}

	    } // End of "if frames to do"
	    break;
	default:
	    // Unknown action
	    assert(0);
	}
	
	// Signal that we're done
	ec = pthread_mutex_lock(&done_mutex);
	if (ec)
	    clog.error("failed to lock done_mutex in worker");
	done_count++;			// Indicate one more thread is done.
	if (done_count==num_threads)
	{
	    ec = pthread_cond_signal(&done_cv);
	    if (ec)
		clog.error("failed to signal cond_cv in worker");
	}
	ec = pthread_mutex_unlock(&done_mutex);

    }
    delete [] w_hid_bias_delta;
    delete [] w_hid_dedx;
    delete [] w_hid_dydx;
    delete [] w_hid_dedy;

    if (w_out_layer_type==QN_OUTPUT_SIGMOID)
    {
	delete [] w_out_dydx;
	delete [] w_out_dedy;
    }
    delete [] w_out_bias_delta;
    delete [] w_out_dedx;

    delete [] w_out_x;
    delete [] w_hid2out_delta;
    delete [] w_hid_y;
    delete [] w_hid_x;
    delete [] w_in2hid_delta;

    clog.log(QN_LOG_PER_RUN, "Thread %d exited.", threadno);
    pthread_exit(NULL);
}

extern "C" {

static void*
worker_wrapper(void *args)
{
    QN_MLP_ThreadFl3* mlp;
    size_t threadno;

    QN_MLP_ThreadFl3_WorkerArg* unvoided_args = \
	(QN_MLP_ThreadFl3_WorkerArg*) args;
    mlp = unvoided_args->mlp;
    threadno = unvoided_args->threadno;

    mlp->worker(threadno);

    return NULL;
}

}; // extern "C"

#endif // #ifde QN_HAVE_LIBPTHREAD

