/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_norm.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $
  
    This program normalizes the features in the current pfile to have
    zero mean and unit variance and optionally selects a subset of
    the features as in pfile_select.

    Written by Jeff Bilmes <bilmes@cs.berkeley.edu>
    updated to include per-utterance/per-group normalization
      by Eric Fosler-Lussier <fosler@icsi.berkeley.edu>
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "error.h"
#include "Range.H"

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;
int quiet;
int debug;

static void
usage(const char* message = 0)
{
    if (message)
        fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help            print this message\n"
	    "-i <file-name>   input pfile\n"
	    "-o <file-name>   output file ('-' for stdout)\n"
	    "-sr range        sentence range\n"
	    "-fr range        feature range\n"
	    "-pr range        per-sentence frame range\n"
	    "-sl <int>        segment group length (def: all)\n"
	    "-slf <file-name> ascii file with segment group lengths\n"
	    "-mean f          resulting pfile has mean f\n"
            "-std f           resulting pfile has std f\n"
	    "-q               quiet mode\n"
	    "-debug <level>   number giving level of debugging output to produce 0=none.\n"
	    //            "Output is: featnum mean std max @sent# @frame# min @sent# @frame# max/stds min/stds [histogram]\n"
    );
    exit(EXIT_FAILURE);
}



static void
pfile_norm(QN_InFtrLabStream& in_stream,
	   QN_OutFtrLabStream& out_stream,
	   Range& srrng,
	   Range& frrng,
	   Range& lrrng,
	   const char *pr_str,
	   const double result_mean,
	   const double result_std,
	   int *seg_markers)
{
    // Feature and label buffers are dynamically grown as needed.

    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const size_t n_ftrs = in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    float *oftr_buf = new float[buf_size * frrng.length()];

    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];
    QNUInt32* olab_buf = new QNUInt32[buf_size * n_labs];

    size_t total_frames = 0;
    double *const ftr_means = new double [frrng.length()];
    double *const ftr_stds = new double [frrng.length()];

    double *ftr_means_p;
    double *ftr_stds_p;

    // new outer loop: for every group of segments, do this loop once.
    // thus, to imitate the behavior of pfile_normutts, this is done for
    // every sentence

    // Set 2 iterators to work in parallel: 1 to do mean/variance calcs,
    // other to do application of means and variances.
    int cur_seg_group;
    Range::iterator srit=srrng.begin();
    Range::iterator srit2=srrng.begin();
    for (cur_seg_group=seg_markers[(*srit)]; 
	 !srit.at_end();) {
      
      // Initialize the above declared arrays
      size_t i;
      for (i=0;i<frrng.length();i++) {
	ftr_means[i] = ftr_stds[i] = 0.0;
      }
      total_frames=0;
      
      if (!quiet)
	printf("Computing means and variances from utt %d onward.\n",(*srit));

      // instead of going through entire range, only calculate ranges in 
      // same seg_group
      for (;!srit.at_end() && seg_markers[(*srit)]==cur_seg_group;srit++) {
        const size_t n_frames = in_stream.num_frames((*srit));
	
	if (debug > 0) 
	  printf("Processing sentence %d\n",(*srit));
	else if (!quiet && (*srit) % 100 == 0)
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

	Range prrng(pr_str,0,n_frames);
	for (Range::iterator prit=prrng.begin();
	     !prit.at_end() ; ++prit) {

	  const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	  ftr_means_p = ftr_means;
	  ftr_stds_p = ftr_stds;

	  for (Range::iterator frit=frrng.begin();
	       !frit.at_end(); ++frit) {
	    const double val = ftr_buf_p[*frit];
	    *ftr_means_p++ += val;
	    *ftr_stds_p++ += (val*val);
	  }
	}
	total_frames += prrng.length();
      }
      // update the seg group
      if (!srit.at_end())
	cur_seg_group=seg_markers[(*srit)];

      if (total_frames == 1) {
	printf("WARNING:: Ranges specify using only one frame for statistics.\n");
      }

      if (total_frames == 0) {
	error("ERROR, must have more than 0 frames in pfile");
      }

      ftr_means_p = ftr_means;
      ftr_stds_p = ftr_stds;
      for (i=0;i<frrng.length();i++) {
	(*ftr_stds_p) = 
	  (((*ftr_stds_p) - (*ftr_means_p)*(*ftr_means_p)/total_frames)/
	   total_frames);
	if (*ftr_stds_p < DBL_MIN) {
	  error("ERROR, computed variance is too small %e\n",*ftr_stds_p);
	} else 
	  *ftr_stds_p  = 1.0/sqrt(*ftr_stds_p);
	(*ftr_means_p) = (*ftr_means_p)/total_frames;
	ftr_means_p++;
	ftr_stds_p++;
      }
      
      if (!quiet) {
	if (srit.at_end())
	  printf("Normalizing from utt %d to utt %d.\n",(*srit2),srrng.last());
	else
	  printf("Normalizing from utt %d to utt %d.\n",(*srit2),(*srit)-1);
      }
      for (;(*srit2)<(*srit) || (srit.at_end() && !srit2.at_end());srit2++) {
        const size_t n_frames = in_stream.num_frames((*srit2));
	
	Range prrng(pr_str,0,n_frames);

	if (debug > 0) 
	  printf("Processing sentence %d\n",(*srit2));
	else if (!quiet && (*srit2) % 100 == 0)
	  printf("Processing sentence %d\n",(*srit2));
	
        if (n_frames == QN_SIZET_BAD)
	  {
            fprintf(stderr,
		    "%s: Couldn't find number of frames "
		    "at sentence %lu in input pfile.\n",
		    program_name, (unsigned long) (*srit2));
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
	
        const QN_SegID seg_id = in_stream.set_pos((*srit2), 0);
	
        if (seg_id == QN_SEGID_BAD)
	  {
            fprintf(stderr,
		    "%s: Couldn't seek to start of sentence %lu "
		    "in input pfile.",
		    program_name, (unsigned long) (*srit2));
            error("Aborting.");
	  }
	
        const size_t n_read =
	  in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);
	
        if (n_read != n_frames)
	  {
            fprintf(stderr, "%s: At sentence %lu in input pfile, "
		    "only read %lu frames when should have read %lu.\n",
		    program_name, (unsigned long) (*srit2),
		    (unsigned long) n_read, (unsigned long) n_frames);
            error("Aborting.");
	  }
	
	float *oftr_buf_p = oftr_buf;
	QNUInt32* olab_buf_p = olab_buf;
	for (Range::iterator prit=prrng.begin();
	     !prit.at_end(); ++prit) {
	  float *ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	  QNUInt32* lab_buf_p = lab_buf + (*prit)*n_labs;
	  ftr_means_p = ftr_means;
	  ftr_stds_p = ftr_stds;
	  for (Range::iterator frit=frrng.begin();
	       !frit.at_end(); ++frit) {
	    *oftr_buf_p++ = (ftr_buf_p[*frit] - *ftr_means_p)*(*ftr_stds_p)*result_std + result_mean;
	    ftr_means_p++;
	    ftr_stds_p++;
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
      } // end inner loop... continue with next normalizing group
    } // end outer loop

    delete [] ftr_buf;
    delete [] lab_buf;

    delete [] ftr_means;
    delete [] ftr_stds;
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

static double
parse_double(const char*const s)
{
    size_t len = strlen(s);
    char *ptr;
    double val;
    val = strtod(s, &ptr);
    if (ptr != (s+len))
        error("Not an floating point argument.");
    return val;
}

class tokFile {
    // A class that accesses a text file containing a list of 
    // white-space separated tokens, which may be interpreted as 
    // plain strings or as integers
public:
    FILE *file;
    char *filename;
    int buflen;
    char *buf;
    int bufpos;
    int linect;
    static const char *WS;

    tokFile(FILE* a_file, const char *a_name = "(unknown)") {
        file = a_file;
        filename = strdup(a_name);
        buflen = 1024;
        buf = new char[buflen];
        bufpos = 0;
        buf[bufpos] = '\0';
        linect = 0;
    }
    ~tokFile(void) {
        free(filename);
        delete [] buf;
    }
    int getNextTok(char **ret);
    int getNextInt(int *ret);
    // Shorthand to close our file (even though we didn't open it)
    void close(void) {
        fclose(file);
    }
};
const char *tokFile::WS = " \t\n";

int tokFile::getNextTok(char **ret) {
    // Pull another space-delimited token from an open file handle.
    // File needs just to be white-space
    // delimited, but can have comment lines starting with "#"
    // Pointer to token string written to *ret. 
    // Return 1 on success, 0 at EOF

    while (buf[bufpos] == '\0') {
        // need a new line
        char *ok = fgets(buf, buflen, file);
        if (!ok) {
            // EOF hit, nothing more got
            return 0;
        }
        int got = strlen(buf);
        while (buf[got-1] != '\n' && !feof(file)) {
            // because line didn't end with EOL, we assume we ran out 
            // of buffer space - realloc & keep going
	    // assert(got == buflen-1);
            int newbuflen = 2*buflen;
            char *newbuf = new char[newbuflen];
            memcpy(newbuf, buf, got);
            delete [] buf;
            buf = newbuf;
            buflen = newbuflen;
            fgets(buf+got, buflen-got, file);
            got = strlen(buf);
        }
        ++linect;
        // strip the trailing CR
        if (buf[got-1] == '\n') {
            buf[got-1] = '\0';
        }
        // OK, now we've got a new line, continue
        bufpos = strspn(buf, WS);
        // but if it's a comment line, skip it by pretending it's a blank line
        if (buf[bufpos] == '#') {
            buf[bufpos] = '\0';
        }
    }

    // Find the next space after the token
    // (dlbufpos already points to non-WS)
    int toklen = strcspn(buf+bufpos, WS);
    // I think this has to work
    //    assert(toklen > 0);

    // Save the result
    *ret = buf+bufpos;

    if (buf[bufpos+toklen] != '\0') {
        // There's more after the tok
        // terminate the string at terminal WS
        buf[bufpos+toklen] = '\0';      
        // Skip over the terminated, returned token
        bufpos += toklen+1;
        // Advance pointer to look at following non-WS next time
        bufpos += strspn(buf+bufpos, WS);
    } else {
        // This token is last in string - point to end of buffer for next time
        bufpos += toklen;
    }
    return 1;
}

int tokFile::getNextInt(int *ret) {
    // Read another number from the deslen file as the desired length
    // in frames of the next frame.  File needs just to be white-space
    // delimited, but can have comment lines starting with "#"
    // Next read value is put into *pdeslen.  Return 1 on success, 0 at EOF
    int rc = 0;
    // Get the next number in the line buffer & advance pointer
    char *str, *end;

    if ( (rc = getNextTok(&str)) ) {
        int val = strtol(str, &end, 10);

        // If unparseable, end points to start
        if (end == str) {
            fprintf(stderr, "unable to parse token '%s' as an integer "
                    "at line %d in file '%s'\n", str, linect, filename);
            return 0;
        }

        // Got the number
        *ret = val;
    }
    return rc;
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
    const char *segment_length_fname = 0; // Segment length file name

    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string    
    Range *fr_rng;
    const char *lr_str = 0;   // label range string    
    Range *lr_rng;
    const char *pr_str = 0;   // per-sentence range string    

    QNUInt32 segment_length = INT_MAX; // treat all sentences as one big segment
    double result_mean = 0;
    double result_std = 1;

    quiet = 0;
    debug = 0;

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
	else if (strcmp(argp, "-q")==0)
	{
           quiet=1;
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
                debug = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No debug level given.");
        }
        else if (strcmp(argp, "-mean")==0)
        {
            if (argc>0)
            {
                // Next argument is debug level.
                result_mean = parse_double(*argv++);
                argc--;
            }
            else
                usage("No -mean value.");
        }
        else if (strcmp(argp, "-std")==0)
        {
            if (argc>0)
            {
                // Next argument is debug level.
                result_std = parse_double(*argv++);
                argc--;
            }
            else
                usage("No -std value.");
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
	else if (strcmp(argp, "-sl")==0)
	{
	     if(argc>0)
	     {
	         int i=parse_long(*argv++);
		 if (i<0)
		   usage("Segment length must be >=0");
		 segment_length=(QNUInt32)(i?i:INT_MAX);
		 argc--;
	     }
             else
	       usage("No segment length given");
	}
	else if (strcmp(argp, "-slf")==0)
	{
	     if(argc>0)
	     {
		 segment_length_fname=*argv++;
		 argc--;
	     }
             else
	       usage("No segment length file name given");
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
        = new QN_InFtrLabStream_PFile(debug, "", in_fp, 1);

    size_t pfile_size= in_streamp->num_segs();
    if (pfile_size==QN_SIZET_BAD) 
      error("Internal error: trying to get pfile size and none available");
    int cur_seg=0;
    //    int *seg_markers=new int[pfile_size](0);
    int *seg_markers=new int[pfile_size];
    int i;
    for (i=0; i<pfile_size; ++i) seg_markers[i] = 0;

    // count up total lengths: should match length of pfile
    if (segment_length_fname!=0) {
      FILE *seg_fp;
      tokFile *segTok;      
      QNUInt32 segCount=0;
      int segLen;

      if ((seg_fp=fopen(segment_length_fname, "r"))==NULL)
	error("Couldn't open segment length file for reading");
      segTok=new tokFile(seg_fp,segment_length_fname);
      while(segTok->getNextInt(&segLen)) {
	if ((segCount+(QNUInt32)segLen)<=pfile_size) {
	  for(QNUInt32 i=segCount;i<segCount+(QNUInt32)segLen;i++)
	    seg_markers[i]=cur_seg;
	  cur_seg++;
	}
	segCount+=(QNUInt32)segLen;
      }
      segTok->close();
      delete segTok;

      if (segCount!=pfile_size) {
	char errstr[1024];
	sprintf(errstr,"Mismatch between segment length file (%d utts) and pfile (%zu utts)",segCount,pfile_size);
	error(errstr);
      }
    } else if (segment_length<INT_MAX) {
      for(QNUInt32 i=0;i<pfile_size;i+=segment_length) {
	for(QNUInt32 j=0;j<segment_length && j+i<pfile_size; j++) 
	  seg_markers[i+j]=cur_seg;
	cur_seg++;
      }
    }

    sr_rng = new Range(sr_str,0,in_streamp->num_segs());
    fr_rng = new Range(fr_str,0,in_streamp->num_ftrs());
    lr_rng = new Range(lr_str,0,in_streamp->num_labs());

    QN_OutFtrLabStream* out_streamp
        = new QN_OutFtrLabStream_PFile(debug,
				       "",
                                       out_fp,
				       fr_rng->length(),
				       lr_rng->length(),
                                       1);


    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_norm(*in_streamp,
	       *out_streamp,
	       *sr_rng,*fr_rng,*lr_rng,pr_str,
	       result_mean,result_std,seg_markers);

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
