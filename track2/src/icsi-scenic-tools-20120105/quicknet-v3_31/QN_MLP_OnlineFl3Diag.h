// $Header: /u/drspeech/repos/quicknet2/QN_MLP_OnlineFl3Diag.h,v 1.1 1996/09/26 00:30:52 davidj Exp $

#ifndef QN_MLP_OnlineFl3Diag_h_INCLUDED
#define QN_MLP_OnlineFl3Diag_h_INLCUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include "QN_MLP.h"
#include "QN_Logger.h"
#include "QN_MLP_OnlineFl3.h"

// "QN_MLP_OnlineFl3Diag" is derived form "QN_MLP_OnlineFl3" but
// also includes facilities for logging internal values of the net.

class QN_MLP_OnlineFl3Diag : private QN_MLP_OnlineFl3
{
public:
    QN_MLP_OnlineFl3Diag(int a_debug, const char* a_dbgname,
			 size_t n_input, size_t n_hidden,
			 size_t n_output, QN_OutputLayerType outtype);
    ~QN_MLP_OnlineFl3Diag();

protected:
    // Forward pass one frame.
    void forward1(const float* in, float* out);

    // Train one frame.
    void train1(const float* in, const float* target,
		float* out);

private:
    // Routine to dump one matrix.
    void dump(const char* matname, size_t rows, size_t cols, const float* mat);
private:
    QN_ClassLogger clog;	// Logging object.
    unsigned long forward_count; // How many times forward1() has been called.
    unsigned long train_count;	// How many times train1() has been called.
};


#endif // #define QN_MLP_OnlineFl3_h_INLCUDED

