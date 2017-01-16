const char* QN_mat_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_mat.cc,v 1.10 2006/02/04 01:15:03 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_mat.h"
#include "QN_fltvec.h"

inline static size_t
min(size_t a, size_t b)
{
    if (a<=b)
	return a;
    else
	return b;
}

QN_Mat_Header::QN_Mat_Header(QNUInt32 a_type, QNUInt32 a_mrows,
			     QNUInt32 a_mcols, QNUInt32 a_imagf,
			     QNUInt32 a_namlen)
{
    type = a_type;
    mrows = a_mrows;
    mcols = a_mcols;
    imagf = a_imagf;
    namlen = a_namlen;
}

size_t QN_Mat_Header::fread(FILE* file)
{
    size_t rc;

    rc = ::fread(this, sizeof(*this), 1, file);
    if (rc == 1)
    {
	// In general, other-endianness will show up as bits in the high
	// 16 bits of the type.  The exception is when type is 0,
	// in which case other-endian is true if we are big endian
	// (as 0 can only happen on little endian machines).
	if ((type & 0xffff0000) || (type==0 && QN_BIGENDIAN))
	{
	    type = qn_swapb_i32_i32(type);
	    mrows = qn_swapb_i32_i32(mrows);
	    mcols = qn_swapb_i32_i32(mcols);
	    imagf = qn_swapb_i32_i32(imagf);
	    namlen = qn_swapb_i32_i32(namlen);
	}
    }
    return rc;
 }

size_t QN_Mat_Header::fwrite(FILE* file)
{
    return ::fwrite(this, sizeof(*this), 1, file);
}

/////


////

#ifdef NEVER

QN_Mat_OutMatrix::QN_Mat_OutMatrix(FILE* a_file, const char* a_name,
				   size_t a_rows, size_t a_cols,
				   size_t a_bufsize)
    : // We save in native format and rely on the reader to convert.
      hdr(QN_MAT_TYPE_DOUBLE + QN_MAT_TYPE_NATIVE + QN_MAT_TYPE_FULL,
	  a_rows, a_cols, 0, strlen(a_name)+1),
      file(a_file),
      name(a_name),
      count(0),
      bufsize(a_bufsize),
      floatbuf(NULL)
{
    size_t cnt;			// Number of items written.

    // Currently only support floats.
    assert (hdr.mrows!=0 && hdr.mcols!=0);

    floatbuf = new float[bufsize];
    cnt = fwrite(&hdr, sizeof(hdr), 1, file);
    if (cnt!=1)
    {	
	QN_ERROR("QN_Mat_OutMatrix",
		 "failed to write matrix '%s' header to '%s' - %s.",
		 name, QN_FILE2NAME(file), strerror(errno));
    }
    cnt = fwrite(name, hdr.namlen, 1, file);
    if (cnt!=1)
    {	
	QN_ERROR("QN_Mat_OutMatrix",
		 "failed to write matrix named '%s' to '%s' - %s.",
		 name, QN_FILE2NAME(file), strerror(errno));
    }
}

QN_Mat_OutMatrix::~QN_Mat_OutMatrix()
{
    // Check that we output all values.
    if (count != (hdr.mrows*hdr.mcols))
    {
	QN_ERROR("QN_Mat_OutMatrix",
		 "incorrect number of values written to matrix '%s'.",
		 name);
    }
    if (floatbuf!=NULL)
	delete[] floatbuf;
}

void
QN_Mat_OutMatrix::output(size_t a_count, const float* a_vals)
{
    size_t cnt;			// Number of items written.

    cnt = fwrite(a_vals, sizeof(float), a_count, file);
    if (cnt!=a_count)
    {	
	QN_ERROR("QN_Mat_OutMatrix",
		 "failed to write contents of matrix '%s'- %s.",
		 name, strerror(errno));
    }
    count += a_count;
}

void
QN_Mat_OutMatrix::dump(size_t rows, size_t cols, float* mat)
{
    size_t i;			// Current column.

    for (i=0; i<cols; i++)	// Iterate over columns.
    {
	size_t j;		// Current row within column.
	float* col;		// Pointer to current part of column.

	col = &(mat[i]);
	for (j=0; j<rows; j+=bufsize) // Iterate over chunks of one column.
	{
	    size_t len = min(rows-j, bufsize); // 
	    
	    qn_copy_svf_vf(len, cols, col, floatbuf);
	    output(len, floatbuf);
	    col += len*cols;
	}
    }
}

#endif // NEVER
