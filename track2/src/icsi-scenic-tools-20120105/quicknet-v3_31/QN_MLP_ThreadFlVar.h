// $Header: /u/drspeech/repos/quicknet2/QN_MLP_ThreadFlVar.h,v 1.1 2006/03/09 02:09:55 davidj Exp $

#ifndef QN_MLP_ThreadFlVar_H_INCLUDED
#define QN_MLP_ThreadFlVar_H_INLCUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_MLP.h"
#include "QN_MLP_BaseFl.h"
#include "QN_Logger.h"
#include <pthread.h>

#ifdef QN_HAVE_LIBPTHREAD

class QN_MLP_ThreadFlVar;

// Arguments to the worker threads - needs to be used by wrapper so
// has to be outsize main MLP class
struct QN_MLP_ThreadFlVar_WorkerArg {
    size_t threadno;		// Thread no, used to calculate offsets
    QN_MLP_ThreadFlVar* mlp;	// Pointer to the threads MLP object
};

class QN_MLP_ThreadFlVar : public QN_MLP_BaseFl
{
public:
    QN_MLP_ThreadFlVar(int a_debug, const char* a_dbgname,
		       size_t a_n_layers,
		       const size_t a_layer_units[QN_MLP_MAX_LAYERS],
		       enum QN_OutputLayerType a_outtype,
		       size_t a_size_bunch, size_t a_threads);
    ~QN_MLP_ThreadFlVar();

    // Find out the size of the net

    // The actual worker thread
    // Should not really be public but it needs to be called from C
    void worker(size_t threadno);

private:
    // Forward pass one frame
    void forward_bunch(size_t n_frames, const float* in, float* out);

    // Train one frame
    void train_bunch(size_t n_frames, const float* in, const float* target,
		     float* out);

    
    // The actions for the worker threads
    enum Action {
	    ACTION_EXIT = 0,
	    ACTION_FORWARD = 1,
	    ACTION_TRAIN = 2
    };
    // Send an action command to the worker threads
    void tell_workers(const char* logstr, enum Action action);
    // Wait for the workers to finish
    void wait_on_workers(const char* logstr);

    // Something we add to the end of thread local arrays to make
    // sure they do not share cache lines with arrays in other threads.
    enum { CACHE_PAD = 256 };

private:
    const enum QN_OutputLayerType out_layer_type; // Type of output layer
						  // (e.g. sigmoid, softmax)
    const size_t num_threads;	// Number of threads

    struct PerThread {
	// Thread/layer-specific weight deltas
	float* delta_weights[MAX_WEIGHTMATS];
	// Thread/layer-specific bias deltas
	float* delta_biases[MAX_LAYERS];
    };
    struct PerThread* per_thread; // Array of per-thread arrays that
				// we need the main object to know about.

    float** threadp;		// Some per-thread working space.
    pthread_t *threads;	        // The thread IDs of the worker threads
    struct QN_MLP_ThreadFlVar_WorkerArg* worker_args; // Array of pointers to each thread args
    pthread_mutex_t action_mutex;  // The mutex for the action variable
    pthread_cond_t action_cv;	   // The cv for the action variable
    unsigned int action_seq;	// The sequence number for the current action
    enum Action action_command;	// The variable that tells us what to do

    size_t action_n_frames;	// Number of frames for this action.
    const float* action_in;	// Input for this action.
    float* action_out;		// Output for this action.
    const float* action_target;	// Target for this action.

    pthread_mutex_t done_mutex;  // The mutex to indicate thread completion
    pthread_cond_t done_cv;	// The cv for thread completion
    size_t done_count;	        // The count for thread completion
};

#endif // #ifdef QN_HAVE_LIBPTHREAD

#endif // #define QN_MLP_ThreadFlVar_H_INLCUDED

