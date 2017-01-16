/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_addsil.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $
  
    This program selects an arbitrary subset set of features from each
    frame of a pfile and creates a new pfile with that
    subset in each frame but with "silence" added to the
    beginning and ending of each utterance. Silence is defined
    by computing means and variances from select ranges within each
    utterance (one range each for producing silence at the beginning
    and the end), and then randomly sampling from a Normal distribution
    with the correspondingly computed means and variances.

    Written By:
         Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "parse_subset.h"
#include "error.h"
#include "Range.H"
#include "rand.h"

RAND rnd(true);

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (1)
#endif

#define MAXHISTBINS 1000


#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;

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
	    "-nb  <n>        Number of new beginning silence frames\n"
	    "-prb <range>    per-sentence range to compute beginning silence\n"
	    "-ne  <n>        Number of new ending silence frames\n"
	    "-pre <range>    per-sentence range to compute ending silence\n"
	    "-mmf <val>      Mean multiplicative factor\n"
	    "-maf <val>      Mean additive factor\n"
	    "-smf <val>      Standard-deviation multiplicative factor\n"
            "-saf <val>      Standard-deviation additive factor\n"
	    "-lr <range>     label range\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


static void
pfile_addsil(QN_InFtrLabStream& in_stream,
	     QN_OutFtrLabStream& out_stream,
	     Range& srrng,
	     Range& frrng,
	     Range& lrrng,
	     const int nb,
	     const char *prb_str,
	     const int ne,
	     const char *pre_str,
	     const double mmf,
	     const double maf,
	     const double smf,
	     const double saf)
{
    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const size_t n_ftrs = in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    float *ftr_buf_p;
    float *oftr_buf = new float[(ne+nb+buf_size) * frrng.length()];
    float *oftr_buf_p;

    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];
    QNUInt32* lab_buf_p;    
    QNUInt32* olab_buf = new QNUInt32[(ne+nb+buf_size) * n_labs];
    QNUInt32* olab_buf_p;    

    double *sums = new double[frrng.length()];
    double *sumsqs = new double[frrng.length()];

    //
    // Go through input pfile to get the initial statistics,
    // i.e., max, min, mean, std, etc.
    for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
        const size_t n_frames = in_stream.num_frames((*srit));

	Range prbrng(prb_str,0,n_frames);
	Range prerng(pre_str,0,n_frames);

	if (prbrng.length() == 0 && nb != 0)
	  error("No frames to compute beginning silence, sentence %d\n",
		(*srit));
	if (prerng.length() == 0 && ne != 0)
	  error("No frames to compute beginning silence, sentence %d\n",
		(*srit));

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
            delete [] ftr_buf;
            delete [] oftr_buf;
            delete [] lab_buf;
            delete [] olab_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = n_frames * 2;

            // Allocate new larger buffers.
            ftr_buf = new float[buf_size * n_ftrs];
            oftr_buf = new float[(nb+ne+buf_size) * frrng.length()];
            lab_buf = new QNUInt32[buf_size * n_labs];
            olab_buf = new QNUInt32[(nb+ne+buf_size) * n_labs];
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


	// compute the beginning means and vars
	oftr_buf_p = oftr_buf;
	olab_buf_p = olab_buf;
	if (nb > 0) {
	  ::memset(sums,0,sizeof(double)*frrng.length());
	  ::memset(sumsqs,0,sizeof(double)*frrng.length());	
	  for (Range::iterator prit=prbrng.begin();
	       !prit.at_end(); ++prit) {
	    ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	    int i=0;
	    for (Range::iterator frit=frrng.begin();
		 !frit.at_end(); ++frit) {
	      double tmp = ftr_buf_p[*frit];
	      sums[i] += tmp;
	      sumsqs[i] += tmp*tmp;
	      i++;
	    }
	  }
	  int i;
	  for (i=0;i<(int)frrng.length();i++) {
	    sums[i] /= prbrng.length();
	    sumsqs[i] = sumsqs[i] / prbrng.length() - sums[i]*sums[i];
	    if (sumsqs[i] <= DBL_MIN)
	      sumsqs[i] = 0.0;
	    else 
	      sumsqs[i] = sqrt(sumsqs[i]);
	    sums[i] = mmf*sums[i] + maf;
	    sumsqs[i] = smf*sumsqs[i] + saf;
	  }
	  for (i=0;i<nb;i++) {
	    for (Range::iterator lrit=lrrng.begin();
		 !lrit.at_end(); ++lrit) {
	      // copy labels from first frame only.
	      *olab_buf_p++ = lab_buf[*lrit];
	    }
	    for (int j=0;j<(int)frrng.length();j++) {
	      *oftr_buf_p++ = sums[j] + 
		sumsqs[j]*inverse_normal_func(rnd.drand48pe());
	    }
	  }
	}

	// copy normal frames.
	for (int frame=0;frame<(int)n_read;frame++) {
	  ftr_buf_p = ftr_buf + (frame)*n_ftrs;
	  lab_buf_p = lab_buf + (frame)*n_labs;
	  for (Range::iterator frit=frrng.begin();
	       !frit.at_end(); ++frit) {
	    *oftr_buf_p++ = ftr_buf_p[*frit];
	  }
	  for (Range::iterator lrit=lrrng.begin();
	       !lrit.at_end(); ++lrit) {
	    *olab_buf_p++ = lab_buf_p[*lrit];
	  }
	}

	if (ne > 0) {
	  ::memset(sums,0,sizeof(double)*frrng.length());
	  ::memset(sumsqs,0,sizeof(double)*frrng.length());	
	  for (Range::iterator prit=prerng.begin();
	       !prit.at_end(); ++prit) {
	    ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	    int i=0;
	    for (Range::iterator frit=frrng.begin();
		 !frit.at_end(); ++frit) {
	      double tmp = ftr_buf_p[*frit];
	      sums[i] += tmp;
	      sumsqs[i] += tmp*tmp;
	      i++;
	    }
	  }
	  int i;
	  for (i=0;i<(int)frrng.length();i++) {
	    sums[i] /= prerng.length();
	    sumsqs[i] = sumsqs[i] / prerng.length() - sums[i]*sums[i];
	    if (sumsqs[i] <= DBL_MIN)
	      sumsqs[i] = 0.0;
	    else 
	      sumsqs[i] = sqrt(sumsqs[i]);
	    sums[i] = mmf*sums[i] + maf;
	    sumsqs[i] = smf*sumsqs[i] + saf;
	  }
	  for (i=0;i<nb;i++) {
	    for (Range::iterator lrit=lrrng.begin();
		 !lrit.at_end(); ++lrit) {
	      // copy label from last frame only.
	      *olab_buf_p++ = (lab_buf + (n_frames-1)*n_labs)[*lrit];
	    }
	    for (int j=0;j<(int)frrng.length();j++) {
	      *oftr_buf_p++ = sums[j] + 
		sumsqs[j]*inverse_normal_func(rnd.drand48pe());
	    }
	  }
	}

	// Write output.
	out_stream.write_ftrslabs((nb+ne+n_frames), oftr_buf, olab_buf);
	out_stream.doneseg((QN_SegID) seg_id);
    }


    delete [] ftr_buf;
    delete [] lab_buf;
    delete [] sums;
    delete [] sumsqs;
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
    const char *prb_str = 0;  // per-sentence beginning silence range string
    const char *pre_str = 0;  // per-sentence ending silence range string
    int nb=0;
    int ne=0;
    int debug_level = 0;

    double mmf = 1.0;
    double maf = 0.0;
    double smf = 1.0;
    double saf = 0.0;

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
        else if (strcmp(argp, "-nb")==0)
        {
            if (argc>0)
            {
	      nb = (int) parse_long(*argv++);
	      argc--;
            }
            else
                usage("No number beginning silence frames given.");
        }
        else if (strcmp(argp, "-ne")==0)
        {
            if (argc>0)
            {
	      ne = (int) parse_long(*argv++);
	      argc--;
            }
            else
                usage("No number ending silence frames given.");
        }
        else if (strcmp(argp, "-mmf")==0)
        {
            if (argc>0)
            {
	      mmf = parse_float(*argv++);
	      argc--;
            }
            else
                usage("No -mmf option given.");
        }
        else if (strcmp(argp, "-maf")==0)
        {
            if (argc>0)
            {
	      maf = parse_float(*argv++);
	      argc--;
            }
            else
                usage("No -maf option given.");
        }
        else if (strcmp(argp, "-smf")==0)
        {
            if (argc>0)
            {
	      smf = parse_float(*argv++);
	      argc--;
            }
            else
                usage("No -smf option given.");
        }
        else if (strcmp(argp, "-saf")==0)
        {
            if (argc>0)
            {
	      saf = parse_float(*argv++);
	      argc--;
            }
            else
                usage("No -saf option given.");
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
        else if (strcmp(argp, "-prb")==0)
        {
            if (argc>0)
            {
	      prb_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-pre")==0)
        {
            if (argc>0)
            {
	      pre_str = *argv++;
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


    if (ne < 0 || nb < 0) {
      error("Number of frames to add must be non-negative");
    }

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

    pfile_addsil(*in_streamp,
		 *out_streamp,
		 *sr_rng,*fr_rng,*lr_rng,
		 nb,prb_str,ne,pre_str,
		 mmf,maf,smf,saf);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in_streamp;
    delete out_streamp;
    delete sr_rng;
    delete fr_rng;

    if (fclose(in_fp))
        error("Couldn't close input pfile.");
    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
