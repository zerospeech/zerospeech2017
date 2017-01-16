#ifndef NO_RCSID
const char* qncopywts_rcsid = 
    "$Header: /u/drspeech/repos/quicknet2/qncopywts.cc,v 1.11 2010/10/29 02:20:38 davidj Exp $";
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


#ifndef FILENAME_MAX
#define FILENAME_MAX (MAXPATHLEN)
#endif

#include "QuickNet.h"

static struct {
    const char* in_file;		// Input weights file.
    const char* in_format;		// Input weights file format.
    const char* out_file;		// Output weights file.
    const char* out_format;		// Output weights file format.
    const char* log_file;		// Stream for output data.
    int verbose;		// 1 if verbose.
    int debug;			// Debug level.
} config;

void
set_defaults(void)
{
    config.in_file = "";
    config.in_format = "rap3";
    config.out_file = "";
    config.out_format = "rap3";
    config.log_file = "-";
    config.verbose = 1;
    config.debug = 0;
}

QN_ArgEntry argtab[] =
{
{ NULL, "QuickNet MLP weight file copier version " QN_VERSION, QN_ARG_DESC },
{ "in_file", "Input weights file", QN_ARG_STR,
  &(config.in_file), QN_ARG_REQ },
{ "in_format", "Input weights file format", QN_ARG_STR, 
  &(config.in_format) }, 
{ "out_file", "Output weights file", QN_ARG_STR,
  &(config.out_file) },
{ "out_format", "Output wights file format", QN_ARG_STR, 
  &(config.out_format) }, 
{ "debug", "Output additional diagnostic information",
  QN_ARG_INT, &(config.debug) },
{ "verbose", "Output diagnostic information",
  QN_ARG_BOOL, &(config.verbose) },
{ NULL, NULL, QN_ARG_NOMOREARGS }
};



void
qncopywts(FILE* log_fp)
{
    const int debug = config.debug;
    const int verbose = config.verbose;
    const char* in_file = config.in_file;
    const char* out_file = config.out_file;
    FILE* in_file_fp;
    FILE* out_file_fp = NULL;
    QN_MLPWeightFile* in_wf;
    QN_MLPWeightFile* out_wf = NULL;
    QN_MLP* mlp; 		// MLP to put the weights in.
    const QN_OutputLayerType layer_type = QN_OUTPUT_SIGMOID;
    size_t i;
    float min, max;		// Weight limits.

    if (strcmp(in_file, "")!=0)
	in_file_fp = QN_open(in_file, "r");
    else
	QN_ERROR(NULL, "No input weight file specified");

    const char* in_format = config.in_format;
    if (verbose)
	QN_OUTPUT("Loading weights...");
    if (strcasecmp(in_format, "rap3")==0)
    {
	in_wf = new QN_MLPWeightFile_RAP3(debug, in_file_fp,
					  QN_READ,"in_file", 
					  0, 0, 0);
    }
    else if (strcasecmp(in_format, "matlab")==0)
    {
	in_wf = new QN_MLPWeightFile_Matlab(debug, "in_file", in_file_fp,
					    QN_READ, 0, NULL);
    }
    else
	QN_ERROR(NULL, "Unknown input weight file format %s.", in_format);
    size_t in_layers = in_wf->num_layers();
    size_t in_layer_size[QN_MLP_MAX_LAYERS];
    for (i = 0; i<in_layers; i++)
	in_layer_size[i] = in_wf->size_layer((QN_LayerSelector) i);

    if (strcmp(out_file, "")!=0)
    {
	const char* out_format = config.out_format;

	out_file_fp = QN_open(out_file, "w");
	if (strcasecmp(out_format, "rap3")==0)
	{
	    if (in_layers!=3)
	    {
		QN_ERROR(NULL, "Input weight file has %lu layers but "
			 "RAP3 output file requires 3 layers.",
			 (unsigned long) in_layers);

	    }
	    else
	    {
		out_wf = new QN_MLPWeightFile_RAP3(debug, out_file_fp,
						   QN_WRITE,"out_file", 
						   in_layer_size[0],
						   in_layer_size[1],
						   in_layer_size[2]);
	    }
	}
	else if (strcasecmp(out_format, "matlab")==0)
	{
	    out_wf = new QN_MLPWeightFile_Matlab(debug, "out_file",
						 out_file_fp,
						 QN_WRITE, in_layers,
						 in_layer_size);
	}
	else
	    QN_ERROR(NULL, "Unknown output weight file format %s.",
		     out_format);
    }
    else
    {
	out_file_fp = NULL;
	if (verbose)
	    QN_OUTPUT("No output weight file.");
    }
    mlp = new QN_MLP_BunchFlVar(debug, "mlp", in_layers, in_layer_size,
				layer_type, 1);
    QN_read_weights(*in_wf, *mlp, &min, &max, debug, "in_file");
    if (verbose)
    {
	QN_OUTPUT("Weights loaded from '%s', min=%g max=%g.",
		  in_file, (double) min, (double) max);
    }

    if (out_wf!=NULL)
    {
	QN_write_weights(*out_wf, *mlp, &min, &max, debug, "out_file");
	if (verbose)
	{
	    QN_OUTPUT("Weights written to '%s', min=%g max=%g.",
		      out_file, min, max);
	}
    }


    delete mlp;
    if (out_wf!=NULL)
	delete out_wf;
    delete in_wf;
}

#ifdef NEVER
static void
copy_run(FILE* out_file, FILE* logfile, int verbose)
{

    float min, max;

    QN_read_weights(*inwfile, *mlp, &min, &max, debug, log_fp);
    QN_OUTPUT("Weights loaded from '%s', min=%g max=%g.",
	      init_weight_file, min, max);


    QN_write_weights(*outwfile, *mlp, &min, &max, debug, log_fp);
    QN_OUTPUT("Weights written to '%s', min=%g max=%g.",
	      config.owts_filename, min, max);

    delete mlp;

}

#endif

int
main(int argc, const char* argv[])
{
    char* progname;		// The name of the program - set by QN_args.
    FILE* log_fp;

    set_defaults();
    QN_initargs(&argtab[0], &argc, &argv, &progname);

    log_fp = QN_open(config.log_file, "w");

    if (config.verbose)
	QN_printargs(log_fp, progname, &argtab[0]);
    QN_logger = new QN_Logger_Simple(log_fp, stderr, progname);

    if (config.verbose)
	QN_output_sysinfo("qncopywts");
    qncopywts(log_fp);

    delete QN_logger;
    exit(EXIT_SUCCESS);
}
