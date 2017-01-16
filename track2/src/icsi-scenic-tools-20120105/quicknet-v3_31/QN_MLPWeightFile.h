// $Header: /u/drspeech/repos/quicknet2/QN_MLPWeightFile.h,v 1.11 2006/02/02 00:26:45 davidj Exp $

#ifndef QN_MLPWeightFile_h_INCLUDED
#define QN_MLPWeightFile_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"

// "QN_MLPWeightFile" is an abstraction of a file used to store weights
// for an MLP.  Note that the weight file can only be read or written
// sequentially, and that different weight files store things in a different
// order, both with respect to which weight matrices come when and whether
// within a give matrix the weights are transposed

class QN_MLPWeightFile
{
public:
    // How many layers are there?
    virtual size_t num_layers() = 0;

    // How many weight sections are there (4 in a 3 layer MLP - ie bias
    // layers count as sections).
    virtual size_t num_sections() = 0;

    // How many units are there in the given layer?
    virtual size_t size_layer(QN_LayerSelector layer) = 0;

    // Are the weights QN_INPUTMAJOR or QN_OUTPUTMAJOR for a given layer?
    virtual enum QN_WeightMaj get_weightmaj() = 0;

    // For inputs 0-n, return the 1st through nth weight matrix type
    virtual enum QN_SectionSelector get_weighttype(int number) = 0;

    // Is this file QN_READ or QN_WRITE?
    virtual enum QN_FileMode get_filemode() = 0;

    // Read in a buffer full of weights.
    // Returns the number of weights read.
    virtual size_t read(float* buf, size_t count) = 0;

    // Write out a buffer full of weights.
    // Returns the number of weights written.
    virtual size_t write(const float* buf, size_t count) = 0;
};

#endif



