/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_gaussian.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $
  
    This program normalizes the features in a pfile to be
    Gaussian distributed with zero mean and unit variance. 
    The features are scaled to be within +/- k standard deviations.
    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "error.h"
#include "Range.H"

#define MAXHISTBINS 1000


#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;

typedef struct { 
  size_t sent_no;
  size_t frame_no;
} PfileLocation;


extern size_t bin_search(float *array,
			 size_t length, // the length of the array
			 float val);     // the value to search for.



double
normal_func(double z)
{
  return 0.5*(1.0 + erf(M_SQRT1_2*z));
}

double 
inverse_normal_func(double p)
{
	/* 
           Source: This routine was derived (using f2c) from the 
                   FORTRAN subroutine MDNRIS found in 
                   ACM Algorithm 602 obtained from netlib.

                   MDNRIS code contains the 1978 Copyright 
                   by IMSL, INC. .  Since MDNRIS has been 
                   submitted to netlib it may be used with 
                   the restriction that it may only be 
                   used for noncommercial purposes and that
                   IMSL be acknowledged as the copyright-holder
                   of the code.
        */

	/* Initialized data */
	static double eps = 1e-10;
	static double g0 = 1.851159e-4;
	static double g1 = -.002028152;
	static double g2 = -.1498384;
	static double g3 = .01078639;
	static double h0 = .09952975;
	static double h1 = .5211733;
	static double h2 = -.06888301;
	static double sqrt2 = M_SQRT2; // 1.414213562373095;

	/* Local variables */
	static double a, w, x;
	static double sd, wi, sn, y;

	double inverse_error_func(double p);

	/* Note: 0.0 < p < 1.0 */
	/* assert ( 0.0 < p && p < 1.0 ); */

	/* p too small, compute y directly */
	if (p <= eps) {
		a = p + p;
		w = sqrt(-(double)log(a + (a - a * a)));

		/* use a rational function in 1.0 / w */
		wi = 1.0 / w;
		sn = ((g3 * wi + g2) * wi + g1) * wi;
		sd = ((wi + h2) * wi + h1) * wi + h0;
		y = w + w * (g0 + sn / sd);
		y = -y * sqrt2;
	} else {
		x = 1.0 - (p + p);
		y = inverse_error_func(x);
		y = -sqrt2 * y;
	}
	return(y);
} 

double 
inverse_error_func(double p) 
{
	/* 
           Source: This routine was derived (using f2c) from the 
                   FORTRAN subroutine MERFI found in 
                   ACM Algorithm 602 obtained from netlib.

                   MDNRIS code contains the 1978 Copyright 
                   by IMSL, INC. .  Since MERFI has been 
                   submitted to netlib, it may be used with 
                   the restriction that it may only be 
                   used for noncommercial purposes and that
                   IMSL be acknowledged as the copyright-holder
                   of the code.
        */



	/* Initialized data */
	static double a1 = -.5751703;
	static double a2 = -1.896513;
	static double a3 = -.05496261;
	static double b0 = -.113773;
	static double b1 = -3.293474;
	static double b2 = -2.374996;
	static double b3 = -1.187515;
	static double c0 = -.1146666;
	static double c1 = -.1314774;
	static double c2 = -.2368201;
	static double c3 = .05073975;
	static double d0 = -44.27977;
	static double d1 = 21.98546;
	static double d2 = -7.586103;
	static double e0 = -.05668422;
	static double e1 = .3937021;
	static double e2 = -.3166501;
	static double e3 = .06208963;
	static double f0 = -6.266786;
	static double f1 = 4.666263;
	static double f2 = -2.962883;
	static double g0 = 1.851159e-4;
	static double g1 = -.002028152;
	static double g2 = -.1498384;
	static double g3 = .01078639;
	static double h0 = .09952975;
	static double h1 = .5211733;
	static double h2 = -.06888301;

	/* Local variables */
	static double a, b, f, w, x, y, z, sigma, z2, sd, wi, sn;

	x = p;

	/* determine sign of x */
	if (x > 0)
		sigma = 1.0;
	else
		sigma = -1.0;

	/* Note: -1.0 < x < 1.0 */

	z = fabs(x);

	/* z between 0.0 and 0.85, approx. f by a 
	   rational function in z  */

	if (z <= 0.85) {
		z2 = z * z;
		f = z + z * (b0 + a1 * z2 / (b1 + z2 + a2 
		    / (b2 + z2 + a3 / (b3 + z2))));

	/* z greater than 0.85 */
	} else {
		a = 1.0 - z;
		b = z;

		/* reduced argument is in (0.85,1.0), 
		   obtain the transformed variable */

		w = sqrt(-(double)log(a + a * b));

		/* w greater than 4.0, approx. f by a 
		   rational function in 1.0 / w */

		if (w >= 4.0) {
			wi = 1.0 / w;
			sn = ((g3 * wi + g2) * wi + g1) * wi;
			sd = ((wi + h2) * wi + h1) * wi + h0;
			f = w + w * (g0 + sn / sd);

		/* w between 2.5 and 4.0, approx. 
		   f by a rational function in w */

		} else if (w < 4.0 && w > 2.5) {
			sn = ((e3 * w + e2) * w + e1) * w;
			sd = ((w + f2) * w + f1) * w + f0;
			f = w + w * (e0 + sn / sd);

		/* w between 1.13222 and 2.5, approx. f by 
		   a rational function in w */
		} else if (w <= 2.5 && w > 1.13222) {
			sn = ((c3 * w + c2) * w + c1) * w;
			sd = ((w + d2) * w + d1) * w + d0;
			f = w + w * (c0 + sn / sd);
		}
	}
	y = sigma * f;
	return(y);
}



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
	    "-o <file-name>  output pfile ('-' for stdout)\n"
	    "-sr range       sentence range\n"
	    "-fr range       feature range\n"
	    "-pr range       per-sentence frame range\n"
	    "-h #            number of histogram bins (default 0)\n"
            "-oh <file-name> statistics output file (if desired)\n"
            "-ih <file-name> read in statistics (rather than calculating)\n"
            "-d #            number of Gaussian standard deviations\n"
	    "-u              Output is uniform[0,1] distributed rather than Gaussian.\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}

double
histc_lookup(float *domain,
	     double *range,
	     size_t length,
	     float value,
	     double inv_dom_step)
{
  double r;
  size_t loc = bin_search(domain,length,value);

  
  if (value <= domain[0]) 
    r = range[0];
  else if (loc == (length-1))
    r = range[length-1];
  else
    r = (range[loc] + 
	 (range[loc+1]-range[loc])*
	 (value - domain[loc])*inv_dom_step);
  return r;
}




static void
pfile_gaussian(QN_InFtrLabStream& in_stream,
	       QN_OutFtrLabStream& out_stream,
	       FILE *in_st_fp,
	       FILE *out_st_fp,
	       Range& srrng,
	       Range& frrng,
	       const char*pr_str,
	       const size_t hist_bins, 
	       const float num_stds,
	       const bool uniform_output)
{
    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const size_t n_ftrs = in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    float *ftr_buf_p;


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

    const double p_num_stds = 1.0 - normal_func(fabs(num_stds));
    const double p_num_stds_mul = 1.0 - 2.0*p_num_stds;

    size_t * histogram = NULL;
    size_t * hist_p = NULL;
    float* ftr_ranges = NULL;
    // cumulative hist function.
    float *histc_dom;
    double *histc_rng;

    double *ftr_sum_p;
    double *ftr_sumsq_p;
    double *ftr_means_p;
    double *ftr_stds_p;
    float *ftr_maxs_p;
    float *ftr_mins_p;
    float *ftr_ranges_p;
    PfileLocation *ftr_maxs_locs_p;
    PfileLocation *ftr_mins_locs_p;

    // 
    // Initialize the above declared arrays
    size_t i,j;
    for (i=0;i<frrng.length();i++) {
      ftr_sum[i] = ftr_sumsq[i] = 0.0;
      ftr_means[i] = ftr_stds[i] = 0.0;
      ftr_maxs[i] = -FLT_MAX;
      ftr_mins[i] = FLT_MAX;
    }

    ftr_ranges = new float[frrng.length()];
    histogram = new size_t [frrng.length()*hist_bins];

    //
    // If we have been given an input stats file, read it (else calculate)
    if (in_st_fp) {

	ftr_means_p = ftr_means;
	ftr_stds_p = ftr_stds;    
	ftr_maxs_p = ftr_maxs;
	ftr_mins_p = ftr_mins;
	ftr_maxs_locs_p = ftr_maxs_locs;
	ftr_mins_locs_p = ftr_mins_locs;
	ftr_ranges_p = ftr_ranges;
	size_t *hist_p = histogram;
	int rc,j;

	int hist_tot, last_hist_tot = -1;

	for (i=0;i<frrng.length();i++) {
	    double maxs_stds, mins_stds;
	    rc = fscanf(in_st_fp,"%d %lf %lf %f %zu %zu %f %zu %zu %lf %lf ",&j,
		    ftr_means_p,ftr_stds_p,
		    ftr_maxs_p,
		    &ftr_maxs_locs_p->sent_no,&ftr_maxs_locs_p->frame_no,
		    ftr_mins_p,
		    &ftr_mins_locs_p->sent_no,&ftr_mins_locs_p->frame_no,
		    &maxs_stds,&mins_stds);
	    if (rc != 11 || j != i) {
		fprintf(stderr,
		       "%s: Error reading input stats file in lead-in %zu.\n",
		       program_name, i);
		error("Aborting.");
	    }
		
	    *ftr_ranges_p = *ftr_maxs_p - *ftr_mins_p;
	    if (hist_bins > 0) {
		hist_tot = 0;
		for (j=0;j<hist_bins;j++) {
		    if (fscanf(in_st_fp,"%zu ", hist_p) != 1) {
			fprintf(stderr,
				"%s: Error reading input stats file, "
				"el %zu bin %d.\n",
				program_name, i, j);
			error("Aborting.");
		    }
		    hist_tot += *hist_p++;
		}
		if (last_hist_tot != -1 && hist_tot != last_hist_tot) {
		    fprintf(stderr, 
			    "%s: Error reading histogram: for for el %zu had "
			    "%d entries, %zu had %d\n", program_name, 
			    i, hist_tot, i-1, last_hist_tot);
		    error("Aborting.");
		}
	    }
	    // fprintf(out_st_fp,"\n");

	    ftr_means_p++;
	    ftr_stds_p++;
	    ftr_maxs_p++;
	    ftr_mins_p++;
	    ftr_maxs_locs_p++;
	    ftr_mins_locs_p++;
	    ftr_ranges_p++;
	}

	// We don't need to make a pre-pass through the data, but we 
	// *do* need to calculate the total number of frames...
	// .. and also to realloc the ftr buf to be the largest
	int max_n_frames = buf_size;
	for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	    const size_t n_frames = in_stream.num_frames((*srit));

	    Range prrng(pr_str,0,n_frames);
	    //total_frames += prrng.length();
	    // Buffer is sized to hold all the segment frames, pre-iterator
	    if (n_frames > max_n_frames) {
		max_n_frames = n_frames;
	    }
	}

	// Actually, use the total_frames from the hist, not the input
	total_frames = hist_tot;


	if (max_n_frames > buf_size) {
	    // Free old buffers.
	    delete ftr_buf;
	    delete lab_buf;
	    
	    // Resize to max required
	    buf_size = max_n_frames;
	    
	    // Allocate new larger buffers.
	    ftr_buf = new float[buf_size * n_ftrs];
	    lab_buf = new QNUInt32[buf_size * n_labs];
	}

    } else {

	printf("Computing pfile feature ranges...\n");
	//
	// Go through input pfile to get the initial statistics,
	// i.e., max, min, mean, std, etc.
	for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	    const size_t n_frames = in_stream.num_frames((*srit));

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
		*ftr_sumsq_p++ += (val)*(val);
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
	  printf("WARNING:: Ranges specify using only one frame for statistics.\n");
	}


	// 
	// actually compute the statistics.
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


	//
	// Now go though input pfile again, computing the histogram
	// for each feature, using hist_bins between the extreme
	// values for each feature.
	::memset(histogram,0,sizeof(size_t)*frrng.length()*hist_bins);
	for (i=0;i<frrng.length();i++)
	  ftr_ranges[i] = ftr_maxs[i]-ftr_mins[i];
	printf("Computing histograms...\n");
	for (Range::iterator srit1=srrng.begin();!srit1.at_end();srit1++) {

	  const size_t n_frames = in_stream.num_frames((*srit1));

	  if ((*srit1) % 100 == 0)
	    printf("Processing sentence %d\n",(*srit1));

	  if (n_frames == QN_SIZET_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't find number of frames "
		      "at sentence %lu in input pfile.\n",
		      program_name, (unsigned long) (*srit1));
	      error("Aborting.");
	    }

	  const QN_SegID seg_id = in_stream.set_pos((*srit1), 0);

	  if (seg_id == QN_SEGID_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't seek to start of sentence %lu "
		      "in input pfile.",
		      program_name, (unsigned long) (*srit1));
	      error("Aborting.");
	    }

	  const size_t n_read =
	    in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);

	  if (n_read != n_frames)
	    {
	      fprintf(stderr, "%s: At sentence %lu in input pfile, "
		      "only read %lu frames when should have read %lu.\n",
		      program_name, (unsigned long) (*srit1),
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
	      const size_t ind = 
		size_t(
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
    }

    printf("Creating mapping functions...\n");
    // 
    // save the statistics if desired.
    if (out_st_fp != NULL) {
      ftr_means_p = ftr_means;
      ftr_stds_p = ftr_stds;    
      ftr_maxs_p = ftr_maxs;
      ftr_mins_p = ftr_mins;
      ftr_maxs_locs_p = ftr_maxs_locs;
      ftr_mins_locs_p = ftr_mins_locs;
      size_t *hist_p = histogram;
      for (i=0;i<frrng.length();i++) {
	const double maxs_stds = (*ftr_maxs_p)/(*ftr_stds_p);
	const double mins_stds = (*ftr_mins_p)/(*ftr_stds_p);
	fprintf(out_st_fp,"%zu %g %g %g %zu %zu %g %zu %zu %g %g ",i,
		*ftr_means_p,*ftr_stds_p,
		*ftr_maxs_p,ftr_maxs_locs_p->sent_no,ftr_maxs_locs_p->frame_no,
		*ftr_mins_p,ftr_mins_locs_p->sent_no,ftr_mins_locs_p->frame_no,
		maxs_stds,mins_stds);
	if (hist_bins > 0) {
	  for (j=0;j<hist_bins;j++) {
	    fprintf(out_st_fp,"%zu ",*hist_p++);
	  }
	}
	fprintf(out_st_fp,"\n");

	ftr_means_p++;
	ftr_stds_p++;
	ftr_maxs_p++;
	ftr_mins_p++;
	ftr_maxs_locs_p++;
	ftr_mins_locs_p++;
      }
    }

    // 
    // create the normalized cumulative feature distribution functions.
    histc_dom = new float [frrng.length()*(hist_bins+1)];
    ::memset(histc_dom,0,sizeof(float)*frrng.length()*(hist_bins+1));
    histc_rng = new double [frrng.length()*(hist_bins+1)];
    ::memset(histc_rng,0,sizeof(float)*frrng.length()*(hist_bins+1));    

    ftr_mins_p = ftr_mins;
    ftr_maxs_p = ftr_maxs;
    hist_p = histogram;
    ftr_ranges_p = ftr_ranges;
    float *histc_dom_p = histc_dom;
    double *histc_rng_p = histc_rng;
    const double inv_total_frames = 1.0/total_frames;
    for (i=0;i<frrng.length();i++) {
      const double dom_step = (*ftr_ranges_p)/hist_bins;
      *histc_dom_p++ = *ftr_mins_p;
      *histc_rng_p++ = 0.0;
      int cumfr = 0;
      for (j=1;j<hist_bins;j++) {
	*histc_dom_p++ = (*ftr_mins_p) + j*dom_step;
	// *histc_rng_p = histc_rng_p[-1] + (*hist_p)*inv_total_frames;
	cumfr += *hist_p;
	*histc_rng_p = cumfr*inv_total_frames;
	histc_rng_p++; hist_p++;
      }
      *histc_dom_p++ = *ftr_maxs_p;
      *histc_rng_p++ = 1.0;
      hist_p++;

      ftr_mins_p++;
      ftr_maxs_p++;
      ftr_ranges_p++;
    }


    //
    // go through and linearly warp the range values so that the
    // resulting distribution will be within +/- num_stds of
    // the mean (which is zero in this case)
    if (!uniform_output) {
      histc_rng_p = histc_rng;

      for (i=0;i<frrng.length();i++) {
	for (j=0;j<(hist_bins+1);j++) {
	  *histc_rng_p = 
	    p_num_stds + (*histc_rng_p)*p_num_stds_mul;
	  histc_rng_p ++;
	}
      }
    }
    
    // Now once again, go through the input pfile, warp
    // the features, and write out to a new pfile of the same
    // size and same labels.

    float *oftr_buf = new float[buf_size * frrng.length()];
    float *oftr_buf_p;

    printf("Writting warped output pfile...\n");
    for (Range::iterator srit2=srrng.begin();!srit2.at_end();srit2++) {

      const size_t n_frames = in_stream.num_frames((*srit2));

      if ((*srit2) % 100 == 0)
	  printf("Processing sentence %d\n",(*srit2));

      if (n_frames == QN_SIZET_BAD)
	{
	  fprintf(stderr,
		  "%s: Couldn't find number of frames "
		  "at sentence %lu in input pfile.\n",
		  program_name, (unsigned long) (*srit2));
	  error("Aborting.");
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

      // Normalize the features.
      // Do it in n_read order for better cache behavior.
      histc_dom_p = histc_dom;
      histc_rng_p = histc_rng;
      ftr_ranges_p = ftr_ranges;
      Range prrng(pr_str,0,n_frames);
      float *oftr_buf_base = oftr_buf;
      for (Range::iterator frit=frrng.begin();
	   !frit.at_end(); ++frit) {
	float *ftr_buf_base = &ftr_buf[(*frit)];
	ftr_buf_p = ftr_buf_base;
	oftr_buf_p = oftr_buf_base;
	const double inv_dom_step = hist_bins/(*ftr_ranges_p);


	if (uniform_output) {
	  for (Range::iterator prit=prrng.begin();
	       !prit.at_end() ; ++prit) {
	    ftr_buf_p = ftr_buf_base + (*prit)*n_ftrs;
	    *oftr_buf_p = histc_lookup(histc_dom_p,
				       histc_rng_p,
				       (hist_bins+1),
				       *ftr_buf_p,
				       inv_dom_step);
	    oftr_buf_p += frrng.length();
	  }
	} else {
	  for (Range::iterator prit=prrng.begin();
	       !prit.at_end() ; ++prit) {
	    ftr_buf_p = ftr_buf_base + (*prit)*n_ftrs;
 	    const double tmp =  histc_lookup(histc_dom_p,
				histc_rng_p,
				(hist_bins+1),
				*ftr_buf_p,
				inv_dom_step);
	    if (!(0.0 < tmp && tmp < 1.0)) {
	      fprintf(stderr,"Error: computing inverse Gaussian of %f at %d/%d/%d\n"
		      "Perhaps try reducing the number of stds\n",tmp, 
		      *srit2, *prit, *frit);
	      exit(-1);
	    }
	    *oftr_buf_p = inverse_normal_func(tmp);
	    oftr_buf_p += frrng.length();
	  }
	}

	histc_dom_p += (hist_bins+1);
	histc_rng_p += (hist_bins+1);
	ftr_ranges_p ++;
	oftr_buf_base++;
      }


      // Write output.
      out_stream.write_ftrslabs(n_frames, oftr_buf, lab_buf);
      out_stream.doneseg((QN_SegID) seg_id);

    }
    printf("...done\n");

    delete histc_rng;
    delete histc_dom;
    delete histogram;
    delete ftr_ranges;
    delete ftr_buf;
    delete oftr_buf;
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
    const char *output_fname = 0;   // Output pfile name.
    const char *input_st_fname = 0;   // Input stats file name.    
    const char *output_st_fname = 0;   // Output stats file name.    

    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string
    Range *fr_rng;
    const char *pr_str = 0;   // per-sentence range string

    size_t hist_bins = 0;
    int debug_level = 0;
    float num_stds = 5;
    bool uniform_output = false;

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
        else if (strcmp(argp, "-oh")==0)
        {
            // Output stats file name.
            if (argc>0)
            {
                // Next argument is output file name.
                output_st_fname = *argv++;
                argc--;
            }
            else
                usage("No stat output filename given.");
        }
        else if (strcmp(argp, "-ih")==0)
        {
            // Input stats file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input_st_fname = *argv++;
                argc--;
            }
            else
                usage("No stat input filename given.");
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
        else if (strcmp(argp, "-u")==0)
        {
	  uniform_output = true;
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
        else if (strcmp(argp, "-d")==0)
        {
            if (argc>0)
            {
                // Next argument is number of std for range.
	        num_stds = parse_float(*argv++);
                argc--;
            }
            else
                usage("No num of standard deviations value given.");
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
    FILE *in_st_fp;
    if (input_st_fname==0)
      in_st_fp = 0;
    else if (!strcmp(input_st_fname,"-"))
      in_st_fp = stdin;
    else {
      if ((in_st_fp = fopen(input_st_fname, "r")) == NULL) {
	error("Couldn't open stat input file for reading.");
      }
    }
    FILE *out_st_fp;
    if (output_st_fname==0)
      out_st_fp = 0;
    else if (!strcmp(output_st_fname,"-"))
      out_st_fp = stdout;
    else {
      if ((out_st_fp = fopen(output_st_fname, "w")) == NULL) {
	error("Couldn't open stat output file for writing.");
      }
    }
    if (hist_bins < 2) {
      error("Invalid value for number of histogram bins\n");
    }


    //////////////////////////////////////////////////////////////////////
    // Set IEEE FP exception masks
    //////////////////////////////////////////////////////////////////////


#ifdef HAVE_IEEE_FLAGS
    char *out;
    ieee_flags("set",
	       "exception",
	       "invalid",
	       &out);
    ieee_flags("set",
	       "exception",
	       "division",
	       &out);
#else
#  ifdef HAVE_FPSETMASK
    fpsetmask(
	      FP_X_INV      /* invalid operation exception */
	      /* | FP_X_OFL */     /* overflow exception */
	      /* | FP_X_UFL */     /* underflow exception */
	      | FP_X_DZ       /* divide-by-zero exception */
	      /* | FP_X_IMP */      /* imprecise (loss of precision) */
	      );
#  else // create a syntax error.
//#    error No way known to trap FP exceptions
#  endif
#endif

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);

    sr_rng = new Range(sr_str,0,in_streamp->num_segs());
    fr_rng = new Range(fr_str,0,in_streamp->num_ftrs());

    QN_OutFtrLabStream* out_streamp
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "",
                                       out_fp,
				       fr_rng->length(),
                                       in_streamp->num_labs(),
                                       1);


    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_gaussian(*in_streamp,
		   *out_streamp,
		   in_st_fp, 
		   out_st_fp,
		   *sr_rng,*fr_rng,pr_str,
		   hist_bins,
		   num_stds,
		   uniform_output);

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
