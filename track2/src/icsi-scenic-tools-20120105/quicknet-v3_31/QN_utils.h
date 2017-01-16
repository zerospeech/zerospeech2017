// $Header: /u/drspeech/repos/quicknet2/QN_utils.h,v 1.50 2011/03/22 05:55:01 davidj Exp $

#ifndef QN_utils_h_INCLUDED
#define QN_utils_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "QN_types.h"
#include "QN_MLP.h"
#include "QN_MLPWeightFile.h"
#include "QN_streams.h"

// Historically we used QN_VERSION but autoconf uses QN_PACKAGE_VERSION
#define QN_VERSION QN_PACKAGE_VERSION

void QN_math_init(int use_pp, int use_fe, int use_blas, int use_cuda,
		  int blas_threads);

// Details about the host CPU
class QN_cpuinfo
{
    enum QN_hyper { QN_HYPER_UNKNOWN, QN_HYPER_TRUE, QN_HYPER_FALSE };
    static const int MAXLINE = 1024; // Max /proc/cpuinfo line len
    char cpu[MAXLINE];		     // CPU string
    char mhz[MAXLINE];		     // MHz string
    int cores;			     // Number of cores
    char hyper[MAXLINE];	     // Hyperthrading status

public:
    QN_cpuinfo(void);
    const char* cputype(void) { return cpu; };
    const char* cpumhz(void) { return mhz; };
    const int numcores(void) { return cores; };
    const char* hyperstat(void) { return hyper; };
};


// A function to call at the start of program to display system info.
void QN_output_sysinfo(const char* progname);

// A function to output MLP size info, with 'name' being the name of the MLP
void QN_output_mlp_size(const char* name, size_t layers,
			const size_t *layer_size);

// A function that is called when "new" runs out of memory.
void QN_new_handler();

// Some simple file management routines

// Open a file.
// This produces an appropriate error message if there is a problem,
// handles "-" as a filename, and registers the stream point for 
// future error message use.
FILE* QN_open(const char* filename, const char* mode, size_t bufsize = 0,
	      const char* tag = NULL);
// A good vbuf size for QN_open to use
enum { QN_FTRFILE_BUF_SIZE = 0x8000 };

extern int qn_io_debug;	// Can be used to change logging for I/O.

// Like strdup, but uses new.
inline
char* QN_strdup(const char* s)
{
    size_t len = strlen(s) + 1;
    char* buf = new char[len];
    strncpy(buf, s, len);
    return buf;
}


// This simply closes a file, checking for errors.
void QN_close(FILE* file);

// Check to see if two paths refer to the same file.
// Returns 0 if paths point to the same file.
int QN_pathcmp(const char* f1, const char* f2);

// Return the current time (seconds from the epoch) as a double with the
// highest precision available.
// Note that this is quite computationally expensive on SPERT - do not try
// calling it every millisecond!
double QN_time();

// Return the time now as a string suitable for output that does not include
// any newlines.

enum { QN_TIMESTR_BUFLEN = 29 };
void QN_timestr(char* buf, size_t len);


// A routine for reading or writing weights from a filename.
void QN_readwrite_weights(int debug, const char* dbgname,
			  QN_MLP& mlp, 
			  const char* wfile_name,
			  QN_WeightFileType wfile_format,
			  QN_FileMode mode,
			  float* minp = NULL, float* maxp = NULL);


// A routine for reading or writing weights from a weightfile object.
void QN_readwrite_weights(int debug, const char* dbgname,
			  QN_FileMode mode, QN_MLPWeightFile& weights,
			  QN_MLP& mlp, 
			  float* minp = NULL, float* maxp = NULL);


// Generic routine for reading a weight file into an MLP - thin
// wrapper around QN_readwrite_weights.
void QN_read_weights(QN_MLPWeightFile& wfile, QN_MLP& mlp,
		     float* min, float* max, int debug = 0,
		     const char* dbgname = NULL);

// Generic routine for writing a weight file from an MLP - thin wrapper around
// QN_readwrite_weights.
void QN_write_weights(QN_MLPWeightFile& wfile, QN_MLP& mlp,
		      float* min, float* max, int debug = 0,
		      const char* dbgname = NULL);


// Randomize the weights and biases in an MLP

void QN_randomize_weights(int debug, int seed, QN_MLP& mlp,
			  float weight_min, float weight_max,
			  float bias_min, float bias_max);

// Randomize the weights and biases of an arbitrary-layerd MLP.
void QN_randomize_weights(int debug, int seed, QN_MLP& mlp,
			  float* weight_min, float* weight_max,
			  float* bias_min, float* bias_max);

// Randomize the weights and biases in a 3 layer MLP - with more control.
void QN_randomize_weights(int debug, int seed, QN_MLP& mlp,
			  float in2hid_min, float in2hid_max,
			  float hidbias_min, float hidbias_max,
			  float hid2out_min, float hid2out_max,
			  float outbias_min, float outbias_max);

// Set the learning rate for all weight sections to the same value
void QN_set_learnrate(QN_MLP& mlp, float rate);

// A function to calculate all sorts of interesting statistics about
// a feature database.  Will fill in all the vectors where there are
// non-null pointers.   Each supplied vector must be long enough to hold
// all the features in the stream.
// Currently we return  mean and standard deviation vectors.

void
QN_ftrstats(int debug, QN_InFtrStream& ftr_stream,
	    float* ftr_mean, float* ftr_sdev);

// Some routines that are useful for timing things

double QN_secs_to_MCPS(double time, size_t n_pres, QN_MLP& mlp);


// Map a weight log file template to a file name
int QN_logfile_template_map(const char* from, char* to, size_t maxlen,
			    QNUInt32 e_arg, QNUInt32 p_arg);

// Check that a weight log file template is okay
int QN_logfile_template_check(const char* templ);

/* dpwe addition */

#define QN_MAXPATHLEN 1024	/* default len of buffer used to build cmds */

int QN_SearchExecPaths(char *cmd, char *paths, char *rslt, int rsltlen);
    /* Search through the WS or colon-separated list of paths
       for an executable file whose tail is cmd.  Return absolute 
       path in rslt, which is allocated at rsltlen.  Return 1 
       if executable found & path successfully returned, else 0. */

// Little function to map from ascii string to QN_NORM_* code 
// for arg handling
int QN_string_to_norm_const(const char *str);

// ftrfile handling for qnstrn & qnsfwd

// Open feature files in SPERT-friendly way; record for later QN_close_ftrfiles.
FILE *QN_open_ftrfile(const char *name);

// Close any streams we opened with QN_open_ftrfile & free buffers
void QN_close_ftrfiles(void);

// Basic function to open a feature file of a specific type
QN_InFtrStream* QN_open_ftrstream(int debug, const char *dbgname, 
			       FILE *ftrfile, 
			       const char* format, size_t width, 
			       size_t min_width, int indexed);

// Higher level function builds a stream of objects
QN_InFtrStream *QN_build_ftrstream(int debug, const char* dbgname, 
				const char *filename, const char* format, 
				size_t width, int index_req, 
				FILE* normfile, 
				size_t first_ftr, size_t num_ftrs,
				size_t sent_start, size_t sent_count, 
				size_t buffer_frames,
				int delta_order, int delta_win, 
				int norm_mode, double norm_am, double norm_av);

// Flags to pass to QN_build_ftrstream's norm_mode
enum { QN_NORM_FILE = 0, QN_NORM_UTTS = 1, QN_NORM_ONL = 2};

// Default values for the time constants (2sec at 10ms)
#define QN_DFLT_NORM_AM (1/200.0)
#define QN_DFLT_NORM_AV (1/200.0)


////////////////////////////////////////////////////////////////
/////////////////////////  OLD STUFF  //////////////////////////
////////////////////////////////////////////////////////////////

// Here are some useful utility functions for dealing with any
// ..kind of application using the QuickNet library

// This routine reads one RAP matvec-style floating point vector from
// ..an ASCII file. 
int QN_read_rapvec(FILE* file, const char* name, size_t len, float* vec);

// This routine writes one RAP matvec-style floating point vector to
// ..an ASCII file. 
int QN_write_rapvec(FILE* file, const char* name, size_t len,
		    const float* vec);

// This routine loads normalization data from a RAP style normalization file
// Note that this returns standard deviations - the reciprocals of the
// values stored in the normalization file
int QN_read_norms_rap(FILE* normfile, size_t n_features,
		     float* means, float* sdevs);

// This routine stores normalization data to a RAP style normalization file
// Note that this takes standard deviations - the reciprocals of the
// values stored in the normalization file
int QN_write_norms_rap(FILE* normfile, size_t n_features,
		       const float* means, const float* sdevs);


#endif
