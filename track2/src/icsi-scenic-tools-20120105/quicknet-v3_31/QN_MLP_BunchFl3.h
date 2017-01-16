// $Header: /u/drspeech/repos/quicknet2/QN_MLP_BunchFl3.h,v 1.10 2011/03/03 00:32:59 davidj Exp $

#ifndef QN_MLP_BunchFl3_H_INCLUDED
#define QN_MLP_BunchFl3_H_INLCUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_MLP.h"
#include "QN_Logger.h"

class QN_MLP_BunchFl3 : public QN_MLP
{
public:
    QN_MLP_BunchFl3(int a_debug, const char* a_dbgname,
		    size_t a_input, size_t a_hidden, size_t a_output,
		    enum QN_OutputLayerType a_outtype, size_t a_size_bunch);
    ~QN_MLP_BunchFl3();

    // Find out the size of the net
    size_t num_layers() const { return 3; };
    size_t num_sections() const { return 4; };
    size_t size_layer(QN_LayerSelector layer) const;
    void size_section(QN_SectionSelector section, size_t* ouput_p,
		      size_t* input_p) const;
    size_t num_connections() const;

    // Find out the bunch size (not part of the abstract interface)
    size_t get_bunchsize();

    // Use and train.
    void forward(size_t n_frames, const float* in, float* out);
    void train(size_t n_frames, const float* in, const float* target,
	       float* out);

    // Access weights.
    void set_weights(enum QN_SectionSelector which,
		     size_t row, size_t col,
		     size_t n_rows, size_t n_cols,
		     const float* weights);
    void get_weights(enum QN_SectionSelector which,
		     size_t row, size_t col,
		     size_t n_rows, size_t n_cols,
		     float* weights);

    // Access learning constants.
    void set_learnrate(enum QN_SectionSelector which, float rate);
    float get_learnrate(enum QN_SectionSelector which) const;

private:
    // Forward pass one frame
    void forward_bunch(size_t n_frames, const float* in, float* out);

    // Train one frame
    void train_bunch(size_t n_frames, const float* in, const float* target,
		     float* out);
    
    // A routine to return details of a specific weight or bias section
    float* findweights(QN_SectionSelector which,
		       size_t row, size_t col,
		       size_t n_rows, size_t n_cols,
		       size_t* total_cols_p) const;
    // A routine to return the name of a given weight layer
    static const char* nameweights(QN_SectionSelector which);

private:
    QN_ClassLogger clog;	// Logging object.
    const size_t n_input;	// No. of input units.
    const size_t n_hidden;	// No. of hidden units.
    const size_t n_output;	// No. of output units.
    const size_t n_in2hid;	// Number of in2hid weights.
    const size_t n_hid2out;	// Number of hid2out weights.
    const size_t size_input;	// Max values in input layer per bunch.
    const size_t size_hidden;	// Max values in hidden layer per bunch.
    const size_t size_output;	// Max values in output layer per bunch.

    const enum QN_OutputLayerType out_layer_type; // Type of output layer
						  // (e.g. sigmoid, softmax)
    const size_t size_bunch;	// Maximum size of bunch

    float *in2hid;              // Input-to-hidden weights.
    float *hid_bias;            // Hidden layer biases.
    float *hid_x;               // Sum into hidden layer.
    float *hid_y;               // Output from hidden units.

    float *hid2out;             // Hidden-to-output weights.
    float *out_bias;            // Output layer biases.
    float *out_x;               // Sum into output layer.

    float *out_dedy;		// Output error.
    float *out_dydx;		// Output sigmoid difference.
    float *out_dedx;            // Feed back error term from output.
    float *out_delta_bias;	// Output bias update value for whole bunch.

    float *hid_dedy;            // Hidden output error.
    float *hid_dydx;		// Hidden sigmoid difference
    float *hid_dedx;            // Hidden feed back error term.
    float *hid_delta_bias;	// Hidden bias update value for whole bunch.

    float neg_in2hid_learnrate;		// Learning rates...
    float neg_hid2out_learnrate;
    float neg_hid_learnrate;
    float neg_out_learnrate;
};


#endif // #define QN_MLP_BunchFl3_H_INLCUDED

