// $Header: /u/drspeech/repos/quicknet2/QN_mat.h,v 1.9 2006/02/04 01:14:54 davidj Exp $

#ifndef QN_mat_h_INCLUDED
#define QN_mat_h_INCLUDED

// Some stuff to aid in the reading and writing of Matlab ".mat" files.
// Note that this is for matlab level 4 files, which are _not_
// written by default with recent versions of matlab.

/* Must include the config.h file first */
#include <QN_config.h>
#include <stddef.h>
#include <stdio.h>
#include "QN_types.h"
#include "QN_Logger.h"


// Some definitions for the "type" field in the Matlab R4 matrix header.
enum {

// The machine type.
    QN_MAT_TYPE_IEEELITTLE = 0,
    QN_MAT_TYPE_IEEEBIG = 1000,
#ifdef QN_WORDS_BIGENDIAN
    QN_MAT_TYPE_NATIVE = QN_MAT_TYPE_IEEEBIG,
#else
    QN_MAT_TYPE_NATIVE = QN_MAT_TYPE_IEEELITTLE,
#endif

// The data type.
    QN_MAT_TYPE_DOUBLE = 0,
    QN_MAT_TYPE_FLOAT = 10,
    QN_MAT_TYPE_INT32 = 20,
    QN_MAT_TYPE_INT16 = 30,
    QN_MAT_TYPE_UINT16 = 40,
    QN_MAT_TYPE_UINT8 = 50,

// The layout.
    QN_MAT_TYPE_FULL = 0,
    QN_MAT_TYPE_TEXT = 2,
    QN_MAT_TYPE_SPARSE = 3,

// The maximum length for a matlab matrix name, including terminating null.
    QN_MAT_NAMLEN_MAX = 32
    
};



// The header that goes at the start of each matrix.
struct QN_Mat_Header
{
    QNUInt32 type;
    QNUInt32 mrows;
    QNUInt32 mcols;
    QNUInt32 imagf;
    QNUInt32 namlen;
    QN_Mat_Header(QNUInt32 a_type = 
		  QN_MAT_TYPE_NATIVE|QN_MAT_TYPE_DOUBLE|QN_MAT_TYPE_FULL,
		  QNUInt32 a_mrows = 0, QNUInt32 a_mcols = 0,
		  QNUInt32 a_imagf = 0, QNUInt32 a_namlen = 0);
    // The machine number from the type field.
    QNUInt32 type_machine() { return ((type/1000) % 10) * 1000; } ;
    // The datatype from the type field.
    QNUInt32 type_datatype() { return ((type / 10) % 10) * 10; } ;
    // The layout from the type field.
    QNUInt32 type_layout() { return type % 10; } ;
    // Read a matrix header, swapping bytes if necessary.
    // Returns 1 on success else 0.
    size_t fread(FILE* file);
    // Write a matrix header in native format (no byte swapping).
    // Returns 1 on success else 0.
    size_t fwrite(FILE* file);
};

// A routine to write a matlab matrix to a file.
size_t QN_Mat_write(FILE* file, const char* name, size_t rows,
		    size_t cols, const float* mat);
size_t QN_Mat_write(FILE* file, const char* name, size_t rows,
		    size_t cols, const double* mat);
// A routine to read a matlab matrix from a file.
// "name" must point to a char array of at least QN_MAT_NAMLEN_MAX chars.
size_t QN_Mat_read(FILE* file, char* name, size_t rows,
		   size_t cols, float* mat);
size_t QN_Mat_read(FILE* file, char* name, size_t rows,
		   size_t cols, double* mat);


#ifdef NEVER
class QN_Mat_OutMatrix
{
public:
    enum { DEFAULT_BUFSIZE = 256 }; // The size of our internal buffer.
    // Create a matrix.
    // a_file = stream to create matrix on.
    // a_name = name of matrix.
    // a_rows = number of rows.
    // a_cols = number of cols.
    QN_Mat_OutMatrix(FILE* a_file, const char* a_name,
		     size_t a_rows, size_t a_cols,
		     size_t a_bufsize = QN_Mat_OutMatrix::DEFAULT_BUFSIZE);
    // Finish off the matrix.
    ~QN_Mat_OutMatrix();

    // Write a given number of values to the output matrix.  Note that
    // output is column major - i.e. we have to transpose traditional C
    // matrices.
    void output(size_t a_count, const float* a_vals);
    // Output a whole floating point matrix in C column-major order.
    void dump(size_t rows, size_t cols, float* mat);
private:
    const QN_Mat_Header hdr;	// The header information.
    FILE* file;			// The file on which we write the data.
    const char* name;		// The name of the matrix.
    size_t count;		// Count of values output so far.
    size_t bufsize;		// Size of the buffer used for conversions.
    float* floatbuf;		// A buffer for conversions.
};

// Some functions to encapsulate the whole matrix output operation.
static inline void
QN_Mat_dump(FILE* file, const char* name, size_t rows, size_t cols,
	    float* mat, size_t bufsize = QN_Mat_OutMatrix::DEFAULT_BUFSIZE)
{
    QN_Mat_OutMatrix om(file, name,
			rows, cols, bufsize);
    om.dump(rows, cols, mat);
}

#endif /* NEVER */

#endif // #define QN_mat_h_INCLUDED

