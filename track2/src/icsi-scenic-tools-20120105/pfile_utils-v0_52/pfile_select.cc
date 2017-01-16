/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_select.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
    This program selects an arbitrary subset set of features from each
    frame of a pfile and creates a new pfile with that
    subset in each frame.
    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "parse_subset.h"
#include "error.h"
#include "Range.H"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (1)
#endif

#define MAXHISTBINS 1000


#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;


extern size_t bin_search(float *array,
			 size_t length, // the length of the array
			 float val);     // the value to search for.



static void
usage(const char* message = 0)
{
    if (message)
        fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help           print this message\n"
	    "-i <file-name>  input pfile\n"
	    "-o <file-name>  output pfile \n"
	    "-sr <range>     sentence range\n"
	    "-fr <range>     feature range\n"
	    "-pr <range>     per-sentence range\n"
	    "-lr <range>     label range\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


static void
pfile_select(QN_InFtrLabStream& in_stream,
	     QN_OutFtrLabStream& out_stream,
	     Range& srrng,
	     Range& frrng,
	     Range& lrrng,
	     const char *pr_str)
{
    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const size_t n_ftrs = in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    float *ftr_buf_p;
    float *oftr_buf = new float[buf_size * frrng.length()];
    float *oftr_buf_p;

    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];
    QNUInt32* lab_buf_p;    
    QNUInt32* olab_buf = new QNUInt32[buf_size * n_labs];
    QNUInt32* olab_buf_p;    


    //
    // Go through input pfile to get the initial statistics,
    // i.e., max, min, mean, std, etc.
    for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
        const size_t n_frames = in_stream.num_frames((*srit));

	Range prrng(pr_str,0,n_frames);

	if ((*srit) % 100 == 0)
	  printf("Processing sentence %d\n",(*srit));

        if (n_frames == QN_SIZET_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't find number of frames "
                   "at sentence %lu in input pfile.\n",
                   program_name, (unsigned long) (*srit));
            error("Aborting.");
        }

        // Increase size of buffers if needed.
        if (n_frames > buf_size)
        {
            // Free old buffers.
            delete ftr_buf;
            delete oftr_buf;
            delete lab_buf;
            delete olab_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = n_frames * 2;

            // Allocate new larger buffers.
            ftr_buf = new float[buf_size * n_ftrs];
            oftr_buf = new float[buf_size * frrng.length()];
            lab_buf = new QNUInt32[buf_size * n_labs];
            olab_buf = new QNUInt32[buf_size * n_labs];
        }

        const QN_SegID seg_id = in_stream.set_pos((*srit), 0);

        if (seg_id == QN_SEGID_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't seek to start of sentence %lu "
                   "in input pfile.",
                   program_name, (unsigned long) (*srit));
            error("Aborting.");
        }

        const size_t n_read =
            in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);

        if (n_read != n_frames)
        {
            fprintf(stderr, "%s: At sentence %lu in input pfile, "
                   "only read %lu frames when should have read %lu.\n",
                   program_name, (unsigned long) (*srit),
                   (unsigned long) n_read, (unsigned long) n_frames);
            error("Aborting.");
        }

	oftr_buf_p = oftr_buf;
	olab_buf_p = olab_buf;
	for (Range::iterator prit=prrng.begin();
	     !prit.at_end(); ++prit) {
	  ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	  lab_buf_p = lab_buf + (*prit)*n_labs;
	  for (Range::iterator frit=frrng.begin();
	       !frit.at_end(); ++frit) {
	    *oftr_buf_p++ = ftr_buf_p[*frit];
	  }
	  for (Range::iterator lrit=lrrng.begin();
	       !lrit.at_end(); ++lrit) {
	    *olab_buf_p++ = lab_buf_p[*lrit];
	  }
	  // Count the number of frames actually placed into
	  // the new pfile which might be less than prrng.length();
	}

	// Write output.
	out_stream.write_ftrslabs(prrng.length(), oftr_buf, olab_buf);
	out_stream.doneseg((QN_SegID) seg_id);

    }


    delete ftr_buf;
    delete lab_buf;
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


main(int argc, const char *argv[])
{
    //////////////////////////////////////////////////////////////////////
    // TODO: Argument parsing should be replaced by something like
    // ProgramInfo one day, with each extract box grabbing the pieces it
    // needs.
    //////////////////////////////////////////////////////////////////////

    const char *input_fname = 0;  // Input pfile name.
    const char *output_fname = 0; // Output pfile name.

    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string    
    Range *fr_rng;
    const char *lr_str = 0;   // label range string    
    Range *lr_rng;
    const char *pr_str = 0;   // per-sentence range string    
    int debug_level = 0;

    program_name = *argv++;
    argc--;

    while (argc--)
    {
        char buf[BUFSIZ];
        const char *argp = *argv++;

        if (strcmp(argp, "-help")==0)
        {
            usage();
        }
        else if (strcmp(argp, "-i")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input_fname = *argv++;
                argc--;
            }
            else
                usage("No input filename given.");
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
	  sprintf(buf,"Unrecognized argument (%s).",argp);
	  usage(buf);
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    if (input_fname==0)
        usage("No input pfile name supplied.");
    FILE *in_fp = fopen(input_fname, "r");
    if (in_fp==NULL)
        error("Couldn't open input pfile for reading.");
    FILE *out_fp;
    if (output_fname==0 || !strcmp(output_fname,"-"))
      out_fp = stdout;
    else {
      if ((out_fp = fopen(output_fname, "w")) == NULL) {
	error("Couldn't open output file for writing.");
      }
    }



    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);


    sr_rng = new Range(sr_str,0,in_streamp->num_segs());
    fr_rng = new Range(fr_str,0,in_streamp->num_ftrs());
    lr_rng = new Range(lr_str,0,in_streamp->num_labs());

    QN_OutFtrLabStream* out_streamp
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "",
                                       out_fp,
				       fr_rng->length(),
				       lr_rng->length(),
                                       1);


    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_select(*in_streamp,
		 *out_streamp,
		 *sr_rng,*fr_rng,*lr_rng,pr_str);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in_streamp;
    delete out_streamp;
    delete sr_rng;
    delete fr_rng;
    delete lr_rng;

    if (fclose(in_fp))
        error("Couldn't close input pfile.");
    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
