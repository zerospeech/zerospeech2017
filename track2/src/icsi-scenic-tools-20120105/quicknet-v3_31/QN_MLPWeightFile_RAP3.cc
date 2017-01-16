#ifndef NO_RCSID
const char* QN_MLPWeightFile_RAP3_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLPWeightFile_RAP3.cc,v 1.23 2006/02/16 17:53:25 davidj Exp $";
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
#include "QN_MLPWeightFile_RAP3.h"


// Name of this class
const char* QN_MLPWeightFile_RAP3::CLASSNAME = "QN_MLPWeightFile_RAP3";

// Constructor for an input weight file

QN_MLPWeightFile_RAP3::QN_MLPWeightFile_RAP3(int dbg, FILE* a_stream,
					     QN_FileMode a_mode,
					     const char* a_name,
					     size_t a_input, size_t a_hidden,
					     size_t a_output)
  : debug(dbg),
    mode(a_mode),
    stream(a_stream),
    filename(a_name)
{
    
    if (a_mode==QN_READ)
    {
	if (debug>=2)
	{
	    QN_LOG(CLASSNAME, "Accessing weight file %s for reading.",
		   filename);
	}
	if (scan_file()) {
	    // seekable file, could scan
	    if (a_input!=0 && n_input!=a_input)
	    {
		QN_ERROR(CLASSNAME, "number of inputs in weight file \'%s\' is"
			 " %lu - it should be %lu.", filename, n_input,a_input);
	    }
	    if (a_hidden!=0 && n_hidden!=a_hidden)
	    {
		QN_ERROR(CLASSNAME, "number of hiddens in weight file \'%s\' is"
			 " %lu - it should be %lu.", filename, n_hidden, a_hidden);
	    }
	    if (a_output!=0 && n_output!=a_output)
	    {
		QN_ERROR(CLASSNAME, "number of outputs in weight file \'%s\' is"
			 " %lu - it should be %lu.", filename, n_output, a_output);
	    }
	} else {
	    // don't yet know size
	    if (a_input == 0 || a_hidden == 0 || a_output == 0) {
		QN_ERROR(CLASSNAME,
			 "input sizes not specified but file \"%s\" not "
			 "seekable.", filename);
	    }
	    n_input = a_input;
	    n_hidden = a_hidden;
	    n_output = a_output;
	}

	// Read past first vector header, so we are ready to read data
	unsigned long in2hid=0;
	int cnt;

	fscanf(stream, " weigvec %lu %n", &in2hid, &cnt);
	if (cnt<10 || in2hid == 0)
	{
	    QN_ERROR(CLASSNAME,
		     "failed to read first 'weigvec' in weight file \'%s\'.",
		     filename);
	}
	if (a_input!=0 && a_hidden != 0 && in2hid!=a_input*a_hidden)
	{
	    QN_ERROR(CLASSNAME, "number of i2hs in weight file \'%s\' is"
		     " %lu - it should be %lu.", filename, in2hid,
		     a_input*a_hidden);
	}
	io_count = in2hid;
	io_state = QN_MLP3_INPUT2HIDDEN;
    }
    else if (a_mode == QN_WRITE)
    {
	if (debug>=2)
	{
	    QN_LOG(CLASSNAME, "Accessing weight file %s for writing.",
		   filename);
	}
	n_input = a_input;
	n_hidden = a_hidden;
	n_output = a_output;

	// Write the first vector header
	int ec;

	ec = fprintf(stream, "weigvec %u\n", (unsigned) (n_input * n_hidden));
	if (ec<0)
	{
	    QN_ERROR(CLASSNAME, "error writing weight file \'%s\' - %s.",
		     filename, strerror(errno));
	}
	io_count = n_input * n_hidden;
	io_state = QN_MLP3_INPUT2HIDDEN;
    }
    else
	assert(0);
}

enum QN_SectionSelector
QN_MLPWeightFile_RAP3::get_weighttype(int section)
{
    switch(section)
    {
    case 0:
	return QN_MLP3_INPUT2HIDDEN;
	break;
    case 1:
	return QN_MLP3_HIDDEN2OUTPUT;
	break;
    case 2:
	return QN_MLP3_HIDDENBIAS;
	break;
    case 3:
	return QN_MLP3_OUTPUTBIAS;
	break;
    default:
	return QN_WEIGHTS_UNKNOWN;
	break;
    }
}

size_t
QN_MLPWeightFile_RAP3::read(float* buf, size_t count)
{
    size_t cur_ele;		// Runs through <count> elements

    assert(mode == QN_READ);

    // This is a hack so we can keep attempting to read past the end of file
    // without crashing
    if (io_count==0 || count==0)
	return(0);
    for (cur_ele=0; cur_ele<count; cur_ele++)
    {
	float val;		// The value read in
	int cnt;		// Count of characters scanned
	unsigned long eles;	// Count of elements in next section

	cnt = 0;
	fscanf(stream, " %f%n", &val, &cnt);
	if (cnt==0)
	{
	    QN_ERROR(CLASSNAME, "failed to read weight value at byte "
		     "offset %lu in file \'%s\'\n", ftell(stream), filename);
	}
	*buf++ = val;
	io_count--;
	if (io_count==0)
	{
	    switch(io_state)
	    {
	    case QN_MLP3_INPUT2HIDDEN:
		fscanf(stream, " weigvec %lu %n", &eles, &cnt);
		if (cnt<10 || eles!=n_hidden*n_output)
		{
		    QN_ERROR(CLASSNAME,
			     "failed to read second 'weigvec' in "
			     "weight file \'%s\'",
			     filename);
		}
		io_state = QN_MLP3_HIDDEN2OUTPUT;
		io_count = eles;
		break;
	    case QN_MLP3_HIDDEN2OUTPUT:
		fscanf(stream, " biasvec %lu %n", &eles, &cnt);
		if (cnt<10 || eles!=n_hidden)
		{
		    QN_ERROR(CLASSNAME,
			     "failed to read first 'biasvec' in "
			     "weight file \'%s\'",
			     filename);
		}
		io_state = QN_MLP3_HIDDENBIAS;
		io_count = eles;
		break;
	    case QN_MLP3_HIDDENBIAS:
		fscanf(stream, " biasvec %lu %n", &eles, &cnt);
		if (cnt<10 || eles!=n_output)
		{
		    QN_ERROR(CLASSNAME,
			     "failed to read second 'biasvec' in "
			     "weight file \'%s\'",
			     filename);
		}
		io_state = QN_MLP3_OUTPUTBIAS;
		io_count = eles;
		break;
	    case QN_MLP3_OUTPUTBIAS:
		return cur_ele+1;
		break;
	    default:
		assert(0);
	    }
	}
    }
    // We get here if we return as many elements as requested
    return count;
}

int
QN_MLPWeightFile_RAP3::write_one(float weight)
{
    int ec;			// Error code

    ec = fprintf(stream, "%g\n", weight);
    return (ec<0) ? QN_BAD : QN_OK;
}


size_t
QN_MLPWeightFile_RAP3::write(const float* buf, size_t count)
{
    size_t cur_ele;		// Runs through <count> elements

    assert(mode == QN_WRITE);

    // This is a hack so we can keep attempting to write past the end of file
    // without crashing
    if (io_count==0 || count==0)
	return(0);
    for (cur_ele=0; cur_ele<count; cur_ele++)
    {
	int ec;			// Error return value

	ec = write_one(*buf++);
	if (ec!=QN_OK)
	{
	    QN_ERROR(CLASSNAME, "error writing weight file \'%s\' - %s",
		     filename, strerror(errno));
	}
	io_count--;
	if (io_count==0)
	{
	    switch(io_state)
	    {
	    case QN_MLP3_INPUT2HIDDEN:
		ec = fprintf(stream, "weigvec %lu\n",
			     (unsigned long) n_hidden * n_output);
		if (ec<0)
		{
		    QN_ERROR(CLASSNAME,"error writing weight file \'%s\' - %s",
		     filename, strerror(errno));
		}
		io_state = QN_MLP3_HIDDEN2OUTPUT;
		io_count = n_hidden * n_output;
		break;
	    case QN_MLP3_HIDDEN2OUTPUT:
		ec = fprintf(stream, "biasvec %lu\n", (unsigned long)
			     n_hidden);
		if (ec<0)
		{
		    QN_ERROR(CLASSNAME,"error writing weight file \'%s\' - %s",
			     filename, strerror(errno));
		}
		io_state = QN_MLP3_HIDDENBIAS;
		io_count = n_hidden;
		break;
	    case QN_MLP3_HIDDENBIAS:
		ec = fprintf(stream, "biasvec %u\n", (unsigned) n_output);
		if (ec<0)
		{
		    QN_ERROR(CLASSNAME,"error writing weight file \'%s\' - %s",
			     filename, strerror(errno));
		}
		io_state = QN_MLP3_OUTPUTBIAS;
		io_count = n_output;
		break;
	    case QN_MLP3_OUTPUTBIAS:
		return cur_ele+1;
		break;
	    default:
		assert(0);
	    }
	}
    }
    return count;
}

// Scan throught the weight file and get the size of the various matrices

int
QN_MLPWeightFile_RAP3::scan_file()
{
    int ec;			// Error code
    fpos_t pos;			// Current position
    unsigned in2hid = 0;	// Input to hidden size
    unsigned hid2out = 0;	// Hidden to output size
    unsigned hidden = 0;	// Hidden size
    unsigned output = 0;	// Output size
    int cnt;			// Count of characters for scanf
    size_t i;			// Local counter
    

    // Remember where we are
    ec = fgetpos(stream, &pos);
    if (ec!=0) {
	//	QN_ERROR(CLASSNAME,
	//		 "failed to get position in weight file \'%s\' - %s",
	//		 filename, strerror(errno));
	return 0;
    }

    // Get the sizes of the weight matrices

    fscanf(stream, " weigvec %u %n", &in2hid, &cnt);
    if (cnt<10 || in2hid==0)
	QN_ERROR(CLASSNAME,
		 "failed to find first 'weigvec' in weight file \'%s\'",
		 filename);
    for (i=0; i<in2hid; i++)
    {
	char c;

	// The file contains floating point numbers separated by whitespace
	c = getc(stream);
	while(!isspace(c))
	    c = getc(stream);
	while(isspace(c))
	    c = getc(stream);
	ungetc(c, stream);
    }

    fscanf(stream, " weigvec %u %n", &hid2out, &cnt);
    if (cnt<10 || hid2out==0)
    {
	QN_ERROR(CLASSNAME,
		 "failed to find second 'weigvec' in weight file \'%s\'",
		 filename);
    }
    for (i=0; i<hid2out; i++)
    {
	char c;

	// The file contains floating point numbers separated by whitespace
	c = getc(stream);
	while(!isspace(c))
	    c = getc(stream);
	while(isspace(c))
	    c = getc(stream);
	ungetc(c, stream);
    }

    fscanf(stream, " biasvec %u %n", &hidden, &cnt);
    if (cnt<10 || hidden==0)
    {
	QN_ERROR(CLASSNAME,
		 "failed to find first 'biasvec' in weight file \'%s\'",
		 filename);
    }
    for (i=0; i<hidden; i++)
    {
	char c;

	// The file contains floating point numbers separated by whitespace
	c = getc(stream);
	while(!isspace(c))
	    c = getc(stream);
	while(isspace(c))
	    c = getc(stream);
	ungetc(c, stream);
    }

    fscanf(stream, " biasvec %u %n", &output, &cnt);
    if (cnt<10 || output==0)
    {
	QN_ERROR(CLASSNAME,
		 "failed to find second 'biasvec' in weight file \'%s\'",
		 filename);
    }

    // Repostion back to where we started
    ec = fsetpos(stream, &pos);
    if (ec!=0)
    {
	QN_ERROR(CLASSNAME, "failed to reposition back to start of "
		 "weight file \'%s\' - %s", filename, strerror(errno));
    }

    n_input = in2hid / hidden;
    n_hidden = hidden;
    n_output = output;
    if (n_input * n_hidden != in2hid)
    {
	QN_ERROR(CLASSNAME, "inconsistent number if input/hidden weights"
		 " in weight file \'%s\'", filename);
    }
    if (n_hidden * n_output != hid2out)
    {
	QN_ERROR(CLASSNAME, "inconsistent number if hidden/output weights"
		 " in weight file \'%s\'", filename);
    }
    return 1;
}

size_t
QN_MLPWeightFile_RAP3::size_layer(QN_LayerSelector layer)
{
    size_t res;

    switch(layer)
    {
    case QN_MLP3_INPUT:
	res = n_input;
	break;
    case QN_MLP3_HIDDEN:
	res = n_hidden;
	break;
    case QN_MLP3_OUTPUT:
	res = n_output;
	break;
    default:
	res = 0;
	break;
    }
    return res;
}

enum QN_WeightMaj
QN_MLPWeightFile_RAP3::get_weightmaj()
{
    return QN_OUTPUTMAJOR;
}

enum QN_FileMode
QN_MLPWeightFile_RAP3::get_filemode()
{
    return mode;
}

