/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_ftrcombo.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $

    pfile_ftrcombo

    Takes the features in two (? or more) pfiles and combines their 
    values by some simple operation such as addition, multiplcation, ...

    1999aug12 dpwe@icsi.berkeley.edu

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
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help           print this message\n"
	    "-i1 <file-name> first input pfile\n"
	    "-i2 <file-name> second input pfile (optional)\n"
	    "-i3 <file-name> third input pfile (optional)\n"
	    "-i4 <file-name> fourth input pfile (optional)\n"
	    "-o <file-name>  output pfile \n"
	    "-sr <range>     sentence range\n"
	    "-fr1 range      range of features selected from first pfile\n"
	    "-fr2 range      range of features selected from second pfile\n"
	    "-fr3 range      range of features selected from third pfile\n"
	    "-fr4 range      range of features selected from fourth pfile\n"
	    "-pr <range>     per-sentence frame range\n"
	    "-c <method>     combination method: add, sub, mul or div (none)\n"
	    "-g1 <val>	     scaling constant for pfile1 vals\n"
	    "-g2 <val>	     scaling constant for pfile2 vals\n"
	    "-g3 <val>	     scaling constant for pfile3 vals\n"
	    "-g4 <val>	     scaling constant for pfile4 vals\n"
	    "-k <val>	     additive offset for final result\n"
	    "-lr <range>     label range\n"
	    "-l #            take labels from first (1, default) or later pfile\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}

// operation flags
enum {
    FTROP_NONE = 0,
    FTROP_ADD, 
    FTROP_SUB,
    FTROP_MUL,
    FTROP_DIV
};

static void
pfile_ftrcombo(
	    QN_InFtrLabStream& in1_stream,
	    QN_InFtrLabStream* pin2_stream,
	    QN_InFtrLabStream* pin3_stream,
	    QN_InFtrLabStream* pin4_stream,
	    QN_OutFtrLabStream& out_stream,
	    Range& sr1rng,
	    Range& sr2rng,
	    Range& sr3rng,
	    Range& sr4rng,
	    Range& fr1rng,
	    Range& fr2rng,
	    Range& fr3rng,
	    Range& fr4rng,
	    Range& lrrng,
	    const char*pr_str,
	    const size_t labels_from, 
	    int ftrop, 
	    double g1, 
	    double g2, 
	    double g3, 
	    double g4, 
	    double k)

{

    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    size_t n1_labs = in1_stream.num_labs();
    size_t n1_ftrs = in1_stream.num_ftrs();
    size_t n2_labs = 0;
    size_t n2_ftrs = 0;
    size_t n3_labs = 0;
    size_t n3_ftrs = 0;
    size_t n4_labs = 0;
    size_t n4_ftrs = 0;
    size_t n_ftrs;

    if (pin2_stream) {
	n2_labs = pin2_stream->num_labs();
	n2_ftrs = pin2_stream->num_ftrs();
	if (fr1rng.length() != fr2rng.length()) {
	    error("number of selected ftr1 ftrs (%d) does not match number of selected ftr2 ftrs (%d)\n",
		  fr1rng.length(), fr2rng.length());
	}
    }
    if (pin3_stream) {
	n3_labs = pin3_stream->num_labs();
	n3_ftrs = pin3_stream->num_ftrs();
	if (fr1rng.length() != fr3rng.length()) {
	    error("number of selected ftr1 ftrs (%d) does not match number of selected ftr3 ftrs (%d)\n",
		  fr1rng.length(), fr3rng.length());
	}
    }
    if (pin4_stream) {
	n4_labs = pin4_stream->num_labs();
	n4_ftrs = pin4_stream->num_ftrs();
	if (fr1rng.length() != fr4rng.length()) {
	    error("number of selected ftr1 ftrs (%d) does not match number of selected ftr4 ftrs (%d)\n",
		  fr1rng.length(), fr4rng.length());
	}
    }
    n_ftrs = fr1rng.length();

    float *ftr1_buf = new float[buf_size * n1_ftrs];
    float *ftr2_buf = new float[buf_size * n2_ftrs];
    float *ftr3_buf = new float[buf_size * n3_ftrs];
    float *ftr4_buf = new float[buf_size * n4_ftrs];
    float *ftr1_buf_p;
    float *ftr2_buf_p;
    float *ftr3_buf_p;
    float *ftr4_buf_p;

    float *oftr_buf = new float[buf_size * n_ftrs];
    float *oftr_buf_p, *oftr_buf_p2, *oftr_buf_p3, *oftr_buf_p4;

    QNUInt32* lab1_buf = new QNUInt32[buf_size * n1_labs];
    QNUInt32* lab2_buf = new QNUInt32[buf_size * n2_labs];
    QNUInt32* lab3_buf = new QNUInt32[buf_size * n3_labs];
    QNUInt32* lab4_buf = new QNUInt32[buf_size * n4_labs];
    QNUInt32* lab_buf_p;

    QNUInt32* olab_buf = new QNUInt32[buf_size * lrrng.length() ];
    QNUInt32* olab_buf_p;

    Range::iterator srit2 = sr2rng.begin(); /* sr2rng valid even w/o i2 */
    Range::iterator srit3 = sr3rng.begin();
    Range::iterator srit4 = sr4rng.begin();
    for (Range::iterator srit1=sr1rng.begin();!srit1.at_end();srit1++) {

        const size_t n1_frames = in1_stream.num_frames((*srit1));

	if ((*srit1) % 100 == 0)
	  printf("Processing sentence %d\n",(*srit1));

        if (n1_frames == QN_SIZET_BAD /* || n2_frames == QN_SIZET_BAD */)
        {
            fprintf(stderr,
                   "%s: Couldn't find number of frames "
                   "at sentence %lu in input pfile.\n",
                   program_name, (unsigned long) (*srit1));
            error("Aborting.");
        }

	size_t n2_frames = 0;
	if (pin2_stream) {
	    n2_frames = pin2_stream->num_frames((*srit2));
	    if (n1_frames != n2_frames) {
	      error("Number of frames in sentence %d different. pf1 = %d, pf2 = %d\n", 
		    (*srit2),n1_frames,n2_frames);
	    }
	}

	size_t n3_frames = 0;
	if (pin3_stream) {
	    n3_frames = pin3_stream->num_frames((*srit3));
	    if (n1_frames != n3_frames) {
	      error("Number of frames in sentence %d different. pf1 = %d, pf3 = %d\n", 
		    (*srit3),n1_frames,n3_frames);
	    }
	}

	size_t n4_frames = 0;
	if (pin4_stream) {
	    n4_frames = pin4_stream->num_frames((*srit4));
	    if (n1_frames != n4_frames) {
	      error("Number of frames in sentence %d different. pf1 = %d, pf4 = %d\n", 
		    (*srit4),n1_frames,n4_frames);
	    }
	}

	Range prrng(pr_str,0,n1_frames);

        // Increase size of buffers if needed.
        if (n1_frames > buf_size)
        {
            // Free old buffers.
            delete ftr1_buf;
            delete ftr2_buf;
            delete ftr3_buf;
            delete ftr4_buf;
            delete oftr_buf;
            delete olab_buf;
            delete lab1_buf;
            delete lab2_buf;
            delete lab3_buf;
            delete lab4_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = n1_frames * 2;

            // Allocate new larger buffers.
            ftr1_buf = new float[buf_size * n1_ftrs];
            ftr2_buf = new float[buf_size * n2_ftrs];
            ftr3_buf = new float[buf_size * n3_ftrs];
            ftr4_buf = new float[buf_size * n4_ftrs];
            oftr_buf = new float[buf_size * n_ftrs];
            olab_buf = new QNUInt32[buf_size * lrrng.length() ];
            lab1_buf = new QNUInt32[buf_size * n1_labs];
            lab2_buf = new QNUInt32[buf_size * n2_labs];
            lab3_buf = new QNUInt32[buf_size * n3_labs];
            lab4_buf = new QNUInt32[buf_size * n4_labs];

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

	if (pin2_stream) {
	    const QN_SegID seg2_id = pin2_stream->set_pos((*srit2), 0);
	    if (seg2_id == QN_SEGID_BAD)
	    {
		fprintf(stderr,
		       "%s: Couldn't seek to start of sentence %lu "
		       "in second input pfile.",
		       program_name, (unsigned long) (*srit2));
		error("Aborting.");
	    }
	}

	if (pin3_stream) {
	    const QN_SegID seg3_id = pin3_stream->set_pos((*srit3), 0);
	    if (seg3_id == QN_SEGID_BAD)
	    {
		fprintf(stderr,
		       "%s: Couldn't seek to start of sentence %lu "
		       "in 3rd input pfile.",
		       program_name, (unsigned long) (*srit3));
		error("Aborting.");
	    }
	}

	if (pin4_stream) {
	    const QN_SegID seg4_id = pin4_stream->set_pos((*srit4), 0);
	    if (seg4_id == QN_SEGID_BAD)
	    {
		fprintf(stderr,
		       "%s: Couldn't seek to start of sentence %lu "
		       "in 4th input pfile.",
		       program_name, (unsigned long) (*srit4));
		error("Aborting.");
	    }
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


	if (pin2_stream) {
	    const size_t n2_read =
		pin2_stream->read_ftrslabs(n2_frames, ftr2_buf, lab2_buf);
	    if (n2_read != n2_frames)
	    {
		fprintf(stderr, "%s: At sentence %lu in second input pfile, "
		       "only read %lu frames when should have read %lu.\n",
		       program_name, (unsigned long) (*srit2),
		       (unsigned long) n2_read, (unsigned long) n2_frames);
		error("Aborting.");
	    }
	}

	if (pin3_stream) {
	    const size_t n3_read =
		pin3_stream->read_ftrslabs(n3_frames, ftr3_buf, lab3_buf);
	    if (n3_read != n3_frames)
	    {
		fprintf(stderr, "%s: At sentence %lu in 3rd input pfile, "
		       "only read %lu frames when should have read %lu.\n",
		       program_name, (unsigned long) (*srit3),
		       (unsigned long) n3_read, (unsigned long) n3_frames);
		error("Aborting.");
	    }
	}

	if (pin4_stream) {
	    const size_t n4_read =
		pin4_stream->read_ftrslabs(n4_frames, ftr4_buf, lab4_buf);
	    if (n4_read != n4_frames)
	    {
		fprintf(stderr, "%s: At sentence %lu in 4th input pfile, "
		       "only read %lu frames when should have read %lu.\n",
		       program_name, (unsigned long) (*srit4),
		       (unsigned long) n4_read, (unsigned long) n4_frames);
		error("Aborting.");
	    }
	}


	// construct the output set of features.
	

	ftr1_buf_p = ftr1_buf;
	ftr2_buf_p = ftr2_buf;
	ftr3_buf_p = ftr3_buf;
	ftr4_buf_p = ftr4_buf;

	oftr_buf_p = oftr_buf;
	olab_buf_p = olab_buf;

	for (Range::iterator prit=prrng.begin();
	     !prit.at_end() ; ++prit) {

	  ftr1_buf_p = ftr1_buf + (*prit)*n1_ftrs;
	  ftr2_buf_p = ftr2_buf + (*prit)*n2_ftrs;
	  ftr3_buf_p = ftr3_buf + (*prit)*n3_ftrs;
	  ftr4_buf_p = ftr4_buf + (*prit)*n4_ftrs;
	  if (labels_from == 1)
	    lab_buf_p = lab1_buf + (*prit)*n1_labs;
	  else if (labels_from == 2)
	    lab_buf_p = lab2_buf + (*prit)*n2_labs;
	  else if (labels_from == 3)
	    lab_buf_p = lab3_buf + (*prit)*n3_labs;
	  else 
	    lab_buf_p = lab4_buf + (*prit)*n4_labs;

	  // remember the base of this frame's ftrs
	  oftr_buf_p2 = oftr_buf_p;

	  for (Range::iterator fr1it=fr1rng.begin();
	       !fr1it.at_end(); ++fr1it) {
	    *oftr_buf_p++ = g1 * ftr1_buf_p[*fr1it];
	  }
	  if (pin2_stream) {
	      oftr_buf_p = oftr_buf_p2;
	      for (Range::iterator fr2it=fr2rng.begin();
		   !fr2it.at_end(); ++fr2it) {
		  double u = *oftr_buf_p;
		  double v = g2 * ftr2_buf_p[*fr2it];
		  switch (ftrop) {
		  case FTROP_ADD:  *oftr_buf_p++ = u+v; break;
		  case FTROP_SUB:  *oftr_buf_p++ = u-v; break;
		  case FTROP_MUL:  *oftr_buf_p++ = u*v; break;
		  case FTROP_DIV:  *oftr_buf_p++ = u/v; break;
		  case FTROP_NONE:	error("no ftr operation given!");
		  default:	error("unknown ftrop %d", ftrop);
		  }
	      }
	  }
	  if (pin3_stream) {
	      oftr_buf_p = oftr_buf_p2;
	      for (Range::iterator fr3it=fr3rng.begin();
		   !fr3it.at_end(); ++fr3it) {
		  double u = *oftr_buf_p;
		  double v = g3 * ftr3_buf_p[*fr3it];
		  switch (ftrop) {
		  case FTROP_ADD:  *oftr_buf_p++ = u+v; break;
		  case FTROP_SUB:  *oftr_buf_p++ = u-v; break;
		  case FTROP_MUL:  *oftr_buf_p++ = u*v; break;
		  case FTROP_DIV:  *oftr_buf_p++ = u/v; break;
		  case FTROP_NONE:	error("no ftr operation given!");
		  default:	error("unknown ftrop %d", ftrop);
		  }
	      }
	  }
	  if (pin4_stream) {
	      oftr_buf_p = oftr_buf_p2;
	      for (Range::iterator fr4it=fr4rng.begin();
		   !fr4it.at_end(); ++fr4it) {
		  double u = *oftr_buf_p;
		  double v = g4 * ftr4_buf_p[*fr4it];
		  switch (ftrop) {
		  case FTROP_ADD:  *oftr_buf_p++ = u+v; break;
		  case FTROP_SUB:  *oftr_buf_p++ = u-v; break;
		  case FTROP_MUL:  *oftr_buf_p++ = u*v; break;
		  case FTROP_DIV:  *oftr_buf_p++ = u/v; break;
		  case FTROP_NONE:	error("no ftr operation given!");
		  default:	error("unknown ftrop %d", ftrop);
		  }
	      }
	  }
	  // global offset
	  oftr_buf_p = oftr_buf_p2;
	  for (int i = 0; i < n_ftrs; ++i) {
	      *oftr_buf_p++ += k;
	  }

	  for (Range::iterator lrit=lrrng.begin();
	       !lrit.at_end(); ++lrit) {
	    *olab_buf_p++ = lab_buf_p[*lrit];
	  }
	}
	// Write output.
	out_stream.write_ftrslabs(prrng.length(), oftr_buf, olab_buf);
	out_stream.doneseg((QN_SegID) seg1_id);

	// do "next" for srit2
	if (pin2_stream) {
	    if (!srit2.at_end()) 
		srit2++;
	}
	if (pin3_stream) {
	    if (!srit3.at_end()) 
		srit3++;
	}
	if (pin4_stream) {
	    if (!srit4.at_end()) 
		srit4++;
	}

    }

    delete ftr1_buf;
    delete ftr2_buf;
    delete ftr3_buf;
    delete ftr4_buf;
    delete oftr_buf;
    delete olab_buf;
    delete lab1_buf;
    delete lab2_buf;
    delete lab3_buf;
    delete lab4_buf;
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
        error("Not a floating point argument.");
    return val;
}


main(int argc, const char *argv[])
{
    const char *input1_fname = 0;  // Input pfile name.
    const char *input2_fname = 0;  // Input pfile name.
    const char *input3_fname = 0;  // Input pfile name.
    const char *input4_fname = 0;  // Input pfile name.
    const char *output_fname = 0; // Output pfile name.


    const char *sr_str = 0;   // sentence range string
    Range *sr1_rng;
    Range *sr2_rng = NULL;
    Range *sr3_rng = NULL;
    Range *sr4_rng = NULL;
    const char *fr1_str = 0;   // first feature range string
    Range *fr1_rng;
    const char *fr2_str = 0;   // second feature range string
    Range *fr2_rng;
    const char *fr3_str = 0;   // third feature range string
    Range *fr3_rng;
    const char *fr4_str = 0;   // fourth feature range string
    Range *fr4_rng;
    const char *lr_str = 0;   // label range string    
    Range *lr_rng;
    const char *pr_str = 0;   // per-sentence range string

    size_t labels_from=1;
    int debug_level = 0;

    double gain1 = 1.0;
    double gain2 = 1.0;
    double gain3 = 1.0;
    double gain4 = 1.0;
    double offset = 0.0;

    int ftrop = FTROP_NONE;

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
        else if (strcmp(argp, "-i3")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input3_fname = *argv++;
                argc--;
            }
            else
                usage("No input filename given.");
        }
        else if (strcmp(argp, "-i4")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input4_fname = *argv++;
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
        else if (strcmp(argp, "-fr3")==0)
        {
            if (argc>0)
            {
	      fr3_str = *argv++;
	      argc--;
            }
            else
                usage("No range given.");
        }
        else if (strcmp(argp, "-fr4")==0)
        {
            if (argc>0)
            {
	      fr4_str = *argv++;
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
        else if (strcmp(argp, "-c")==0)
        {
            if (argc>0)
            {
	        const char *s = *argv++;
                argc--;
		if (strcmp(s, "add")==0) {
		    ftrop = FTROP_ADD;
		} else if (strcmp(s, "sub")==0) {
		    ftrop = FTROP_SUB;
		} else if (strcmp(s, "mul")==0) {
		    ftrop = FTROP_SUB;
		} else if (strcmp(s, "div")==0) {
		    ftrop = FTROP_SUB;
		} else {
		    usage("Ftr op must be add/sub/mul/div.");
		}
            }
            else
                usage("No ftr op designator given.");
        }
        else if (strcmp(argp, "-k")==0)
        {
            if (argc>0)
            {
	        offset = parse_float(*argv++);
                argc--;
            }
            else
                usage("No pfile labels designator given.");
        }
        else if (strcmp(argp, "-g1")==0)
        {
            if (argc>0)
            {
	        gain1 = parse_float(*argv++);
                argc--;
            }
            else
                usage("No gain1 value given.");
        }
        else if (strcmp(argp, "-g2")==0)
        {
            if (argc>0)
            {
	        gain2 = parse_float(*argv++);
                argc--;
            }
            else
                usage("No gain2 value given.");
        }
        else if (strcmp(argp, "-g3")==0)
        {
            if (argc>0)
            {
	        gain3 = parse_float(*argv++);
                argc--;
            }
            else
                usage("No gain3 value given.");
        }
        else if (strcmp(argp, "-g4")==0)
        {
            if (argc>0)
            {
	        gain4 = parse_float(*argv++);
                argc--;
            }
            else
                usage("No gain4 value given.");
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

    FILE *in2_fp = NULL;
    if (input2_fname==0 || strlen(input2_fname)==0 ) {
	// It's ok - just operate on one file
        //usage("No second input pfile name supplied.");
    } else {
	in2_fp = fopen(input2_fname, "r");
	if (in2_fp==NULL)
	    error("Couldn't open input pfile %s for reading.",input2_fname);
    }

    FILE *in3_fp = NULL;
    if (input3_fname==0 || strlen(input3_fname)==0 ) {
	// It's ok - just operate on one file
        //usage("No second input pfile name supplied.");
    } else {
	in3_fp = fopen(input3_fname, "r");
	if (in3_fp==NULL)
	    error("Couldn't open input pfile %s for reading.",input3_fname);
    }

    FILE *in4_fp = NULL;
    if (input4_fname==0 || strlen(input4_fname)==0 ) {
	// It's ok - just operate on one file
        //usage("No second input pfile name supplied.");
    } else {
	in4_fp = fopen(input4_fname, "r");
	if (in4_fp==NULL)
	    error("Couldn't open input pfile %s for reading.",input4_fname);
    }

    FILE *out_fp;
    if (output_fname==0 || !strcmp(output_fname,"-"))
      out_fp = stdout;
    else {
      if ((out_fp = fopen(output_fname, "w")) == NULL) {
	error("Couldn't open output file for writing.");
      }
    }

    if (labels_from != 1 && (labels_from != 2 || in2_fp == NULL) \
	&& (labels_from != 3 || in3_fp == NULL)\
	&& (labels_from != 4 || in4_fp == NULL) )
      error("labels argument (%ud) must correspond to specified infile\n",labels_from);

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in1_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in1_fp, 1);
    sr1_rng = new Range(sr_str,0,in1_streamp->num_segs());
    fr1_rng = new Range(fr1_str,0,in1_streamp->num_ftrs());

    QN_InFtrLabStream* in2_streamp = NULL;
    if (in2_fp) {
        in2_streamp = new QN_InFtrLabStream_PFile(debug_level, "", in2_fp, 1);
	sr2_rng = new Range(sr_str,0,in2_streamp->num_segs());
	fr2_rng = new Range(fr2_str,0,in2_streamp->num_ftrs());
	if (sr1_rng->length() != sr2_rng->length()) {
	    error("Number of selected sentences in each pfile must be the same\n");
	}
    } else {
	// dummy sr2_rng
	sr2_rng = new Range("nil",0,0);
    }

    QN_InFtrLabStream* in3_streamp = NULL;
    if (in3_fp) {
        in3_streamp = new QN_InFtrLabStream_PFile(debug_level, "", in3_fp, 1);
	sr3_rng = new Range(sr_str,0,in3_streamp->num_segs());
	fr3_rng = new Range(fr3_str,0,in3_streamp->num_ftrs());
	if (sr1_rng->length() != sr3_rng->length()) {
	    error("Number of selected sentences in each pfile must be the same\n");
	}
    } else {
	// dummy sr3_rng
	sr3_rng = new Range("nil",0,0);
    }

    QN_InFtrLabStream* in4_streamp = NULL;
    if (in4_fp) {
        in4_streamp = new QN_InFtrLabStream_PFile(debug_level, "", in4_fp, 1);
	sr4_rng = new Range(sr_str,0,in4_streamp->num_segs());
	fr4_rng = new Range(fr4_str,0,in4_streamp->num_ftrs());
	if (sr1_rng->length() != sr4_rng->length()) {
	    error("Number of selected sentences in each pfile must be the same\n");
	}
    } else {
	// dummy sr4_rng
	sr4_rng = new Range("nil",0,0);
    }

    if (labels_from == 1) 
      lr_rng = new Range(lr_str,0,in1_streamp->num_labs());
    else if (labels_from == 2) 
      lr_rng = new Range(lr_str,0,in2_streamp->num_labs());
    else if (labels_from == 3) 
      lr_rng = new Range(lr_str,0,in3_streamp->num_labs());
    else 
      lr_rng = new Range(lr_str,0,in4_streamp->num_labs());

    QN_OutFtrLabStream* out_streamp
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "",
                                       out_fp,
				       fr1_rng->length(),
				       lr_rng->length(),
                                       1);

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_ftrcombo(*in1_streamp,
		in2_streamp,
		in3_streamp,
		in4_streamp,
		*out_streamp,
		*sr1_rng,
		*sr2_rng,
		*sr3_rng,
		*sr4_rng,
		*fr1_rng,
		*fr2_rng,
		*fr3_rng,
		*fr4_rng,
		*lr_rng,
		pr_str,
		labels_from, 
		ftrop,
		gain1,
		gain2,
		gain3,
		gain4,
		offset);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in1_streamp;
    delete out_streamp;
    delete sr1_rng;
    delete sr2_rng;
    delete sr3_rng;
    delete sr4_rng;
    delete fr1_rng;
    delete lr_rng;
    if (in2_streamp) {
	delete in2_streamp;
	delete fr2_rng;
	if (fclose(in2_fp))
	    error("Couldn't close input2 pfile.");
    }
    if (in3_streamp) {
	delete in3_streamp;
	delete fr3_rng;
	if (fclose(in3_fp))
	    error("Couldn't close input3 pfile.");
    }
    if (in4_streamp) {
	delete in4_streamp;
	delete fr4_rng;
	if (fclose(in4_fp))
	    error("Couldn't close input4 pfile.");
    }

    if (fclose(in1_fp))
        error("Couldn't close input1 pfile.");

    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
