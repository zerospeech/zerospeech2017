/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_stats.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $
  
    This program computes some simple statistics on pfiles such
    as the mean, stds, minimum, maximum, locations thereof, and
    histograms of each feature.

    Written by Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "error.h"
#include "Range.H"

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;

typedef struct { 
  size_t sent_no;
  size_t frame_no;
} PfileLocation;

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
	    "-o <file-name>  output file ('-' for stdout)\n"
	    "-sr range       sentence range\n"
	    "-fr range       feature range\n"
	    "-pr range       per-sentence frame range\n"
	    "-h #            number of histogram bins (default 0)\n"
	    "-q              quite mode\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
            "Output is: featnum mean std max @sent# @frame# min @sent# @frame# max/stds min/stds [histogram]\n"
    );
    exit(EXIT_FAILURE);
}




static void
pfile_stats(QN_InFtrLabStream& in_stream,FILE *out_fp,
	    Range& srrng,
	    Range& frrng,
	    const char*pr_str,
	    const size_t hist_bins, 
	    const bool quiet_mode)
{

    // Feature and label buffers are dynamically grown as needed.

    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const size_t n_ftrs = in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];

    size_t total_frames = 0;
    double *const ftr_sum = new double [frrng.length()];
    double *const ftr_sumsq = new double [frrng.length()];
    double *const ftr_means = new double [frrng.length()];
    double *const ftr_stds = new double [frrng.length()];
    float *const ftr_maxs = new float [frrng.length()];
    PfileLocation *const ftr_maxs_locs = new PfileLocation [frrng.length()];
    float *const ftr_mins = new float [frrng.length()];
    PfileLocation *const ftr_mins_locs = new PfileLocation [frrng.length()];
    size_t * histogram = NULL;

    double *ftr_sum_p;
    double *ftr_sumsq_p;
    double *ftr_means_p;
    double *ftr_stds_p;
    float *ftr_maxs_p;
    float *ftr_mins_p;
    PfileLocation *ftr_maxs_locs_p;
    PfileLocation *ftr_mins_locs_p;


    // Initialize the above declared arrays
    size_t i,j;
    for (i=0;i<frrng.length();i++) {
      ftr_sum[i] = ftr_sumsq[i] = 0.0;
      ftr_means[i] = ftr_stds[i] = 0.0;
      ftr_maxs[i] = -FLT_MAX;
      ftr_mins[i] = FLT_MAX;
    }

    for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
        const size_t n_frames = in_stream.num_frames((*srit));

	if (!quiet_mode) {
	  if ((*srit) % 100 == 0)
	    printf("Processing sentence %d\n",(*srit));
	}

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
            delete lab_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = n_frames * 2;

            // Allocate new larger buffers.
            ftr_buf = new float[buf_size * n_ftrs];
            lab_buf = new QNUInt32[buf_size * n_labs];
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
	  ftr_sum_p = ftr_sum;
	  ftr_sumsq_p = ftr_sumsq;
	  ftr_maxs_p = ftr_maxs;
	  ftr_mins_p = ftr_mins;
	  ftr_maxs_locs_p = ftr_maxs_locs;
	  ftr_mins_locs_p = ftr_mins_locs;
	  

	  for (Range::iterator frit=frrng.begin();
	       !frit.at_end(); ++frit) {
	    const double val = ftr_buf_p[*frit];
	    *ftr_sum_p++ += val;
	    *ftr_sumsq_p++ += (val*val);
	    if (val > *ftr_maxs_p) {
	      *ftr_maxs_p = val;
	      ftr_maxs_locs_p->sent_no = (*srit);
	      ftr_maxs_locs_p->frame_no = (*prit);
	    }
	    else if (val < *ftr_mins_p) {
	      *ftr_mins_p = val;
	      ftr_mins_locs_p->sent_no = (*srit);
	      ftr_mins_locs_p->frame_no = (*prit);
	    }
	    ftr_maxs_p++;
	    ftr_mins_p++;
	    ftr_maxs_locs_p++;
	    ftr_mins_locs_p++;
	  }
	}
	total_frames += prrng.length();
    }

    if (total_frames == 1) {
      if (!quiet_mode) {
	  printf("WARNING:: Ranges specify using only one frame for statistics.\n");
      }
    }


    ftr_means_p = ftr_means;
    ftr_stds_p = ftr_stds;
    ftr_sum_p = ftr_sum;
    ftr_sumsq_p = ftr_sumsq;
    for (i=0;i<frrng.length();i++) {
      (*ftr_means_p) = (*ftr_sum_p)/total_frames;
      (*ftr_stds_p) = 
	sqrt(
	     ((*ftr_sumsq_p) - (*ftr_sum_p)*(*ftr_sum_p)/total_frames)/
	     total_frames);
      ftr_means_p++;
      ftr_stds_p++;
      ftr_sum_p++;
      ftr_sumsq_p++;
    }

    if (hist_bins > 0) {
      //  go through and do a second pass on the file.
      size_t *hist_p;
      float* ftr_ranges = new float[frrng.length()];
      histogram = new size_t [frrng.length()*hist_bins];
      ::memset(histogram,0,sizeof(size_t)*frrng.length()*hist_bins);
      for (i=0;i<frrng.length();i++)
	ftr_ranges[i] = ftr_maxs[i]-ftr_mins[i];

      if (!quiet_mode) {
	printf("Computing histograms..\n");
      }
      for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {

	  const size_t n_frames = in_stream.num_frames((*srit));

	  if (!quiet_mode) {
	    if ((*srit) % 100 == 0)
	      printf("Processing sentence %d\n",(*srit));
	  }

	  if (n_frames == QN_SIZET_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't find number of frames "
		      "at sentence %lu in input pfile.\n",
		      program_name, (unsigned long) (*srit));
	      error("Aborting.");
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
	    hist_p = histogram;
	    ftr_maxs_p = ftr_maxs;
	    ftr_mins_p = ftr_mins;
	    float *ftr_ranges_p = ftr_ranges;
	    
	    for (Range::iterator frit=frrng.begin();
		 !frit.at_end(); ++frit) {
	      const double val = ftr_buf_p[*frit];
	      size_t ind = size_t(
				  hist_bins*0.9999*
				  (val-*ftr_mins_p)/(*ftr_ranges_p));
	      hist_p[ind]++;
	      hist_p += hist_bins;
	      ftr_maxs_p++;
	      ftr_mins_p++;
	      ftr_ranges_p++;
	    }
	  }
      }
      delete ftr_ranges;
    }

    ftr_means_p = ftr_means;
    ftr_stds_p = ftr_stds;    
    ftr_sum_p = ftr_sum;
    ftr_sumsq_p = ftr_sumsq;
    ftr_maxs_p = ftr_maxs;
    ftr_mins_p = ftr_mins;
    ftr_maxs_locs_p = ftr_maxs_locs;
    ftr_mins_locs_p = ftr_mins_locs;

    double max_maxs_stds=-FLT_MAX;
    double min_mins_stds=+FLT_MAX;
    size_t *hist_p = histogram;
    for (i=0;i<frrng.length();i++) {
      const double maxs_stds = (*ftr_maxs_p)/(*ftr_stds_p);
      const double mins_stds = (*ftr_mins_p)/(*ftr_stds_p);
      fprintf(out_fp,"%zu %g %g %g %zu %zu %g %zu %zu %g %g ",i,
	     *ftr_means_p,*ftr_stds_p,
	     *ftr_maxs_p,ftr_maxs_locs_p->sent_no,ftr_maxs_locs_p->frame_no,
	     *ftr_mins_p,ftr_mins_locs_p->sent_no,ftr_mins_locs_p->frame_no,
	     maxs_stds,mins_stds);
      if (hist_bins > 0) {
	for (j=0;j<hist_bins;j++) {
	  fprintf(out_fp,"%lu ",*hist_p++);
	}
      }
      fprintf(out_fp,"\n");

      ftr_means_p++;
      ftr_stds_p++;
      ftr_maxs_p++;
      ftr_mins_p++;
      ftr_maxs_locs_p++;
      ftr_mins_locs_p++;
      if (maxs_stds > max_maxs_stds)
	max_maxs_stds = maxs_stds;
      if (mins_stds < min_mins_stds)
	min_mins_stds = mins_stds;
    }
    if (!quiet_mode) {
      printf("total sents used = %d, total frames used = %zu\n",
	      srrng.length(),total_frames);
      printf("max_maxs_stds = %f, min_mins_stds = %f\n",
	     max_maxs_stds,min_mins_stds);
    }

    delete ftr_buf;
    delete lab_buf;

    delete ftr_sum;
    delete ftr_sumsq;    
    delete ftr_means;
    delete ftr_stds;
    delete ftr_maxs;
    delete ftr_maxs_locs;
    delete ftr_mins;
    delete ftr_mins_locs;

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

    const char *input_fname = 0;   // Input pfile name.
    const char *output_fname = 0;   // Input ASCII name.

    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string
    Range *fr_rng;
    const char *pr_str = 0;   // per-sentence range string

    size_t hist_bins = 0;
    int debug_level = 0;
    bool quiet_mode = false;

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
        else if (strcmp(argp, "-h")==0)
        {
            if (argc>0)
            {
                // Next argument is number of histogram bins.
	        hist_bins = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No histogram bins value given.");
        }
        else if (strcmp(argp, "-q")==0)
        {
	  quiet_mode = true;
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
	error("Couldn't open output file for writting.");
      }
    }

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);

    sr_rng = new Range(sr_str,0,in_streamp->num_segs());
    fr_rng = new Range(fr_str,0,in_streamp->num_ftrs());

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_stats(*in_streamp,out_fp,
		*sr_rng,*fr_rng,pr_str,
		hist_bins,quiet_mode);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in_streamp;
    delete sr_rng;
    delete fr_rng;
    if (fclose(in_fp))
        error("Couldn't close input pfile.");
    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
