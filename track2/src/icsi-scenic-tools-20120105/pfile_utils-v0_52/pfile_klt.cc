/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_klt.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $
  
    This program performs a klt analysis on the input pfile
    and outputs the zero-mean, (optionally not) unity variance,  completely
    uncorrelated features, in the order of decreasing eigenvalues.
    By selecting a subset of the output features, the result
    is a Karhunen-Loeve transformation (or principal component
    analysis) of the pfile.

    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include "pfile_utils.h"


#include <assert.h>

#include "QN_PFile.h" 
#include "QN_filters.h"
#include "QN_utils.h"
#include "error.h"
extern "C" {
#include "eig.h"
}
#include "Range.H"

extern "C" void mul_mdmd_md(const int M, const int K, const int N, 
		       const double *const A, const double *const B, double *const C, 
		       const int Astride, const int Bstride, const int Cstride);

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
	    "-o <file-name>  output pfile ('-' for stdout)\n"
	    "-os <file-name>  output stats (covariance, mean, eigenvectors, eigenvalues) matrices binary double precision ('-' for stdout)\n"
	    "-is <file-name> input stats (covariance, mean, eigenvectors, eigenvalues) matrices (i.e., do not compute them).\n"
	    "-a              stat matrices written/read in ascii rather than binary doubles\n"
	    "-sr range       sentence range\n"
	    "-fr range       output feature range\n"
	    "-n              do *not* multiply features by inverse eigenvalues (to not have unity variance)\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}





void
readStats(FILE*f,
	  size_t N,
	  bool ascii,
	  double *cor,
	  double *means,
	  double *vecs,
	  double *vals)
{
  size_t i,j;

  for (i=0;i<N;i++) {
    if (!ascii) {
      for (j=0;j<N;j++) 
	if (!fread(&cor[i*N+j],sizeof(cor[0]),1,f))
	  error("input stat file eof error, cor(%d,%d)\n",i,j);
      if (!fread(&means[i],sizeof(means[0]),1,f))
	  error("input stat file eof error, mean(%d)\n",i);
      for (j=0;j<N;j++) 
	if (!fread(&vecs[i*N+j],sizeof(vecs[0]),1,f))
	  error("input stat file eof error, vecs(%d,%d)\n",i,j);
      if (!fread(&vals[i],sizeof(vals[0]),1,f))
	  error("input stat file eof error, vals(%d)\n",i);
    } else {
      for (j=0;j<N;j++) 
	if (!fscanf(f,"%lf",&cor[i*N+j]))
	  error("input stat file eof error, cor(%d,%d)\n",i,j);
      if (!fscanf(f,"%lf",&means[i]))
	  error("input stat file eof error, mean(%d)\n",i);
      for (j=0;j<N;j++) 
	if (!fscanf(f,"%lf",&vecs[i*N+j]))
	  error("input stat file eof error, vecs(%d,%d)\n",i,j);
      if (!fscanf(f,"%lf",&vals[i]))
	  error("input stat file eof error, vals(%d)\n",i);
    }
  }
  // Could check for eof condition to be
  // true here.
}

void
writeStats(FILE*f,
	  size_t N,
	  bool ascii,
	  double *cor,
	  double *means,
	  double *vecs,
	  double *vals)
{
  size_t i,j;

  for (i=0;i<N;i++) {
    if (ascii) {
      for (j=0;j<N;j++)
	fprintf(f,"%.15f ",cor[i*N+j]);
      fprintf(f,"%.15f ",means[i]);
      for (j=0;j<N;j++)
	fprintf(f,"%.15f ",vecs[i*N+j]);
      fprintf(f,"%.15f\n",vals[i]);
    } else {
      fwrite(&cor[i*N],sizeof(double),N,f);
      fwrite(&means[i],sizeof(double),1,f);
      fwrite(&vecs[i*N],sizeof(double),N,f);
      fwrite(&vals[i],sizeof(double),1,f);
    }
  }
}


static void
pfile_klt(QN_InFtrStream* in_streamp, //bj QN_InFtrLabStream* in_streamp
	  QN_OutFtrStream* out_streamp,  //bj QN_OutFtrLabStream* out_streamp
	  FILE *in_st_fp,
	  FILE *out_st_fp,
	  Range& srrng,Range& frrng,
	  const bool unity_variance,
	  const bool ascii)
{

    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    //bj const size_t n_labs = in_streamp->num_labs();
    const size_t n_ftrs = in_streamp->num_ftrs();
    size_t max_n_frames = 0;

    float *ftr_buf = new float[buf_size * n_ftrs];
    float *ftr_buf_p;
    //bj QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];

    size_t total_frames = 0;

    // mean vector E[X}
    double *const ftr_means = new double [n_ftrs];
    double *ftr_means_p;
    double *const ftr_means_endp = ftr_means+n_ftrs;
    double *ftr_cov;

    double *ftr_eigenvecs;
    double *ftr_eigenvals;

    // 
    // Initialize the above declared arrays
    size_t i,j;
    memset(ftr_means,0,n_ftrs*sizeof(double));


    if (in_st_fp == NULL) {
      // correlation vector (i.e., E[X X^T]
      double *const ftr_cor = new double [n_ftrs*(n_ftrs+1)];
      double *ftr_cor_p;
      memset(ftr_cor,0,n_ftrs*(n_ftrs+1)*sizeof(double)/2);

      printf("Computing pfile feature means and covariance..\n");
      //
      // Go through input pfile to get the initial statistics,
      for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	  const size_t n_frames = in_streamp->num_frames((*srit));

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

	  if (n_frames > max_n_frames)
	    max_n_frames = n_frames;
	  // Increase size of buffers if needed.
	  if (n_frames > buf_size)
	    {
	      // Free old buffers.
	      delete ftr_buf;
	      //bj delete lab_buf;

	      // Make twice as big to cut down on future reallocs.
	      buf_size = n_frames * 2;

	      // Allocate new larger buffers.
	      ftr_buf = new float[buf_size * n_ftrs];
	      //bj lab_buf = new QNUInt32[buf_size * n_labs];
	    }

	  const QN_SegID seg_id = in_streamp->set_pos((*srit), 0);

	  if (seg_id == QN_SEGID_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't seek to start of sentence %lu "
		      "in input pfile.",
		      program_name, (unsigned long) (*srit));
	      error("Aborting.");
	    }

	  const size_t n_read =
            //bj in_streamp->read_ftrslabs(n_frames, ftr_buf, lab_buf);
            in_streamp->read_ftrs(n_frames, ftr_buf);

	  if (n_read != n_frames)
	    {
	      fprintf(stderr, "%s: At sentence %lu in input pfile, "
		      "only read %lu frames when should have read %lu.\n",
		      program_name, (unsigned long) (*srit),
		      (unsigned long) n_read, (unsigned long) n_frames);
	      error("Aborting.");
	    }

	  float *ftr_buf_base = ftr_buf;
	  for (i=0;i<n_read;i++) {
	    size_t j;
	    ftr_means_p = ftr_means;
	    ftr_cor_p = ftr_cor;
	    ftr_buf_p = ftr_buf_base;
	    for (j=0;j<n_ftrs;j++) {
	      *ftr_means_p += *ftr_buf_p;

	      register double tmp = *ftr_buf_p;

	      // Only compute the upper triangular part
	      // since matrix is symmetric.
	      float *ftr_buf_pp = ftr_buf_base+j;
	      for (size_t k=j;k<n_ftrs;k++) {
		*ftr_cor_p++ += (tmp)*(*ftr_buf_pp++);
	      }

	      ftr_means_p++;
	      ftr_buf_p++;
	    }
	    ftr_buf_base += n_ftrs;
	  }



	  total_frames += n_read;
	}

      ftr_cov = new double [n_ftrs*n_ftrs];
      double total_frames_inv = 1.0/total_frames;
      // actually compute the means and covariances.

      ftr_means_p = ftr_means;
      while (ftr_means_p != ftr_means_endp)
	(*ftr_means_p++) *= total_frames_inv;

      ftr_means_p = ftr_means;
      ftr_cor_p = ftr_cor;
      for (i=0;i<n_ftrs;i++) {
	double *ftr_means_pp = ftr_means_p;
	double *ftr_cov_rp = ftr_cov + i*n_ftrs+i; // row pointer
	double *ftr_cov_cp = ftr_cov_rp; // column pointer
	for (j=i;j<n_ftrs;j++) {
	  double tmp;
	  tmp = (*ftr_cor_p++)*total_frames_inv - 
	    (*ftr_means_p)*(*ftr_means_pp);
	  *ftr_cov_rp = *ftr_cov_cp = tmp;
	
	  ftr_cov_rp++;
	  ftr_cov_cp += n_ftrs;
	  ftr_means_pp++;
	}
	ftr_means_p++;
      }

      // don't need any longer
      delete ftr_cor;

      // now compute the eigen vectors and values
      ftr_eigenvecs = new double[n_ftrs*n_ftrs];
      ftr_eigenvals = new double[n_ftrs];
      eigenanalyasis(n_ftrs,ftr_cov,ftr_eigenvals,ftr_eigenvecs);

      // save it
      if (out_st_fp != NULL) 
	writeStats(out_st_fp,n_ftrs,ascii,
		   ftr_cov,ftr_means,
		   ftr_eigenvecs,ftr_eigenvals);

    } else {
      // read in the matrix containing the data.
      ftr_cov = new double [n_ftrs*n_ftrs];
      ftr_eigenvecs = new double[n_ftrs*n_ftrs];
      ftr_eigenvals = new double[n_ftrs];
      readStats(in_st_fp,n_ftrs,ascii,
		   ftr_cov,ftr_means,
		   ftr_eigenvecs,ftr_eigenvals);


      // God knows why, but if the user
      // ask for this, save it
      if (out_st_fp != NULL) 
	writeStats(out_st_fp,n_ftrs,ascii,
		   ftr_cov,ftr_means,
		   ftr_eigenvecs,ftr_eigenvals);

    }


    // at this point we no longer need the following
    delete ftr_cov;


    if (out_streamp != NULL) {
      printf("Writing warped output pfile..\n");
      
      if (unity_variance) {
	// multily in the eigenvalues into the eigenvectors
	double *valp = ftr_eigenvals;
	double *vecp = ftr_eigenvecs;
	for (i=0;i<n_ftrs;i++) {
	  double *vecpp = vecp;
	  double valp_inv = sqrt(1.0/(*valp));;
	  for (j=0;j<n_ftrs;j++) {
	    (*vecpp) *= valp_inv;
	    vecpp+=n_ftrs;
	  }
	  valp++;
	  vecp++;
	}
      }


      // allocate space performing the orthogonalization.
      if (max_n_frames > 0)
	buf_size = max_n_frames;

      float *oftr_buf = new float[buf_size * frrng.length()];
      float *oftr_buf_p;
      double *ftr_dbuf_src = new double[buf_size*n_ftrs];
      double *ftr_dbuf_src_p;
      double *ftr_dbuf_src_endp;
      double *ftr_dbuf_dst = new double[buf_size*n_ftrs];
      double *ftr_dbuf_dst_p;

      for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	const size_t n_frames = in_streamp->num_frames((*srit));

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
	      //bj delete lab_buf;
	      delete oftr_buf;
	      delete ftr_dbuf_src;
	      delete ftr_dbuf_dst;

	      // Make twice as big to cut down on future reallocs.
	      buf_size = n_frames * 2;

	      // Allocate new larger buffers.
	      ftr_buf = new float[buf_size * n_ftrs];
	      //bj lab_buf = new QNUInt32[buf_size * n_labs];
	      oftr_buf = new float[buf_size * frrng.length()];
	      ftr_dbuf_src = new double[buf_size*n_ftrs];
	      ftr_dbuf_dst = new double[buf_size*n_ftrs];

	    }


	const QN_SegID seg_id = in_streamp->set_pos((*srit), 0);

	if (seg_id == QN_SEGID_BAD)
	  {
	    fprintf(stderr,
		    "%s: Couldn't seek to start of sentence %lu "
		    "in input pfile.",
		    program_name, (unsigned long) (*srit));
	    error("Aborting.");
	  }

	const size_t n_read =
	  //bj in_streamp->read_ftrslabs(n_frames, ftr_buf, lab_buf);
	  in_streamp->read_ftrs(n_frames, ftr_buf);

	if (n_read != n_frames)
	  {
	    fprintf(stderr, "%s: At sentence %lu in input pfile, "
		    "only read %lu frames when should have read %lu.\n",
		    program_name, (unsigned long) (*srit),
		    (unsigned long) n_read, (unsigned long) n_frames);
	    error("Aborting.");
	  }

	// While subtracting off the means, convert the features to 
	// doubles and do the normalization there.
	ftr_dbuf_src_p = ftr_dbuf_src;
	ftr_dbuf_src_endp = ftr_dbuf_src + n_frames*n_ftrs;
	ftr_buf_p  = ftr_buf;
	while (ftr_dbuf_src_p != ftr_dbuf_src_endp) {
	  ftr_means_p = ftr_means;
	  while (ftr_means_p != ftr_means_endp)
	    *ftr_dbuf_src_p++ = (double) *ftr_buf_p++ - *ftr_means_p++;
	}


	// Normalize the features with matrix multiply.
	// mul_mdmd_md(const int M, const int K, const int N, 
	//             const double *const A, const double *const B, double *const C, 
        //             const int Astride, const int Bstride, const int Cstride)

	mul_mdmd_md(n_frames,n_ftrs,n_ftrs,
		    ftr_dbuf_src,ftr_eigenvecs,ftr_dbuf_dst,
		    n_ftrs,n_ftrs,n_ftrs);


	ftr_dbuf_dst_p = ftr_dbuf_dst;
	oftr_buf_p = oftr_buf;
	for (i=0;i<n_read;i++) {
	  for (Range::iterator frit=frrng.begin();
	       !frit.at_end(); ++frit) {
	    *oftr_buf_p++ = (float)ftr_dbuf_dst_p[*frit];
	  }
	  ftr_dbuf_dst_p += n_ftrs;
	}

	// Write output.
	//bj out_streamp->write_ftrslabs(n_frames, oftr_buf, lab_buf);
    out_streamp->write_ftrs(n_frames, oftr_buf);
	out_streamp->doneseg((QN_SegID) seg_id);

      }

      delete oftr_buf;
      delete ftr_dbuf_src;
      delete ftr_dbuf_dst;
    }
    printf("..done\n");

    delete ftr_buf;
    //bj delete lab_buf;
    delete ftr_means;
    delete ftr_eigenvecs;
    delete ftr_eigenvals;

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
    const char *output_st_fname = 0;   // Output stats file name.    
    const char *input_st_fname = 0;   // input stats file name.    


    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string
    Range *fr_rng;


    int debug_level = 0;
    bool unity_variance = true;
    bool ascii=false;


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
        else if (strcmp(argp, "-n")==0)
        {
            unity_variance = false;
        }
        else if (strcmp(argp, "-a")==0)
        {
            ascii = true;
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
        else if (strcmp(argp, "-os")==0)
        {
            // Output file name.
            if (argc>0)
            {
                // Next argument is input file name.
                output_st_fname = *argv++;
                argc--;
            }
            else
                usage("No output stat filename given.");
        }
        else if (strcmp(argp, "-is")==0)
        {
            // Output file name.
            if (argc>0)
            {
                // Next argument is input file name.
                input_st_fname = *argv++;
                argc--;
            }
            else
                usage("No input stat filename given.");
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
        else {
	  sprintf(buf,"Unrecognized argument (%s).",argp);
	  usage(buf);
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    // An input pfile name must be supplied.
    if (input_fname==0)
        usage("No input pfile name supplied.");
    //bjoern (Wed 06 Nov 2002 05:22:06 PM PST)
    //FILE *in_fp = fopen(input_fname, "r");
    //if (in_fp==NULL)
    //    error("Couldn't open input pfile for reading.");

    // If an output pfile name is not supplied, we just
    // compute the statistics.
    FILE *out_fp=NULL;
    if (output_fname==0) 
      out_fp = NULL; // no output pfile desired.
    else {
      if ((out_fp = fopen(output_fname, "w")) == NULL) {
	error("Couldn't open output file for writing.");
      }
    }

    
    FILE *in_st_fp=NULL;
    if (input_st_fname == 0)
      in_st_fp = NULL;
    else {
      in_st_fp = fopen(input_st_fname, "r");
      if (in_st_fp==NULL)
        error("Couldn't open input stats for reading.");
    }

    FILE *out_st_fp=NULL;
    if (output_st_fname==0)
      out_st_fp = 0; // no output desired.
    else if (!strcmp(output_st_fname,"-"))
      out_st_fp = stdout;
    else {
      if ((out_st_fp = fopen(output_st_fname, "w")) == NULL) {
	error("Couldn't open stat output file for writing.");
      }
    }

    if ((out_fp == NULL && out_st_fp == NULL)) {
      error("Error: arguments specify a condition that does no real work.\n");
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

    //bjoern (Wed 06 Nov 2002 05:05:14 PM PST)
    //QN_InFtrLabStream* in_streamp
    //    = new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);
    char* ifn = strdup(input_fname);
    QN_InFtrStream* in_streamp =
        QN_build_ftrstream(debug_level, "",
                        ifn, "pfile",
                        0, 1,
                        NULL,
                        0, 0,
                        0, (size_t)QN_ALL,
                        0,
                        0, 0,
                        QN_NORM_FILE, 0.0, 0.0);
    free(ifn);

    sr_rng = new Range(sr_str,0,in_streamp->num_segs());
    fr_rng = new Range(fr_str,0,in_streamp->num_ftrs());


    QN_OutFtrLabStream* out_streamp;
    if (out_fp != NULL)
      out_streamp
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "",
                                       out_fp,
				       fr_rng->length(),
                                       //bjoern (Thu 07 Nov 2002 01:59:36 PM
                                       //PST)
                                       //in_streamp->num_labs(),
                                       0,
                                       1);
    else
      out_streamp = NULL;



    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_klt(in_streamp,
	      out_streamp,
	      in_st_fp,
	      out_st_fp,
	      *sr_rng,*fr_rng,
	      unity_variance,
	      ascii);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in_streamp;
    delete out_streamp;
    delete sr_rng;
    delete fr_rng;
    //bjoern (Thu 07 Nov 2002 03:25:11 PM PST)
    //if (in_fp && fclose(in_fp))
    //    error("Couldn't close input pfile.");
    if (out_fp && fclose(out_fp))
        error("Couldn't close output file.");
    if (in_st_fp && fclose(in_st_fp))
        error("Couldn't close input pfile.");
    if (out_st_fp && fclose(out_st_fp))
        error("Couldn't close output file.");


    return EXIT_SUCCESS;
}
