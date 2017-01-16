#ifndef NO_RCSID
const char* qnsfwd_rcsid = 
    "$Header: /u/drspeech/repos/quicknet2/qnsfwd.cc,v 1.66 2006/06/24 01:55:25 davidj Exp $";
#endif

#include <QN_config.h>
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#define EXIT_FAILURE (1)
#endif
#include <sys/types.h>
#ifdef QN_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef QN_HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <unistd.h>

#ifdef QN_HAVE_SET_NEW_HANDLER
extern "C" {
    typedef void (*new_handler)(void);
    new_handler set_new_handler (new_handler);
}
#endif

#ifndef FILENAME_MAX
#define FILENAME_MAX (MAXPATHLEN)
#endif

#include "QuickNet.h"

static struct {
    const char* ftr1_file;
    const char* ftr1_format;
    int ftr1_width;
    const char* ftr2_file;
    const char* ftr2_format;
    int ftr2_width;
    const char* unary_file;
    const char* hardtarget_file;
    const char* hardtarget_format;
    const char* ftr1_norm_file;
    const char* ftr2_norm_file;
    int ftr1_ftr_start;
    int ftr2_ftr_start;
    int ftr1_ftr_count;
    int ftr2_ftr_count;
    int hardtarget_lastlab_reject;
    int window_extent;
    int ftr1_window_offset;
    int ftr2_window_offset;
    int unary_window_offset;
    int hardtarget_window_offset;
    int ftr1_window_len;
    int ftr2_window_len;
    int ftr1_delta_order;
    int ftr1_delta_win;
    char* ftr1_norm_mode_str;
    int ftr1_norm_mode;
    double ftr1_norm_am;
    double ftr1_norm_av;
    int ftr2_delta_order;
    int ftr2_delta_win;
    char* ftr2_norm_mode_str;
    int ftr2_norm_mode;
    double ftr2_norm_am;
    double ftr2_norm_av;
    long fwd_sent_start;
    long fwd_sent_count;
    char* fwd_sent_range;
    int unary_enumerate;
    const char* init_weight_file;
    int unary_size;
    int mlp3_input_size;
    int mlp3_hidden_size;
    int mlp3_output_size;
    const char* mlp3_output_type;
    int mlp3_fx;		// Unused
    int mlp3_in2hid_exp;	// Unused
    int mlp3_hid2out_exp;	// Unused
    int mlp3_bunch_size;
    int mlp3_blas;
    int mlp3_pp;
    int realtime;
    int realtime_latency;
    const char* activation_file;
    const char* activation_format;
    int mlp3_threads;
    int slaves;			// NO LONGER USED
    const char *cpu;		// NO LONGER USED
    const char* log_file;	// Stream for storing status messages.
    int verbose;
    int debug;			// Debug level.
} config;

static void
set_defaults(void)
{
    config.ftr1_file = "";
    config.ftr1_format = "pfile";
    config.ftr1_width = 0;
    config.ftr2_file = "";
    config.ftr2_format = "pfile";
    config.ftr2_width = 0;
    config.unary_file = "";
    config.hardtarget_file = "";
    config.hardtarget_format = "";
    config.ftr1_norm_file = "";
    config.ftr2_norm_file = "";
    config.ftr1_ftr_start = 0;
    config.ftr2_ftr_start = 0;
    config.ftr1_ftr_count = 0;
    config.ftr2_ftr_count = 0;
    config.hardtarget_lastlab_reject = 0;
    config.window_extent = 9;
    config.ftr1_window_offset = 0;
    config.ftr2_window_offset = 4;
    config.unary_window_offset = 3;
    config.hardtarget_window_offset = 0;
    config.ftr1_window_len = 9;
    config.ftr2_window_len = 0;
    config.ftr1_delta_order = 0;
    config.ftr1_delta_win = 9;
    config.ftr1_norm_mode_str = NULL;
    config.ftr1_norm_mode = QN_NORM_FILE;
    config.ftr1_norm_am = QN_DFLT_NORM_AM;
    config.ftr1_norm_av = QN_DFLT_NORM_AV;
    config.ftr2_delta_order = 0;
    config.ftr2_delta_win = 9;
    config.ftr2_norm_mode_str = NULL;
    config.ftr2_norm_mode = QN_NORM_FILE;
    config.ftr2_norm_am = QN_DFLT_NORM_AM;
    config.ftr2_norm_av = QN_DFLT_NORM_AV;
    config.fwd_sent_start = 0;
    config.fwd_sent_count = INT_MAX;
    config.fwd_sent_range = 0;
    config.unary_enumerate = 0;
    config.init_weight_file = "";
    config.unary_size = 0;
    config.mlp3_input_size = 153;
    config.mlp3_hidden_size = 200;
    config.mlp3_output_size = 56;
    config.mlp3_output_type = "softmax";
    config.mlp3_fx = 0;
    config.mlp3_in2hid_exp = 0;
    config.mlp3_hid2out_exp = 0;
    config.mlp3_bunch_size = 16;
#ifdef QN_HAVE_LIBBLAS
    config.mlp3_blas = 1;
#else
    config.mlp3_blas = 0;
#endif
    config.mlp3_pp = 1;
    config.realtime = 0;
    config.realtime_latency = 64;
    config.activation_file = "-";
    config.activation_format = "rapascii";
    config.mlp3_threads = 1;
    config.slaves = 0;
    config.cpu = "host";
    config.log_file = "";
    config.verbose = 0;
    config.debug = 0;
}

QN_ArgEntry argtab[] =
{
{ NULL, "Quicknet MLP forward pass program version " QN_VERSION, QN_ARG_DESC },
{ "ftr1_file", "Main input feature file", QN_ARG_STR,
  &(config.ftr1_file), QN_ARG_REQ },
{ "ftr1_format", "Main feature file format [pfile,pre,onlftr,lna,srifile,srilist]", QN_ARG_STR,
  &(config.ftr1_format) },
{ "ftr1_width", "Main feature file feature columns", QN_ARG_INT,
  &(config.ftr1_width) },
{ "ftr2_file", "Second input feature file", QN_ARG_STR,
  &(config.ftr2_file) },
{ "ftr2_format","Secondary feature file format [pfile,pre,onlftr,lna,srifile,srilist]", QN_ARG_STR,
  &(config.ftr2_format) },
{ "ftr2_width", "Secondary feature file feature columns", QN_ARG_INT,
  &(config.ftr2_width) },
{ "unary_file", "Auxilliary unary file", QN_ARG_STR,
  &(config.unary_file) },
{ "hardtarget_file", "Target label file", QN_ARG_STR,
  &(config.hardtarget_file) },
{ "hardtarget_format", "Target label file format [pfile,pre,ilab]", QN_ARG_STR,
  &(config.hardtarget_format) },
{ "ftr1_norm_file", "Normalization parameters for ftr1_file", QN_ARG_STR,
  &(config.ftr1_norm_file) },
{ "ftr2_norm_file", "Normalization parameters for ftr2_file", QN_ARG_STR,
  &(config.ftr2_norm_file) },
{ "ftr1_ftr_start", "First feature used from ftr1_file",
  QN_ARG_INT, &(config.ftr1_ftr_start) },
{ "ftr2_ftr_start", "First feature used from ftr2_file",
  QN_ARG_INT, &(config.ftr2_ftr_start) },
{ "ftr1_ftr_count", "Number of features used from ftr1_file",
  QN_ARG_INT, &(config.ftr1_ftr_count) },
{ "ftr2_ftr_count", "Number of features used from ftr2_file",
  QN_ARG_INT, &(config.ftr2_ftr_count) },
{ "hardtarget_lastlab_reject", "Last label value indicates no-train frames",
  QN_ARG_BOOL, &(config.hardtarget_lastlab_reject) },
{ "window_extent", "Extent of all windows (frames)", QN_ARG_INT,
  &(config.window_extent) },
{ "ftr1_window_offset", "Offset of window on ftr1_file (frames)",
  QN_ARG_INT, &(config.ftr1_window_offset) },
{ "ftr2_window_offset", "Offset of window on ftr2_file (frames)",
  QN_ARG_INT, &(config.ftr2_window_offset) },
{ "unary_window_offset", "Offset of window on unaryfile (frames)",
  QN_ARG_INT, &(config.unary_window_offset) },
{ "hardtarget_window_offset", "Offset of window on target label file (frames)",
  QN_ARG_INT, &(config.hardtarget_window_offset) },
{ "ftr1_window_len", "Length of window on ftr1_file (frames)", QN_ARG_INT,
  &(config.ftr1_window_len) },
{ "ftr2_window_len", "Length of window on ftr2_file (frames)", QN_ARG_INT,
  &(config.ftr2_window_len) },
{ "ftr1_delta_order", "Order of derivatives added to ftr1_file", QN_ARG_INT,
  &(config.ftr1_delta_order) },
{ "ftr1_delta_win", "Window size for ftr1_file delta-calculation", QN_ARG_INT,
  &(config.ftr1_delta_win) },
{ "ftr1_norm_mode", "Normalization mode (file/utts/online)", QN_ARG_STR,
  &(config.ftr1_norm_mode_str) },
{ "ftr1_norm_alpha_m", "Update constant for online norm means", QN_ARG_DOUBLE,
  &(config.ftr1_norm_am) },
{ "ftr1_norm_alpha_v", "Update constant for online norm vars", QN_ARG_DOUBLE,
  &(config.ftr1_norm_av) },
{ "ftr2_delta_order", "Order of derivatives added to ftr2_file", QN_ARG_INT,
  &(config.ftr2_delta_order) },
{ "ftr2_delta_win", "Window size for ftr2_file delta-calculation", QN_ARG_INT,
  &(config.ftr2_delta_win) },
{ "ftr2_norm_mode", "Normalization mode (file/utts/online)", QN_ARG_STR,
  &(config.ftr2_norm_mode_str) },
{ "ftr2_norm_alpha_m", "Update constant for online norm means", QN_ARG_DOUBLE,
  &(config.ftr2_norm_am) },
{ "ftr2_norm_alpha_v", "Update constant for online norm vars", QN_ARG_DOUBLE,
  &(config.ftr2_norm_av) },
{ "fwd_sent_start", "Number of first sentence",
  QN_ARG_LONG, &(config.fwd_sent_start) },
{ "fwd_sent_count", "Number of sentences",
  QN_ARG_LONG, &(config.fwd_sent_count) },
{ "fwd_sent_range", "Sentences to process as QN_Range(3) string",
  QN_ARG_STR, &(config.fwd_sent_range) },
{ "init_weight_file", "Input weight file", QN_ARG_STR,
  &(config.init_weight_file),QN_ARG_REQ },
{ "unary_enumerate", "Use all possible unary input values", QN_ARG_BOOL,
  &(config.unary_enumerate) },
{ "unary_size", "Number of unary inputs to net",
  QN_ARG_INT, &(config.unary_size)},
{ "mlp3_input_size", "Number of units in input layer",
  QN_ARG_INT, &(config.mlp3_input_size)},
{ "mlp3_hidden_size","Number of units in hidden layer",
  QN_ARG_INT, &(config.mlp3_hidden_size) },
{ "mlp3_output_size","Number of units in output layer",
  QN_ARG_INT, &(config.mlp3_output_size) },
{ "mlp3_output_type","Type of non-linearity in MLP output layer [sigmoid,sigmoidx,softmax,linear]",
  QN_ARG_STR, &(config.mlp3_output_type) },
{ "mlp3_fx","NO LONGER USED",
  QN_ARG_BOOL, &(config.mlp3_fx) },
{ "mlp3_in2hid_exp","NO LONGER USED",
  QN_ARG_INT, &(config.mlp3_in2hid_exp) },
{ "mlp3_hid2out_exp","NO LONGER USED",
  QN_ARG_INT, &(config.mlp3_hid2out_exp) },
{ "mlp3_bunch_size","Number of patterns to bunch at once",
  QN_ARG_INT, &(config.mlp3_bunch_size) },
{ "mlp3_blas","Use BLAS libraries",
  QN_ARG_BOOL, &(config.mlp3_blas) },
{ "mlp3_pp","Use internal high-performance libraries",
  QN_ARG_BOOL, &(config.mlp3_pp) },
{ "mlp3_threads","Number of threads in MLP object",
  QN_ARG_INT, &(config.mlp3_threads) },
{ "realtime","Peform real time recognition",
  QN_ARG_BOOL, &(config.realtime) },
{ "realtime_latency","Real time latency control",
  QN_ARG_INT, &(config.realtime_latency) },
{ "activation_file", "Output activation file", QN_ARG_STR,
  &(config.activation_file) },
{ "activation_format", "Output format [rapascii,raphex,rapbin,onlftr,lna,pfile,ascii]",
      QN_ARG_STR, &(config.activation_format) },
{ "slaves","NO LONGER USED",
  QN_ARG_INT, &(config.slaves) },
{ "cpu","NO LONGER USED",
  QN_ARG_STR, &(config.cpu) },
{ "log_file", "File for status messages", QN_ARG_STR, &(config.log_file) },
{ "verbose", "Output extra status messages",
  QN_ARG_BOOL, &(config.verbose) },
{ "debug", "Level of internal diagnostic output",
  QN_ARG_INT, &(config.debug) },
{ NULL, NULL, QN_ARG_NOMOREARGS }
};


// A function to create a feature stream for a given
// feature file.  Also handles opening multiple files if 
// stream comes from a sequence of files.
// If buffer_frames is 0, batch run.
void
create_ftrstream(int debug, const char* dbgname, const char *filename, 
		 const char* format, size_t width,
		 FILE* normfile, size_t first_ftr, size_t num_ftrs,
		 size_t fwd_sent_start, size_t fwd_sent_count,
		 char* fwd_sent_range, 
		 size_t window_extent, size_t window_offset,
		 size_t window_len, size_t buffer_frames,
		 int delta_order, int delta_win, 
		 int norm_mode, double norm_am, double norm_av, 
		 QN_InFtrStream** str_ptr)
{
    QN_InFtrStream* ftr_str = NULL;	// Temporary stream holder.
    int index = (fwd_sent_range==0)?0:1; // build_ will index if it needs to anyway

    if (fwd_sent_range != 0) {
	if ( !(fwd_sent_start == 0 && fwd_sent_count == QN_ALL) ) {
	    QN_ERROR("create_ftrstream",
		     "You cannot specify fwd_sents by both range "
		     "and start/count.");
	}
    }

    ftr_str = QN_build_ftrstream(debug, dbgname, filename, format, 
			      width, index, normfile, 
			      first_ftr, num_ftrs, 
			      fwd_sent_start, fwd_sent_count,
			      buffer_frames, 
			      delta_order, delta_win, 
			      norm_mode, norm_am, norm_av);

    if (fwd_sent_range) {
	// Using range strings
	QN_InFtrStream_CutRange* fwd_ftr_str 
	    = new QN_InFtrStream_CutRange(debug, dbgname, *ftr_str, 
					  fwd_sent_range);
	ftr_str = (QN_InFtrStream*)fwd_ftr_str;
    }

    // Create window.
    size_t bot_margin = window_extent - window_offset - window_len;
    QN_InFtrStream_SeqWindow* winftr_str =
	new QN_InFtrStream_SeqWindow(debug, dbgname,
				     *ftr_str, window_len,
				     window_offset, bot_margin,
				     buffer_frames);
    *str_ptr = winftr_str;
}

// A function to create a label stream from a given
// label file.

void
create_labstream(int debug, const char* dbgname, FILE* hardtarget_file,
		 const char* format, size_t width, 
		 size_t fwd_sent_start, size_t fwd_sent_count,
		 char *fwd_sent_range, 
		 size_t window_extent, size_t window_offset,
		 size_t buffer_frames,
		 QN_InLabStream** str_ptr)
{
    QN_InLabStream* lab_str;	// Temporary stream holder.

    // Only index files if we have to.
    const int index = (fwd_sent_start!=0) || (fwd_sent_count!=(size_t) QN_ALL) || (fwd_sent_range != 0);

    // Convert the file descriptor into a stream.
    if (strcmp(format, "pfile")==0)
    {
	QN_InFtrLabStream_PFile* pfile_str =
	    new QN_InFtrLabStream_PFile(debug, // Select debugging.
					dbgname, // Debugging tag.
					hardtarget_file, // Label file.
					index // Indexed flag.
		);
	if (pfile_str->num_labs()!=1)
	{
	    QN_ERROR("create_labstreams",
		     "Label file has %lu features, should only be 1.",
		     (unsigned long) pfile_str->num_labs() );
	}
	lab_str = pfile_str;
    }
    else if (strcmp(format, "pre")==0)
    {
	QN_InFtrLabStream_PreFile* prefile_str =
	    new QN_InFtrLabStream_PreFile(debug, // Select debugging.
					  dbgname, // Debugging tag.
					  hardtarget_file, // Label file.
					  width, // No of ftrs.
					  index // Indexed flag.
		);
	lab_str = prefile_str;
    }
    else if (strcmp(format, "ilab")==0)
    {
	QN_InLabStream_ILab* ilab_str =
	    new QN_InLabStream_ILab(debug, // Select debugging.
				    dbgname, // Debugging tag.
				    hardtarget_file, // Label file.
				    index // Indexed flag.
				    );
	lab_str = ilab_str;
    }
    else
    {
	QN_ERROR(dbgname, "unknown label file format '%s'.", format);
	lab_str = NULL;
    }
    //    QN_InFtrLabStream_PFile* pfile_str =
    //	new QN_InFtrLabStream_PFile(debug, // Select debugging.
    //				    dbgname, // Debugging tag.
    //				    hardtarget_file, // Feature file.
    //				    index // Indexed flag.
    //	    );
    //if (pfile_str->num_labs()!=1)
    //{
    //	    QN_ERROR("create_labstreams",
    //		     "Label file has %lu features, should only be 1.",
    //		     (unsigned long) pfile_str->num_labs() );
    //}
    //lab_str = pfile_str;

    // Get sentences we want.
    QN_InLabStream_Cut* fwd_lab_str = NULL;
    if (fwd_sent_start!=0 || fwd_sent_count!=(size_t) QN_ALL)
    {
	fwd_lab_str = new QN_InLabStream_Cut(debug, dbgname, *lab_str,
					     fwd_sent_start, fwd_sent_count);
	lab_str = fwd_lab_str;
    }

    if (fwd_sent_range) {
	// Using range strings
	QN_InLabStream_CutRange* fwd_lab_str 
	    = new QN_InLabStream_CutRange(debug, dbgname, *lab_str, 
					  fwd_sent_range);
	lab_str = (QN_InLabStream*)fwd_lab_str;
    }

    // Create window.
    const size_t window_len = 1;
    size_t bot_margin = window_extent - window_offset - window_len;
    QN_InLabStream_SeqWindow* winlab_str =
	new QN_InLabStream_SeqWindow(debug, dbgname,
				     *lab_str, window_len,
				     window_offset, bot_margin,
				     buffer_frames
				      );
    *str_ptr = winlab_str;
}

void
create_fwdmlp(int debug, const char*,
	      size_t n_input, size_t n_hidden, size_t n_output,
	      const char* mlp3_output_type, int mlp3_bunch_size, 
	      int threads,  QN_MLP** mlp_ptr)
{
    // Create MLP and load weights.
    QN_MLP* mlp3 = NULL;


    QN_OutputLayerType outlayer_type;
    if (strcmp(mlp3_output_type, "sigmoid")==0)
	outlayer_type = QN_OUTPUT_SIGMOID;
    else if (strcmp(mlp3_output_type, "sigmoidx")==0)
	outlayer_type = QN_OUTPUT_SIGMOID_XENTROPY;
    else if (strcmp(mlp3_output_type, "softmax")==0)
	outlayer_type = QN_OUTPUT_SOFTMAX;
    else if (strcmp(mlp3_output_type, "linear")==0)
	outlayer_type = QN_OUTPUT_LINEAR;
    else
    {
	QN_ERROR("create_fwdmlp", "unknown output unit type '%s'.",
		 mlp3_output_type);
	outlayer_type = QN_OUTPUT_SIGMOID;
    }
    // floating point
    if (mlp3_bunch_size == 0) {
	QN_ERROR("create_fwdmlp",
		 "bunch size of zero is illegal for forward pass"); 
    }
    if (threads==1)
    {
	// Test of multi-layer MLP
	size_t layer_units[3];
	layer_units[0] = n_input;
	layer_units[1] = n_hidden;
	layer_units[2] = n_output;
	mlp3 = new QN_MLP_BunchFl3(debug, "fwdmlp",
				   layer_units[0],
				   layer_units[1],
				   layer_units[2],
				   outlayer_type, mlp3_bunch_size);
	QN_OUTPUT("MLP type: QN_MLP_BunchFl3.");

//	mlp3 = new QN_MLP_BunchFl3(debug, "fwdmlp",
//				   n_input, n_hidden, n_output,
//				   outlayer_type, mlp3_bunch_size);
    }
    else
    {
#ifdef QN_HAVE_LIBPTHREAD
	if (threads>mlp3_bunch_size)
	{
	    QN_ERROR("create_fwdmlp", "number of threads must "
		     "be less than the bunch size.");
	}
	else
	{
	    mlp3 = new QN_MLP_ThreadFl3(debug, "fwdmlp", n_input,
					n_hidden, n_output, outlayer_type,
					mlp3_bunch_size, threads);
	    QN_OUTPUT("MLP type: QN_MLP_ThreadFl3.");
	}
#else
	QN_ERROR("create_fwdmlp",
		 "cannot use multiple threads as libpthread "
		 "was not linked with this executable.");
#endif
    }
    *mlp_ptr = mlp3;
}

// A function to create an output stream.
void
create_outstream(int debug, const char*, FILE* outfile,
		 size_t width, const char* format,
		 int realtime,
		 QN_OutFtrStream** outstr_ptr)
{
    QN_StreamType str_type;	// The type of the output stream.
    QN_OutFtrStream* outstream;	// The actual output stream.

    if (strcmp(format, "rapascii")==0)
	str_type = QN_STREAM_RAPACT_ASCII;
    else if (strcmp(format, "raphex")==0)
	str_type = QN_STREAM_RAPACT_HEX;
    else if (strcmp(format, "rapbin")==0)
	str_type = QN_STREAM_RAPACT_BIN;
    else if (strcmp(format, "ascii")==0)
	str_type = QN_STREAM_ASCII;
    else if (strcmp(format, "onlftr")==0)
	str_type = QN_STREAM_ONLFTR;
    else if (strcmp(format, "lna8")==0 || strcmp(format, "lna")==0)
	str_type = QN_STREAM_LNA8;
    else if (strcmp(format, "pfile")==0)
	str_type = QN_STREAM_PFILE;
    else
    {
	QN_ERROR("create_outstream", "unknown output format '%s'.", format);
	str_type = QN_STREAM_UNKNOWN; // Avoid warning
    }
    if (realtime \
	&& !(str_type==QN_STREAM_RAPACT_BIN || str_type==QN_STREAM_LNA8) )
    {
    	QN_ERROR("create_outstream", "can only use stream type 'rapbin' or "
		 "'lna8'for realtime recognitions.");
    }
    if (str_type==QN_STREAM_LNA8)
    {
	outstream = new QN_OutFtrLabStream_LNA8(debug, "activation",
						outfile, width, realtime );
    }
    else if (str_type==QN_STREAM_ONLFTR)
    {
	outstream = new QN_OutFtrStream_OnlFtr(debug, "activation",
					       outfile, width);
    }
    else if (str_type==QN_STREAM_PFILE)
    {
	outstream =
	    new QN_OutFtrLabStream_PFile(debug, "activation",
					 outfile,
					 width,
					 0, // No labels.
					 1  // Indexed.
		);
    }
    else if (str_type==QN_STREAM_ASCII)
    {
	outstream =
	    new QN_OutFtrLabStream_Ascii(debug, "activation",
					 outfile,
					 width,
					 0, // No labels.
					 0, // Not indexed
					 NULL, // No token map
					 0 // New style %g
		);
    }
    else
    {
	outstream = 
	    new QN_OutFtrStream_Rapact(debug,	// Debug.
				       "activation", // Instance name.
				       outfile,	// output stream.
				       width,   // Number of units.
				       str_type,// Which particular rapact.
				       realtime ); // Whether online or not.
    }
    *outstr_ptr = outstream;
}

void
qnsfwd()
{
    int verbose = config.verbose;
    int realtime = config.realtime;
    size_t iobuf_size;		// The size used for IO buffers.
    size_t buf_frames;		// No of frames to buffer in streams
				// (controls latency)
    time_t now;
    enum { 
	DEFAULT_IOBUF_SIZE = 0x8000,
	REALTIME_IOBUF_SIZE = 0x1000
	};	// 

    if (realtime)
    {
	iobuf_size = REALTIME_IOBUF_SIZE;
	buf_frames = config.realtime_latency;
    }
    else
    {
	iobuf_size = DEFAULT_IOBUF_SIZE;
	buf_frames = 500;
    }

    time(&now);
// A note for the logfile, including some system info.
    QN_output_sysinfo("qnsfwd");
    QN_OUTPUT("Program start: %.24s.", ctime(&now));

// Open files and provisionally check arguments.
    if (verbose>0)
    {
	QN_OUTPUT("Opening files...");
    }

    // hardtarget_file.
    const char* hardtarget_file = config.hardtarget_file;
    FILE* hardtarget_fp = NULL;
    int lastlab_reject = config.hardtarget_lastlab_reject;
    if (strcmp(hardtarget_file,"")!=0)
    {
	hardtarget_fp = QN_open(hardtarget_file, "r", iobuf_size,
				"hardtarget_file");
    }
    else
    {
	if(lastlab_reject)
	{
	    QN_ERROR(NULL, "hardtarget_lastlab_reject cannot be true if no "
		     "hardtarget_file is specified");
	}
    }

    // unary_file.
    const char* unary_file = config.unary_file;
    FILE* unary_fp = NULL;
    if (strcmp(unary_file, "")!=0)
    {
	unary_fp = QN_open(unary_file, "r", iobuf_size, "unary_file");
    }

    // ftr1_norm_file.
    FILE* ftr1_norm_fp = NULL;
    const char* ftr1_norm_file = config.ftr1_norm_file;
    if (strcmp(ftr1_norm_file, "")!=0)
    {
	ftr1_norm_fp = QN_open(ftr1_norm_file, "r", 0, "ftr1_norm_file");
    }
    
    // ftr2_norm_file.
    FILE* ftr2_norm_fp = NULL;
    const char* ftr2_norm_file = config.ftr2_norm_file;
    if (strcmp(ftr2_norm_file, "")!=0)
    {
	if (strcmp(config.ftr2_file, "") == 0)
	    QN_ERROR(NULL, "ftr2_norm_file is specified but ftr2_file "
		     "is not.");
	else
	    ftr2_norm_fp = QN_open(ftr2_norm_file, "r", 0, "ftr2_norm_file");
    }

    // Weight files.
    FILE* init_weight_fp = NULL;
    const char* init_weight_file = config.init_weight_file;
    if (strcmp(init_weight_file, "")==0)
    {
	QN_ERROR(NULL, "Null input weightfile specified.");
    }
    else
	init_weight_fp = QN_open(init_weight_file, "r", 0, "init_weight_file");

    FILE* output_fp = NULL;
    const char* activation_file = config.activation_file;
    output_fp = QN_open(activation_file, "w");

    // Windowing.
    int window_extent = config.window_extent;
    if (window_extent<0 || window_extent>1000)
    {
	QN_ERROR(NULL, "window_extent must be in range 0-1000.");
    }
    int ftr1_window_offset = config.ftr1_window_offset;
    if (ftr1_window_offset<0 || ftr1_window_offset>=window_extent)
    {
	QN_ERROR(NULL, "ftr1_window_offset must be less than "
		 " window_extent.");
    }
    int ftr1_window_len = config.ftr1_window_len;
    if (ftr1_window_len<=0)
    {
	QN_ERROR(NULL, "ftr1_window_len must be greater than 0.");
    }
    if ((ftr1_window_offset + ftr1_window_len) > window_extent)
    {
	QN_ERROR(NULL, "ftr1_window_offset+ftr1_window_len must be "
		 "less than window_extent.");
    }
    int ftr2_window_offset = config.ftr2_window_offset;
    int ftr2_window_len = config.ftr2_window_len;
    if (strcmp(config.ftr2_file, "") != 0) {
	if (ftr2_window_offset<0 || ftr2_window_offset>=window_extent)
	    {
		QN_ERROR(NULL, "ftr2_window_offset must be less than "
			 " window_extent.");
	    }
	if (ftr2_window_len<0)
	    {
		QN_ERROR(NULL, "ftr2_window_len must be positive.");
	    }
	if ((ftr2_window_offset + ftr2_window_len) > window_extent)
	    {
		QN_ERROR(NULL, "ftr2_window_offset+ftr2_window_len must be "
			 "less than window_extent.");
	    }
    }
    int hardtarget_window_offset = config.hardtarget_window_offset;
    if (hardtarget_window_offset<0 || hardtarget_window_offset>=window_extent)
    {
	QN_ERROR(NULL, "hardtarget_window_offset must be less than "
		 " window_extent.");
    }
    int unary_window_offset = config.unary_window_offset;
    if (strcmp(unary_file, "")!=0 \
        && (unary_window_offset<0 || unary_window_offset>=window_extent))
    {
	QN_ERROR(NULL, "unary_window_offset must be less than "
		 " window_extent.");
    }

    char* fwd_sent_range = config.fwd_sent_range;
    size_t fwd_sent_start = config.fwd_sent_start;
    size_t fwd_sent_count = (config.fwd_sent_count==INT_MAX) ?
	(size_t) QN_ALL : config.fwd_sent_count;
    if (realtime && (fwd_sent_start!=0 || fwd_sent_count!=QN_ALL))
    {
	QN_ERROR(NULL, "for realtime recognition, fwd_sent_start must be 0 "
		 "and fwd_sent_count must be %i.", INT_MAX);
    }

    // Check for mlp3_input_size consistency.
    size_t ftr1_ftr_start = config.ftr1_ftr_start;
    size_t ftr2_ftr_start = config.ftr2_ftr_start;
    size_t ftr1_ftr_count = config.ftr1_ftr_count;
    size_t ftr2_ftr_count = config.ftr2_ftr_count;
    size_t unary_size = config.unary_size;
    int unary_enumerate = config.unary_enumerate;
    if (unary_enumerate && unary_fp!=NULL)
    {
	QN_ERROR(NULL, "cannot specify unary_enumerate and a unary_file"
		 " together");

    }
    size_t ftrfile_num_input = ftr1_ftr_count * ftr1_window_len
	+ ftr2_ftr_count * ftr2_window_len + unary_size;
    size_t mlp3_input_size = config.mlp3_input_size;
    size_t mlp3_hidden_size = config.mlp3_hidden_size;
    size_t mlp3_output_size = config.mlp3_output_size;
    if (ftrfile_num_input!=mlp3_input_size)
    {
	QN_ERROR(NULL, "number of inputs to the net %lu does not equal width"
		 " of data stream from feature files %lu.",
		 (unsigned long) mlp3_input_size, 
		 (unsigned long) ftrfile_num_input);
    }
    
    int debug = config.debug;

    // Create the MLP.
    QN_MLP* mlp;
    create_fwdmlp(debug, "mlp",
		  mlp3_input_size, mlp3_hidden_size, mlp3_output_size,
		  config.mlp3_output_type, config.mlp3_bunch_size,
		  config.mlp3_threads, &mlp);

    float min, max;
    if (verbose>0)
    {
	QN_OUTPUT("Loading weights...");
    }
    QN_MLPWeightFile_RAP3 inwfile(debug, init_weight_fp,
				  QN_READ,
				  init_weight_file,
				  mlp3_input_size, mlp3_hidden_size,
				  mlp3_output_size);
    QN_read_weights(inwfile, *mlp, &min, &max, debug);
    QN_OUTPUT("Weights loaded from '%s', min=%g max=%g.",
	      init_weight_file, min, max);

    // Do activation_file stream creation.
    // Do this before creating input streams so processes downstream
    // can read headers and get going.
    QN_OutFtrStream* outfile_str = NULL;
    size_t output_width;
    if (unary_enumerate)
    {
	if (unary_size==0)
	{
	    QN_ERROR(NULL, "unary_size must be non-zero when"
		     " unary_enumerate is true.");
	}
	output_width = mlp3_output_size * unary_size;
    }
    else
	output_width = mlp3_output_size;
    create_outstream(debug, "outstream", output_fp,
		     output_width, config.activation_format,
		     realtime,
		     &outfile_str);

    if (verbose>0)
    {
	QN_OUTPUT("Scanning feature and label files...");
    }

    // Do ftr1_file stream creation.
    QN_InFtrStream* ftr1_str = NULL;
    create_ftrstream(debug, "ftr1_file", config.ftr1_file,
		     config.ftr1_format, config.ftr1_width,
		     ftr1_norm_fp,
		     ftr1_ftr_start, ftr1_ftr_count,
		     fwd_sent_start, fwd_sent_count, fwd_sent_range, 
		     window_extent,
		     ftr1_window_offset, ftr1_window_len,
		     buf_frames,
		     config.ftr1_delta_order, config.ftr1_delta_win, 
		     config.ftr1_norm_mode, 
		     config.ftr1_norm_am, config.ftr1_norm_av, 
		     &ftr1_str);
		      
    // Do ftr2_file stream creation.
    QN_InFtrStream* ftr2_str = NULL;
    if (strcmp(config.ftr2_file, "")!=0)
    {
	create_ftrstream(debug, "ftr2_file", config.ftr2_file,
			 config.ftr2_format, config.ftr2_width,
			 ftr2_norm_fp,
			 ftr2_ftr_start, ftr2_ftr_count,
			 fwd_sent_start, fwd_sent_count, fwd_sent_range, 
			 window_extent,
			 ftr2_window_offset, ftr2_window_len,
			 buf_frames,
			 config.ftr2_delta_order, config.ftr2_delta_win, 
			 config.ftr2_norm_mode,
			 config.ftr2_norm_am, config.ftr2_norm_av, 
			 &ftr2_str);
    }

    // Merge the two streams.
    QN_InFtrStream* ftrfile_str;
    if (ftr2_str!=NULL)
    {
	ftrfile_str = new QN_InFtrStream_JoinFtrs(debug, "ftrfile",
						  *ftr1_str,
						  *ftr2_str);
    }
    else
    {
	ftrfile_str = ftr1_str;
    }

    // If necessary, add the unary input features.
    if (unary_fp!=NULL)
    {
	QN_InLabStream* unary_str = NULL;
	create_labstream(debug, "unaryfile", unary_fp,
			 "pfile", 0, 
			 fwd_sent_start, fwd_sent_count, fwd_sent_range, 
			 window_extent,
			 unary_window_offset,
			 buf_frames,
			 &unary_str);
 	// Convert the unary input label into a feature stream.
	QN_InFtrStream* unaryftr_str = NULL;
	unaryftr_str = new QN_InFtrStream_OneHot(debug,
						       "unaryfile",
						       *unary_str,
						       unary_size);
	
 	// Merge in the feature streams.
	ftrfile_str = new QN_InFtrStream_JoinFtrs(debug,
						  "unaryfile",
						  *ftrfile_str,
						  *unaryftr_str);
    }
    

    // Does config.ftr1_file refer to just a single file?
    int ftr1_onefile = 1;
    if (strchr(config.ftr1_file, ',') != NULL) {
	// filename looks like a comma-separated list
	ftr1_onefile = 0;
	// won't try to run pathcmp on it.
    }

    // Do hardtarget_file stream creation.
    QN_InLabStream* hardtarget_str = NULL;
    if (hardtarget_fp!=NULL)
    {
	// Handle formats where we need to know the number of ftrs to
	// extract the labels.
	// A bit of a hack!!
	size_t hardtarget_width;
	if (ftr1_onefile && QN_pathcmp(config.ftr1_file, hardtarget_file)==0)
	    hardtarget_width = config.ftr1_width;
	else
	    hardtarget_width = 0;
	const char* hardtarget_format = config.hardtarget_format;
	if (strcmp(hardtarget_format, "")==0)
	    hardtarget_format = config.ftr1_format;

	create_labstream(debug, "labfile", hardtarget_fp,
			 hardtarget_format, hardtarget_width, 
			 fwd_sent_start, fwd_sent_count, fwd_sent_range, 
			 window_extent,
			 hardtarget_window_offset,
			 buf_frames,
			 &hardtarget_str);
    }


    // Do the forward pass.
    if (verbose)
    {
	QN_OUTPUT("Starting forward pass...");
    }
    if (unary_enumerate)
    {
	if (config.mlp3_bunch_size!=1)
	{
	    QN_WARN(NULL, "unary_enumerate option ignores non-unity"
		    "mlp3_bunch_size.");
	}
	QN_enumForward(debug,	// Level of debugging.
		       "fwd",	// A name for this object.
		       verbose,	// Level of status reporting.
		       mlp,	// The MLP to use.
		       ftrfile_str, // The stream containing the windowd net input.
		       NULL,    // The stream containing the correct label.
		       outfile_str, // The net output stream.
		       unary_size  // The width of the unary input.
	    );
    }
    else
    {
	QN_hardForward(debug,	// Level of debugging.
		       "fwd",	// A name for this object.
		       verbose,	// Level of status reporting.
		       mlp,	// The MLP to use.
		       ftrfile_str,	// The stream containing the windowd net input.
		       hardtarget_str,// The stream containing the correct label.
		       outfile_str,	// The net output stream.
		       NULL,	// The label output stream.
                       config.mlp3_bunch_size,
		       lastlab_reject // True if reject frames allowed
	    );
    }
    
// A note for the logfile.
    time(&now);
    QN_OUTPUT("Program stop: %.24s", ctime(&now));
    delete mlp;
    delete outfile_str;

    QN_close(init_weight_fp);
    if (ftr2_norm_fp!=NULL)
	QN_close(ftr2_norm_fp);
    if (ftr1_norm_fp!=NULL)
	QN_close(ftr1_norm_fp);
    
    if (hardtarget_fp!=NULL)
    {
	QN_close(hardtarget_fp);
    }
    QN_close_ftrfiles();
}

int
main(int argc, const char* argv[])
{
    char* progname;		// The name of the prog - set by QN_initargs.

    FILE* log_fp;
    char log_buf[160];

    set_defaults();
    QN_initargs(&argtab[0], &argc, &argv, &progname);

    // map norm_mode_str to val
    config.ftr1_norm_mode = QN_string_to_norm_const(config.ftr1_norm_mode_str);
    config.ftr2_norm_mode = QN_string_to_norm_const(config.ftr2_norm_mode_str);

    if (strcmp(config.log_file, "")==0)
	log_fp = stderr;
    else
    {
	log_fp = QN_open(config.log_file, "w");
#ifdef QN_HAVE_SETVBUF
	if (setvbuf(log_fp, log_buf, _IOLBF, sizeof(log_buf)))
	    abort();		// Fail on error.
#endif       
    }
    QN_printargs(log_fp, progname, &argtab[0]);
    QN_logger = new QN_Logger_Simple(log_fp, stderr, progname);
    qn_io_debug = config.debug;

    // Cannot send logging and output to same stream.
    if ( (strcmp(config.log_file, "-")==0)
	 && (strcmp(config.activation_file, "-")==0) )
    {
	QN_ERROR(NULL, "cannot send both activations and status messages "
		 "to stdout.");
    }
    
    // Install our own out-of-memory handler if possible.
#ifdef QN_HAVE_SET_NEW_HANDLER
    set_new_handler(QN_new_handler);
#endif

    // Set the math mode
    qn_math = config.mlp3_pp ? (QN_MATH_PP|QN_MATH_FE) : QN_MATH_NV;
#ifdef QN_HAVE_LIBBLAS
    qn_math |= config.mlp3_blas ? QN_MATH_BL : 0;
#else 
    if (config.mlp3_blas)
    {
	QN_ERROR(NULL, "cannot enable BLAS library as none is linked with the "
		 "executable."); 
    }
#endif // #ifdef QN_HAVE_LIBBLAS

    qnsfwd();

    exit(EXIT_SUCCESS);
}
