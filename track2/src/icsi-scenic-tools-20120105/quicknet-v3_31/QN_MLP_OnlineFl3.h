// $Header: /u/drspeech/repos/quicknet2/QN_MLP_OnlineFl3.h,v 1.13 2011/03/03 00:32:59 davidj Exp $

#ifndef QN_MLP_OnlineFl3_h_INCLUDED
#define QN_MLP_OnlineFl3_h_INLCUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include "QN_MLP.h"
#include "QN_Logger.h"

// "QN_MLP_OnlineFl3" is a three layer MLP that does online training
// and is implemented in floating point.

class QN_MLP_OnlineFl3 : public QN_MLP
{
public:
    QN_MLP_OnlineFl3(int a_debug, const char* a_dbgname,
		     size_t n_input, size_t n_hidden,
		     size_t n_output, QN_OutputLayerType outtype);
    ~QN_MLP_OnlineFl3();

    // Find out the size of the net.
    size_t num_layers() const { return 3; };
    size_t num_sections() const { return 4; };
    size_t size_layer(QN_LayerSelector layer) const;
    size_t num_connections() const;
    void size_section(QN_SectionSelector section, size_t* ouput_p,
		      size_t* input_p) const;

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
protected:
    // Forward pass one frame.
    void forward1(const float* in, float* out);

    // Train one frame.
    void train1(const float* in, const float* target,
		float* out);
    
    // A routine to return details of a specific weight or bias matrix.
    float* findweights(QN_SectionSelector which,
		       size_t row, size_t col,
		       size_t n_rows, size_t n_cols,
		       size_t* total_cols_p) const;
    // A routine to return the name of a given weight layer.
    static const char* nameweights(QN_SectionSelector which);

protected:
    QN_ClassLogger clog;	// Handles logging.
    const size_t n_input;	// No of input units.
    const size_t n_hidden;	// No of hidden units.
    const size_t n_output;	// No of output units.
    const size_t n_in2hid;	// Number of in2hid weights.
    const size_t n_hid2out;	// Number of hid2out weights.

    const QN_OutputLayerType out_layer_type; // Type of output layer
					     // (e.g. sigmoid, softmax).

    float *in2hid;              // Input-to-hidden weights.
    float *hid_bias;            // Hidden layer biases.
    float *hid_x;               // Sum into hidden layer.
    float *hid_y;               // Output from hidden units.

    float *hid2out;             // Hidden-to-output weights.
    float *out_bias;            // Output layer biases.
    float *out_x;               // Sum into output layer.

    float *out_dedy;		// Output error.
    float *out_dydx;		// Output sigmoid difference
    float *out_dedx;            // Feed back error term from output.

    float *hid_dedy;            // Hidden output error.
    float *hid_dydx;		// Hidden sigmoid difference
    float *hid_dedx;            // Hidden feed back error term.

    float neg_in2hid_learnrate;		// Learning rates...
    float neg_hid2out_learnrate;
    float neg_hid_learnrate;
    float neg_out_learnrate;
};


#endif // #define QN_MLP_OnlineFl3_h_INLCUDED

