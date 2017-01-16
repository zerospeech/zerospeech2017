/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_concat.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
    This program simply reads one or more pfiles and writes out 
    all their frames, end to end, in a new longer pfile. 
    It is based on pfile_merge, which does the same thing except 
    side-by-side instead of end-to-end.

    1998may07 dpwe@icsi.berkeley.edu
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "parse_subset.h"
#include "error.h"
#include "Range.H"

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;

static void
usage(const char* message = 0)
{
    if (message)
        fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options> pfile_in1 [pfile_in2 ...]\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help           print this message\n"
	    "-o <file-name>  output pfile (required)\n"
	    "-sr <range>     sentence range (in each pfile)\n"
	    "-fr <range>     range of features selected from each pfile\n"
	    "-pr <range>     per-sentence frame range\n"
	    "-lr <range>     label range\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
	    "-q              quiet mode\n"
    );
    exit(EXIT_FAILURE);
}


static void
pfile_concat(const char *output_fname, 
             const char *sr_str, 
	     const char *fr_str, 
	     const char *lr_str, 
	     const char *pr_str, 
	     const char *input_fnames[],
	     int n_input_fnames, 
	     const int debug_level, 
	     const bool quiet)

{
    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    char errmsg[1024];

    // pre-open the first input file so we can check its size
    const char *pfile_name;
    assert(n_input_fnames > 0);
    pfile_name = input_fnames[0];
    FILE *in_fp = fopen(pfile_name, "r");
    if (in_fp==NULL) {
	sprintf(errmsg, "Couldn't open input pfile %s for reading.",pfile_name);
	error(errmsg);
    }
    QN_InFtrLabStream* in_stream
        = new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);

    // Now we know the input feature size, construct the ranges
    Range *fr_rng = new Range(fr_str,0,in_stream->num_ftrs());
    Range *lr_rng = new Range(lr_str,0,in_stream->num_labs());

    // I'd like to reconstruct the ranges for every pfile, but 
    // the size corresponding to a given label range spec string 
    // can vary for different input pfiles, so it wouldn't (always)
    // work.  Jeff wants the ability to specify different range 
    // specs for each pfile in the list, but the syntax (as well as the 
    // parsing) gets clunky.

    // .. so, knowing fr_rng->length() etc, we can create the output pfile
    FILE *out_fp = fopen(output_fname, "w");
    if (out_fp==NULL) {
	error("Couldn't open pfile %s for writing.", output_fname);
    }
    QN_OutFtrLabStream* out_stream
        = new QN_OutFtrLabStream_PFile(debug_level, "", out_fp,
				       fr_rng->length(), lr_rng->length(), 1);

    const size_t n_labs = in_stream->num_labs();
    const size_t n_ftrs = in_stream->num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    float *oftr_buf = new float[buf_size * fr_rng->length()];
    float *ftr_buf_p, *oftr_buf_p;

    QNUInt32 *lab_buf = new QNUInt32[buf_size * n_labs];
    QNUInt32 *olab_buf = new QNUInt32[buf_size * lr_rng->length() ];
    QNUInt32 *lab_buf_p, *olab_buf_p;

    int out_seg_id = 1;	// don't start at zero ??? for outstream.doneseg()

    // Outer loop around input filenames
    int file_ix;
    for (file_ix = 0; file_ix < n_input_fnames; ++file_ix) {
	pfile_name = input_fnames[file_ix];
	// in_fp actually already open first time into the loop
	if (in_fp == NULL) {
	    in_fp = fopen(pfile_name, "r");
	    if (in_fp==NULL) {
		error("Couldn't open input pfile %s for reading.",pfile_name);
	    }
	    in_stream
		= new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);

	    // Check that this input pfile is the same size as predecessors
	    if (in_stream->num_labs() != n_labs \
		|| in_stream->num_ftrs() != n_ftrs) {
		sprintf(errmsg, "Features/labels of %s (%zu/%zu) don't match first input file (%lu/%lu)", pfile_name, in_stream->num_labs(), in_stream->num_ftrs(), n_labs, n_ftrs);
		error(errmsg);
	    }
	}

	// create the sentence-range iterator specifically for this 
	// input file
	Range sr_rng(sr_str,0,in_stream->num_segs());

	for (Range::iterator srit=sr_rng.begin();!srit.at_end();srit++) {

	    const size_t n_frames = in_stream->num_frames((*srit));

	    if (!quiet && (*srit) % 100 == 0)
		printf("Processing sentence %d of file %s\n",
		       (*srit), pfile_name);

	    if (n_frames == QN_SIZET_BAD)
		{
		    sprintf(errmsg,
			    "%s: Couldn't find number of frames "
			    "at sentence %lu in input pfile %s.",
			    program_name, (unsigned long) (*srit), pfile_name);
		    error(errmsg);
		}

	    Range pr_rng(pr_str,0,n_frames);

	    // Increase size of buffers if needed.
	    if (n_frames > buf_size)
		{
		    // Free old buffers.
		    delete olab_buf;
		    delete lab_buf;
		    delete oftr_buf;
		    delete ftr_buf;

		    // Make twice as big to cut down on future reallocs.
		    buf_size = n_frames * 2;

		    // Allocate new larger buffers.
		    ftr_buf = new float[buf_size * n_ftrs];
		    oftr_buf = new float[buf_size * fr_rng->length()];
		    lab_buf = new QNUInt32[buf_size * n_labs];
		    olab_buf = new QNUInt32[buf_size * lr_rng->length() ];
		}

	    const QN_SegID seg_id = in_stream->set_pos((*srit), 0);
	    if (seg_id == QN_SEGID_BAD)
		{
		    sprintf(errmsg,
			    "%s: Couldn't seek to start of sentence %lu "
			    "in input pfile %s.",
			    program_name, (unsigned long) (*srit), pfile_name);
		    error(errmsg);
		}

	    const size_t n_read =
		in_stream->read_ftrslabs(n_frames, ftr_buf, lab_buf);
	    if (n_read != n_frames)
		{
		    sprintf(errmsg, "%s: At sentence %lu in input pfile %s, "
			    "only read %lu frames when should have read %lu.",
			    program_name, (unsigned long) (*srit), pfile_name, 
			    (unsigned long) n_read, (unsigned long) n_frames);
		    error(errmsg);
		}

	    // construct the output set of features.
	
	    oftr_buf_p = oftr_buf;
	    olab_buf_p = olab_buf;

	    for (Range::iterator prit=pr_rng.begin();
		 !prit.at_end() ; ++prit) {

		ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
		lab_buf_p = lab_buf + (*prit)*n_labs;

		for (Range::iterator frit=fr_rng->begin();
		     !frit.at_end(); ++frit) {
		    *oftr_buf_p++ = ftr_buf_p[*frit];
		}
		for (Range::iterator lrit=lr_rng->begin();
		     !lrit.at_end(); ++lrit) {
		    *olab_buf_p++ = lab_buf_p[*lrit];
		}
	    }
	    // Write output.
	    out_stream->write_ftrslabs(pr_rng.length(), oftr_buf, olab_buf);
	    out_stream->doneseg((QN_SegID) out_seg_id++);
	}

	fclose(in_fp);
	delete in_stream;
	// make sure top of loop knows to open next file
	in_fp = NULL;
    }
    
    // All done; close output
    // must delete pfile object first, because it needs to rewrite
    delete out_stream;
    if (fclose(out_fp))
        error("Couldn't close output file.");
    
    delete lr_rng;
    delete fr_rng;

    delete olab_buf;
    delete lab_buf;
    delete oftr_buf;
    delete ftr_buf;
}


static long
parse_long(const char*const s)
{
    size_t len = strlen(s);
    char *ptr;
    long val;

    val = strtol(s, &ptr, 0);

    if (ptr != (s+len))
        error("Not an integer argument.");

    return val;
}

static float
parse_float(const char*const s)
{
    size_t len = strlen(s);
    char *ptr;
    double val;
    val = strtod(s, &ptr);
    if (ptr != (s+len))
        error("Not an floating point argument.");
    return val;
}

#define MAX_IP_NAMES 1024

main(int argc, const char *argv[])
{
    char errmsg[1024];

    const char *output_fname = 0; // Output pfile name.
    const char *input_fnames[MAX_IP_NAMES];   // array of output fnames

    const char *sr_str = 0;   // sentence range string
    const char *fr_str = 0;   // feature range string
    const char *lr_str = 0;   // label range string    
    const char *pr_str = 0;   // per-sentence range string

    int debug_level = 0;
    bool quiet = 0;

    int n_input_fnames = 0;

    program_name = *argv++;
    argc--;

    while (argc--)
    {
        const char *argp = *argv++;

        if (strcmp(argp, "-help")==0)
        {
            usage();
        }
        else if (strcmp(argp, "-o")==0)
        {
            // Output file name.
            if (argc>0)
            {
                // Next argument is input file name.
                output_fname = *argv++;
                argc--;
            }
            else
                usage("No output filename given.");
        }
        else if (strcmp(argp, "-debug")==0)
        {
            if (argc>0)
            {
                // Next argument is debug level.
                debug_level = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No debug level given.");
        }
        else if (strcmp(argp, "-q")==0)
        {
            quiet = true;
        }
        else if (strcmp(argp, "-sr")==0)
        {
            if (argc>0)
            {
	      sr_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-fr")==0)
        {
            if (argc>0)
            {
	      fr_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-pr")==0)
        {
            if (argc>0)
            {
	      pr_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-lr")==0)
        {
            if (argc>0)
            {
	      lr_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else {
	    //sprintf(errmsg,"Unrecognized argument (%s).",argp);
	    //usage(errmsg);
	    // All remaining args are taken as input pfiles
	    // But skip over -i without complaining
	    if (strcmp(argp, "-i")==0) {
		if (argc>0) {
		    argp = *argv++;
		    argc--;
		} else {
		    usage("No filename given (to redundant -i flag).");
		}
	    }

	    if (n_input_fnames >= MAX_IP_NAMES) {
		sprintf(errmsg, "more than %d input pfiles specified", 
			MAX_IP_NAMES);
		usage(errmsg);
	    }
	    // just record a pointer to this part of argv
	    input_fnames[n_input_fnames++] = argp;
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    if (n_input_fnames==0) {
        usage("No input pfiles supplied.");
    }

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////
    
    // done in subroutine now, after we open the first input file

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_concat(output_fname, 
		 sr_str, fr_str, lr_str, pr_str, 
		 input_fnames, n_input_fnames, 
		 debug_level, quiet);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    return EXIT_SUCCESS;
}
