#ifndef NO_RCSID
const char* qnnorm_rcsid = 
    "$Header: /u/drspeech/repos/quicknet2/qnnorm.cc,v 1.46 2010/10/29 02:20:38 davidj Exp $";
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
    char* ftr_filename;
    const char* ftr_format;
    int ftr_width;
    int delta_order;
    int delta_win;
    const char* outnorms_filename;
    int first_sent;		// First sentence to recognize.
    int num_sents;		// Number of sentences to recognize.
    int dbg;			// Debug level.
    int verbose;
} config;

void
set_defaults(void)
{
    config.ftr_filename = NULL;
    config.ftr_format = "pfile";
    config.ftr_width = 0;
    config.delta_order = 0;
    config.delta_win = 9;
    config.outnorms_filename = "-";
    config.first_sent = 0;	// First sentence to recognize.
    config.num_sents = INT_MAX;	// Number of sentences to recognize.
    config.dbg = 0;
    config.verbose = 1;
}

QN_ArgEntry argtab[] =
{
{ NULL, "QuickNet Speech feature file normalizer version " QN_VERSION, QN_ARG_DESC },
{ "norm_ftrfile", "Input feature file", QN_ARG_STR,
  &(config.ftr_filename), QN_ARG_REQ },
{ "ftr_format", "Input file format", QN_ARG_STR, 
  &(config.ftr_format) }, 
{ "ftr_width", "Input file width (for pre, lna)", QN_ARG_INT, 
  &(config.ftr_width) },
{ "delta_order", "Degree for online delta calculation", QN_ARG_INT, 
  &(config.delta_order) },
{ "delta_win", "Window length for online delta calculation", QN_ARG_INT, 
  &(config.delta_win) },
{ "output_normfile", "Output normalization file", QN_ARG_STR,
  &(config.outnorms_filename) },
{ "first_sent", "Number of first sentence",
  QN_ARG_INT, &(config.first_sent) },
{ "num_sents", "Number of sentences",
  QN_ARG_INT, &(config.num_sents) },
{ "debug", "Output additional diagnostic information",
  QN_ARG_INT, &(config.dbg) },
{ "verbose", "Output diagnostic information",
  QN_ARG_BOOL, &(config.verbose) },
{ NULL, NULL, QN_ARG_NOMOREARGS }
};


static void
norm_run(char* ftr_filename, FILE* out_norms, FILE* logfile, int verbose)
{
    if (verbose>0)              // A note for the logfile.
    {
        time_t now = time(NULL);
	fprintf(logfile, "\nProgram version: qnnorm %s.\n", QN_VERSION);
#ifdef QN_HAVE_GETHOSTNAME
	char hostname[MAXHOSTNAMELEN];
	assert(gethostname(hostname, MAXHOSTNAMELEN)==0);
	fprintf(logfile, "Host: %s\n", hostname);
#endif
	fprintf(logfile, "Program start: %s", ctime(&now));
    }
    if (verbose>0)
    {
	fprintf(logfile, "Opening feature file...\n");
    }

    int indexed = 0;
    int buffer_frames = 500;
    size_t num_sents;
    // Command line arguments use INT_MAX for "all sentences", streams
    // use QN_ALL, which has a different value.
    if (config.num_sents==INT_MAX) {
	num_sents = QN_ALL;
    } else {
	num_sents = config.num_sents;
    }
    QN_InFtrStream* ftr_str = QN_build_ftrstream(config.dbg, "ftrfile", 
				ftr_filename, config.ftr_format, 
				config.ftr_width, indexed, 
				NULL, 0, 0, 
				config.first_sent, num_sents, buffer_frames,
				config.delta_order, config.delta_win, 
				QN_NORM_FILE, 0, 0);

    const size_t n_ftrs = ftr_str->num_ftrs();

    if (verbose>0)
    {
		fprintf(logfile, "File contains %lu feature(s).\n",
				(unsigned long) n_ftrs);
        fprintf(logfile, "Normalizing features.\n");
    }

    float *const ftr_means = new float[n_ftrs];
    float *const ftr_sdevs = new float[n_ftrs];
//    float *const ftr_mins = new float[n_ftrs];
//    float *const ftr_maxs = new float[n_ftrs];

    QN_ftrstats(config.dbg,	// Debug level
				*ftr_str,	// Stream to analyze.
				ftr_means,	// Where to put means.
				ftr_sdevs	// Where to put standard deviations.
		);
    QN_write_norms_rap(out_norms, n_ftrs, ftr_means, ftr_sdevs);

    delete ftr_sdevs;
    delete ftr_means;
    delete ftr_str;
//    delete ftr_mins;
//    delete ftr_maxs;
    
    if (verbose)
    {
		fprintf(logfile, "Norms file written: %s\n", config.outnorms_filename);
    }
    if (verbose)                // A note for the logfile.
    {
        time_t now = time(NULL);
		fprintf(logfile, "Program stop: %s", ctime(&now));
    }
}

int
main(int argc, const char* argv[])
{
    char* progname;		// The name of the program - set by QN_args.
    //    FILE* db;
    FILE* outnorms;
    FILE* logfile;
    char logbuf[160];		// A buffer for logging messages.
    int stdout_is_normfile;	// 1 if stdout is used for norm output.

    set_defaults();
    QN_initargs(&argtab[0], &argc, &argv, &progname);

    if (strcmp(config.outnorms_filename, "-")==0
	|| strcmp(config.outnorms_filename, "")==0)
    {
	stdout_is_normfile = 1;
	logfile = stderr;
    }
    else
    {
	stdout_is_normfile = 0;
	logfile = stdout;
    }
    // Enable line buffering on log file.
    // *** Note: there is a bug with newlib-1.6.1 (as used on SPERT) that
    // *** makes setvbuf fail if the second argument is NULL.
    if (setvbuf(logfile, logbuf, _IOLBF, sizeof(logbuf)))
	abort();		// Fail on error
    if (config.verbose)
    {
	QN_printargs(logfile, progname, &argtab[0]);
	QN_logger = new QN_Logger_Simple(logfile, stderr, progname);
    }
    else
    {
	logfile = NULL;
	QN_logger = new QN_Logger_Simple(logfile, stderr, progname);
    }

    // Ensure the lines are sent to the logfile as soon as they are printed.

//    db = fopen(config.ftr_filename, "r");
//    if (db==NULL)
//    {
//	QN_ERROR(NULL, "failed to open feature database %s.",
//		 config.ftr_filename);
//    }
    if (stdout_is_normfile)
    {
	outnorms = stdout;
    }
    else
    {
	outnorms = QN_open(config.outnorms_filename, "w", 0,
			    "outnorms_filename");
	if (outnorms==NULL)
	{
	    QN_ERROR(NULL, "failed to open norms file %s for writing.",
		     config.outnorms_filename);
	}
    }
    norm_run(config.ftr_filename, outnorms, logfile, config.verbose);
    if (!stdout_is_normfile)
	QN_close(outnorms);

    QN_close_ftrfiles();

    exit(EXIT_SUCCESS);
}
