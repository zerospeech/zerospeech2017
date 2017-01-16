// $Header: /u/drspeech/repos/quicknet2/QN_MLPWeightFile_Matlab.h,v 1.6 2006/02/02 03:49:47 davidj Exp $

#ifndef QN_MLPWeightFile_Matlab_h_INCLUDED
#define QN_MLPWeightFile_Matlab_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_MLPWeightFile.h"
#include "QN_mat.h"

// A weight file format that uses the matlab level 4 format
// to store weight matrices.  Works for up to 5 layer MLPs.

class QN_MLPWeightFile_Matlab : public QN_MLPWeightFile
{
public:
    enum {
	QN_MAX_LAYERS = 5,
	QN_MAX_WEIGHTMATS = QN_MAX_LAYERS-1,
	QN_MAX_SECTIONS = QN_MAX_LAYERS-1 + QN_MAX_WEIGHTMATS
    };

    // If layer sizes are non-zero for input, the file is checked to
    // see if they are consistent
    QN_MLPWeightFile_Matlab(int a_dbg, const char* a_dbgname, FILE* a_stream,
			    QN_FileMode a_mode, size_t a_layers,
			    const size_t* a_layer_units = NULL);
    virtual ~QN_MLPWeightFile_Matlab();

    // How many layers are there?
    size_t num_layers();

    // How many sections?
    size_t num_sections();

    // How many units are there in the given layer?
    size_t size_layer(QN_LayerSelector layer);

    // Return whether weight matrices are input or output major
    enum QN_WeightMaj get_weightmaj();

    // Return the order of the weights in the file
    enum QN_SectionSelector get_weighttype(int section);

    // Is this fine QN_READ or QN_WRITE?
    enum QN_FileMode get_filemode();

    // Read in a buffer full of weights
    // Note that it is illegal to read across weight matrix boundaries.
    size_t read(float* dest, size_t count);

    // Write out a buffer full of weights
    size_t write(const float* buf, size_t count);

private:
    // A function to write the matrix info at the start
    // of a given section
    void write_section_header();
    // Get the headers of relevant matrices into the minfo array.
    void read_all_hdrs();
    // Go to the start of a given section.
    void seek_section_header();
    
private:
    // Names of the section matrices.
    QN_ClassLogger clog;	// Handles logging
    FILE* const stream;		// Stream used to access weights files
    const enum QN_FileMode mode;

    // Names of the matrices for each section.
    static const char* sectname[QN_MAX_SECTIONS];

    size_t n_layers;		// Number of layers in the net.
    size_t layer_units[QN_MAX_LAYERS]; // The size of each layer.
    size_t io_state;		// What section we are in.
    size_t io_count;		// Count of items remaining in this section.
    int isbigendian;		// Is the current matrix being read bigendian?
    int isdouble;		// Is the current matrix being read double?

    // The names of the various sections.
    // How we store the matlab data.
    enum { QN_DEFAULT_TYPE = QN_MAT_TYPE_FLOAT
	   + QN_MAT_TYPE_NATIVE + QN_MAT_TYPE_FULL };

    // A structure for recording the location of each matrix on disk.
    struct MatInfo {
	fpos_t pos;		// The location of the matrix in the file.
	int isbigendian;	// Big endian?
	int isdouble;		// Double precision?
	size_t rows;		// Number of rows.
	size_t cols;		// Number of columns.
    };
    struct MatInfo minfo[QN_MAX_SECTIONS]; // Info on the matrices on disk.
    enum { QN_MAXNAMLEN = 32 };	// The maximum matrix name length.

};

#endif

