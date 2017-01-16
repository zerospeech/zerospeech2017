const char* QN_utils_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_utils.cc,v 1.107 2011/05/24 03:05:51 davidj Exp $";

/* Must include the config.h file first */
#include "QN_config.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef QN_HAVE_FLOAT_H
#include <float.h>
#endif
#ifdef QN_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef QN_HAVE_OMP_H
#include <omp.h>
#endif
#include "QN_fltvec.h"
#include "QN_libc.h"

#ifdef QN_HAVE_ATLAS_ATLAS_BUILDINFO_H
#include <atlas/atlas_buildinfo.h>
#else
#ifdef QN_HAVE_ATLAS_BUILDINFO_H
#include <atlas_buildinfo.h>
#endif
#ifdef QN_HAVE_MKL_H
#include <mkl.h>
#endif
#endif
#ifdef QN_CUDA
#include "QN_CudaUtils.h"
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#define EXIT_FAILURE (1)
#endif

#if !QN_HAVE_DECL_SRAND48
extern "C" {
void srand48(long);
}
#endif

#ifndef FLT_MAX
#define FLT_MAX (3.40282347e+38f)
#endif

#include "QN_types.h"
#include "QN_utils.h"
#include "QN_Logger.h"

// For the open_ftrstream fns
#include "QN_streams.h"
#include "QN_filters.h"
#include "QN_OnlNorm.h"
#include "QN_fir.h"
#include "QN_split.h"
#include "QN_windows.h"
#include "QN_cut.h"
#include "QN_paste.h"
#include "QN_RapAct.h"
#include "QN_PFile.h"
#include "QN_SRIfeat.h"
#include "QN_ListStreamSRI.h"
#include "QN_ILab.h"
#include "QN_AsciiStream.h"
#include "QN_camfiles.h"
#include "QN_MLPWeightFile_RAP3.h"
#include "QN_MLPWeightFile_Matlab.h"

#ifdef QN_HAVE_ATLAS_BUILDINFO_H
#define QN_HAVE_ATLAS_BUILDINFO
#endif
#ifdef QN_HAVE_ATLAS_ATLAS_BUILDINFO_H
#define QN_HAVE_ATLAS_BUILDINFO
#endif

// Initialize the math stuff

void
QN_math_init(int use_pp, int use_fe, int use_blas, int use_cuda,
	     int blas_threads)
{
    // Set the math mode
    qn_math = use_pp ? QN_MATH_PP : QN_MATH_NV;
    qn_math |= use_fe ? QN_MATH_FE : 0;
#ifdef QN_HAVE_LIBBLAS
    qn_math |= use_blas ? QN_MATH_BL : 0;
#else 
    if (use_blas)
    {
	QN_ERROR(NULL, "cannot enable BLAS library as none is linked with the "
		 "executable."); 
    }
#ifndef QN_CUDA
    if (use_cuda)
    {
	QN_ERROR(NULL, "cannot use CUDA GPU hardware because this version of QuickNet compiled without CUDA support."); 
    }
#endif // #ifnde QN_CUDA
#endif // #ifdef QN_HAVE_LIBBLAS
#ifdef QN_MKL_THREADING_GCC
    mkl_set_threading_layer(MKL_THREADING_GNU);
#endif

}


// Details about the host CPU

QN_cpuinfo::QN_cpuinfo(void)
    : cores(0)
{
    strcpy(cpu, "unknown");
    strcpy(mhz, "unknown");
    strcpy(hyper, "unknown");
    
    FILE* f;
    int ec;
    char buf[MAXLINE];
    f = fopen("/proc/cpuinfo", "r");

    int cur_siblings = 0;
    int cur_cores = 0;
    if (f!=NULL)
    {
	while(!feof(f))
	{
	    char* s;
	    s = fgets(buf, MAXLINE, f);
	    buf[MAXLINE-1] = '\0';
	    if (s==NULL)
	    {
		if (ferror(f))
		    QN_ERROR("QN_cpuinfo", "failed to read '/proc/cpuinfo'");
		else
		    break;
	    }
	    // Work out the CPU model
	    const char* model_line = "model name\t: ";
	    if (strncmp(buf, model_line, strlen(model_line))==0)
	    {
		strncpy(cpu, &buf[strlen(model_line)], MAXLINE);
		cpu[strlen(cpu)-1] = '\0'; // Get rid of the newline
	    }
	    // Work out the CPU mhz
	    const char* mhz_line = "cpu MHz\t\t: ";
	    if (strncmp(buf, mhz_line, strlen(mhz_line))==0)

	    {
		strncpy(mhz, &buf[strlen(mhz_line)], MAXLINE);
		mhz[strlen(mhz)-1] = '\0'; // Get rid of the newline
		strcat(mhz, " MHz"); // There's always room for this cos we
				    // stripped out the start of the line
	    }
	    // Work out the number of threads
	    const char* proc_line = "processor\t: ";
	    if (strncmp(buf, proc_line, strlen(proc_line))==0)
	    {
		int procnum;

		ec = sscanf(&buf[strlen(proc_line)], "%d", &procnum);
		if (ec==1)
		{
		    procnum++;	// Processor numbers start at 0
		    if (procnum>cores)
			cores = procnum;
		}
	    }

	    // Checks for hyperthreadings
	    const char* siblings_line = "siblings\t: ";
	    const char* cores_line = "cpu cores\t: ";
	    if (strncmp(buf, siblings_line, strlen(siblings_line))==0)
	    {
		ec = sscanf(&buf[strlen(siblings_line)], "%d", &cur_siblings);
	    }
	    if (strncmp(buf, cores_line, strlen(cores_line))==0)
	    {
		ec = sscanf(&buf[strlen(cores_line)], "%d", &cur_cores);
	    }
	    // Seperator lines are empty
	    if (strcmp(buf, "\n")==0)
	    {
		if (cur_cores!=cur_siblings)
		    strcpy(hyper, "enabled");
		else
		    strcpy(hyper, "disabled");
	    }
	}
	ec = fclose(f);
	if (ec)
	    QN_ERROR("QN_cpuinfo", "failed to close '/proc/cpuinfo'");
    }
    else
    {
	// One day there may be other ways of checking the cpu info
    }
    
}

// A function to output some of the info about the current system
// and libraries usign QN_OUTPUT

void
QN_output_sysinfo(const char* progname)
{
    QN_cpuinfo info;

    QN_OUTPUT("Program version: %s %s.", progname, QN_VERSION);
#ifdef QN_HAVE_GETHOSTNAME
    char hostname[MAXHOSTNAMELEN];
    assert(gethostname(hostname, MAXHOSTNAMELEN)==0);
    QN_OUTPUT("Host: %s.", hostname);
#endif
    QN_OUTPUT("Wordsize: %i bits.", sizeof(size_t)*8);
    QN_OUTPUT("CPU: %s.", info.cputype());
    QN_OUTPUT("CPU speed: %s.", info.cpumhz());
    if (info.numcores()>0)
	QN_OUTPUT("CPU cores: %i.", info.numcores());
    else
	QN_OUTPUT("CPU cores: unknown.");
    QN_OUTPUT("CPU hyperthreading: %s", info.hyperstat());
#ifdef QN_CUDA
    QN_OUTPUT(QN_cuda_version());
#endif
#ifdef QN_HAVE_LIBPTHREAD
    QN_OUTPUT("Posix threads: enabled.");
#endif
#ifdef QN_HAVE_LIBBLAS
    QN_OUTPUT("BLAS: enabled.");
#endif
#ifdef QN_HAVE_ATLAS_BUILDINFO
    QN_OUTPUT("Atlas version: %s %s.", ATL_VERS, ATL_ARCH);
#ifdef ATL_CCVERS
    QN_OUTPUT("Atlas CC: %s.", ATL_CCVERS);
#endif
#ifdef ATL_ICCVERS
    QN_OUTPUT("Atlas ICCVERS: %s.", ATL_ICCVERS);
#endif
#ifdef ATL_CCFLAGS
    QN_OUTPUT("Atlas CCFLAGS: %s", ATL_CCFLAGS);
#endif
#ifdef ATL_ICCFLAGS
    QN_OUTPUT("Atlas ICCFLAGS: %s", ATL_ICCFLAGS);
#endif
#endif
#ifdef QN_HAVE_LIBACML
    int acml_major, acml_minor, acml_patch;
    acmlversion(&acml_major, &acml_minor, &acml_patch);
    QN_OUTPUT("ACML version: %d.%d.%d.", acml_major, acml_minor, acml_patch);
#endif
#ifdef QN_HAVE_LIBMKL
    const int MKL_VER_LEN = 128;
    char qn_mkl_ver[MKL_VER_LEN];
    mkl_get_version_string(qn_mkl_ver, MKL_VER_LEN);
    QN_OUTPUT("MKL version: %s.", qn_mkl_ver);
    QN_OUTPUT("MKL max threads: %d.", mkl_get_max_threads());
#endif
#ifdef QN_HAVE_LIBESSL
    QN_OUTPUT("ESSL version: unknown.");
#ifdef QN_HAVE_LIBESSLSMP
    QN_OUTPUT("ESSL type: SMP.");
#ifdef QN_HAVE_OMP_GET_MAX_THREADS
    QN_OUTPUT("ESSL OMP max threads: %d.", omp_get_max_threads());
#endif
#else // ! QN_HAVE_LIBESSLSMP
     QN_OUTPUT("ESSL type: serial.");
#endif
#endif // QN_HAVE_LIBESSL
    QN_OUTPUT("Large PFiles: %s.", (QN_PFILE_LARGEFILES|| (sizeof(long)>4)) ? "enabled" : "disabled");
}

void
QN_output_mlp_size(const char* name, size_t layers, const size_t* layer_size)
{
    char buf[80];
    char* ptr = buf;
    size_t i;
    assert(name);

    QN_OUTPUT("MLP layers: %lu.", (unsigned long) layers);
        ptr += sprintf(ptr, "%lu", (unsigned long) layer_size[0]);
    for (i = 1; i<layers; i++)
	ptr += sprintf(ptr, " x %lu", (unsigned long) layer_size[i]);
    QN_OUTPUT("MLP size: %s.", buf);
}

// A function that is called when "new" runs out of memory.
void
QN_new_handler()
{
    QN_ERROR(NULL, "memory exhausted in 'new'");
}

int qn_io_debug = 0;

// Open a file.
// This produces an appropriate error message if there is a problem,
// handles "-" as a filename, and registers the stream name for 
// future error message use.  It also can create a custom-size buffer
// And associate a tag string with the file.

static const char* QN_GZIP = "gzip";

FILE*
QN_open(const char* filename, const char* mode, size_t bufsize,
	const char* tag)
{
    FILE* f;
    int ec;
    QN_ClassLogger clog(qn_io_debug, "QN_open", tag);

    size_t len = strlen(filename);
    if ((len > 3) && strcmp(filename+len-3, ".gz") == 0) {
#ifdef QN_HAVE_POPEN
	/* It's a gzipped file, so read/write through a popen to gzip */
	char buf[BUFSIZ];

	if (mode[0] == 'r') {
	    sprintf(buf, "%s --stdout --decompress --force %s", QN_GZIP, filename);
	} else if (mode[0] == 'w') {
	    sprintf(buf, "%s --stdout --force  > %s", QN_GZIP, filename);
	} else {
	    QN_ERROR("QN_open", "mode of '%s' is not 'r' or 'w'\n", 
		    filename, mode);
	}

	f = popen(buf, mode);

	if(f == NULL) {
	    QN_ERROR("QN_open", "popen failed for '%s', mode '%s', "
		     "command '%s' - %s.",
		     filename, mode, buf, strerror(errno));
	}
	clog.log(QN_LOG_PER_EPOCH,
		 "Opened pipe '%s' command '%s'.", filename,
		 buf);
	// Remember this filename for later
	QN_filetoname.create_entry(f, filename, QN_FILE_PIPE, NULL, tag);
#else
	QN_ERROR("QN_open", "Can't auto-create .gz files without popen().");
#endif /* QN_HAVE_POPEN */

    }
    else if (strcmp(filename, "-")==0)
    {
	if (strcmp(mode, "r")==0)
	    f = stdin;
	else if (strcmp(mode, "w")==0)
	    f = stdout;
	else
	{
	    QN_ERROR("QN_open", "Cannot use filename '-' with mode '%s'.",
		     mode);
	    f = NULL;		// To suppress warnings
	}
    }
    else
    {
	char* buf = NULL;
	f = fopen(filename, mode);
	if (f==NULL)
	{
	    QN_ERROR("QN_open", "Open failed for '%s', mode '%s' - %s.",
		     filename, mode, strerror(errno));
	}
#ifdef QN_HAVE_SETVBUF
	if (bufsize!=0)
	{
	    buf = new char[bufsize];
	    ec = setvbuf(f, buf, _IOFBF, bufsize);
	    if (ec!=0)
		QN_ERROR(NULL, "Failed to setvbuf().");
	}
#else
	bufsize = 0;
#endif
	clog.log(QN_LOG_PER_EPOCH,
		 "Opened file '%s' bufsize %lu.", filename,
		 (unsigned long) bufsize);
	// Remember this for later error messages.
	QN_filetoname.create_entry(f, filename, QN_FILE_FILE, buf, tag);
    }
    return f;
}

// Close a file

void
QN_close(FILE* f)
{
    int ec = 0;
    char* buf;
    QN_ClassLogger clog(qn_io_debug, "QN_close");

    if (f!=stdin && f!=stdout && f!=stderr)
    {
	switch(QN_filetoname.filetype(f))
	{
	case QN_FILE_PIPE:
#ifdef QN_HAVE_POPEN
	    ec = pclose(f);
	    clog.log(QN_LOG_PER_EPOCH, "Closing pipe %s.", QN_FILE2NAME(f));
#else
	    assert(0);
#endif
	    break;
	case QN_FILE_FILE:
	    ec = fclose(f);
	    clog.log(QN_LOG_PER_EPOCH, "Closing file %s.", QN_FILE2NAME(f));
	    break;
	case QN_FILE_STDINOUT:
	    break;
	default:
	    QN_ERROR("QN_close", "Unknown stream type in QN_close.");
	}
	if (ec!=0)
	{
	    QN_ERROR("QN_close", "Failed to close '%s' - %s.",
		     QN_FILE2NAME(f), strerror(errno));
	}

	// Delete memory of file name and any buffers and tags.
	buf = QN_filetoname.delete_entry(f);
	if (buf!=NULL)
	    clog.log(QN_LOG_PER_EPOCH, "Freeing buffer.");
    }
}

// Check to see if two paths refer to the same file.

int
QN_pathcmp(const char* f1, const char* f2)
{
    struct stat s1;
    struct stat s2;
    int ec;			// Error return.

    ec = stat(f1, &s1);
    if (ec!=0)
    {
	    QN_ERROR("QN_pathcmp", "Failed to stat '%s' - %s.",
		     f1, strerror(errno));
    }
    ec = stat(f2, &s2);
    if (ec!=0)
    {
	    QN_ERROR("QN_pathcmp", "Failed to stat '%s' - %s.",
		     f2, strerror(errno));
    }
    return (!((s1.st_ino==s2.st_ino) && (s1.st_dev==s2.st_dev)));
}

// Return the current time (seconds from the epoch) as a double with the
// highest precision available.
// Note that this is quite computationally expensive on SPERT.

double
QN_time()
{
    double now;			// Value for returning the time.
// Use gettimeofday() if available.
#if defined(QN_HAVE_GETTIMEOFDAY)
    timeval tv;

    gettimeofday(&tv, NULL);
    now = (double) tv.tv_sec + ((double) tv.tv_usec) / 1e6;
#else
    now = (double) time(NULL);
#endif
    return now;
}

// Return the time now as a string suitable for output.

void
QN_timestr(char* buf, size_t len)
{
    if (len<QN_TIMESTR_BUFLEN)
	return;
    else
    {
	time_t t;

	t = time(NULL);
#ifdef QN_HAVE_CTIME_R
	ctime_r(t, buf, len);
#else
	strncpy(buf, ctime(&t), len);
#endif	
	// Remove the newline from the buffer.
	buf[24] = '\0';
    }
}

// A routine to find a good size for a buffer for accessing the
// weights in an MLP.  This is either a constant or the size of the largest
// layer, whichever is bigger.  Whilst doing this, it checks that the sizes of
// the MLP and a user-provided weights file are the same.

// The default buffer size.
#define QN_LAYER_BUF_DFLT	1000

static size_t
good_weight_buf_size(int debug, const char* dbgname,
		     QN_MLP& mlp, QN_MLPWeightFile& weights)
{
    enum
    {
	GOODWEIGHT_MAX_BUFSIZE = QN_LAYER_BUF_DFLT
    };
    size_t max_layer_size = 0; // Maximum size of a layer
    size_t layer;		// Current layer

    const size_t mlp_num_layers = mlp.num_layers();
    const size_t file_num_layers = weights.num_layers();

    if (mlp_num_layers != file_num_layers)
    {
	QN_ERROR("good_weight_buf_size",
		 "For %s, number of layers in weight file is %lu "
		 "and number in mlp is %lu.", dbgname,
		 (unsigned long) file_num_layers,
		 (unsigned long) mlp_num_layers);
    }

    const size_t mlp_num_sections = mlp.num_sections();
    const size_t file_num_sections = weights.num_sections();

    if (mlp_num_sections != file_num_sections)
    {
	QN_ERROR("good_weight_buf_sizes",
		 "For %s, number of sections in weight file is %lu "
		 "and number in mlp is %lu.", dbgname,
		 (unsigned long) file_num_sections,
		 (unsigned long) mlp_num_sections);
    }

    // Find the size of the biggest layer in the net, and allocate a
    // buffer bigger than this
    for (layer=0; layer<mlp_num_layers; layer++)
    {
	size_t mlp_size_layer;
	size_t file_size_layer;
	
	mlp_size_layer = mlp.size_layer((QN_LayerSelector) layer);
	file_size_layer = weights.size_layer((QN_LayerSelector) layer);
	if (mlp_size_layer!=file_size_layer)
	{
	    QN_ERROR("good_weight_buf_sizes"
		     ,"For %s, size of layer %lu in weight file is %lu "
		     "and size in mlp is %lu.", dbgname,
		     (unsigned long) layer+1,
		     (unsigned long) file_size_layer,
		     (unsigned long) mlp_size_layer);
	}
	if (mlp_size_layer > max_layer_size)
	    max_layer_size = mlp_size_layer;
    }

    return qn_max_zz_z(max_layer_size, QN_LAYER_BUF_DFLT);
}


void
QN_readwrite_weights(int debug, const char* dbgname,
		     QN_MLP& mlp,
		     const char* wfile_name, QN_WeightFileType wfile_format,
		     QN_FileMode mode,
		     float* minp, float* maxp)
{
    size_t i;
    FILE* wfile;	// Input/output weight file.
    const char* modestr = (mode==QN_READ) ? "r" : "w";
    QN_MLPWeightFile* wf = NULL;
    size_t n_input;
    size_t n_hidden;
    size_t n_output;
    size_t num_layers;
    size_t size_layers[QN_MLP_MAX_LAYERS];

    wfile = QN_open(wfile_name, modestr);
    switch(wfile_format)
    {
    case QN_WEIGHTFILE_RAP3:
	assert(mlp.num_layers()==3);
	n_input = mlp.size_layer(QN_MLP3_INPUT);
	n_hidden = mlp.size_layer(QN_MLP3_HIDDEN);
	n_output = mlp.size_layer(QN_MLP3_OUTPUT);
	wf = new QN_MLPWeightFile_RAP3(debug, wfile, mode,
				       dbgname,
				       n_input, n_hidden, n_output);
	break;
    case QN_WEIGHTFILE_MATLAB:
	num_layers = mlp.num_layers();
	for (i=0; i<num_layers; i++)
	    size_layers[i] = mlp.size_layer((QN_LayerSelector) i);
	wf = new QN_MLPWeightFile_Matlab(debug, dbgname, wfile, mode,
					 num_layers, size_layers);
	break;
    default:
	assert(0);
    }
    QN_readwrite_weights(debug, dbgname, mode, *wf, mlp, NULL, NULL);
    delete wf;
    QN_close(wfile);
}

void
QN_readwrite_weights(int debug, const char* dbgname, QN_FileMode mode,
		     QN_MLPWeightFile& weights, QN_MLP& mlp,
		     float* minp, float* maxp)
{
    float min_weight = FLT_MAX;	// Minimum weight loaded
    float max_weight = -FLT_MAX; // Maximum weight loaded
    float min_tmp, max_tmp;	 // Temporary mins and maxs
    float* buf = NULL;		// A pointer to a buffer full of weights
    float* buf2 = NULL;		// A pointer to a buffer full of transposed
    size_t buf_size;		// The size of the buffer
    size_t num_layers;		// Number of layers in net.
    size_t num_sections;	// Number of sections (ie weight blocks) in net
    int transpose;		// Are weights OUTPUTMINOR in file?
    const char* modestr;

    if (dbgname==NULL)
	dbgname = "unknown";
    if (mode==QN_READ)
	modestr = "read";
    else
	modestr = "write";
    if (mode != weights.get_filemode())
    {
	QN_ERROR("QN_readwrite_weights", "whilst handling %s, mode of "
		 " operation (%s) and mode of weight file do not match.",
		 dbgname, modestr);
    }
    // Check to see if we have to transpose whilst reading.
    transpose = (weights.get_weightmaj()==QN_INPUTMAJOR);

    num_layers = weights.num_layers();
    num_sections = weights.num_sections();

    buf_size = good_weight_buf_size(debug, dbgname, mlp, weights);
    buf = new float [buf_size];
    if (transpose)
	buf2 = new float [buf_size];
    
    size_t sectno;		// Index of current weight section
    for (sectno=0; sectno<num_sections; sectno++)
    {
	enum QN_SectionSelector section;	// Which section in the MLP?
	size_t num_inputs;	// Number of inputs in this section
	size_t num_outputs;	// Number of outputs in this section
	size_t total_rows;	// Number of rows in weight file matrix
	size_t total_cols;	// Number of cols in weight file matrix
	size_t rows_in_chunk;	// Number of rows read at one time
	size_t current_row;	// The current row in the weight file
				// that we are reading.
	
	// The weight file determines the order of the sections
	section = weights.get_weighttype(sectno);
	// Find the size of the weight section we are dealing with
	mlp.size_section(section, &num_outputs, &num_inputs);
	if (transpose)
	{
	    total_rows = num_inputs;
	    total_cols = num_outputs;
	}
	else
	{
	    total_rows = num_outputs;
	    total_cols = num_inputs;
	}
	
	rows_in_chunk = buf_size/total_cols;
	for (current_row=0; current_row<total_rows;
	     current_row += rows_in_chunk)
	{
	    size_t rows_in_current_chunk; // Rows in the current chunk.
	    size_t els_in_current_chunk;  // Elements in the current chunk.
	    size_t count;		  // Count of elements read.

	    rows_in_current_chunk = qn_min_zz_z(rows_in_chunk,
						total_rows-current_row);
	    els_in_current_chunk = rows_in_current_chunk * total_cols;
	    if (mode==QN_READ)
	    {
		count = weights.read(buf, els_in_current_chunk);
		if (count!=els_in_current_chunk)
		{
		    QN_ERROR("QN_readwrite_weights",
			     "Failed to read a buffer full of weights.");
		}
		qn_maxmin_vf_ff(els_in_current_chunk, buf, &max_tmp, &min_tmp);
		max_weight = qn_max_ff_f(max_weight, max_tmp);
		min_weight = qn_min_ff_f(min_weight, min_tmp);
		if (transpose)
		{
		    qn_trans_mf_mf(rows_in_current_chunk, total_cols, buf, buf2);
		    mlp.set_weights(section, 0, current_row, total_cols,
				    rows_in_current_chunk, buf2);
		}
		else
		{
		    mlp.set_weights(section, current_row, 0, rows_in_current_chunk,
				    total_cols, buf);
		}
	    }
	    else
	    {
		assert(mode==QN_WRITE);
		if (transpose)
		{
		    mlp.get_weights(section, 0, current_row, total_cols,
				    rows_in_current_chunk, buf2);
		    qn_trans_mf_mf(total_cols, rows_in_current_chunk,
				   buf2, buf);
		}
		else
		{
		    mlp.get_weights(section, current_row, 0,
				    rows_in_current_chunk, total_cols, buf);
		}
		qn_maxmin_vf_ff(els_in_current_chunk, buf, &max_tmp, &min_tmp);
		max_weight = qn_max_ff_f(max_weight, max_tmp);
		min_weight = qn_min_ff_f(min_weight, min_tmp);
		count = weights.write(buf, els_in_current_chunk);
		if (count!=els_in_current_chunk)
		{
		    QN_ERROR("QN_readwrite_weights",
			     "Failed to write a buffer full of weights.");
		}
	    }
	}
    }

    if (minp!=NULL)
	*minp = min_weight;
    if (maxp!=NULL)
	*maxp = max_weight;
    if (debug>=2)
    {
	QN_LOG("QN_readwrite_weights", "Weights %s, min=%f, max=%f.",
	       modestr, min_weight, max_weight);
    }
    if (buf2!=NULL)
	delete [] buf2;
    delete [] buf;
}

// Read a weight file into an MLP.
void
QN_read_weights(QN_MLPWeightFile& weights, QN_MLP& mlp,
		float* minp, float* maxp, int debug,
		const char* dbgname)
{
    QN_readwrite_weights(debug, dbgname, QN_READ,
			 weights, mlp, minp, maxp);
}

// Write a weight file out from an MLP.
void
QN_write_weights(QN_MLPWeightFile& weights, QN_MLP& mlp,
		float* minp, float* maxp, int debug,
		const char* dbgname)
{
    QN_readwrite_weights(debug, dbgname, QN_WRITE,
			 weights, mlp, minp, maxp);
}


// Read or write a weight file to/from an MLP.
////////////////////////////////////////////////////////////////

// A "helper" routine for randomizing weights - not usable by the
// rest of the world (mainly because it uses rand48() and assumes
// the seed has already been set).

static void
randomize_section_weights(QN_MLP& mlp, enum QN_SectionSelector which,
			  size_t n_rows, size_t n_cols,
			  float minval, float maxval)
{
    enum
    {
	RANDSECT_MAX_BUFSIZE = QN_LAYER_BUF_DFLT
    };
    size_t chunk_size;		// The size of data we randomize at once
    size_t rows_in_chunk;	// The number of rows in one buffer
    size_t current_row;		// The current first row we are intializing
    float* buf;			// A pointer to a buffer full of random weights

    if (n_cols > RANDSECT_MAX_BUFSIZE)
    {
	rows_in_chunk = 1;
	chunk_size = n_cols;
    }
    else
    {
	rows_in_chunk = RANDSECT_MAX_BUFSIZE / n_cols;
	chunk_size = rows_in_chunk * n_cols;
    }
    buf = new float [chunk_size];
    for (current_row = 0; current_row<n_rows;
	 current_row += rows_in_chunk)
    {
	size_t rows_in_current_chunk; // Rows in the current chunk
	size_t els_in_current_chunk;  // Elements in the current chunk

	rows_in_current_chunk = qn_min_zz_z(rows_in_chunk, n_rows-current_row);
	els_in_current_chunk = rows_in_current_chunk * n_cols;

	qn_urand48_ff_vf(els_in_current_chunk, minval, maxval, buf);
	mlp.set_weights(which, current_row, 0,
			rows_in_current_chunk, n_cols, buf);
    }
    delete [] buf;
}


// Randomize the weights and biases in a whole MLP

void
QN_randomize_weights(int /* debug */, int seed, QN_MLP& mlp,
		     float weight_min, float weight_max,
		     float bias_min, float bias_max)
{
    size_t section;

    srand48(seed);
    for (section=0; section<mlp.num_sections(); section++)
    {
	float min, max;		// Minimum and maximum values for random weight
	size_t n_output, n_input; // Size of weight matrix
	assert(QN_MLP_MAX_LAYERS<=5 && mlp.num_layers()<=QN_MLP_MAX_LAYERS);
	if (section==QN_LAYER2_BIAS
	    || section==QN_LAYER3_BIAS
	    || section==QN_LAYER4_BIAS
	    || section==QN_LAYER5_BIAS )
	{
	    min = bias_min;
	    max = bias_max;
	}
	else
	{
	    min = weight_min;
	    max = weight_max;
	}
	mlp.size_section((QN_SectionSelector) section, &n_output, &n_input);
	randomize_section_weights(mlp, (QN_SectionSelector) section,
				  n_output, n_input, min, max);
    }
}

// Another routine for randomizing the weights and biases in a whole 
// 3-layer MLP, with more control.

void
QN_randomize_weights(int /* debug */, int seed, QN_MLP& mlp,
		     float in2hid_min, float in2hid_max,
		     float hidbias_min, float hidbias_max,
		     float hid2out_min, float hid2out_max,
		     float outbias_min, float outbias_max)
{
    size_t section;

    srand48(seed);
    assert(mlp.num_layers()==3);
    for (section=0; section<mlp.num_sections(); section++)
    {
	float min, max;		// Minimum and max values for random weights.
	size_t n_output, n_input; // Size of weight matrix.
	if (section==QN_MLP3_INPUT2HIDDEN)
	{
	    min = in2hid_min; max = in2hid_max;
	}
	else if (section==QN_MLP3_HIDDENBIAS)
	{
	    min = hidbias_min; max = hidbias_max;
	}
	else if (section==QN_MLP3_HIDDEN2OUTPUT)
	{
	    min = hid2out_min; max = hid2out_max;
	}
	else
	{
	    min = outbias_min; max = outbias_max;
	}
	mlp.size_section((QN_SectionSelector) section, &n_output, &n_input);
	randomize_section_weights(mlp, (QN_SectionSelector) section,
				  n_output, n_input, min, max);
    }
}
// Another routine for randomizing the weights and biases of a while
// MLP with aribrary number of layers.  
void
QN_randomize_weights(int /* debug */, int seed, QN_MLP& mlp,
		     float* weight_min, float* weight_max,
		     float* bias_min, float* bias_max)
{
    size_t section;

    srand48(seed);
    for (section=0; section<mlp.num_sections(); section++)
    {
	float min, max;		// Minimum and max values for random weights.
	size_t index = section/2; // Index into the mins/maxes.
	size_t n_input, n_output; // Section sizes.
	if (section==index*2)
	{
	    // Even sections are weights
	    min = weight_min[index];
	    max = weight_max[index];
	}
	else
	{
	    min = bias_min[index];
	    max = bias_max[index];
	}
	mlp.size_section((QN_SectionSelector) section, &n_output, &n_input);
	randomize_section_weights(mlp, (QN_SectionSelector) section,
				  n_output, n_input, min, max);
    }
}


void
QN_set_learnrate(QN_MLP& mlp, float rate)
{
    unsigned section;
    for (section=0; section<mlp.num_sections(); section++)
    {
	mlp.set_learnrate((QN_SectionSelector) section, rate);
    }
}

double
QN_secs_to_MCPS(double time, size_t n_pres, QN_MLP& mlp)
{
    size_t n_conns = mlp.num_connections();
    double mcps = (double) n_pres * (double) n_conns / time * .000001;

    return mcps;
}


// Calculate statistics on a feature stream

void
QN_ftrstats(int debug, QN_InFtrStream& ftr_stream,
	    float* ftr_mean, float* ftr_sdev)
{
    static const char* FUNCNAME = "QN_ftrstats";
    size_t num_ftrs;		// The number of features in one frame
    size_t total_frames = 0;	// The total number of frames we see

    num_ftrs = ftr_stream.num_ftrs();
    assert(num_ftrs>0);

    if (debug>=1)
    {
	QN_LOG(FUNCNAME, "Analyzing stream with %lu features.", num_ftrs);
    }

    // Allocate some workspace 
    float*const ftr_buf = new float [num_ftrs];   // Read features into here
    double*const ftr_sum = new double [num_ftrs];  // Sum features here
    double*const ftr_sum2 = new double [num_ftrs]; // Sum square of features here
    double*const ftr_tmp = new double [num_ftrs];  // Temporary holder

    // Initialize accumulators
    qn_copy_d_vd(num_ftrs, 0.0, ftr_sum);
    qn_copy_d_vd(num_ftrs, 0.0, ftr_sum2);

    while (ftr_stream.nextseg()!=QN_SEGID_BAD) {
        // Iterate over all sentences.  Remember, nextseg BEFORE first read
	size_t frames;

	// Read in one feature vector
	frames = ftr_stream.read_ftrs(1, ftr_buf);
	while (frames!=0)
	{
	    total_frames ++;

	    // Calculate sum of features.
	    qn_conv_vf_vd(num_ftrs, ftr_buf, ftr_tmp);
	    qn_acc_vd_vd(num_ftrs, ftr_tmp, ftr_sum);
	    
	    // Calculate sum of squares of features.
	    qn_sqr_vd_vd(num_ftrs, ftr_tmp, ftr_tmp);
	    qn_acc_vd_vd(num_ftrs, ftr_tmp, ftr_sum2);
	    
	    // Read in one feature vector
	    frames = ftr_stream.read_ftrs(1, ftr_buf);
	}
    }

    const double recip_frames = 1.0/(double) total_frames;
    // Calculate means
    if (ftr_mean!=NULL)
    {
            // Features mean = sum / N
            qn_mul_vdd_vd(num_ftrs, ftr_sum, recip_frames, ftr_tmp);
            qn_conv_vd_vf(num_ftrs, ftr_tmp, ftr_mean);
    }

    // Calculate standard deviations
    if (ftr_sdev!=NULL)
    {
	// Features standard deviation
	// = sqrt( (sum_of_squares - (sum*sum / N)) / N)
	qn_sqr_vd_vd(num_ftrs, ftr_sum, ftr_tmp);
	qn_mul_vdd_vd(num_ftrs, ftr_tmp, recip_frames, ftr_tmp);
	qn_sub_vdvd_vd(num_ftrs, ftr_sum2, ftr_tmp, ftr_tmp);
	qn_mul_vdd_vd(num_ftrs, ftr_tmp, recip_frames, ftr_tmp);
	qn_conv_vd_vf(num_ftrs, ftr_tmp, ftr_sdev);
	qn_sqrt_vf_vf(num_ftrs, ftr_sdev, ftr_sdev);
    }
    if (debug>=3)
    {
	size_t i;

	if (ftr_mean!=NULL)
	{
	    QN_LOG(FUNCNAME, "Feature means:");
	    for (i=0; i<num_ftrs; i++)
		QN_LOG(FUNCNAME, "%e", (double) ftr_mean[i]);
	}

	if (ftr_sdev!=NULL)
	{
	    QN_LOG(FUNCNAME, "Feature SDs:");
	    for (i=0; i<num_ftrs; i++)
		QN_LOG(FUNCNAME, "%e", (double) ftr_sdev[i]);
	}
    }
    if (debug>=1)
    {
	QN_LOG(FUNCNAME, "Feature stream analysis finished.");
    }
    // Free used workspace
    delete [] ftr_tmp;
    delete [] ftr_sum2;
    delete [] ftr_sum;
    delete [] ftr_buf;
}



// Check that a weight log file template is okay
// If it is, return QN_OK, else QN_BAD
// Weight file templates use % as an escape character
//  %% -> %
//  %p -> process number
//  %e -> epoch number

int
QN_logfile_template_check(const char* templ)
{
    size_t len;
    size_t i;
    char next;

    len = strlen(templ);
    if (len==0)
	return QN_BAD;
    for (i=0; i<len; i++)
    {
	if (templ[i]=='%')
	{
	    next = templ[i+1];
	    if (next!='%' && next!='p' && next!='e' && next!='t'
		          && next!='h')
		return QN_BAD;
	    i++;
	}
    }
    return QN_OK;
}

// Map a weight log file template to a file name

int
QN_logfile_template_map(const char* from, char* to, size_t maxlen,
			QNUInt32 e_arg, QNUInt32 p_arg)
{
    enum { MAXSTR32 = 11 };	// The maximum string len for a 32 bit integer
    char c;			// Character being copied
    size_t len = 0;		// Number of characters copied
    int count;			// Number of character sprintfd

    do 
    {
	c = *from++;
	if (c=='%')
	{
	    c = *from++;
	    switch(c)
	    {
	    case '%':
		*to++ = '%';
		break;
	    case 'e':
		if (len + MAXSTR32 < maxlen)
		{
		    sprintf(to, "%u", e_arg);
		    count = strlen(to);
		    len += count;
		    to += count;
		}
		else
		{
		    return QN_BAD;
		}
		break;
	    case 'p':
		if (len + MAXSTR32 < maxlen)
		{
		    sprintf(to, "%u", p_arg);
		    count = strlen(to);
		    len += count;
		    to += count;
		}
		else
		{
		    return QN_BAD;
		}
		break;
	    case 't':
		// Replace %t with the date and time.
		if (len + (8+6+1) < maxlen)
		{
		    time_t t;
		    struct tm *lt;

		    t = time(NULL);
		    lt = localtime(&t);
		    sprintf(to, "%4.4u%2.2u%2.2u-%2.2u:%2.2u", lt->tm_year+1900,
			    lt->tm_mon+1, lt->tm_mday, lt->tm_hour,
			    lt->tm_min);
		    count = strlen(to);
		    len += count;
		    to += count;
		}
		else
		    return QN_BAD;
		break;
	    case 'h':
		// Replace %h with the first bit of the hosts name.
	    {
		const char* hostnamechar =
		    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
		char hostname[MAXHOSTNAMELEN+1];
		size_t hlen;

		assert(gethostname(hostname, MAXHOSTNAMELEN+1)==0);
		hlen = strspn(hostname, hostnamechar);
		if (len+hlen+1 < maxlen)
		{
		    strncpy(to, hostname, hlen);
		    len += hlen;
		    to += hlen;
		}
		else
		    return QN_BAD;
		break;
	    }
	    case '\0':
		// Should never have single % at end of name
		return QN_BAD;
		break;
	    default:
		// Any other character just gets copied
		*to++ = c;
		break;
	    }
	}
	else
	{
	    *to++ = c;
	    len++;
	}
    } while (c!='\0');
    return QN_OK;
}

////////////////////////////////////////////////////////////////
// Search for an executable in a list of paths 
// - from feacalc, now used to find spertrun when vectoring 
//   qnstrn cpu=spert slaves=0 etc.
// Only available for non-spert systems... (too hard to port)
////////////////////////////////////////////////////////////////

int QN_SearchExecPaths(char *cmd, char *paths, char *rslt, int rsltlen)
{   /* Search through the WS or colon-separated list of paths
       for an executable file whose tail is cmd.  Return absolute 
       path in rslt, which is allocated at rsltlen.  Return 1 
       if executable found & path successfully returned, else 0. */
#ifdef QN_HAVE_SYSSPERT
    return 0;
#else /* !SPERT */
    char testpath[QN_MAXPATHLEN];
    int cmdlen = strlen(cmd);

    /* zero-len testpath means not found, so preset it that way */
    testpath[0] = '\0';
    /* If the cmd contains a slash, don't search paths */
    if (strchr(cmd, '/') != NULL) {
	/* if it *starts* with a "/", treat it as absolute, else prepend cwd */
	if(cmd[0] == '/') {
	    assert(cmdlen+1 < QN_MAXPATHLEN);
	    strcpy(testpath, cmd);
	} else {
	    testpath[0] = '\0';
	    getcwd(testpath, QN_MAXPATHLEN);
	    assert(strlen(testpath) > 0 \
		   && strlen(testpath)+cmdlen+2 < QN_MAXPATHLEN);
	    strcat(testpath, "/");
	    strcat(testpath, cmd);
	}    
	/* See if this path is executable */
	//fprintf(stderr, "testing '%s'...\n", testpath);
	if(access(testpath, X_OK) != 0) {
	    /* exec access failed, fall through with zero-len */
	    testpath[0] = '\0';
	}
    } else {  /* no slash in cmd, search the path components */
	/* Loop through the pieces of <paths> trying each one */
	int plen;
	while(strlen(paths)) {
	    /* Skip over current separator (or excess WS) */
	    paths += strspn(paths, ": \t\r\n");
	    /* extract next path */
	    plen = strcspn(paths, ": \t\r\n");
	    if(plen) {
		assert(plen+cmdlen+2 < QN_MAXPATHLEN);
		/* copy the path part */
		strncpy(testpath, paths, plen);
		/* shift on so next path is grabbed next time */
		paths += plen;
		/* build the rest of the path */
		testpath[plen] = '/';
		strcpy(testpath+plen+1, cmd);
		//fprintf(stderr, "testing '%s'...\n", testpath);
		/* see if it's there */
                if(access(testpath, X_OK) != 0) {
	            /* exec access failed, fall through with zero-len */
		    testpath[0] = '\0';
		} else {
		    /* found the ex, drop out' the loop */
		    break;
		}
	    }
	}
    }
    /* Now we either have a working path in testpath, or we found nothing */
    if(strlen(testpath) == 0) {
	return 0;
    } else {
	if(rslt) {	/* we have a string to copy to */
	    if(strlen(testpath)+1 < (size_t)rsltlen) {
		strcpy(rslt, testpath);
	    } else {
		fprintf(stderr, "SearchExec: rsltlen (%d) too small for %ld\n", rsltlen, strlen(testpath));
		return 0;
	    }
	}
	return 1;
    }
#endif /* !SPERT */
}

//
// Little function to map from ascii string to QN_NORM_* code 
// for arg handling
//
int QN_string_to_norm_const(const char *str) {
    // default, "0", "[nN].*", "[fF].*" all give norm from file
    // "1", "[yY].*", "[tT].*", "p*erutt", "u*tt" give per-utt norm
    // "o*nline" give online norm
    char c = '0';
    int n = 0;
    int rc = QN_NORM_FILE;

    if (str) n = strlen(str);
    if (n) c = *str;

    if (n < 1 || c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F') {
	rc = QN_NORM_FILE;
    } else if (c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T' \
	       || strncmp(str, "perutt", n)==0 || strncmp(str, "utts", n)==0) {
	rc = QN_NORM_UTTS;
    } else if (strncmp(str, "online", n) == 0) {
	rc = QN_NORM_ONL;
    } else {
	QN_ERROR("QN_string_to_norm_const",
		 "cannot interpret '%s' as a norm type (file/utt/onl)", str);
    }
    return rc;
}

//
// Functions for opening then cleaning up a series of feature files 
// of various formats.  Originally from qnstrn; moved here so they 
// could be used by qnsfwd too.
//

// Static data (ugh)
#define MAX_FTR_STREAMS 1024
static int n_ftr_strs = 0;
static FILE *ftr_str_fps[MAX_FTR_STREAMS];

// Close any streams we opened here & free buffers
void QN_close_ftrfiles(void) {
    int i;
    for (i=0; i < n_ftr_strs; ++i) {
	QN_close(ftr_str_fps[i]);
    }
    n_ftr_strs = 0;
}

FILE *QN_open_ftrfile(const char *name, const char* tag) {
    assert(n_ftr_strs < MAX_FTR_STREAMS);

    // Use a bigger buffer for the feature files.
    FILE* ftr_fp = QN_open(name, "r", QN_FTRFILE_BUF_SIZE, tag);
    // save in globals for closing
    ftr_str_fps[n_ftr_strs] = ftr_fp;
    ++n_ftr_strs;
    return ftr_fp;
}

// Basic function to open a feature file of a specific type
QN_InFtrStream* QN_open_ftrstream(int debug, const char *dbgname, 
			       FILE *ftrfile, 
			       const char* format, size_t width, 
			       size_t min_width, int indexed) {
    // Convert the file descriptor into a stream.
    QN_InFtrStream* ftr_str = NULL;

    if (strcmp(format, "pfile")==0) {
	QN_InFtrLabStream_PFile* pfile_str =
	    new QN_InFtrLabStream_PFile(debug, // Select debugging.
					dbgname, // Debugging tag.
					ftrfile, // Input file.
					indexed // Indexed flag.
		);
	size_t pfile_num_ftrs = pfile_str->num_ftrs();
	if (width!=0 && (pfile_num_ftrs!=width) ) {
	    QN_ERROR(dbgname, "PFile '%s' has %lu features, command line "
		     "specifies %lu.", QN_FILE2NAME(ftrfile),
		     (unsigned long) pfile_num_ftrs,
		     (unsigned long) width);
	}
	if (pfile_num_ftrs < min_width)	{
	    QN_ERROR(dbgname, "Minimum file width of %lu features required, "
		     "PFile '%s' only has %lu.",
		     (unsigned long) min_width, QN_FILE2NAME(ftrfile),
		     (unsigned long)pfile_num_ftrs);
	}
	ftr_str = pfile_str;
    } else if (strcmp(format, "srifile")==0) {
	QN_InFtrStream_SRI* sri_str =
	    new QN_InFtrStream_SRI(debug, // Select debugging.
				   dbgname, // Debugging tag.
				   ftrfile  // Input file.
				   // the 'indexed' parameter is ignored
		);

	size_t sri_num_ftrs = sri_str->num_ftrs();
	if (width!=0 && (sri_num_ftrs!=width) ) {
	    QN_ERROR(dbgname, "SRI feat file '%s' has %lu features, command line "
		     "specifies %lu.", QN_FILE2NAME(ftrfile),
		     (unsigned long) sri_num_ftrs,
		     (unsigned long) width);
	}
	if (sri_num_ftrs < min_width)	{
	    QN_ERROR(dbgname, "Minimum file width of %lu features required, "
		     "SRI feat file '%s' only has %lu.",
		     (unsigned long) min_width, QN_FILE2NAME(ftrfile),
		     (unsigned long)sri_num_ftrs);
	}
	ftr_str = sri_str;
    }  else if (strcmp(format, "srilist")==0) {
	QN_InFtrStream_ListSRI* sri_list_str =
	    new QN_InFtrStream_ListSRI(debug, // Select debugging.
				       dbgname, // Debugging tag.
				       ftrfile,  // Input file.
				       indexed // whether indexed
		);

	size_t sri_num_ftrs = sri_list_str->num_ftrs();
	if (width!=0 && (sri_num_ftrs!=width) ) {
	    QN_ERROR(dbgname, "SRI List file '%s' has %lu features, "
		     "command line specifies %lu.", QN_FILE2NAME(ftrfile),
		     (unsigned long) sri_num_ftrs,
		     (unsigned long) width);
	}
	if (sri_num_ftrs < min_width)	{
	    QN_ERROR(dbgname, "Minimum file width of %lu features required, "
		     "SRI List file '%s' only has %lu.",
		     (unsigned long) min_width, QN_FILE2NAME(ftrfile),
		     (unsigned long)sri_num_ftrs);
	}
	ftr_str = sri_list_str;
    } else if (strcmp(format, "pre")==0) {
	if (width==0) {
	    QN_ERROR(dbgname, "Need to specify a non-zero width for 'pre' "
		     "format files.");
	}
	QN_InFtrLabStream_PreFile* prefile_str =
	    new QN_InFtrLabStream_PreFile(debug, // Select debugging.
					  dbgname, // Debugging tag.
					  ftrfile, // Feature file.
					  width, // No. of ftrs in file.
					  indexed // Whether indexed.
		);
	ftr_str = prefile_str;
    } else if (strcmp(format, "lna")==0 || strcmp(format, "lna8")==0) {
	if (width==0) {
	    QN_ERROR(dbgname, "Need to specify a non-zero width for 'lna' "
		     "format files.");
	}
	QN_InFtrLabStream_LNA8* lnafile_str =
	    new QN_InFtrLabStream_LNA8(debug, // Select debugging.
				       dbgname, // Debugging tag.
				       ftrfile, // Feature file.
				       width, // No. of ftrs in file.
				       indexed // Whether indexed.
		);
	ftr_str = lnafile_str;
    } else if (strcmp(format, "onlftr")==0) {
	if (width==0) {
	    QN_ERROR(dbgname, "Need to specify a non-zero width for 'onlftr' "
		     "format files.");
	}
	QN_InFtrLabStream_OnlFtr* onlftr_str =
	    new QN_InFtrLabStream_OnlFtr(debug, // Select debugging.
				       dbgname, // Debugging tag.
				       ftrfile, // Feature file.
				       width, // No. of ftrs in file.
				       indexed // Whether indexed.
		);
	ftr_str = onlftr_str;
    } else {
	QN_ERROR(dbgname, "unknown feature file format '%s'.", format);
	ftr_str = NULL;
    }
    return ftr_str;
}

// A function to create a feature stream for a given
// feature file.  Also handles opening multiple files if 
// stream comes from a sequence of files.
// If buffer_frames is 0, batch run.
QN_InFtrStream *QN_build_ftrstream(int debug, const char* dbgname, 
				const char *filename, const char* format, 
				size_t width, int index_req, 
				FILE* normfile, 
				size_t first_ftr, size_t num_ftrs,
				size_t sent_start, size_t sent_count, 
				size_t buffer_frames,
				int delta_order, int delta_win,
				int norm_mode, double norm_am, double norm_av)
{
    // norm_mode: 0-> just norm file 1-> per-utts 2-> online, init from file

    QN_InFtrStream* ftr_str = NULL;	// Temporary stream holder.
    size_t min_width;		// Minimum width of feature file.

    if (num_ftrs!=0)
	min_width = (first_ftr + num_ftrs + delta_order)/(delta_order+1);
    else
	min_width = (first_ftr + 1 + delta_order)/(delta_order+1);
    if (width!=0 && width<min_width) {
	QN_ERROR(dbgname, "Minimum required file width of %lu is larger than %d.",
		 (unsigned long) min_width, width);
    }
    // Only index files if we have to, or if caller asked us to
    // have to index if norm_utts because two-pass
    const int index = index_req || (norm_mode == QN_NORM_UTTS) \
	|| (sent_start!=0) || (sent_count!=(size_t) QN_ALL);

    // Parse the file name (may be a list), open the individual files,
    // and convert to a stream.
    // Comma in file name separates ftr files
    char* buf = new char[strlen(filename)+1];
    char* ptr = buf;
    strcpy(buf, filename);
    while (strlen(ptr)) {
	char *comma = strchr(ptr, ',');
	// Terminate string there for now
	if (comma) *comma = '\0';
	// open the file
	FILE *fp = QN_open_ftrfile(ptr, dbgname);
	// convert to a ftr stream
	QN_InFtrStream* my_f_str = QN_open_ftrstream(debug, dbgname, fp, 
						     format, width, min_width, 
						     index);
	// append to existing, if any
	// means we forget to delete previous occupants of ftr_str, oh well
	if (ftr_str) {
	    ftr_str = new QN_InFtrStream_Paste(debug, dbgname, 
					       *ftr_str, *my_f_str);
	} else {
	    ftr_str = my_f_str;
	}
	if (comma) {
	    // restore string
	    *comma = ',';
	    // move to next ptr
	    ptr = comma+1;
	} else {
	    // move to end to end loop
	    ptr += strlen(ptr);
	}
    }
    delete [] buf;

    // delta, normutts from qncopy...

    int base_n_ftrs = ftr_str->num_ftrs();
    // Maybe insert a QN_fir filter
    if (delta_order > 0) {
	// calculate the delta filter
	float *filt = new float[delta_win];

	QN_InFtrStream_FIR::FillDeltaFilt(filt, delta_win);
	QN_InFtrStream_FIR *fir 
	    = new QN_InFtrStream_FIR(debug, "ftrfile_fir", *ftr_str, 
				     delta_win, delta_win/2, filt, 
				     0, base_n_ftrs);
	ftr_str = fir;
    }
    if (delta_order > 1) {
	// calculate the double-delta filter
	float *filt = new float[delta_win];

	QN_InFtrStream_FIR::FillDoubleDeltaFilt(filt, delta_win);
	QN_InFtrStream_FIR *fir2 
	    = new QN_InFtrStream_FIR(debug, "ftrfile_fir2", *ftr_str, 
				     delta_win, delta_win/2, filt, 
				     0, base_n_ftrs);
	ftr_str = fir2;
    }

    // Maybe insert per-utterance normalizer
    if (norm_mode == QN_NORM_UTTS) {
	QN_InFtrStream_NormUtts *nuts
	    = new QN_InFtrStream_NormUtts(debug, "ftrfile_nrm", *ftr_str);
	ftr_str = nuts;
    } else {
	float *norm_bias = NULL;
	float *norm_scale = NULL;

	if (normfile != NULL) {
	    // Normalize the stream with constants from file
	    // .. or, for online_norm, set them up as defaults
	    size_t normfile_num_ftrs = ftr_str->num_ftrs();
	    float* norm_mean = new float[normfile_num_ftrs];
	    float* norm_sdevs = new float[normfile_num_ftrs];
	    norm_bias = new float[normfile_num_ftrs];
	    norm_scale = new float[normfile_num_ftrs];
	    if (QN_read_norms_rap(normfile, normfile_num_ftrs,
				  norm_mean, norm_sdevs))
	    {
		QN_ERROR(dbgname, "fail to read normfile '%s' (need %d cols).", 
			 QN_FILE2NAME(normfile), normfile_num_ftrs);
	    }
	    // We actually want to subtract the mean and divide by the standard
	    // deviation - the "Norm" filter adds a bias and multiplies by a
	    // scale.
	    qn_neg_vf_vf(normfile_num_ftrs, norm_mean, norm_bias);
	    qn_recip_vf_vf(normfile_num_ftrs, norm_sdevs, norm_scale);
	    delete [] norm_sdevs;
	    delete [] norm_mean;
	}

	if (norm_mode == QN_NORM_FILE && normfile != NULL) {
	    ftr_str = new QN_InFtrStream_Norm(debug, dbgname, *ftr_str,
					       norm_bias, norm_scale);
	} else if (norm_mode == QN_NORM_ONL) {
	    ftr_str = new QN_InFtrStream_OnlNorm(debug, dbgname, *ftr_str,
						 norm_bias, norm_scale, 
						 norm_am, norm_av);
	}

	if (normfile != NULL) {
	    delete [] norm_scale;
	    delete [] norm_bias;
	}
    }

    // Cut out unwanted columns of the feature file.
    // (do this after normalization so that norms file applies to 
    //  full width)
    if (first_ftr!=0 || (ftr_str->num_ftrs()!=num_ftrs && num_ftrs!=0) ) {
	size_t narrow_num_ftrs;
	if (num_ftrs==0)
	    narrow_num_ftrs = QN_ALL;
	else
	    narrow_num_ftrs = num_ftrs;
	QN_InFtrStream_Narrow* narrow_str =
	    new QN_InFtrStream_Narrow(debug, dbgname,
				      *ftr_str, first_ftr,
				      narrow_num_ftrs, buffer_frames);
	ftr_str = narrow_str;
    }

    // Select sentences we need.
    if (sent_start != 0 || sent_count != (size_t)QN_ALL) {
	QN_InFtrStream_Cut* fwd_ftr_str = 
	              new QN_InFtrStream_Cut(debug, dbgname, *ftr_str,
					     sent_start, sent_count);
	ftr_str = fwd_ftr_str;
    }

    return ftr_str;
}

////////////////////////////////////////////////////////////////
/////////////////////////  OLD STUFF  //////////////////////////
////////////////////////////////////////////////////////////////

// This routine reads one RAP matvec-style floating point vector from
// ..an ASCII file. 

int
QN_read_rapvec(FILE* file, const char* name, size_t len, float* vec)
{
    int cnt;			// Count of values read
    unsigned int veclen;	// The length of the vector read from the file
    float f;			// A floating point value read from the file
    size_t i;			// A local counter

    while (*name!='\0')
    {
	if (*name++ != getc(file))
	    return -1;
    }
    cnt = fscanf(file, "vec %u ", &veclen);
    if (cnt!=1 || veclen!=len)
	return -1;
    for (i=0; i<len; i++)
    {
	cnt = fscanf(file, " %f ", &f);
	if (cnt!=1)
	    return -1;
	*vec++ = f;
    }
    return 0;
}


// This version reads just the first <n> elements of a rapvec. 
// This is used when reading just the 'direct' parts of a norms 
// file that includes deltas and double deltas.  For now, this 
// is how we handle it, although it does generate a warning for 
// the norms file to be too big.   1999oct14 dpwe@icsi.berkeley.eddu
int
QN_read_rapvec_subset(FILE* file, const char* name, size_t len, float* vec)
{
    int cnt;
    unsigned int veclen;	// The length of the vector in the file
    float f;			// A floating point value read from the file
    size_t i;			// A local counter

    while (*name!='\0')
    {
	if (*name++ != getc(file))
	    return -1;
    }
    cnt = fscanf(file, "vec %u ", &veclen);
    if (cnt!=1 || veclen < len)
	return -1;
    if (veclen != len) {
	QN_WARN("read_rapvec_subset", "consumed just %d of %d vector points", len, veclen);
    }
    for (i=0; i<len; i++)
    {
	cnt = fscanf(file, " %f ", &f);
	if (cnt!=1)
	    return -1;
	*vec++ = f;
    }
    /* consume remainder of points (if any), so file is left at right point */
    for (i=len; i<veclen; i++)
    {
	cnt = fscanf(file, " %f ", &f);
	if (cnt!=1)
	    return -1;
    }
    return 0;
}

// This routine writes one RAP matvec-style floating point vector to
// ..an ASCII file. 

int
QN_write_rapvec(FILE* file, const char* name, size_t len, const float* vec)
{
    size_t i;			// A local counter

    if (name==NULL)
	name = "";
    fprintf(file, "%svec %lu\n", name, (unsigned long) len);
    for (i=0; i<len; i++)
    {
	fprintf(file, "%g\n", vec[i]);
    }
    return 0;
}


int
QN_read_norms_rap(FILE* normfile, size_t n_features,
		 float* means, float* sdevs)
{
    int ec;

    //    ec = QN_read_rapvec(normfile, "", n_features, means);
    ec = QN_read_rapvec_subset(normfile, "", n_features, means);
    if (ec!=0)
    {
	return(ec);
    }
    //    ec = QN_read_rapvec(normfile, "", n_features, sdevs);
    ec = QN_read_rapvec_subset(normfile, "", n_features, sdevs);
    if (ec!=0)
    {
	return(ec);
    }
    qn_recip_vf_vf(n_features, sdevs, sdevs);
    return 0;
}

int
QN_write_norms_rap(FILE* normfile, size_t n_features,
		   const float* means, const float* sdevs)
{
    int ec;
    float* tmp = new float [n_features];
    
    qn_recip_vf_vf(n_features, sdevs, tmp);
    ec = QN_write_rapvec(normfile, "", n_features, means);
    if (ec!=0)
    {
	return(ec);
    }
    ec = QN_write_rapvec(normfile, "", n_features, tmp);
    if (ec!=0)
    {
	return(ec);
    }
    delete [] tmp;
    return 0;
}


