// $Header: /u/drspeech/repos/quicknet2/QN_MLP_OnlineFx3.h,v 1.26 1996/09/20 22:06:00 davidj Exp $

#ifndef QN_MLP_OnlineFx3_h_INCLUDED
#define QN_MLP_OnlineFx3_h_INCLUDED

#include <QN_config.h>
#include "QN_Logger.h"

#ifdef QN_HAVE_LIBFXLIB

#ifdef __cplusplus
extern "C" {
#endif
#define FX_INLINING
#include "qnfxlib.h"
#ifdef __cplusplus
}
#endif
#include "QN_MLP.h"

#ifdef USE_IPM
#include "IPM_timer.h"
#endif

class QN_MLP_OnlineFx3 : public QN_MLP
{
public:
    QN_MLP_OnlineFx3(int debug,       // Level of debugging
		     size_t n_input,  // Size of input layer
		     size_t n_hidden, // Size of hidden layer
		     size_t n_output, // Size of output layer
		     enum QN_OutputLayerType outtype, // Type of output layer
		     int a_in2hid_exp = 0, // Fixed point exponent of weights
		     int a_hid2out_exp = 0
	            );
    ~QN_MLP_OnlineFx3();

    // Find out the size of the net
    size_t num_layers() const { return 3; };
    size_t num_sections() const { return 4; };
    size_t size_layer(QN_LayerSelector layer) const;
    void size_section(QN_SectionSelector section, size_t* ouput_p,
		      size_t* input_p) const;

    size_t num_connections() const;
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
		     float* weights) const;

    // Access learning constants.
    void set_learnrate(enum QN_SectionSelector which, float rate);
    float get_learnrate(enum QN_SectionSelector which) const;
private:
    // Forward pass one frame
    void forward1(const float* in, float* out);

    // Train one frame
    void train1(const float* in, const float* target,
		float* out);

    QN_ClassLogger clog;	// Logger object.

    const size_t n_input;	// No of input units
    const size_t n_hidden;	// No of hidden units
    const size_t n_output;	// No of output units
    const size_t n_in2hid;	// Number of in2hid weights.
    const size_t n_hid2out;	// Number of hid2out weights.

    const enum QN_OutputLayerType out_layer_type; // Type of output layer
					       // (e.g. sigmoid, softmax)     

    // These exponenents are user settable
    const int in2hid_exp;
    const int hid2out_exp;

    fxint16 *in_y;		// Input as 16 bit fixed point.
    enum { in_y_exp = 3 };	// Exponent for above.
    fxint16 *in2hid;		// Input-to-hidden weights.
    fxint32 *hid_bias;		// Hidden layer biases.
    enum { hid_bias_exp = 8 };
    fxint32 *hid_x32;		// Sum into hidden layer.
    enum { hid_x32_exp = hid_bias_exp };
    fxint16 *hid_x16;		// Sum into hidden layer.
    enum { hid_x16_exp = 4 };
    fxint16 *hid_y;		// Output from hidden units.
    enum { hid_y_exp = 0 };
    fxint16 *hid_sy;		// Scaled output from hidden units.
    // hid_sy_exp calculated in train1().

    fxint16 *hid2out;		// Hidden-to-output weights.
    fxint32 *out_bias;		// Output layer biases.
    enum { out_bias_exp = 8 };
    fxint32 *out_x32;		// Sum into output layer.
    enum { out_x32_exp = out_bias_exp };
    fxint16 *out_x16;		// Sum into output layer.
    enum { out_x16_exp = 4 };
    fxint16 *out_y;		// Output from output layer.
    enum { out_y_exp = 0 };

    fxint16 *target16;		// Correct output
    enum { target16_exp = out_y_exp };
    fxint16 *out_dedy;		// Output error
    enum { out_dedy_exp = 0 };
    fxint16 *out_dydx;		// Output sigmoid difference
    enum { out_dydx_exp = -2 };
    fxint16 *out_dedx;		// Error at output
    enum { out_dedx_exp = target16_exp };
    
    fxint32 *hid_dedy32;	// Hidden output error.
    enum { hid_dedy32_exp = 6 };
    fxint16 *hid_dedy16;	// Hidden output error.
    enum { hid_dedy16_exp = 2 };
    fxint16 *hid_dydx;		// Hidden sigmoid difference
    enum { hid_dydx_exp = -2 };
    fxint16 *hid_dedx;		// Hidden feed back error term.
    enum { hid_dedx_exp = hid_dydx_exp + hid_dedy16_exp };
    fxint16 *in2hid_sdedx;		// Error scaled by learning rate
    // in2hid_sdedx_exp calculated in train1()
    // depends on in2hid_learnrate_exp

    fxint16 neg_in2hid_learnrate;	// Input to hidden learning rate
    fxexp_t neg_in2hid_learnrate_exp;	// Exponent for above
    fxint16 neg_hid_learnrate;		// Hidden bias learning rate
    fxexp_t neg_hid_learnrate_exp;	// Exponent for above
    fxint16 neg_hid2out_learnrate;	// Hidden to output learning rate 
    fxexp_t neg_hid2out_learnrate_exp;	// Exponent for above
    fxint16 neg_out_learnrate;		// Output bias learning rate
    fxexp_t neg_out_learnrate_exp;	// Exponent for above
    int feature_sat_warning_given;	// Used to suppress excess warning
					// messages.
    int weight_sat_warning_given;	// Used to suppress excess warning
					// messages.
    int bias_sat_warning_given;		// Used to suppress excess warning
					// messages.
    int lr_sat_warning_given;		// Used to suppress excess warning
  					// messages.
#ifdef USE_IPM
    IPM_timer forw, back, wup, misc, tofix, fromfix, total;
#endif
};

#else // #ifdef QN_HAVE_LIBFXLIB

// Some compilers break if there is nothing to compile, so have a dummy
// class.  Also, this makes sure no one else tries to define a class with
// the same name.

class QN_MLP_OnlineFx3 
{
public:
    int dummy();
};


#endif // not #ifdef QN_HAVE_LIBFXLIB

#endif // #define QN_MLP_OnlineFx3_h_INCLUDED





