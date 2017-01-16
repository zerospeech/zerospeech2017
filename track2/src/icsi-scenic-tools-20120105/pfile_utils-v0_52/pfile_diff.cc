/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_diff.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
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

#define MAX(a,b) ((a)>(b)?(a):(b))
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
	    "-sr <range>     sentence range\n"
	    "[-sr2 <range>]  sentence range for 2nd pfile if desired.\n"
	    "-fr range       range of features selected from first pfile\n"
	    "[-fr2 range]    feature range for 2nd pfile if desired\n"
	    "-pr <range>     per-sentence frame range\n"
	    "[-pr <range>]   per-sentence range for 2nd pfile if desired\n" 
	    "-lr <range>     label range\n"
	    "[-lr2 <range>]   label range for 2nd pfile if desired\n"
	    "-tol float      tolarance absolute value\n"
	    "-stop           Do *not* continue after first difference found.\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


static void
pfile_diff(QN_InFtrLabStream& in1_stream,
	   QN_InFtrLabStream& in2_stream,
	   Range& sr1rng,Range& sr2rng,
	   Range& fr1rng,Range& fr2rng,	    
	   Range& lr1rng,Range& lr2rng,
	   const char*pr1_str,
	   const char*pr2_str,
	   const float tolerance,
	   bool stop)
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

    QNUInt32* lab1_buf = new QNUInt32[buf_size * n1_labs];
    QNUInt32* lab2_buf = new QNUInt32[buf_size * n2_labs];
    QNUInt32* lab1_buf_p;
    QNUInt32* lab2_buf_p;

    size_t print_count = 0;


    // Go through input pfile to get the initial statistics,
    // i.e., max, min, mean, std, etc.
    Range::iterator srit1=sr1rng.begin();
    Range::iterator srit2=sr2rng.begin();
    for (;!srit1.at_end();srit1++,srit2++) {

        const size_t n1_frames = in1_stream.num_frames((*srit1));
        const size_t n2_frames = in2_stream.num_frames((*srit2));

	if (print_count ++ % 100 == 0) 
	  printf("Processing sentence %d and %d\n",(*srit1),(*srit2));

        if (n1_frames == QN_SIZET_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't find number of frames "
                   "at sentence %lu in first input pfile.\n",
                   program_name, (unsigned long) (*srit1));
            error("Aborting.");
        }

        if (n2_frames == QN_SIZET_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't find number of frames "
                   "at sentence %lu in second input pfile.\n",
                   program_name, (unsigned long) (*srit2));
            error("Aborting.");
        }

	Range pr1rng(pr1_str,0,n1_frames);
	Range pr2rng(pr2_str,0,n2_frames);

	if (pr1rng.length() != pr2rng.length()) {
	  error("Num frames of per-sentence ranges in sentences %d of file1 and %d of file2 different. pf1 = %d, pf2 = %d\n", 
		(*srit1),(*srit2),pr1rng.length(),pr2rng.length());
	}


        // Increase size of buffers if needed.
        if (n1_frames > buf_size || n2_frames > buf_size)
        {
            // Free old buffers.
            delete ftr1_buf;
            delete ftr2_buf;
            delete lab1_buf;
            delete lab2_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = MAX(n1_frames,n2_frames) * 2;

            // Allocate new larger buffers.
            ftr1_buf = new float[buf_size * n1_ftrs];
            ftr2_buf = new float[buf_size * n2_ftrs];
            lab1_buf = new QNUInt32[buf_size * n1_labs];
            lab2_buf = new QNUInt32[buf_size * n2_labs];

        }

        const QN_SegID seg1_id = in1_stream.set_pos((*srit1), 0);
        if (seg1_id == QN_SEGID_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't seek to start of sentence %lu "
                   "in first input pfile.",
                   program_name, (unsigned long) (*srit1));
            error("Aborting.");
        }

        const QN_SegID seg2_id = in2_stream.set_pos((*srit2), 0);
        if (seg2_id == QN_SEGID_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't seek to start of sentence %lu "
                   "in second input pfile.",
                   program_name, (unsigned long) (*srit2));
            error("Aborting.");
        }


        const size_t n1_read =
            in1_stream.read_ftrslabs(n1_frames, ftr1_buf, lab1_buf);
        if (n1_read != n1_frames)
        {
            fprintf(stderr, "%s: At sentence %lu in first input pfile, "
                   "only read %lu frames when should have read %lu.\n",
                   program_name, (unsigned long) (*srit1),
                   (unsigned long) n1_read, (unsigned long) n1_frames);
            error("Aborting.");
        }


        const size_t n2_read =
            in2_stream.read_ftrslabs(n2_frames, ftr2_buf, lab2_buf);
        if (n2_read != n2_frames)
        {
            fprintf(stderr, "%s: At sentence %lu in second input pfile, "
                   "only read %lu frames when should have read %lu.\n",
                   program_name, (unsigned long) (*srit2),
                   (unsigned long) n2_read, (unsigned long) n2_frames);
            error("Aborting.");
        }



	// construct the output set of features.
	

	ftr1_buf_p = ftr1_buf;
	ftr2_buf_p = ftr2_buf;

	Range::iterator prit1=pr1rng.begin();
	Range::iterator prit2=pr2rng.begin();

	for (; !prit1.at_end() ; ++prit1,++prit2) {

	  ftr1_buf_p = ftr1_buf + (*prit1)*n1_ftrs;
	  ftr2_buf_p = ftr2_buf + (*prit2)*n2_ftrs;

	  Range::iterator frit1=fr1rng.begin();
	  Range::iterator frit2=fr2rng.begin();
	  for (;!frit1.at_end(); ++frit1,++frit2) {
	    const float diff = ftr1_buf_p[*frit1] - ftr2_buf_p[*frit2];
	    if (fabs(diff) > tolerance) {
	      printf("sent,pfrm,ftr of file1(%d,%d,%d) and file2(%d,%d,%d) diff > tolerance\n",(*srit1),(*prit1),(*frit1),(*srit2),(*prit2),(*frit2));
	      if (stop)
		goto done;
	    }
	  }
	  
	  lab1_buf_p = lab1_buf + (*prit1)*n1_labs;
	  lab2_buf_p = lab2_buf + (*prit2)*n2_labs;


	  Range::iterator lrit1=lr1rng.begin();
	  Range::iterator lrit2=lr2rng.begin();

	  for (;!lrit1.at_end(); ++lrit1,++lrit2) {
	    if (lab1_buf_p[*lrit1] != lab2_buf_p[*lrit2]) {
	      printf("sent,pfrm lab of file1(%d,%d) and file2(%d,%d) differ.\n",
		     (*srit1),(*prit1),(*srit2),(*prit2));
	      if (stop)
		goto done;
	    }
	  }
	}
    }

done:

    delete ftr1_buf;
    delete ftr2_buf;
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


    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // first feature range string
    Range *fr_rng;
    const char *lr_str = 0;   // label range string    
    Range *lr_rng;
    const char *pr_str = 0;   // per-sentence range string


    const char *sr2_str = 0;   // sentence range string
    Range *sr2_rng;
    const char *fr2_str = 0;   // first feature range string
    Range *fr2_rng;
    const char *lr2_str = 0;   // label range string    
    Range *lr2_rng;
    const char *pr2_str = 0;   // per-sentence range string

    float tolerance = 0.0;
    bool stop = false;


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
        else if (strcmp(argp, "-sr2")==0)
        {
            if (argc>0)
            {
	      sr2_str = *argv++;
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
        else if (strcmp(argp, "-pr2")==0)
        {
            if (argc>0)
            {
	      pr2_str = *argv++;
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
        else if (strcmp(argp, "-lr2")==0)
        {
            if (argc>0)
            {
	      lr2_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-tol")==0)
        {
            if (argc>0)
            {
	      tolerance = (float) parse_float(*argv++);
	      argc--;
            }
            else
                usage("No pfile labels designator given.");
        }
        else if (strcmp(argp, "-stop")==0)
        {
	  stop = true;
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

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in1_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in1_fp, 1);
    QN_InFtrLabStream* in2_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in2_fp, 1);

    sr_rng = new Range(sr_str,0,in1_streamp->num_segs());
    if (sr2_str == NULL)
      sr2_rng = new Range(sr_str,0,in2_streamp->num_segs());
    else 
      sr2_rng = new Range(sr2_str,0,in2_streamp->num_segs());
    if (sr_rng->length() != sr2_rng->length())
      error("ERROR: Sentence ranges for the two pfiles specify different total sizes.");
    
    fr_rng = new Range(fr_str,0,in1_streamp->num_ftrs());
    if (fr2_str == NULL) 
      fr2_rng = new Range(fr_str,0,in2_streamp->num_ftrs());
    else 
      fr2_rng = new Range(fr2_str,0,in2_streamp->num_ftrs());
    if (fr_rng->length() != fr2_rng->length())
      error("ERROR: Feature ranges for the two pfiles specify different total sizes.");


    lr_rng = new Range(lr_str,0,in1_streamp->num_labs());    
    if (lr2_str == NULL) 
      lr2_rng = new Range(lr_str,0,in2_streamp->num_labs());
    else 
      lr2_rng = new Range(lr2_str,0,in2_streamp->num_labs());
    if (lr_rng->length() != lr2_rng->length())
      error("ERROR: Label ranges for the two pfiles specify different total sizes.");

    if (pr2_str == NULL)
      pr2_str = pr_str;

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_diff(*in1_streamp,
	       *in2_streamp,
	       *sr_rng,*sr2_rng,
	       *fr_rng,*fr2_rng,
	       *lr_rng,*lr2_rng,
	       pr_str,pr2_str,
	       tolerance,stop);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in1_streamp;
    delete in2_streamp;
    delete sr_rng;
    delete fr_rng;
    delete lr_rng;
    delete sr2_rng;
    delete fr2_rng;
    delete lr2_rng;

    if (fclose(in1_fp))
        error("Couldn't close input1 pfile.");
    if (fclose(in2_fp))
        error("Couldn't close input2 pfile.");

    return EXIT_SUCCESS;
}
