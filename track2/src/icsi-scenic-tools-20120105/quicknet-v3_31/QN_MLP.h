// $Header: /u/drspeech/repos/quicknet2/QN_MLP.h,v 1.10 2011/03/03 00:32:58 davidj Exp $

#ifndef QN_MLP_h_INCLUDED
#define QN_MLP_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stddef.h>
#include "QN_types.h"

// QN_MLP is a base class used to define an interface to MLPs
//
// It is designed to hide as much as possible, so allowing a variety of
// implementations - specifically fixed point implementations on SPERT.
// Efficiency is a key issue.
//
// Some notable points:
//
// - It does forward pass or training on multiple frames at a time.
//   This is more
//   efficient even for online training (e.g. conversions to/from float are
//   done on larger vectors), but also allows batch-like training using the
//   same interface.
// - No inherent knowledge of file formats etc.
// - Flexible with regard to initialization and learning rates.
// - No file format information encoded - get the weights out yourself
//   and do what you want with them.
// - No separate backward pass - being as the state from the forward pass
//   is needed to go backward, we have to do forward then backward for 
//   training to be meaningful.
// - Most stuff hidden - you can change the implementation as you require.

class QN_MLP
{
public:
    virtual ~QN_MLP() {};

    // How many layers are there?
    virtual size_t num_layers() const = 0;

    // How many sections (i.e. weight matrices - 4 in 3 layer MLP) are there?
    virtual size_t num_sections() const = 0;

    // How many units are there in the given layer?
    virtual size_t size_layer(QN_LayerSelector layer) const = 0;

    // How many connections in the net
    virtual size_t num_connections() const = 0;

    // What is the size of the given section - outputs and inputs
    virtual void size_section(QN_SectionSelector section, size_t* ouput_p,
			      size_t* input_p) const = 0;
    

    // Train and use
    virtual void forward(size_t n_frames, const float *in, float *out) = 0;
    virtual void train(size_t n_frames, const float* in, const float* target,
		       float* out) = 0;

    // Access weights/biases - returned in output-major order (i.e. The second
    // weight returned is used to scale the second input to form the first
    // output)
    // e.g. for input to hidden: rows==n_hidden, cols==n_input
    // for hidden biases: rows==n_hidden, cols==1
    virtual void set_weights(enum QN_SectionSelector which,
			     size_t row, size_t col,
			     size_t n_rows, size_t n_cols,
			     const float* weights) = 0;
    virtual void get_weights(enum QN_SectionSelector which,
			     size_t row, size_t col,
			     size_t n_rows, size_t n_cols,
			     float* weights) = 0;

    // Access learning constants.
    virtual void set_learnrate(enum QN_SectionSelector which,
			       float learnrate) = 0;
    virtual float get_learnrate(enum QN_SectionSelector which) const = 0;
};

#endif // #define QN_QN_MLP_h_INCLUDED

