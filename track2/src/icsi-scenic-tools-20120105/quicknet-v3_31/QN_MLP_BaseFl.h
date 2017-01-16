// $Header: /u/drspeech/repos/quicknet2/QN_MLP_BaseFl.h,v 1.2 2011/03/03 00:32:58 davidj Exp $

#ifndef QN_MLP_BaseFl_H_INCLUDED
#define QN_MLP_BaseFl_H_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_MLP.h"
#include "QN_Logger.h"

// A base class for floating point MLP classes.   Handles everything
// except the train_bunch and forward_bunch routines.


class QN_MLP_BaseFl : public QN_MLP
{
public:
    enum {
	MAX_LAYERS = QN_MLP_MAX_LAYERS,
	MAX_WEIGHTMATS = QN_MLP_MAX_LAYERS-1
    };

    // Constructor.  Note the messy layer specification so derived
    // classes can initialize this.x
    QN_MLP_BaseFl(int a_debug, const char* a_dbgname,
		  const char* a_classname,
		  size_t a_size_bunch,
		  size_t a_n_layers,
		  size_t a_layer1_units,
		  size_t a_layer2_units,
		  size_t a_layer3_units = 0,
		  size_t a_layer4_units = 0,
		  size_t a_layer5_units = 0);
    virtual ~QN_MLP_BaseFl();

    // Find out the size of the net
    virtual size_t num_layers() const { return n_layers; };
    virtual size_t num_sections() const { return n_sections; };
    virtual size_t size_layer(QN_LayerSelector layer) const;
    virtual void size_section(QN_SectionSelector section, size_t* ouput_p,
			      size_t* input_p) const;
    virtual size_t num_connections() const;

    // Find out the bunch size (not part of the abstract interface)
    virtual size_t get_bunchsize() const;

    // Use and train.
    virtual void forward(size_t n_frames, const float* in, float* out);
    virtual void train(size_t n_frames, const float* in, const float* target,
		       float* out);

    // Access weights.
    virtual void set_weights(enum QN_SectionSelector which,
		     size_t row, size_t col,
		     size_t n_rows, size_t n_cols,
		     const float* weights);
    virtual void get_weights(enum QN_SectionSelector which,
			     size_t row, size_t col,
			     size_t n_rows, size_t n_cols,
			     float* weights);

    // Access learning constants.
    virtual void set_learnrate(enum QN_SectionSelector which, float rate);
    virtual float get_learnrate(enum QN_SectionSelector which) const;

protected:
    // Forward pass one frame
    virtual void forward_bunch(size_t n_frames, const float* in, float* out)
	= 0;

    // Train one frame
    virtual void train_bunch(size_t n_frames, const float* in,
			     const float* target, float* out)
	= 0;
    
    // A routine to return details of a specific weight or bias section
    float* findweights(QN_SectionSelector which,
		       size_t row, size_t col,
		       size_t n_rows, size_t n_cols,
		       size_t* total_cols_p) const;
    // A routine to return the name of a given weight layer
    static const char* nameweights(QN_SectionSelector which);

protected:
    QN_ClassLogger clog;	// Logging object.
    size_t n_layers;		// The number of layers.
    size_t n_weightmats;	// The number of weight matrices.
    size_t n_sections;		// The number of weight/bias sections.
    const size_t size_bunch;	// Maximum size of bunch.

    size_t layer_units[MAX_LAYERS]; // The size of each layer in units.
    size_t layer_size[MAX_LAYERS]; // The space needed for one bunch
				// at each layer.
    size_t weights_size[MAX_WEIGHTMATS]; // The size of the weight matrices.

    float *weights[MAX_WEIGHTMATS]; // Weight matrices.
    float *layer_bias[MAX_LAYERS]; // Biases.

    // Learning rates for the biases and wieghts.
    float neg_weight_learnrate[MAX_WEIGHTMATS];
    float neg_bias_learnrate[MAX_LAYERS];  // Note we never use for layer 0

    // Boolean array that indicates which backprop steps we need to do
    // (based on learnrate==0.0 for the relevant weight matrices).
    int backprop_weights[MAX_WEIGHTMATS];
};


#endif // #define QN_MLP_BaseFl_H_INLCUDED

