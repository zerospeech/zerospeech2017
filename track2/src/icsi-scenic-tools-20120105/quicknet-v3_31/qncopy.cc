#ifndef NO_RCSID
const char* qncopy_rcsid = 
    "$Header: /u/drspeech/repos/quicknet2/qncopy.cc,v 1.11 2010/10/29 02:20:38 davidj Exp $";
#endif

/* Must include the config.h file first */
#include <QN_config.h>
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


#ifndef FILENAME_MAX
#define FILENAME_MAX (MAXPATHLEN)
#endif

#include "QuickNet.h"

static struct {
    const char* ftr_filename;
    const char* ftr_format;
    int ftr_width;
    const char* ftr_norm_file;
    const char* outftr_filename;
    int delta_win;		// length of delta window
    int delta_order;		// order of deltas to add
    int first_ftr;		// first feature to filter
    int num_ftrs;		// how many features to filter
    int first_sent;		// First sentence to process
    int num_sents;		// Number of sentences to process
    const char *sent_range;	        // arbitrary sentence specification
    const char *norm_mode_str;	// token from command line for norm_mode
    int norm_mode;		// Normalize from file, per utt, or online?
    double norm_am;		// time const for onlnorm means
    double norm_av;		// time const for onlnorm vars
    int indexedop;		// Build an index in the output file?
    int dbg;			// Debug level.
    int verbose;
} config;

void
set_defaults(void)
{
    config.ftr_filename = NULL;
    config.ftr_format = "pfile";
    config.ftr_width = 0;
    config.ftr_norm_file = NULL;
    config.outftr_filename = "-";
    config.delta_win = 9;
    config.delta_order = 0;	// by default, don't add deltas
    config.first_ftr = 0;
    config.num_ftrs= 0;		// default to all ftrs if not spec'd
    config.first_sent = 0;	// First sentence to recognize.
    config.num_sents = INT_MAX;	// Number of sentences to recognize.
    config.sent_range = 0;
    config.norm_mode_str = NULL;
    config.norm_mode = QN_NORM_FILE;
    config.norm_am = QN_DFLT_NORM_AM;
    config.norm_av = QN_DFLT_NORM_AV;
    config.indexedop = 1;
    config.dbg = 0;
    config.verbose = 1;
}

QN_ArgEntry argtab[] =
{
{ NULL, "QuickNet speech feature file copyer version " QN_VERSION, QN_ARG_DESC },
{ "ftr_file", "Input feature file", QN_ARG_STR,
  &(config.ftr_filename), QN_ARG_REQ },
{ "ftr_format", "Input file format", QN_ARG_STR, 
  &(config.ftr_format) }, 
{ "ftr_width", "Input file width (for pre, lna)", QN_ARG_INT, 
  &(config.ftr_width) },
{ "ftr_norm_file", "Normalize features from norms file", QN_ARG_STR, 
  &(config.ftr_norm_file) },
{ "outftr_file", "Output feature file", QN_ARG_STR,
  &(config.outftr_filename) },
{ "delta_win", "Length of delta-filter", QN_ARG_INT, 
  &(config.delta_win) },
{ "delta_order", "Add delta features to what degree?", QN_ARG_INT, 
  &(config.delta_order) },
{ "ftr_start", "First feature column (of net stream) to write", 
  QN_ARG_INT, &(config.first_ftr) },
{ "ftr_count", "Number of feature columns to write",
  QN_ARG_INT, &(config.num_ftrs) },
{ "sent_start", "Number of first sentence",
  QN_ARG_INT, &(config.first_sent) },
{ "sent_count", "Number of sentences",
  QN_ARG_INT, &(config.num_sents) },
{ "sent_range", "Specify sentences to use as a QN_Range(3) token", 
  QN_ARG_STR, &(config.sent_range) },
{ "norm_mode", "Mode for normalization - file/utt/online", 
  QN_ARG_STR, &(config.norm_mode_str) },
{ "norm_alpha_m", "Update constant for online norm means", 
  QN_ARG_DOUBLE, &(config.norm_am) },
{ "norm_alpha_v", "Update constant for online norm vars", 
  QN_ARG_DOUBLE, &(config.norm_av) },
{ "indexedop", "Build an index for the output file?",
  QN_ARG_INT, &(config.indexedop) },
{ "debug", "Output additional diagnostic information",
  QN_ARG_INT, &(config.dbg) },
{ "verbose", "Output diagnostic information",
  QN_ARG_BOOL, &(config.verbose) },
{ NULL, NULL, QN_ARG_NOMOREARGS }
};


static void
copy_run(FILE* out_file, FILE* logfile, int verbose)
{
    if (verbose>0) {
	// A note for the logfile.
        time_t now = time(NULL);
	fprintf(logfile, "\nProgram version: qncopy %s.\n", QN_VERSION);
#ifdef QN_HAVE_GETHOSTNAME
	char hostname[MAXHOSTNAMELEN];
	assert(gethostname(hostname, MAXHOSTNAMELEN)==0);
	fprintf(logfile, "Host: %s\n", hostname);
#endif
	fprintf(logfile, "Program start: %s", ctime(&now));
    }
    if (verbose>0) {
	fprintf(logfile, "Opening feature file...\n");
    }

    int indexed = (config.sent_range==0)?0:1;	// has to be indexed to cut
    int buffer_frames = 500;
    size_t num_sents;
    // Command line arguments use INT_MAX for "all sentences", streams
    // use QN_ALL, which has a different value.
    if (config.num_sents==INT_MAX) {
	num_sents = QN_ALL;
    } else {
	num_sents = config.num_sents;
    }
    FILE *norm_file = NULL;
    if (config.ftr_norm_file != NULL) {
	norm_file = fopen(config.ftr_norm_file, "r");
	if (norm_file == NULL) {
	    QN_ERROR(NULL, "failed to open norm file %s for reading.",
		     config.ftr_norm_file);
	}
    }    

    QN_InFtrStream* ftr_str = QN_build_ftrstream(config.dbg, "ftrfile", 
				config.ftr_filename, config.ftr_format, 
				config.ftr_width, indexed, 
				norm_file, 
				config.first_ftr, config.num_ftrs, 
				config.first_sent, num_sents, buffer_frames,
				config.delta_order, config.delta_win, 
				config.norm_mode, config.norm_am, 
				config.norm_av);

    if (norm_file)	fclose(norm_file);

    // Select sentences we need.
    if (config.sent_range) {
	QN_InFtrStream_CutRange* fwd_ftr_str = 
	    new QN_InFtrStream_CutRange(config.dbg, "ftrfile",
					*ftr_str, config.sent_range);
	//	QN_InFtrStream_Cut* fwd_ftr_str = 
	//            new QN_InFtrStream_Cut(config.dbg, "ftrfile",
	//				  *ftr_str, 1, 10);
	ftr_str = (QN_InFtrStream*)fwd_ftr_str;
    }


    const size_t n_ftrs = ftr_str->num_ftrs();

    if (verbose>0) {
	fprintf(logfile, "Data will have %lu feature(s).\n",
		(unsigned long) n_ftrs);
        fprintf(logfile, "Copying features...\n");
    }

    int buffrms = 1000;
    float *ftrbuf = new float[buffrms * n_ftrs];

    int tot_utts = 0;
    int tot_frms = 0;

    // Create the output stream
    int n_labs = 0;	// cannot copy labels to output - so write none
    QN_OutFtrStream *outpf;
    outpf = new QN_OutFtrLabStream_PFile(config.dbg, "outpfile", out_file, 
					 n_ftrs, n_labs, config.indexedop);
    QN_SegID segid;
    while( (segid = ftr_str->nextseg()) != QN_SEGID_BAD) {
	size_t nred;
	while( (nred =ftr_str->read_ftrs(buffrms, ftrbuf)) > 0) {
	    outpf->write_ftrs(nred, ftrbuf);
	    tot_frms += nred;
	}
	outpf->doneseg(segid);
	++tot_utts;
    }

    // Close the output file
    delete outpf;

    delete ftr_str;

    if (verbose) {
	fprintf(logfile, "%d utts (%d frames) written: %s\n", 
		tot_utts, tot_frms, config.outftr_filename);
    }
    if (verbose) {
	// A note for the logfile.
        time_t now = time(NULL);
	fprintf(logfile, "Program stop: %s", ctime(&now));
    }
}

int
main(int argc, const char* argv[])
{
    char* progname;		// The name of the program - set by QN_args.
    FILE* out;
    FILE* logfile;
    char logbuf[160];

    set_defaults();
    QN_initargs(&argtab[0], &argc, &argv, &progname);

    // map norm_mode_str to val
    config.norm_mode = QN_string_to_norm_const(config.norm_mode_str);

    // Check argument sanity
    if (config.delta_order > 0) {
	if (config.delta_order > 2) {
	    fprintf(stderr, "qncopy: delta_order %d must be between 0 and 2\n", 
		    config.delta_order);
	    exit(-1);
	}
	if (config.delta_win < 3 || (config.delta_win & 1) == 0) {
	    fprintf(stderr, "qncopy: delta_window %d must be odd and > 1\n",
		    config.delta_win);
	    exit(-1);
	}
    }

    logfile = stderr;

    // Enable line buffering on log file.
    // *** Note: there is a bug with newlib-1.6.1 (as used on SPERT) that
    // *** makes setvbuf fail if the second argument is NULL.
    if (setvbuf(logfile, logbuf, _IOLBF, sizeof(logbuf)))
	abort();		// Fail on error
    if (config.verbose) {
	QN_printargs(logfile, progname, &argtab[0]);
	QN_logger = new QN_Logger_Simple(logfile, stderr, progname);
    } else {
	logfile = NULL;
	QN_logger = new QN_Logger_Simple(logfile, stderr, progname);
    }
    // Ensure the lines are sent to the logfile as soon as they are printed.

    out = fopen(config.outftr_filename, "w");
    if (out==NULL) {
	QN_ERROR(NULL, "failed to open ftr file %s for writing.",
		 config.outftr_filename);
    }

    // Do the transfer
    copy_run(out, logfile, config.verbose);

    if (fclose(out)) {
	QN_ERROR(NULL, "failed to close output file %s.",
		 config.outftr_filename);
    }
    QN_close_ftrfiles();

    exit(EXIT_SUCCESS);
}
