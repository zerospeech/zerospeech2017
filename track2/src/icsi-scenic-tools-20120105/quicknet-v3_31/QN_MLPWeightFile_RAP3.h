// $Header: /u/drspeech/repos/quicknet2/QN_MLPWeightFile_RAP3.h,v 1.13 2006/02/02 00:25:40 davidj Exp $

#ifndef QN_MLPWeightFile_RAP3_h_INCLUDED
#define QN_MLPWeightFile_RAP3_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_MLPWeightFile.h"

// An interface to a  "RAP" style weight file as used by BoB and Clones
// This only works for 3 layer MLPs

class QN_MLPWeightFile_RAP3 : public QN_MLPWeightFile
{
public:
    // If layer sizes are non-zero for input, the file is checked to
    // see if they are consistent
    QN_MLPWeightFile_RAP3(int dbg, FILE* stream, QN_FileMode mode,
			  const char* name,
			  size_t a_input = 0, size_t a_hidden = 0,
			  size_t a_output = 0);

    // How many layers are there?
    size_t num_layers() { return 3; }

    // How many sections?
    size_t num_sections() { return 4; }

    // How many units are there in the given layer?
    size_t size_layer(QN_LayerSelector layer);

    // Return whether weight matrices are input or output major
    enum QN_WeightMaj get_weightmaj();

    // Return the order of the weights in the file
    enum QN_SectionSelector get_weighttype(int section);

    // Is this fine QN_READ or QN_WRITE?
    enum QN_FileMode get_filemode();

    // Read in a buffer full of weights
    size_t read(float* buf, size_t count);

    // Write out a buffer full of weights
    size_t write(const float* buf, size_t count);
    
private:
    int debug;			// Level of verbosity
    const enum QN_FileMode mode;
    FILE* const stream;		// Stream used to access weights files
    size_t n_input;		// Size of the input layer
    size_t n_hidden;		// Size of the hidden layer
    size_t n_output;		// Size of the output layer
    const char* filename;	// Name of the weight file

    static const char* CLASSNAME; // Name of this class

    int scan_file();		// Read through the file if seekable, finding
				// out the size of the layers
    int write_one(float weight);// Write one weight to the file
    

    size_t io_count;		// Number of elements left in current section
				// ..of weights file
    size_t io_state;		// Which section of weight file we are
				// ..reading/writing
};

#endif

