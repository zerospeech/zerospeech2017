#ifndef NO_RCSID
const char* qnstrn_rcsid = 
    "$Header: /u/drspeech/repos/quicknet2/qnstrn.cc,v 1.66 2010/10/29 02:20:38 davidj Exp $";
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

#if !QN_HAVE_DECL_SRAND48
extern "C" {
void srand48(long);
}
#endif

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
    const char* softtarget_file;
    const char* softtarget_format;
    int softtarget_width;
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
    int softtarget_window_offset;
    int ftr1_window_len;
    int ftr2_window_len;
    int ftr1_delta_order;
    int ftr1_delta_win;
    const char* ftr1_norm_mode_str;
    int ftr1_norm_mode;
    double ftr1_norm_am;
    double ftr1_norm_av;
    int ftr2_delta_order;
    int ftr2_delta_win;
    const char* ftr2_norm_mode_str;
    int ftr2_norm_mode;
    double ftr2_norm_am;
    double ftr2_norm_av;
    long train_cache_frames;
    int train_cache_seed;
    long train_sent_start;
    long train_sent_count;
    const char* train_sent_range;
    long cv_sent_start;
    long cv_sent_count;
    const char* cv_sent_range;

    QN_Arg_ListFloat init_random_bias_min;
    QN_Arg_ListFloat init_random_bias_max;

    QN_Arg_ListFloat init_random_weight_min;
    QN_Arg_ListFloat init_random_weight_max;

    int init_random_seed;
    const char* init_weight_file;
    const char* log_weight_file;
    const char* ckpt_weight_file;
    int ckpt_hours;
    const char* out_weight_file;
    const char* learnrate_schedule;
    QN_Arg_ListFloat learnrate_vals;
    long learnrate_epochs;
    float learnrate_scale;
    int unary_size;
    int mlp3_input_size;
    int mlp3_hidden_size;
    int mlp3_output_size;
    const char* mlp3_output_type;
    int mlp3_fx;		// NO LONGER USED
    int mlp3_weight_bits;	// NO LONGER USED
    int mlp3_in2hid_exp;	// NO LONGER USED
    int mlp3_hid2out_exp;	// NO LONGER USED
    int mlp3_bunch_size;
    int mlp3_blas;
    int mlp3_pp;
    int mlp3_threads;
    int slaves;			// NO LONGER USED
    const char *cpu;			// NO LONGER USED
    const char* log_file;		// Stream for storing status messages.
    int verbose;
    int debug;			// Debug level.
} config;

static void
set_defaults(void)
{
    static float default_learnrate[1] = { 0.008 };
    static float default_bias_min[1] = { -4.1 };
    static float default_bias_max[1] = { -3.9 };
    static float default_weight_min[1] = { -0.1 };
    static float default_weight_max[1] = { 0.1 };
    
    config.ftr1_file = "";
    config.ftr1_format = "pfile";
    config.ftr1_width = 0;
    config.ftr2_file = "";
    config.ftr2_format = "pfile";
    config.ftr2_width = 0;
    config.unary_file = "";
    config.hardtarget_file = "";
    config.hardtarget_format = "";
    config.softtarget_file = "";
    config.softtarget_format = "pfile";
    config.softtarget_width = 0;
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
    config.softtarget_window_offset = 0;
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
    config.train_cache_frames = 10000;
    config.train_cache_seed = 0;
    config.train_sent_start = 0;
    config.train_sent_count = INT_MAX;
    config.train_sent_range = 0;
    config.cv_sent_start = 0;
    config.cv_sent_count = INT_MAX;
    config.cv_sent_range = 0;

    config.init_random_bias_min.count = 1;
    config.init_random_bias_min.vals = &default_bias_min[0];
    config.init_random_bias_max.count = 1;
    config.init_random_bias_max.vals = &default_bias_max[0];

    config.init_random_weight_min.count = 1;
    config.init_random_weight_min.vals = &default_weight_min[0];
    config.init_random_weight_max.count = 1;
    config.init_random_weight_max.vals = &default_weight_max[0];

    config.init_random_seed = 0;
    config.init_weight_file = "";
    config.log_weight_file = "log%p.weights";
    config.ckpt_weight_file = "ckpt-%h-%t.weights";
    config.ckpt_hours = 0;
    config.out_weight_file = "out.weights";
    config.learnrate_schedule = "newbob";
    config.learnrate_vals.count = 1;
    config.learnrate_vals.vals = &default_learnrate[0];
    config.learnrate_epochs = 9999;
    config.learnrate_scale = 0.5;
    config.unary_size = 0;
    config.mlp3_input_size = 153;
    config.mlp3_hidden_size = 200;
    config.mlp3_output_size = 56;
    config.mlp3_output_type = "softmax";
    config.mlp3_fx = 0;
    config.mlp3_weight_bits = 32;
    config.mlp3_in2hid_exp = 2;
    config.mlp3_hid2out_exp = 2;
    config.mlp3_bunch_size = 16;
#ifdef QN_HAVE_LIBBLAS
    config.mlp3_blas = 1;
#else
    config.mlp3_blas = 0;
#endif
    config.mlp3_pp = 1;
    config.mlp3_threads = 1;
    config.slaves = 0;
    config.cpu = "host";
    config.log_file = "-";
    config.verbose = 0;
    config.debug = 0;
}

QN_ArgEntry argtab[] =
{
{ NULL, "QuickNet MLP training program version " QN_VERSION, QN_ARG_DESC },
{ "ftr1_file", "Input feature file", QN_ARG_STR,
  &(config.ftr1_file), QN_ARG_REQ },
{ "ftr1_format", "Main feature file format [pfile,pre,lna,onlftr,srifile,srilist]", QN_ARG_STR,
  &(config.ftr1_format) },
{ "ftr1_width", "Main feature file feature columns", QN_ARG_INT,
  &(config.ftr1_width) },
{ "ftr2_file", "Second input feature file", QN_ARG_STR,
  &(config.ftr2_file) },
{ "ftr2_format","Secondary feature file format [pfile,pre,lna,onlftr,srifile,srilist]", QN_ARG_STR,
  &(config.ftr2_format) },
{ "ftr2_width", "Secondary feature file feature columns", QN_ARG_INT,
  &(config.ftr2_width) },
{ "unary_file", "Auxilliary unary file", QN_ARG_STR,
  &(config.unary_file) },
{ "hardtarget_file", "Target label file", QN_ARG_STR,
  &(config.hardtarget_file) },
{ "hardtarget_format", "Target label file format [pfile,pre,ilab]", QN_ARG_STR,
  &(config.hardtarget_format) },
{ "softtarget_file", "Target feature file", QN_ARG_STR,
  &(config.softtarget_file) },
{ "softtarget_format", "Target feature file format [pfile,pre,lna,onlftr]", QN_ARG_STR,
  &(config.softtarget_format) },
{ "softtarget_width", "Target feature file feature columns", QN_ARG_INT,
  &(config.softtarget_width) },
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
{ "unary_window_offset", "Offset of window on unary_file (frames)",
  QN_ARG_INT, &(config.unary_window_offset) },
{ "hardtarget_window_offset", "Offset of window on target label file (frames)",
  QN_ARG_INT, &(config.hardtarget_window_offset) },
{ "softtarget_window_offset", "Offset of window on target feature file (frames)",
  QN_ARG_INT, &(config.softtarget_window_offset) },
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
{ "train_cache_frames", "Number of training frames in cache",
  QN_ARG_LONG, &(config.train_cache_frames) },
{ "train_cache_seed", "Training presentation randomization seed",
  QN_ARG_INT, &(config.train_cache_seed) },
{ "train_sent_start", "Number of first training sentence",
  QN_ARG_LONG, &(config.train_sent_start) },
{ "train_sent_count", "Number of training sentences",
  QN_ARG_LONG, &(config.train_sent_count) },
{ "train_sent_range", "Training sentence indices in QN_Range(3) format",
  QN_ARG_STR, &(config.train_sent_range) },
{ "cv_sent_start", "Number of first cross validation sentence",
  QN_ARG_LONG, &(config.cv_sent_start) },
{ "cv_sent_count", "Number of cross validation sentences",
  QN_ARG_LONG, &(config.cv_sent_count) },
{ "cv_sent_range", "Cross validation sentence indices in QN_Range(3) format",
  QN_ARG_STR, &(config.cv_sent_range) },
{ "init_random_bias_min", "Minimum random bias (per layer)", QN_ARG_LIST_FLOAT,
  &(config.init_random_bias_min) },
{ "init_random_bias_max", "Maximum random bias (per layer)", QN_ARG_LIST_FLOAT,
  &(config.init_random_bias_max) },
{ "init_random_weight_min", "Minimum random weight (per layer)", QN_ARG_LIST_FLOAT,
  &(config.init_random_weight_min) },
{ "init_random_weight_max", "Maximum random weight (per layer)", QN_ARG_LIST_FLOAT,
  &(config.init_random_weight_max) },
{ "init_random_seed", "Net initialization random number seed",
  QN_ARG_INT, &(config.init_random_seed) },
{ "init_weight_file", "Input weight file", QN_ARG_STR,
  &(config.init_weight_file) },
{ "log_weight_file", "Log weight file", QN_ARG_STR,
  &(config.log_weight_file) },
{ "ckpt_weight_file", "Checkpoint weight file", QN_ARG_STR,
  &(config.ckpt_weight_file) },
{ "ckpt_hours", "Checkpoint interval (in hours)", QN_ARG_INT,
  &(config.ckpt_hours) },
{ "out_weight_file", "Output weight file", QN_ARG_STR,
  &(config.out_weight_file) },
{ "learnrate_schedule", "LR schedule type [newbob,list,smoothdecay]",
      QN_ARG_STR, &(config.learnrate_schedule) },
{ "learnrate_vals", "Learning rates",
      QN_ARG_LIST_FLOAT, &(config.learnrate_vals) },
{ "learnrate_epochs", "Maximum number of epochs", QN_ARG_LONG,
  &(config.learnrate_epochs) },
{ "learnrate_scale", "Scale factor of successive learning rates", QN_ARG_FLOAT,
  &(config.learnrate_scale) },
{ "unary_size", "Number of unary inputs to net",
  QN_ARG_INT, &(config.unary_size)},
{ "mlp3_input_size", "Number of units in input layer",
  QN_ARG_INT, &(config.mlp3_input_size)},
{ "mlp3_hidden_size","Number of units in hidden layer",
  QN_ARG_INT, &(config.mlp3_hidden_size) },
{ "mlp3_output_size","Number of units in output layer",
  QN_ARG_INT, &(config.mlp3_output_size) },
{ "mlp3_output_type","Type of non-linearity in MLP output layer [sigmoid,sigmoidx,softmax]",
  QN_ARG_STR, &(config.mlp3_output_type) },
{ "mlp3_fx","NO LONGER USED",
  QN_ARG_BOOL, &(config.mlp3_fx) },
{ "mlp3_weight_bits","NO LONGER USED",
  QN_ARG_INT, &(config.mlp3_weight_bits) },
{ "mlp3_in2hid_exp","NO LONGER USED",
  QN_ARG_INT, &(config.mlp3_in2hid_exp) },
{ "mlp3_hid2out_exp","NO LONGER USED",
  QN_ARG_INT, &(config.mlp3_hid2out_exp) },
{ "mlp3_bunch_size","Size of bunches used in MLP training",
  QN_ARG_INT, &(config.mlp3_bunch_size) },
{ "mlp3_blas","Use BLAS libraries",
  QN_ARG_BOOL, &(config.mlp3_blas) },
{ "mlp3_pp","Use internal high-performance libraries",
  QN_ARG_BOOL, &(config.mlp3_pp) },
{ "mlp3_threads","Number of threads in MLP object",
  QN_ARG_INT, &(config.mlp3_threads) },
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

// QN_open_ftrstream, QN_open_ftrfile and QN_close_ftrfiles all moved to QN_utils.cc

// A function to create a train and cross validation stream for a given
// feature file.  Also handles opening multiple files if 
// stream comes from a sequence of files.

void
create_ftrstreams(int debug, const char* dbgname, const char* filename,
		  const char* format, size_t width,
		  FILE* normfile, size_t first_ftr, size_t num_ftrs,
		  size_t train_sent_start, size_t train_sent_count,
		  const char* train_sent_range, 
		  size_t cv_sent_start, size_t cv_sent_count,
		  const char* cv_sent_range, 
		  size_t window_extent, size_t window_offset,
		  size_t window_len, 
		  int delta_order, int delta_win,  
		  int norm_mode, double norm_am, double norm_av, 
		  size_t train_cache_frames, int train_cache_seed,
		  QN_InFtrStream** train_str_ptr, QN_InFtrStream** cv_str_ptr)
{
    QN_InFtrStream* ftr_str = NULL;	// Temporary stream holder.
    int index = 1; 			// training always requires indexed
    int buffer_frames = 500;

    ftr_str = QN_build_ftrstream(debug, dbgname, filename, format, 
			      width, index, normfile, 
			      first_ftr, num_ftrs, 
			      0, QN_ALL, // do utt selection ourselves
			      buffer_frames, 
			      delta_order, delta_win, 
			      norm_mode, norm_am, norm_av);

    // Create training and cross-validation streams.
    QN_InFtrStream_Cut* train_ftr_str = NULL;
    QN_InFtrStream_Cut2* cv_ftr_str = NULL;

    if (train_sent_range != 0) {
	if ( !(train_sent_start == 0 && train_sent_count == QN_ALL) ) {
	    QN_ERROR("create_ftrstreams",
		     "You cannot specify train_sents by both range "
		     "and start/count.");
	}
    }

    if (cv_sent_range != 0) {
	if ( !(cv_sent_start == 0 && cv_sent_count == QN_ALL) ) {
	    QN_ERROR("create_ftrstreams",
		     "You cannot specify cv_sents by both range "
		     "and start/count.");
	}
    }

    if ( (train_sent_range == 0 && cv_sent_range != 0) \
	 || (train_sent_range != 0 && cv_sent_range == 0) ) {
	QN_ERROR("create_ftrstreams",
		 "If you use ranges for one of train_sents or cv_sents, "
		 "you must use it for both.");
    }

    if (train_sent_range == 0) {
	// Using old-style start & count, not range strings
	QN_InFtrStream_Cut* fwd_ftr_str 
	    = new QN_InFtrStream_Cut(debug, dbgname, *ftr_str,
					       train_sent_start, 
					       train_sent_count,
					       cv_sent_start, 
					       cv_sent_count);
	train_ftr_str = (QN_InFtrStream_Cut*)fwd_ftr_str;
    } else {
	// Using range strings
	QN_InFtrStream_CutRange* fwd_ftr_str 
	    = new QN_InFtrStream_CutRange(debug, dbgname, *ftr_str, 
					  train_sent_range, 
					  cv_sent_range);
	train_ftr_str = (QN_InFtrStream_Cut*)fwd_ftr_str;
    }
    cv_ftr_str = new QN_InFtrStream_Cut2(*train_ftr_str);

    // Create training and CV windows.
    size_t bot_margin = window_extent - window_offset - window_len;
    QN_InFtrStream_RandWindow* train_winftr_str =
	new QN_InFtrStream_RandWindow(debug, dbgname,
				      *train_ftr_str, window_len,
				      window_offset, bot_margin,
				      train_cache_frames, train_cache_seed
	    );
    QN_InFtrStream_SeqWindow* cv_winftr_str =
	new QN_InFtrStream_SeqWindow(debug, dbgname,
				      *cv_ftr_str, window_len,
				      window_offset, bot_margin
				      );
    *train_str_ptr = train_winftr_str;
    *cv_str_ptr = cv_winftr_str;
}

// A function to create a train and cross validation stream for a given
// label file.

void
create_labstreams(int debug, const char* dbgname, FILE* hardtarget_file,
		  const char* format, size_t width,
		  size_t train_sent_start, size_t train_sent_count,
		  const char* train_sent_range, 
		  size_t cv_sent_start, size_t cv_sent_count,
		  const char* cv_sent_range, 
		  size_t window_extent, size_t window_offset,
		  size_t train_cache_frames, int train_cache_seed,
		  QN_InLabStream** train_str_ptr, QN_InLabStream** cv_str_ptr)
{
    QN_InLabStream* lab_str;	// Temporary stream holder.

    // Convert the file descriptor into a stream.
    if (strcmp(format, "pfile")==0)
    {
	QN_InFtrLabStream_PFile* pfile_str =
	    new QN_InFtrLabStream_PFile(debug, // Select debugging.
					dbgname, // Debugging tag.
					hardtarget_file, // Label file.
					1 // Indexed flag.
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
					  1 // Indexed flag.
		);
	lab_str = prefile_str;
    }
    else if (strcmp(format, "ilab")==0)
    {
	QN_InLabStream_ILab* ilab_str =
	    new QN_InLabStream_ILab(debug, // Select debugging.
				    dbgname, // Debugging tag.
				    hardtarget_file, // Label file.
				    1 // Indexed flag.
				    );
	lab_str = ilab_str;
    }
    else
    {
	QN_ERROR(dbgname, "unknown label file format '%s'.", format);
	lab_str = NULL;
    }
	

    // Create training and cross-validation streams.
    QN_InLabStream_Cut* train_lab_str = NULL;
    QN_InLabStream_Cut2* cv_lab_str = NULL;
    if (train_sent_range != 0) {
	if ( !(train_sent_start == 0 && train_sent_count == QN_ALL) ) {
	    QN_ERROR("create_labstreams",
		     "You cannot specify train_sents by both range "
		     "and start/count.");
	}
    }

    if (cv_sent_range != 0) {
	if ( !(cv_sent_start == 0 && cv_sent_count == QN_ALL) ) {
	    QN_ERROR("create_labstreams",
		     "You cannot specify cv_sents by both range "
		     "and start/count.");
	}
    }

    if ( (train_sent_range == 0 && cv_sent_range != 0) \
	 || (train_sent_range != 0 && cv_sent_range == 0) ) {
	QN_ERROR("create_labstreams",
		 "If you use ranges for one of train_sents or cv_sents, "
		 "you must use it for both.");
    }

    if (train_sent_range == 0) {
	// Using old-style start & count, not range strings
	QN_InLabStream_Cut* fwd_lab_str 
	    = new QN_InLabStream_Cut(debug, dbgname, *lab_str,
					       train_sent_start, 
					       train_sent_count,
					       cv_sent_start, 
					       cv_sent_count);
	train_lab_str = (QN_InLabStream_Cut*)fwd_lab_str;
    } else {
	// Using range strings
	QN_InLabStream_CutRange* fwd_lab_str 
	    = new QN_InLabStream_CutRange(debug, dbgname, *lab_str, 
					  train_sent_range, 
					  cv_sent_range);
	train_lab_str = (QN_InLabStream_Cut*)fwd_lab_str;
    }
    cv_lab_str = new QN_InLabStream_Cut2(*train_lab_str);

    // Create training and CV windows.

    const size_t window_len = 1;
    size_t bot_margin = window_extent - window_offset - window_len;
    QN_InLabStream_RandWindow* train_winlab_str =
	new QN_InLabStream_RandWindow(debug, dbgname,
				      *train_lab_str, window_len,
				      window_offset, bot_margin,
				      train_cache_frames, train_cache_seed
	    );
    QN_InLabStream_SeqWindow* cv_winlab_str =
	new QN_InLabStream_SeqWindow(debug, dbgname,
				      *cv_lab_str, window_len,
				      window_offset, bot_margin
				      );
    *train_str_ptr = train_winlab_str;
    *cv_str_ptr = cv_winlab_str;
}

void
create_mlp(int debug, const char*,
	   size_t n_input, size_t n_hidden, size_t n_output,
	   const char* mlp3_output_type, int mlp3_bunch_size,
	   int threads, QN_MLP** mlp_ptr)
{
    // Create MLP and load weights.
    QN_MLP* mlp3 = NULL;

    QN_OutputLayerType outlayer_type;
    if (strcmp(mlp3_output_type, "sigmoid")==0) {
	outlayer_type = QN_OUTPUT_SIGMOID;
    } else if (strcmp(mlp3_output_type, "sigmoidx")==0) {
	outlayer_type = QN_OUTPUT_SIGMOID_XENTROPY;
    } else if (strcmp(mlp3_output_type, "softmax")==0) {
	outlayer_type = QN_OUTPUT_SOFTMAX;
    } else {
	QN_ERROR("create_mlp", "unknown output unit type '%s'.",
		 mlp3_output_type);
	outlayer_type = QN_OUTPUT_SIGMOID;
    }


    if (mlp3_bunch_size == 0) {
	// NOT bunch
	    if (config.mlp3_threads==1)
	    {
		    mlp3 = new QN_MLP_OnlineFl3(debug, "train",
						n_input, n_hidden, n_output,
						outlayer_type);
		    QN_OUTPUT("MLP type: QN_MLP_OnlineFl3.");
	    }
	    else
	    {
		    QN_ERROR("create_mlp", "threads must be 1 for online "
			     "training.");
	    }
    } else {
	    // Bunch
	    if (threads>1)
	    {
#ifdef QN_HAVE_LIBPTHREAD
		    if (threads>mlp3_bunch_size)
		    {
			    QN_ERROR("create_mlp", "number of threads must "
				     "be less than the bunch size.");
		    }
		    else
		    {
			    // Bunch threaded
			    mlp3 = new QN_MLP_ThreadFl3(debug, "train",
							n_input, n_hidden,
							n_output,
							outlayer_type,
							mlp3_bunch_size,
							threads);
			    QN_OUTPUT("MLP type: QN_MLP_ThreadFl3.");
		    }
#else
		    QN_ERROR("create_mlp",
			     "cannot use multiple threads as libpthread "
			     "was not linked with this executable.");
#endif
	    }
	    else if (threads==1)
	    {
		// Test of multi-layer MLP
		size_t layer_units[3];
		layer_units[0] = n_input;
		layer_units[1] = n_hidden;
		layer_units[2] = n_output;
//		mlp3 = new QN_MLP_BunchFlVar(debug, "train",
//					     3, layer_units,
//					     outlayer_type, mlp3_bunch_size);

		// Bunch unthreaded
		mlp3 = new QN_MLP_BunchFl3(debug, "train",
					   n_input, n_hidden, n_output,
					   outlayer_type, mlp3_bunch_size);
		QN_OUTPUT("MLP type: QN_MLP_BunchFl3.");
	    }
	    else
	    {
		    QN_ERROR("create_mlp","threads must be >= 1.");
	    }
    }
    *mlp_ptr = mlp3;
}

void
create_learnrate_schedule(int, const char*,
			  const char* learnrate_schedule,
			  float* learnrate_vals,
			  size_t learnrate_count,
			  float learnrate_scale,
			  size_t learnrate_epochs,
			  QN_RateSchedule** lr_schedule)
{
    QN_RateSchedule* rate_sched;
    if (learnrate_scale>1.0)
    {
	QN_ERROR("create_learnrate_schedule", "Learning rate scale is %g, but "
		 "it should be less that 1.0.");
    } 
    if (strcmp(learnrate_schedule, "newbob")==0)
    {
	rate_sched = new QN_RateSchedule_NewBoB(*learnrate_vals,
						learnrate_scale,
						0.5f, 0.5f,
						100.0f,learnrate_epochs);
    }
    else if (strcmp(learnrate_schedule, "list")==0)
    {
	long count;

	if (learnrate_epochs < learnrate_count)
	    count = learnrate_epochs;
	else
	    count = learnrate_count;
	rate_sched = new QN_RateSchedule_List(learnrate_vals, count);
    }
    else if (strcmp(learnrate_schedule, "smoothdecay")==0)
      {
	size_t search_epochs;

	if (learnrate_count<3 || learnrate_count>4) {
	  QN_ERROR(NULL,"learnrate_vals should have 3 or 4 values if learnrate_schedule is smoothdecay");
	}
	
	if (learnrate_count==4) {
	  search_epochs=(size_t)learnrate_vals[3];
	} else {
	  search_epochs=1;
	}

	QN_OUTPUT("Setting up smooth decay learning rate (lr=%.6f,decay=%.6f,stopcriterion=%.6f",learnrate_vals[0],learnrate_vals[1],learnrate_vals[2]);
	rate_sched = new QN_RateSchedule_SmoothDecay(learnrate_vals[0],
						     learnrate_vals[1],
						     learnrate_vals[2],
						     search_epochs,
						     100.0f, 0,
						     learnrate_epochs);
      }
    else
    {
	QN_ERROR("create_learnrate_schedule",
		 "Unknown learning rate schedule '%s'.",
		 learnrate_schedule);
	rate_sched = NULL;
    }
    *lr_schedule = rate_sched;
}

void
qnstrn()
{
    int verbose = config.verbose;
    time_t now;

    time(&now);

// A note for the logfile, including some system info.
    QN_output_sysinfo("qnstrn");
    QN_OUTPUT("Program start: %.24s.", ctime(&now));

// Open files and provisionally check arguments.
    if (verbose>0)
    {
	QN_OUTPUT("Opening feature file...");
    }

    // ftr files are now opened inside create_ftrstreams in order to 
    // accommodate multiple pasted-together files

    // ftr1_file.
    //    enum { FTRFILE1_BUF_SIZE = 0x8000 };
    //    const char* ftr1_file = config.ftr1_file;
    //    FILE* ftr1_fp = QN_open(ftr1_file, "r");

    // ftr2_file.
    //    enum { FTRFILE2_BUF_SIZE = 0x8000 };
    //    const char* ftr2_file = config.ftr2_file;
    //    FILE* ftr2_fp = NULL;
    //    char* ftr2_buf = NULL;
    //    if (strcmp(ftr2_file, "")!=0)
    //    {
    //	ftr2_fp = QN_open(ftr2_file, "r");
    //    }

    // unary_file.
    enum { UNARYFILE_BUF_SIZE = 0x8000 };
    const char* unary_file = config.unary_file;
    FILE* unary_fp = NULL;
    if (strcmp(unary_file, "")!=0)
	unary_fp = QN_open(unary_file, "r", UNARYFILE_BUF_SIZE, "unary_file");

    const char* hardtarget_file = config.hardtarget_file;
    const char* softtarget_file = config.softtarget_file;
    FILE* hardtarget_fp = NULL;
    //    FILE* softtarget_fp = NULL;
    char* hardtarget_buf = NULL;
    //    char* softtarget_buf = NULL;
    int lastlab_reject = config.hardtarget_lastlab_reject;
    enum { LABFILE_BUF_SIZE = 0x8000 };
    // hardtarget_file.
    if (strcmp(hardtarget_file, "")!=0 && strcmp(softtarget_file, "")==0)
	hardtarget_fp = QN_open(hardtarget_file, "r", LABFILE_BUF_SIZE,
				"hardtarget_file");
    else if (strcmp(hardtarget_file, "")==0 && strcmp(softtarget_file, "")!=0)
    {
	// opened within create_ftrstream

	// softtarget_file.
	//	enum { LABFILE_BUF_SIZE = 0x8000 };
	//	softtarget_fp = QN_open(softtarget_file, "r");
	//	softtarget_buf = new char[LABFILE_BUF_SIZE];
	if (lastlab_reject)
	{
	    QN_ERROR(NULL, "hardtarget_lastlab_reject cannot be true if no "
		     "hardtarget_file is specified");
	}
    }
    else
    {
	QN_ERROR(NULL, "must specify one and only one of hardtarget_file "
		 "and softtarget_file");
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
	if (strcmp(config.ftr2_file, "")==0)
	    QN_ERROR(NULL, "ftr2_norm_file is specified but ftr2_file "
		     "is not.");
	else if (config.ftr2_ftr_count==0)
	    QN_ERROR(NULL, "ftr2_norm_file is specified but ftr2_ftr_count "
		     "is 0.");
	else
	    ftr2_norm_fp = QN_open(ftr2_norm_file, "r", 0, "ftr2_norm_file");
    }

    // Weight files.
    FILE* init_weight_fp = NULL;
    const char* init_weight_file = config.init_weight_file;
    if (strcmp(init_weight_file, "")!=0)
    {
	init_weight_fp = QN_open(init_weight_file, "r", 0, "init_weight_file");
    }
    FILE* out_weight_fp = NULL;
    const char* out_weight_file = config.out_weight_file;
    out_weight_fp = QN_open(out_weight_file, "w", 0, "out_weight_file");

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
    // don't test ftr2_window_offset unless we have a file
    if (strcmp(config.ftr2_file, "")!= 0 && config.ftr2_ftr_count > 0) {
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
    // Don't worry about the unary_window_offset unless there is actually
    // a unary_file (default value of 3 causes error for window_extent=1)
    int unary_window_offset = config.unary_window_offset;
    if ( (strcmp(unary_file, "")!=0) \
         && (unary_window_offset<0 || unary_window_offset>=window_extent))
    {
	QN_ERROR(NULL, "unary_window_offset must be less than "
		 " window_extent.");
    }
    int hardtarget_window_offset = config.hardtarget_window_offset;
    if (hardtarget_window_offset<0 || hardtarget_window_offset>=window_extent)
    {
	QN_ERROR(NULL, "hardtarget_window_offset must be less than "
		 " window_extent.");
    }
    int softtarget_window_offset = config.softtarget_window_offset;
    if (softtarget_window_offset<0 || softtarget_window_offset>=window_extent)
    {
	QN_ERROR(NULL, "softtarget_window_offset must be less than "
		 " window_extent.");
    }

    // Check for overlapping training and CV ranges.
    size_t train_sent_start = config.train_sent_start;
    size_t train_sent_count = (config.train_sent_count==INT_MAX) ?
	(size_t) QN_ALL : config.train_sent_count;
    size_t last_train_sent = (train_sent_count==QN_ALL) ? 
	INT_MAX : train_sent_start + train_sent_count - 1;
    const char* train_sent_range = config.train_sent_range;
    size_t cv_sent_start = config.cv_sent_start;
    size_t cv_sent_count = (config.cv_sent_count==INT_MAX) ?
	(size_t) QN_ALL : config.cv_sent_count;
    const char* cv_sent_range = config.cv_sent_range;
    size_t last_cv_sent = (cv_sent_count==QN_ALL) ? 
	INT_MAX : cv_sent_start + cv_sent_count - 1;
    if (train_sent_range == 0 && cv_sent_range == 0 &&
	((cv_sent_start>=train_sent_start && cv_sent_start<=last_train_sent)
	 || (last_cv_sent>=train_sent_start && last_cv_sent<=last_train_sent)))
    {
	QN_WARN(NULL, "training and cv sentence ranges overlap.");
    }

    // Check for mlp3_input_size consistency.
    size_t ftr1_ftr_start = config.ftr1_ftr_start;
    size_t ftr2_ftr_start = config.ftr2_ftr_start;
    size_t ftr1_ftr_count = config.ftr1_ftr_count;
    size_t ftr2_ftr_count = config.ftr2_ftr_count;
    size_t unary_size = config.unary_size;
    size_t ftrfile_num_input = ftr1_ftr_count  * ftr1_window_len
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
    

    // Sentence and randomization details.
    long train_cache_frames = config.train_cache_frames;
    int train_cache_seed = config.train_cache_seed;
    if (train_cache_frames<1000)
    {
	QN_ERROR(NULL, "train_cache_frames must be greater than 1000.");
    }

    
    int init_random_seed = config.init_random_seed;
    int debug = config.debug;

    // Do ftr1_file stream creation.
    QN_InFtrStream* ftr1_train_str = NULL;
    QN_InFtrStream* ftr1_cv_str = NULL;
    create_ftrstreams(debug, "ftr1_file", config.ftr1_file,
		      config.ftr1_format, config.ftr1_width,
		      ftr1_norm_fp,
		      ftr1_ftr_start, ftr1_ftr_count,
		      train_sent_start, train_sent_count,
		      train_sent_range, 
		      cv_sent_start, cv_sent_count,
		      cv_sent_range, 
		      window_extent,
		      ftr1_window_offset, ftr1_window_len,
		      config.ftr1_delta_order, config.ftr1_delta_win, 
		      config.ftr1_norm_mode, 
		      config.ftr1_norm_am, config.ftr1_norm_av,  
		      train_cache_frames, train_cache_seed,
		      &ftr1_train_str, &ftr1_cv_str);
		      
    // Do ftr2_file stream creation.
    QN_InFtrStream* ftr2_train_str = NULL;
    QN_InFtrStream* ftr2_cv_str = NULL;
    if (strcmp(config.ftr2_file, "")!=0)
    {
	if (config.ftr2_ftr_count==0)
	    QN_WARN(NULL, "ftr2_file is set but ftr2_ftr_count is 0.");
	create_ftrstreams(debug, "ftr2_file", config.ftr2_file,
			  config.ftr2_format, config.ftr2_width,
			  ftr2_norm_fp,
			  ftr2_ftr_start, ftr2_ftr_count,
			  train_sent_start, train_sent_count,
			  train_sent_range, 
			  cv_sent_start, cv_sent_count,
			  cv_sent_range, 
			  window_extent,
			  ftr2_window_offset, ftr2_window_len,
			  config.ftr2_delta_order, config.ftr2_delta_win, 
			  config.ftr2_norm_mode, 
			  config.ftr2_norm_am, config.ftr2_norm_av,  
			  train_cache_frames, train_cache_seed,
			  &ftr2_train_str, &ftr2_cv_str);
    }

    // Merge the two training feature streams.
    QN_InFtrStream* ftrfile_train_str;
    QN_InFtrStream* ftrfile_cv_str;
    if (ftr2_train_str!=NULL)
    {
	assert(ftr2_cv_str!=NULL);
	ftrfile_train_str = new QN_InFtrStream_JoinFtrs(debug, "train_ftrfile",
						       *ftr1_train_str,
 						       *ftr2_train_str);
	ftrfile_cv_str = new QN_InFtrStream_JoinFtrs(debug, "cv_ftrfile",
						     *ftr1_cv_str,
						     *ftr2_cv_str);
    }
    else
    {
	assert(ftr2_cv_str==NULL);
	assert(ftr2_train_str==NULL);
	ftrfile_train_str = ftr1_train_str;
	ftrfile_cv_str = ftr1_cv_str;
    }

    // If necessary, add the unary input feature.
    if (unary_fp!=NULL)
    {
	QN_InLabStream* unary_train_str = NULL;
	QN_InLabStream* unary_cv_str = NULL;
	
	create_labstreams(debug, "unary", unary_fp,
			  "pfile", 0,
			  train_sent_start, train_sent_count,
			  train_sent_range, 
			  cv_sent_start, cv_sent_count,
			  cv_sent_range, 
			  window_extent,
			  unary_window_offset,
			  train_cache_frames, train_cache_seed,
			  &unary_train_str, &unary_cv_str);

	// Convert the unary input label into a feature stream.
	QN_InFtrStream* unaryftr_train_str = NULL;
	QN_InFtrStream* unaryftr_cv_str = NULL;
	
	unaryftr_train_str = new QN_InFtrStream_OneHot(debug,
						       "train_unaryfile",
						       *unary_train_str,
						       unary_size);
	unaryftr_cv_str = new QN_InFtrStream_OneHot(debug,
						    "cv_unaryfile",
						    *unary_cv_str,
						    unary_size);
						   
	// Merge in the feature streams.
	ftrfile_train_str = new QN_InFtrStream_JoinFtrs(debug,
							"train_unaryfile",
							*ftrfile_train_str,
							*unaryftr_train_str);
	ftrfile_cv_str = new QN_InFtrStream_JoinFtrs(debug, "cv_unaryfile",
						     *ftrfile_cv_str,
						     *unaryftr_cv_str);
	
    }
    

    QN_InLabStream* hardtarget_train_str = NULL;
    QN_InLabStream* hardtarget_cv_str = NULL;
    QN_InFtrStream* softtarget_train_str = NULL;
    QN_InFtrStream* softtarget_cv_str = NULL;

    // Does config.ftr1_file refer to just a single file?
    int ftr1_onefile = 1;
    if (strchr(config.ftr1_file, ',') != NULL) {
	// filename looks like a comma-separated list
	ftr1_onefile = 0;
	// won't try to run pathcmp on it.
    }

    if (hardtarget_fp!=NULL)
    {
	// Do hardtarget stream creation.

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
	
	create_labstreams(debug, "hardtarget", hardtarget_fp,
			  hardtarget_format, hardtarget_width,
			  train_sent_start, train_sent_count,
			  train_sent_range, 
			  cv_sent_start, cv_sent_count,
			  cv_sent_range, 
			  window_extent,
			  hardtarget_window_offset,
			  train_cache_frames, train_cache_seed,
			  &hardtarget_train_str, &hardtarget_cv_str);
    }
    else if (strcmp(softtarget_file,"")!=0)
    {
	size_t softtarget_width = config.softtarget_width;
	const char* softtarget_format = config.softtarget_format;
	if (strcmp(softtarget_format, "")==0)
	    softtarget_format = config.ftr1_format;
	
	create_ftrstreams(debug, "softtarget", (char *)softtarget_file,
			  softtarget_format, softtarget_width,
			  NULL,
			  0, 0,
			  train_sent_start, train_sent_count,
			  train_sent_range, 
			  cv_sent_start, cv_sent_count,
			  cv_sent_range, 
			  window_extent,
			  softtarget_window_offset, 1,
			  0, 0, 0,  /* no deltas or per-utt normalization */
			  0.0, 0.0, 
			  train_cache_frames, train_cache_seed,
			  &softtarget_train_str, &softtarget_cv_str);
	
    }
    else
	assert(0);
   

    // Create the MLP.
    QN_MLP* mlp;
    create_mlp(debug, "mlp",
	       mlp3_input_size, mlp3_hidden_size, mlp3_output_size,
	       config.mlp3_output_type,
	       config.mlp3_bunch_size, config.mlp3_threads,
	       &mlp);

    // Create the leaning rate schedule.
    QN_RateSchedule* lr_schedule;
    create_learnrate_schedule(debug, "learnrate",
			      config.learnrate_schedule,
			      config.learnrate_vals.vals,
			      config.learnrate_vals.count,
			      config.learnrate_scale,
			      config.learnrate_epochs,
			      &lr_schedule);

    
    // A weight file of "" means randomize.
    if (init_weight_fp==NULL)
    {
	if (verbose>0)
	{
	    QN_OUTPUT("Randomizing weights...");
	}
	if (config.init_random_weight_min.count<1 ||
	    config.init_random_weight_min.count>2 ||
	    config.init_random_weight_max.count<1 ||
	    config.init_random_weight_max.count>2 ||
	    config.init_random_bias_min.count<1 ||
	    config.init_random_bias_min.count>2 ||
	    config.init_random_bias_max.count<1 ||
	    config.init_random_bias_max.count>2) {
	  QN_ERROR(NULL,"weight/bias list initializations must either have 1 or 2 elements");
	}
	float in2hid_min = config.init_random_weight_min.vals[0];
	float in2hid_max = config.init_random_weight_max.vals[0];
	float hidbias_min = config.init_random_bias_min.vals[0];
	float hidbias_max = config.init_random_bias_max.vals[0];
	/* if initialization lists have 1 member, use for both layer 1 and 2
	   if 2 members, use separate initializations */
	float hid2out_min = config.init_random_weight_min.vals[(config.init_random_weight_min.count==1)?0:1];
	float hid2out_max = config.init_random_weight_max.vals[(config.init_random_weight_max.count==1)?0:1];
	float outbias_min = config.init_random_bias_min.vals[(config.init_random_bias_min.count==1)?0:1];
	float outbias_max = config.init_random_bias_max.vals[(config.init_random_bias_max.count==1)?0:1];
	
	QN_randomize_weights(debug, init_random_seed, *mlp,
			     in2hid_min, in2hid_max,
			     hidbias_min, hidbias_max,
			     hid2out_min, hid2out_max,
			     outbias_min, outbias_max);
	if (verbose>0)
	{
	    QN_OUTPUT("Randomized weights.");
	}
    }
    else
    {
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
	QN_OUTPUT("Weights loaded from file, min=%g max=%g.",
		  min, max);
    }

    const char* log_weight_file = config.log_weight_file;
    const char* ckpt_weight_file = config.ckpt_weight_file;
    size_t train_chunk_size;	// The number of presentations read
				// at one time.
    size_t mlp3_bunch_size = config.mlp3_bunch_size;
    if (mlp3_bunch_size>1)
    {
	train_chunk_size = mlp3_bunch_size;
    }
    else
	train_chunk_size = 16;	// By default, use a size of 16.
    unsigned long ckpt_seconds = config.ckpt_hours * 3600;
//    unsigned long ckpt_seconds = (unsigned long) config.ckpt_hours;
    if (strcmp(ckpt_weight_file, "")==0)
    {
	QN_ERROR(NULL, "ckpt_hours is non-zero but ckpt_weight_file is NULL");
    }
    if (hardtarget_train_str!=NULL)
    {
	assert(hardtarget_cv_str!=NULL);
	QN_HardSentTrainer* trainer =
	    new QN_HardSentTrainer(debug,               // Debugging level.
				   "trainer",           // Debugging tag.
				   verbose,	            // Verbosity level.
				   mlp,                 // MLP.
				   ftrfile_train_str,   // Training ftr strm.
				   hardtarget_train_str, // Training label str.
				   ftrfile_cv_str,      // CV feature stream.
				   hardtarget_cv_str,   // CV label stream.
				   lr_schedule,	    // Learning rate scheduler.
				   0.0,		    // Low target.
				   1.0,		    // High target.
				   log_weight_file, // Where we log weights.
				   QN_WEIGHTFILE_RAP3, // Format of wght file.
				   ckpt_weight_file,  // Where we checkpoint.
				   QN_WEIGHTFILE_RAP3, // Format of ckpt file.
				   ckpt_seconds,       // Time in seconds
				   train_chunk_size, // Batch size.
				   lastlab_reject   // Allow untrainable frames
			       );
	trainer->train();
	delete trainer;
    }
    else
    {
	assert(softtarget_train_str!=NULL);
	assert(softtarget_cv_str!=NULL);

	QN_SoftSentTrainer* trainer =
	    new QN_SoftSentTrainer(debug,               // Debugging level.
				   "trainer",           // Debugging tag.
				   verbose,	            // Verbosity level.
				   mlp,                 // MLP.
				   ftrfile_train_str,   // Training ftr strm.
				   softtarget_train_str, // Training label str.
				   ftrfile_cv_str,      // CV feature stream.
				   softtarget_cv_str,   // CV label stream.
				   lr_schedule,	    // Learning rate scheduler.
				   0.0,		    // Low target.
				   1.0,		    // High target.
				   log_weight_file, // Where we log weights.
				   QN_WEIGHTFILE_RAP3, // Format of wght file.
				   ckpt_weight_file,  // Where we checkpoint.
				   QN_WEIGHTFILE_RAP3, // Format of ckpt file.
				   ckpt_seconds,       // Time in seconds
				   train_chunk_size // Batch size.
			       );
	trainer->train();
	delete trainer;
    }

    if (verbose>0)
    {
	QN_OUTPUT("Starting to write weights...");
    }
    float min, max;
    QN_MLPWeightFile_RAP3 outwfile(debug, out_weight_fp, QN_WRITE,
				   out_weight_file,
				   mlp3_input_size, mlp3_hidden_size,
				   mlp3_output_size);
    QN_write_weights(outwfile, *mlp, &min, &max, debug);
    QN_OUTPUT("Weights written to '%s'.", out_weight_file);

// A note for the logfile.
    time(&now);
    QN_OUTPUT("Program stop: %.24s", ctime(&now));
    delete mlp;

    if (out_weight_fp!=NULL)
	QN_close(out_weight_fp);
    if (init_weight_fp!=NULL)
	QN_close(init_weight_fp);
    if (ftr2_norm_fp!=NULL)
	QN_close(ftr2_norm_fp);
    if (ftr1_norm_fp!=NULL)
	QN_close(ftr1_norm_fp);
    //    if (softtarget_fp!=NULL)
    //	{
    //	QN_close(softtarget_fp);
    //	delete softtarget_buf;
    //    }
    if (hardtarget_fp!=NULL)
    {
	QN_close(hardtarget_fp);
	delete [] hardtarget_buf;
    }
    if (unary_fp!=NULL)
    {
	QN_close(unary_fp);
    }
    //    if (ftr2_fp!=NULL)
    //    {
    //	QN_close(ftr2_fp);
    //	delete ftr2_buf;
    //    }
    //    QN_close(ftr1_fp);
    //    delete ftr1_buf;
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

    // Seed the random number generator.
    srand48(config.init_random_seed);

    log_fp = QN_open(config.log_file, "w");
#ifdef QN_HAVE_SETVBUF
    assert(setvbuf(log_fp, log_buf, _IOLBF, sizeof(log_buf))==0);
#endif

    QN_printargs(log_fp, progname, &argtab[0]);
    QN_logger = new QN_Logger_Simple(log_fp, stderr, progname);
    qn_io_debug = config.debug;

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

    qnstrn();

    exit(EXIT_SUCCESS);
}
