/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_merge.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
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
	    "-i1 <file-name> first input pfile\n"
	    "-i2 <file-name> second input pfile\n"
	    "-o <file-name>  output pfile \n"
	    "-sr <range>     sentence range\n"
	    "-fr1 range      range of features selected from first pfile\n"
	    "-fr2 range      range of features selected from second pfile\n"
	    "-pr <range>     per-sentence frame range\n"
	    "-lr <range>     label range\n"
	    "-l #            take labels from first (1, default) or second (2) pfile\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


static void
pfile_merge(QN_InFtrLabStream& in1_stream,
	    QN_InFtrLabStream& in2_stream,
	    QN_OutFtrLabStream& out_stream,
	    Range& srrng,
	    Range& fr1rng,
	    Range& fr2rng,	    
	    Range& lrrng,
	    const char*pr_str,
	    const size_t labels_from)

{

    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n1_labs = in1_stream.num_labs();
    const size_t n2_labs = in2_stream.num_labs();
    const size_t n1_ftrs = in1_stream.num_ftrs();
    const size_t n2_ftrs = in2_stream.num_ftrs();

    float *ftr1_buf = new float[buf_size * n1_ftrs];
    float *ftr2_buf = new float[buf_size * n2_ftrs];
    float *ftr1_buf_p;
    float *ftr2_buf_p;

    float *oftr_buf = new float[buf_size * (fr1rng.length()+fr2rng.length())];
    float *oftr_buf_p;

    QNUInt32* lab1_buf = new QNUInt32[buf_size * n1_labs];
    QNUInt32* lab2_buf = new QNUInt32[buf_size * n2_labs];
    QNUInt32* lab_buf_p;

    QNUInt32* olab_buf = new QNUInt32[buf_size * lrrng.length() ];
    QNUInt32* olab_buf_p;



    // Go through input pfile to get the initial statistics,
    // i.e., max, min, mean, std, etc.
    for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {

        const size_t n1_frames = in1_stream.num_frames((*srit));
        const size_t n2_frames = in2_stream.num_frames((*srit));

	if ((*srit) % 100 == 0)
	  printf("Processing sentence %d\n",(*srit));

        if (n1_frames == QN_SIZET_BAD || n2_frames == QN_SIZET_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't find number of frames "
                   "at sentence %lu in input pfile.\n",
                   program_name, (unsigned long) (*srit));
            error("Aborting.");
        }

	if (n1_frames != n2_frames) {
	  error("Number of frames in sentence %d different. pf1 = %d, pf2 = %d\n", 
		(*srit),n1_frames,n2_frames);
	}

	Range prrng(pr_str,0,n1_frames);

        // Increase size of buffers if needed.
        if (n1_frames > buf_size)
        {
            // Free old buffers.
            delete ftr1_buf;
            delete ftr2_buf;
            delete oftr_buf;
            delete olab_buf;
            delete lab1_buf;
            delete lab2_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = n1_frames * 2;

            // Allocate new larger buffers.
            ftr1_buf = new float[buf_size * n1_ftrs];
            ftr2_buf = new float[buf_size * n2_ftrs];
            oftr_buf = new float[buf_size * (fr1rng.length()+fr2rng.length())];
            olab_buf = new QNUInt32[buf_size * lrrng.length() ];
            lab1_buf = new QNUInt32[buf_size * n1_labs];
            lab2_buf = new QNUInt32[buf_size * n2_labs];

        }

        const QN_SegID seg1_id = in1_stream.set_pos((*srit), 0);
        if (seg1_id == QN_SEGID_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't seek to start of sentence %lu "
                   "in first input pfile.",
                   program_name, (unsigned long) (*srit));
            error("Aborting.");
        }

        const QN_SegID seg2_id = in2_stream.set_pos((*srit), 0);
        if (seg2_id == QN_SEGID_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't seek to start of sentence %lu "
                   "in second input pfile.",
                   program_name, (unsigned long) (*srit));
            error("Aborting.");
        }


        const size_t n1_read =
            in1_stream.read_ftrslabs(n1_frames, ftr1_buf, lab1_buf);
        if (n1_read != n1_frames)
        {
            fprintf(stderr, "%s: At sentence %lu in first input pfile, "
                   "only read %lu frames when should have read %lu.\n",
                   program_name, (unsigned long) (*srit),
                   (unsigned long) n1_read, (unsigned long) n1_frames);
            error("Aborting.");
        }


        const size_t n2_read =
            in2_stream.read_ftrslabs(n2_frames, ftr2_buf, lab2_buf);
        if (n2_read != n2_frames)
        {
            fprintf(stderr, "%s: At sentence %lu in second input pfile, "
                   "only read %lu frames when should have read %lu.\n",
                   program_name, (unsigned long) (*srit),
                   (unsigned long) n2_read, (unsigned long) n2_frames);
            error("Aborting.");
        }



	// construct the output set of features.
	

	ftr1_buf_p = ftr1_buf;
	ftr2_buf_p = ftr2_buf;

	oftr_buf_p = oftr_buf;
	olab_buf_p = olab_buf;

	for (Range::iterator prit=prrng.begin();
	     !prit.at_end() ; ++prit) {

	  ftr1_buf_p = ftr1_buf + (*prit)*n1_ftrs;
	  ftr2_buf_p = ftr2_buf + (*prit)*n2_ftrs;
	  if (labels_from == 1)
	    lab_buf_p = lab1_buf + (*prit)*n1_labs;
	  else 
	    lab_buf_p = lab2_buf + (*prit)*n2_labs;

	  for (Range::iterator fr1it=fr1rng.begin();
	       !fr1it.at_end(); ++fr1it) {
	    *oftr_buf_p++ = ftr1_buf_p[*fr1it];
	  }
	  for (Range::iterator fr2it=fr2rng.begin();
	       !fr2it.at_end(); ++fr2it) {
	    *oftr_buf_p++ = ftr2_buf_p[*fr2it];
	  }
	  for (Range::iterator lrit=lrrng.begin();
	       !lrit.at_end(); ++lrit) {
	    *olab_buf_p++ = lab_buf_p[*lrit];
	  }
	}
	// Write output.
	out_stream.write_ftrslabs(prrng.length(), oftr_buf, olab_buf);
	out_stream.doneseg((QN_SegID) seg1_id);
    }

    delete ftr1_buf;
    delete ftr2_buf;
    delete oftr_buf;
    delete olab_buf;
    delete lab1_buf;
    delete lab2_buf;
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

    const char *input1_fname = 0;  // Input pfile name.
    const char *input2_fname = 0;  // Input pfile name.
    const char *output_fname = 0; // Output pfile name.


    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr1_str = 0;   // first feature range string
    Range *fr1_rng;
    const char *fr2_str = 0;   // second feature range string
    Range *fr2_rng;
    const char *lr_str = 0;   // label range string    
    Range *lr_rng;
    const char *pr_str = 0;   // per-sentence range string

    size_t labels_from=1;
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
        else if (strcmp(argp, "-i1")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input1_fname = *argv++;
                argc--;
            }
            else
                usage("No input filename given.");
        }
        else if (strcmp(argp, "-i2")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input2_fname = *argv++;
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
        else if (strcmp(argp, "-fr1")==0)
        {
            if (argc>0)
            {
	      fr1_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-fr2")==0)
        {
            if (argc>0)
            {
	      fr2_str = *argv++;
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
        else if (strcmp(argp, "-l")==0)
        {
            if (argc>0)
            {
	        labels_from = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No pfile labels designator given.");
        }
        else {
	  sprintf(buf,"Unrecognized argument (%s).",argp);
	  usage(buf);
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    if (input1_fname==0)
        usage("No first input pfile name supplied.");
    FILE *in1_fp = fopen(input1_fname, "r");
    if (in1_fp==NULL)
        error("Couldn't open input pfile %s for reading.",input1_fname);

    if (input2_fname==0)
        usage("No first input pfile name supplied.");
    FILE *in2_fp = fopen(input2_fname, "r");
    if (in2_fp==NULL)
        error("Couldn't open input pfile %s for reading.",input2_fname);

    FILE *out_fp;
    if (output_fname==0 || !strcmp(output_fname,"-"))
      out_fp = stdout;
    else {
      if ((out_fp = fopen(output_fname, "w")) == NULL) {
	error("Couldn't open output file for writing.");
      }
    }

    if (labels_from != 1 && labels_from != 2)
      error("labels argument (%ud) must be either 1 or 2\n",labels_from);

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in1_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in1_fp, 1);
    QN_InFtrLabStream* in2_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in2_fp, 1);

    if (in1_streamp->num_segs() != in2_streamp->num_segs()) {
      error("Number of sentences in each pfile must be the same\n");
    }

    sr_rng = new Range(sr_str,0,in1_streamp->num_segs());
    fr1_rng = new Range(fr1_str,0,in1_streamp->num_ftrs());
    fr2_rng = new Range(fr2_str,0,in2_streamp->num_ftrs());
    if (labels_from == 1) 
      lr_rng = new Range(lr_str,0,in1_streamp->num_labs());
    else 
      lr_rng = new Range(lr_str,0,in2_streamp->num_labs());

    QN_OutFtrLabStream* out_streamp
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "",
                                       out_fp,
				       fr1_rng->length()+
				       fr2_rng->length(),
				       lr_rng->length(),
                                       1);

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_merge(*in1_streamp,
		*in2_streamp,
		*out_streamp,
		*sr_rng,
		*fr1_rng,
		*fr2_rng,
		*lr_rng,
		pr_str,
		labels_from);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in1_streamp;
    delete in2_streamp;
    delete out_streamp;
    delete sr_rng;
    delete fr1_rng;
    delete fr2_rng;
    delete lr_rng;

    if (fclose(in1_fp))
        error("Couldn't close input1 pfile.");
    if (fclose(in2_fp))
        error("Couldn't close input2 pfile.");

    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
