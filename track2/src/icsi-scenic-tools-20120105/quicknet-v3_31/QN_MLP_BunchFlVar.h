// $Header: /u/drspeech/repos/quicknet2/QN_MLP_BunchFlVar.h,v 1.7 2006/03/09 02:09:51 davidj Exp $

#ifndef QN_MLP_BunchFlVar_H_INCLUDED
#define QN_MLP_BunchFlVar_H_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_MLP.h"
#include "QN_MLP_BaseFl.h"
#include "QN_Logger.h"

// An MLP class that supports bunch mode and has a variable number
// of layers.  

class QN_MLP_BunchFlVar : public QN_MLP_BaseFl
{
public:
    QN_MLP_BunchFlVar(int a_debug, const char* a_dbgname,
		      size_t a_n_layers,
		      const size_t a_layer_units[QN_MLP_MAX_LAYERS],
		      enum QN_OutputLayerType a_outtype, size_t a_size_bunch);
    ~QN_MLP_BunchFlVar();

protected:
    // Forward pass one frame
    void forward_bunch(size_t n_frames, const float* in, float* out);

    // Train one frame
    void train_bunch(size_t n_frames, const float* in, const float* target,
		     float* out);
    
private:
    const enum QN_OutputLayerType out_layer_type; // Type of output layer
						  // (e.g. sigmoid, softmax).

    float *layer_x[MAX_LAYERS]; // Sum into layer (hid_x).
    float *layer_y[MAX_LAYERS]; // Output from non linearity (hid_y).

    float *layer_dedy[MAX_LAYERS]; // Output error.
    float *layer_dydx[MAX_LAYERS]; // Output sigmoid difference.
    float *layer_dedx[MAX_LAYERS]; // Feed back error term from output.
    float *layer_delta_bias[MAX_LAYERS]; // Output bias update value for whole bunch.
};


#endif // #define QN_MLP_BunchFlVar_H_INLCUDED

