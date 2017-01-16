#ifndef NO_RCSID
const char* QN_MLPWeightFile_Matlab_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLPWeightFile_Matlab.cc,v 1.9 2006/06/24 01:43:39 davidj Exp $";
#endif

/* Must include the config.h file first */
#include <QN_config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_utils.h"
#include "QN_libc.h"
#include "QN_MLPWeightFile_Matlab.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"

const char* QN_MLPWeightFile_Matlab::sectname[] =
{
    "weights12",
    "bias2",
    "weights23",
    "bias3",
    "weights34",
    "bias4",
    "weights45",
    "bias5"
};

// Constructor for an input weight file

QN_MLPWeightFile_Matlab::QN_MLPWeightFile_Matlab(int a_debug, 
						 const char* a_dbgname,
						 FILE* a_stream,
						 QN_FileMode a_mode,
						 size_t a_layers,
						 const size_t* a_layer_units)
    : clog(a_debug, "QN_MLPWeightFile_Matlab", a_dbgname),
      stream(a_stream),
      mode(a_mode),
      n_layers(a_layers)
{
    size_t i;

    assert((size_t) QN_MAX_LAYERS <= (size_t) QN_MLP_MAX_LAYERS);
    if (a_layers>QN_MAX_LAYERS)
    {
	clog.error("Requested %d layers, maximum is %d.",
		   a_layers, QN_MAX_LAYERS);
    }
    // Initialize our layer sizes.
    for (i = 0; i<n_layers; i++)
    {
	if (a_layer_units!=NULL)
	    layer_units[i] = a_layer_units[i];
	else
	    layer_units[i] = 0;
    }

    if (a_mode==QN_READ)
    {
	clog.log(QN_LOG_PER_EPOCH, "Accessing weight file '%s' for reading.",
		 QN_FILE2NAME(a_stream));
	for (i=0; i<QN_MAX_SECTIONS; i++)
	{
	    minfo[i].rows = 0;
	    minfo[i].cols = 0;
	}
	// Read in all of the matrix headers, setting minfo up on the way.
	read_all_hdrs();

	// Work out how many layers the file seems to have.
	size_t file_n_layers;	// How many layers the file seems to have
	size_t file_layer_units[QN_MAX_LAYERS]; // The apparent size of each
					     // layer.
	// Use the weights to work out the number of layers.
	file_n_layers = 0;
	for (i=0; i<QN_MAX_SECTIONS; i+=2)
	{
	    if (minfo[i].rows>0)
		file_n_layers = i/2 + 2;
	}
	clog.log(QN_LOG_PER_EPOCH, "Weight file appears to have %lu layers.",
		 (unsigned long) file_n_layers);

	// Use the weights again to work out the size of each layer.
	file_layer_units[0] = minfo[0].cols;
	for (i=1; i<file_n_layers; i++)
	{
	    file_layer_units[i] = minfo[(i-1)*2].rows;
	}
	for (i=0; i<file_n_layers; i++)
	{
	    clog.log(QN_LOG_PER_EPOCH, "Layer %lu has %lu units.",
		     (unsigned long) i+1, (unsigned long) file_layer_units[i]);
	}

	// Now check the size of each matrix.
	size_t file_n_sections = (file_n_layers-1) * 2;
	size_t sect, lay;
	for (sect = 0, lay = 0; sect<file_n_sections; sect+=2, lay++)
	{
	    if ( (minfo[sect].cols!=file_layer_units[lay]) ||
		 (minfo[sect].rows!=file_layer_units[lay+1]) ||
		 (minfo[sect+1].cols!=file_layer_units[lay+1]) ||
		 (minfo[sect+1].rows!=1) )
	    {
		clog.error("Matlab Weight file '%s' has inconsistent "
			   "matrix sizes.", QN_FILE2NAME(stream));
	    }
		
	}
	// If constructor specifies number of layers, check file agress.
	if (a_layers!=0 && a_layers != file_n_layers)
	{
	    clog.error("Matlab Weight file constructor requested %lu layers, "
		       "file '%s' only had %lu layers.",
		       a_layers, QN_FILE2NAME(stream), file_n_layers);
	}
	n_layers = file_n_layers;
	// If constructor specifies size of layer, check file agrees.
	for (i=0; i<n_layers; i++)
	{
	    if (a_layer_units!=NULL && a_layer_units[i]!=0
		    && a_layer_units[i]!=file_layer_units[i])
	    {
		clog.error("Matlab Weight file constructor requested "
			   "layer %lu had %lu units, file '%s' had "
			   "%lu in the layer.",
			   (unsigned long) i+1,
			   (unsigned long) a_layer_units[i],
			   QN_FILE2NAME(stream),
			   (unsigned long) file_layer_units[i]);
	    }
	    layer_units[i] = file_layer_units[i];
	}
	// QN_WEIGHTS_UNKNOWN is used to signal start of file.
	io_state = QN_WEIGHTS_UNKNOWN;
	seek_section_header();
    }
    else if (a_mode == QN_WRITE)
    {
	clog.log(QN_LOG_PER_EPOCH, "Accessing matlab weight file "
		 "'%s' for writing.", QN_FILE2NAME(a_stream));
	for (i = 0; i< n_layers; i++)
	{
	    if (layer_units[i] == 0)
		clog.error("Cannot specify layer %d to have 0 units when writing.");
	}
	// QN_WEIGHTS_UNKNOWN is used to signal start of file.
	io_state = QN_WEIGHTS_UNKNOWN;
	// Write the header for the current, intial, matrix.
	write_section_header();
    }
    else
    {
	clog.error("Unknown file access mode.");
    }
}

QN_MLPWeightFile_Matlab::~QN_MLPWeightFile_Matlab()
{
}


size_t QN_MLPWeightFile_Matlab::num_layers()
{
    return n_layers;
}

size_t QN_MLPWeightFile_Matlab::num_sections()
{
    return (n_layers-1)*2;
}

void QN_MLPWeightFile_Matlab::write_section_header()
{
    const char* name = NULL;	// The name of the matrix.
    size_t rows = 0;		// The number of rows in the matrix.
    size_t cols = 0;		// The number of cols in the matrix.
    size_t ret;


    switch(io_state)
    {
    case QN_WEIGHTS_UNKNOWN:
	io_state = QN_LAYER12_WEIGHTS;
	rows = layer_units[1];
	cols = layer_units[0];
	io_count = rows * cols;
	name = sectname[0];
	break;
    case QN_LAYER12_WEIGHTS:
	io_state = QN_LAYER2_BIAS;
	rows = 1;
	cols = layer_units[1];
	io_count = rows * cols;
	name = sectname[1];
	break;
    case QN_LAYER2_BIAS:
	if (n_layers >2)
	{
	    io_state = QN_LAYER23_WEIGHTS;
	    rows = layer_units[2];
	    cols = layer_units[1];
	    io_count = rows * cols;
	    name = sectname[2];
	}
	else
	    io_state = QN_WEIGHTS_UNKNOWN;
	break;
    case QN_LAYER23_WEIGHTS:
	io_state = QN_LAYER3_BIAS;
	rows = 1;
	cols = layer_units[2];
	io_count = rows * cols;
	name = sectname[3];
	break;
    case QN_LAYER3_BIAS:
	if (n_layers >3)
	{
	    io_state = QN_LAYER34_WEIGHTS;
	    rows = layer_units[3];
	    cols = layer_units[2];
	    io_count = rows * cols;
	    name = sectname[4];
	}
	else
	    io_state = QN_WEIGHTS_UNKNOWN;
	break;
    case QN_LAYER34_WEIGHTS:
	io_state = QN_LAYER4_BIAS;
	rows = 1;
	cols = layer_units[3];
	io_count = rows * cols;
	name = sectname[5];
	break;
    case QN_LAYER4_BIAS:
	if (n_layers>4)
	{
	    io_state = QN_LAYER45_WEIGHTS;
	    rows = layer_units[4];
	    cols = layer_units[3];
	    io_count = rows * cols;
	    name = sectname[6];
	}
	else
	    io_state = QN_WEIGHTS_UNKNOWN;
	break;
    case QN_LAYER45_WEIGHTS:
	io_state = QN_LAYER5_BIAS;
	rows = 1;
	cols = layer_units[4];
	io_count = rows * cols;
	name = sectname[7];
	break;
    case QN_LAYER5_BIAS:
	io_state = QN_WEIGHTS_UNKNOWN;
	break;
    default:
	assert(0);
    }
    if (io_state !=QN_WEIGHTS_UNKNOWN)
    {
	QN_Mat_Header hdr;	// A header for the current matrix.
	hdr.type = QN_DEFAULT_TYPE;
	hdr.mrows = rows;
	hdr.mcols = cols;
	hdr.imagf = 0;
	hdr.namlen = strlen(name)+1; // Note we write the null char too.
	ret = hdr.fwrite(stream);
	if (ret!=1)
	{
	    clog.error("Failed to write header of matrix '%s' to "
		       "matlab weights file '%s' - %s.",
		       name, QN_FILE2NAME(stream), strerror(errno));
	}
	ret = fwrite(name, 1, hdr.namlen, stream);
	if (ret!=hdr.namlen)
	{
	    clog.error("Failed to write name of matrix '%s' to "
		       "matlab weights file '%s' - %s.",
		       name, QN_FILE2NAME(stream), strerror(errno));
	}
    }
}

void QN_MLPWeightFile_Matlab::seek_section_header()
{
    const char* matname;	// The name of the matrix.
    size_t rows;		// The number of rows in the matrix.
    size_t cols;		// The number of cols in the matrix.
    fpos_t pos;			// Current position.
    QN_Mat_Header hdr;		// Matrix header.
    char name[QN_MAT_NAMLEN_MAX];
    int ec;
    size_t cnt;

    if (io_state == QN_WEIGHTS_UNKNOWN)
	io_state = 0;
    else
	io_state++;
    if (io_state>((n_layers-1)*2))
    {
	io_state = QN_WEIGHTS_UNKNOWN;
	return;
    }
    
    rows = minfo[io_state].rows;
    cols = minfo[io_state].cols;
    io_count = rows * cols;
    matname = sectname[io_state];
    pos = minfo[io_state].pos;
    isbigendian = minfo[io_state].isbigendian;
    isdouble = minfo[io_state].isdouble;

    ec = fsetpos(stream, &pos);
    if (ec!=0)
    {
	clog.error("Failed to seek to matrix '%s' in "
		   "matlab weights file '%s' - %s.",
		   matname, QN_FILE2NAME(stream), strerror(errno));
    }
    // Read in the matrix header.
    cnt = hdr.fread(stream);
    if (cnt !=1)
    {
	clog.error("Failed to read matrix '%s' from '%s' - %s.",
		   matname, QN_FILE2NAME(stream), strerror(errno));
    }

    // Do less checking here because we have already scanned the header.
    if (hdr.namlen>QN_MAT_NAMLEN_MAX)
    {
	clog.error("Matrix name too long in matrix '%s' from "
		   "matlab weights file '%s'.", matname,
		   QN_FILE2NAME(stream));
    }
    cnt = fread(name, 1, hdr.namlen, stream);
    if (cnt!=hdr.namlen)
    {
	clog.error("Failed to read name of matrix '%s' from "
		   "matlab weights file '%s' - %s.",
		   matname,  QN_FILE2NAME(stream), strerror(errno));
    }
    if (name[hdr.namlen-1]!='\0')
    {
	clog.error("Missing terminating null in name of "
		   "matrix '%s' from matlab weights file '%s'.",
		   matname,
		   QN_FILE2NAME(stream));
    }
    if (strcmp(name, matname)!=0)
    {
	clog.error("Name of matrix '%s' read as '%s' from "
		   "matlab weights file '%s', should be '%s'.",
		   matname, name,  QN_FILE2NAME(stream), matname);
    }
}

enum QN_SectionSelector
QN_MLPWeightFile_Matlab::get_weighttype(int section)
{
    enum QN_SectionSelector rc = QN_WEIGHTS_UNKNOWN;

    switch(section)
    {
    case 0:
	rc =  QN_LAYER12_WEIGHTS;
	break;
    case 1:
	rc = QN_LAYER2_BIAS;
	break;
    case 2:
	if (n_layers>2)
	    rc =  QN_LAYER23_WEIGHTS;
	break;
    case 3:
	if (n_layers>2)
	    rc =  QN_LAYER3_BIAS;
	break;
    case 4:
	if (n_layers>3)
	    rc =  QN_LAYER34_WEIGHTS;
	break;
    case 5:
	if (n_layers>3)
	    rc =  QN_LAYER4_BIAS;
	break;
    case 6:
	if (n_layers>4)
	    rc =  QN_LAYER45_WEIGHTS;
	break;
    case 7:
	if (n_layers>4)
	    rc =  QN_LAYER5_BIAS;
	break;
    }
    if (rc==QN_WEIGHTS_UNKNOWN)
    {
	clog.error("Trying to get section %d weights when we only have "
		   "%lu layers.", section, (unsigned long) n_layers);
    }
    return rc;
}

size_t
QN_MLPWeightFile_Matlab::read(float* dest, size_t count)
{
    size_t res;

    if (mode != QN_READ)
    {
	clog.error("Tried to read writeable matlab weights file '%s'.",
		   QN_FILE2NAME(stream));
    }

    if (io_count==0)
	seek_section_header();
    if (io_state==QN_WEIGHTS_UNKNOWN)
    {
	clog.error("Tried to read past end of matlab weights file '%s'.",
		   QN_FILE2NAME(stream));
    }
    if (io_count<count)
    {
	clog.error("Tried to read past end of section in matlab weights "
		   " file '%s'.", QN_FILE2NAME(stream));
    }
    if (isdouble)
    {
	if (isbigendian)
	    res = qn_fread_Bd_vf(count, stream, dest);
	else
	    res = qn_fread_Ld_vf(count, stream, dest);
    }
    else
    {
	if (isbigendian)
	    res = qn_fread_Bf_vf(count, stream, dest);
	else
	    res = qn_fread_Lf_vf(count, stream, dest);
    }
    if (res!=count)
    {
	clog.error("Failed to read %lu values from matlab weight "
		   "file '%s' - %s.", (unsigned long) count,
		   QN_FILE2NAME(stream), strerror(errno));
    }
    io_count -= count;
    return count;
}


size_t
QN_MLPWeightFile_Matlab::write(const float* buf, size_t count)
{
    size_t rv;
    size_t ret;

    rv = count;

    if (mode != QN_WRITE)
    {
	clog.error("Tried to write readable matlab weights file '%s'.",
		   QN_FILE2NAME(stream));
    }
    if (io_state==QN_WEIGHTS_UNKNOWN)
    {
 	clog.error("Tried to write beyond end of weight file '%s'.",
		   QN_FILE2NAME(stream));
    }
    if (count > io_count)
    {
 	clog.error("Tried to write %lu values when only %lu space in "
		   "the current matrix.", count, io_count);
    }
    ret = fwrite(buf, sizeof(float), count, stream);
    if (ret!=count)
    {
	clog.error("Failed to write %lu floats to matlab weights file '%s' - "
		   "%s.", (unsigned long) count, QN_FILE2NAME(stream),
		   strerror(errno));
    }
    io_count -= count;
    if (io_count == 0)		// If we have done this matrix, on to next one.
	write_section_header();
    return rv;
}


size_t
QN_MLPWeightFile_Matlab::size_layer(QN_LayerSelector layer)
{
    size_t res = QN_SIZET_BAD;

    switch(layer)
    {
    case QN_LAYER1:
	res = layer_units[0];
	break;
    case QN_LAYER2:
	res = layer_units[1];
	break;
    case QN_LAYER3:
	if (n_layers>2)
	    res = layer_units[2];
	break;
    case QN_LAYER4:
	if (n_layers>3)
	    res = layer_units[3];
	break;
    case QN_LAYER5:
	if (n_layers>4)
	    res = layer_units[4];
	break;
    default:
	res = 0;
	clog.error("size_layer: uknown layer number %lu.",
		   (unsigned long) layer);
	break;
    }
    if (res==QN_SIZET_BAD)
    {
	clog.error("size_layer: layer %lu requested, this net only "
		   "has %lu layers.",
		   (unsigned long) layer, (unsigned long) n_layers);
    }
    return res;
}

enum QN_WeightMaj
QN_MLPWeightFile_Matlab::get_weightmaj()
{
    // The actual matlab matrices are OUTPUTMAJOR, but matlab
    // uses fortran row-major format to store the matrices so the
    // format on disk is actually INPUTMAJOR.
    return QN_INPUTMAJOR;
}

enum QN_FileMode
QN_MLPWeightFile_Matlab::get_filemode()
{
    return mode;
}


void QN_MLPWeightFile_Matlab::read_all_hdrs()
{
    int ec;
    size_t i;
    size_t ret;
    fpos_t startpos;		// Where we are in file at start.
    size_t matno = 0;		// The number of the current matrix.

    int isbigendian = 0;	// IS this matrix bigendian?
    int isdouble = 0;		// Is this matrix double (c.f. float)?

    QN_Mat_Header hdr;		// A local copy of the header.
    size_t namlen;		// The length of the matrix name.
    char name[QN_MAT_NAMLEN_MAX];	// The name of the matrix.
    size_t elesize = 0;		// The size of a single element.


    ec = fgetpos(stream, &startpos);
    if (ec!=0)
    {
	clog.error("Failed go get current position in "
		   "matlab weights file '%s' - %s.",
		   QN_FILE2NAME(stream), strerror(errno));
    }
    while (1)
    {
	int skip;		// Skip this matrix if non-zero.
	fpos_t pos;		// Position for this matrix.
	size_t index;		// INdex of the current section.

	ec = fgetpos(stream, &pos);
	if (ec!=0)
	{
	    clog.error("Failed go get current position in "
		       "matlab weights file '%s' - %s.",
		       QN_FILE2NAME(stream), strerror(errno));
	}
	// Read in the matrix header, does endian swapping too.
	ret = hdr.fread(stream);
	if (ret !=1)
	{
	    if (feof(stream))
		break;
	    else
	    {
		clog.error("Failed to read matrix header from '%s' - %s.",
			   QN_FILE2NAME(stream), strerror(errno));
	    }
	}
	namlen = hdr.namlen;
	if (namlen>QN_MAT_NAMLEN_MAX)
	{
	    clog.error("Matrix name too long in matrix number %lu from "
		       "matlab weights file '%s'.", (unsigned long) matno,
		       QN_FILE2NAME(stream));
	}
        ret = fread(name, 1, namlen, stream);
	if (ret!=namlen)
	{
	    clog.error("Failed to read name of matrix number %lu from "
		       "matlab weights file '%s' - %s.",
		       (unsigned long) matno,
		       QN_FILE2NAME(stream), strerror(errno));
	}
	if (name[namlen-1]!='\0')
	{
	    clog.error("Missing terminating null in name of "
		       "matrix number %lu from matlab weights file '%s'.",
		       (unsigned long) matno,
		       QN_FILE2NAME(stream));
	}
	// Work out matrices we cannot handle.
	skip = 0;
	switch (hdr.type_machine())
	{
	case QN_MAT_TYPE_IEEELITTLE:
	    isbigendian = 0;
	    break;
	case QN_MAT_TYPE_IEEEBIG:
	    isbigendian = 1;
	    break;
	default:
	    skip = 1;
	}   
	switch (hdr.type_datatype())
	{
	case QN_MAT_TYPE_DOUBLE:
	    isdouble = 1;
	    elesize = sizeof(double);
	    break;
	case QN_MAT_TYPE_FLOAT:
	    isdouble = 0;
	    elesize = sizeof(float );
	    break;
	default:
	    skip = 1;
	}
	switch (hdr.type_layout())
	{
	case QN_MAT_TYPE_FULL:
	    break;
	default:
	    skip = 1;
	}
	if (hdr.mrows==0 || hdr.mcols==0 || hdr.imagf!=0)
	    skip = 1;
	index = QN_SIZET_BAD; // An index value indicating unknown sect.
	for (i=0; i<QN_MAX_SECTIONS; i++)
	{
	    if (!strcmp(name, sectname[i]))
	    {
		index = i;
	    }
	}
	if (index==QN_SIZET_BAD)
	    skip |= 1;
	if (skip)
	{
	    clog.warning("Skipping matrix number %lu, name '%s' in file '%s'.",
			 (unsigned long) matno, name, QN_FILE2NAME(stream));
	}
	else
	{
	    // Use non-zero rows to check for previous matrix of same name.
	    if (minfo[index].rows>0)
	    {
		clog.warning("Duplicate matrix '%s' in Matlab weights file "
			     "'%s'.", sectname[index], QN_FILE2NAME(stream));
			   
	    }
	    minfo[index].pos = pos;
	    minfo[index].isbigendian = isbigendian;
	    minfo[index].isdouble = isdouble;
	    minfo[index].rows = hdr.mrows;
	    minfo[index].cols = hdr.mcols;
	}

	// Skip over the data.
	ec = fseek(stream, hdr.mrows * hdr.mcols * elesize, SEEK_CUR);
	if (ec)
	{
	    clog.error("Failed to seek over matrix number %lu in matlab "
		       "weight file '%s'.", (unsigned long) matno,
		       QN_FILE2NAME(stream));
	}
	matno++;		// On to next matrix in file.
    }

    // Seek back to start of file.
    ec = fsetpos(stream, &startpos);
    if (ec!=0)
    {
	clog.error("Failed go seek to start in "
		   "matlab weights file '%s' - %s.",
		   QN_FILE2NAME(stream), strerror(errno));
    }
}
