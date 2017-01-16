/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_lda.cc,v 1.8 2012/01/05 20:32:02 dpwe Exp $

    This program performs LDA on the input pfile and outputs either
    the transformation matrix or a transformed pfile (or both).

    This is basically a modification of Jeff Bilmes'
    pfile_klt.cc. Here is the description of Jeff's original
    pfile_klt.cc:

    This program performs a klt analysis on the input pfile and
    outputs the zero-mean, (optionally not) unity variance, completely
    uncorrelated features, in the order of decreasing eigenvalues.  By
    selecting a subset of the output features, the result is a
    Karhunen-Loeve transformation (or principal component analysis) of
    the pfile.

    Jeff Bilmes <bilmes@cs.berkeley.edu> 
*/

#include "pfile_utils.h"

#include "icsiarray.h"
#include "linalg.h"

#include <vector>

#include "QN_PFile.h"
#include "QN_filters.h"
#include "QN_utils.h"
#include "error.h"
extern "C" {
#include "eig.h"
}
#include "Range.H"

extern "C" void mul_mdmd_md(const int M, const int K, const int N, 
			    const double *const A, const double *const B, 
			    double *const C, const int Astride, 
			    const int Bstride, const int Cstride);

static const char* program_name;

template<class T> void print_hdrs(vector<icsiarray<T> >& v, const char* pref);


using namespace std;

static void usage(const char* message = 0) {
    if (message)
	fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help           print this message\n"
		"-il <file-name> input labels pfile\n"
	    "-i  <file-name> input feature pfile\n"
	    "-o  <file-name> output pfile ('-' for stdout)\n"
	    "-os <file-name> output stats (eigenvalues, eigenvectors) ('-' for stdout)\n"
	    "-is <file-name> input stats (eigenvalues, eigenvectors) (i.e., do not compute them).\n"
	    "-a              stat matrices written/read in ascii rather than binary doubles\n"
	    "-sr range       sentence range\n"
	    "-fr range       output feature range\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
	);
    exit(EXIT_FAILURE);
}

void
readStats(FILE*f,
	  const size_t N,
	  const bool ascii,
	  icsiarray<double>& vecs,
	  icsiarray<double>& vals) 
{
    size_t i,j;

    if (!ascii) {
	for (i=0;i<N;i++)
	    if (!fread(&vals[i],sizeof(double),1,f))
		error("input stat file eof error, vals(%d)\n",i);
	for (i=0;i<N;i++)
	    for (j=0;j<N;j++) 
		if (!fread(&vecs[i*N+j],sizeof(double),1,f))
		    error("input stat file eof error, vecs(%d,%d)\n",i,j);
    } else {
	for (i=0;i<N;i++)
	    if (!fscanf(f,"%lf",&vals[i]))
		error("input stat file eof error, vals(%d)\n",i);

	for (i=0;i<N;i++)
	    for (j=0;j<N;j++) 
		if (!fscanf(f,"%lf",&vecs[i*N+j]))
		    error("input stat file eof error, vecs(%d,%d)\n",i,j);

    }

    // Could check for eof condition to be
    // true here.
}

// Write out the eigen values and eigen vectors
void
writeStats(FILE*f,
	   const size_t N,
	   const bool ascii,
	   icsiarray<double>& vecs,
	   icsiarray<double>& vals)
{
    size_t i,j;

    if (ascii) {
	for (i=0;i<N;i++)
	    fprintf(f,"%.15f ",vals[i]);
	fprintf(f,"\n");
	for (i=0;i<N;i++){
	    for(j=0;j<N;j++)
		fprintf(f,"%.15f ",vecs[i*N+j]);
	    fprintf(f,"\n");
	}
    } else {
	for (i=0;i<N;i++)
	    fwrite(&vals[i],sizeof(double),1,f);
	for (i=0;i<N;i++)
	    fwrite(&vecs[i*N],sizeof(double),N,f);
    }

}


/**
 ** Labels assumed to be in the range of [0:C-1], where C is the number of
 ** labels types.
 */
static void
pfile_lda(QN_InFtrStream* in_streamp,
		  QN_InLabStream* in_labstreamp,
	  QN_OutFtrLabStream* out_streamp,
	  FILE *in_st_fp,
	  FILE *out_st_fp,
	  Range& srrng,Range& frrng,
	  const bool ascii)
{

    // Feature and label buffers are dynamically grown as needed.
    size_t buf_size = 300;      // Start with storage for 300 frames.
    size_t nClassesGuess = 50;	// Assume 50 classes
    size_t nClasses=0;		// actual number of classes found

    // The number of features per frame.
    const size_t n_ftrs = in_streamp->num_ftrs();
    size_t max_n_frames = 0;

    // Temp storage for the features for one sentence
    icsiarray<float> ftr_buf(buf_size*n_ftrs);
    float *ftr_buf_p;

    // The number of labels per frame (probably only one).
    size_t n_labs=1;
    if (in_labstreamp!=NULL) {
	  n_labs = in_labstreamp->num_labs();
	}
	// Temp storage for the labels for one sentence
    icsiarray<QNUInt32> lab_buf(buf_size*n_labs);


    // Used to hold the counts of each class which will be used to
    // calculate the priors for each class.
    // May have to be dynamically resized.
    vector<size_t> prior_counts(nClassesGuess);

    // Total number of frames in all of the data
    size_t total_frames = 0;

    icsiarray<double> new_ftr_eigenvecs;
    icsiarray<double> new_ftr_eigenvals;

    size_t i,j;

    if (in_st_fp == NULL) {

	// correlation vector (i.e., E[X X^T]
	double *const ftr_cor = new double [n_ftrs*(n_ftrs+1)];
	double *ftr_cor_p;
	memset(ftr_cor,0,n_ftrs*(n_ftrs+1)*sizeof(double)/2);

	vector<icsiarray<double> > new_ftr_means(nClassesGuess);
	for(i=0;i<new_ftr_means.size();i++)
	    new_ftr_means[i].resize(n_ftrs);

	// correlation matrix. used for computing covariance.
	// for each class, we have a matrix of n_ftrs x n_ftrs
	vector<icsiarray<double> > new_ftr_cor(nClassesGuess);
	for(i=0;i<nClassesGuess;i++)
	    new_ftr_cor[i].resize(n_ftrs*n_ftrs);

	// class-dependent covariance matrices.
	// for each class, we have a matrix of n_ftrs x n_ftrs
	vector<icsiarray<double> > new_ftr_cov(nClassesGuess);
	for(i=0;i<nClassesGuess;i++)
	    new_ftr_cov[i].resize(n_ftrs*n_ftrs);


	//
	// Go through input pfile to get the initial statistics,
	for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	    const size_t n_frames = in_streamp->num_frames((*srit));
		// check if labelfile has same number of frames as ftrfile
		if (n_frames!=in_labstreamp->num_frames((*srit))) {
		  error ("Label file and feature file don't have the same number of frames at %d\n",(*srit));
		}
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
	    ftr_buf.resize(n_frames * n_ftrs);
	    lab_buf.resize(n_frames * n_labs);

	    const QN_SegID seg_id = in_streamp->set_pos((*srit), 0);
			   
	    if (seg_id == QN_SEGID_BAD){
		fprintf(stderr,
			"%s: Couldn't seek to start of sentence %lu "
			"in input pfile.",
			program_name, (unsigned long) (*srit));
		error("Aborting.");
	    }
	    const QN_SegID lseg_id = in_labstreamp->set_pos((*srit), 0);
			   
	    if (lseg_id == QN_SEGID_BAD){
		fprintf(stderr,
			"%s: Couldn't seek to start of sentence %lu "
			"in input labels pfile.",
			program_name, (unsigned long) (*srit));
		error("Aborting.");
	    }


	    const size_t n_read =
		in_streamp->read_ftrs(n_frames,ftr_buf.get_thearray());

	    if (n_read != n_frames) {
		fprintf(stderr, "%s: At sentence %lu in input pfile, "
			"only read %lu frames when should have read %lu.\n",
			program_name, (unsigned long) (*srit),
			(unsigned long) n_read, (unsigned long) n_frames);
		error("Aborting.");
	    }

		const size_t labn_read =in_labstreamp->read_labs(n_frames,lab_buf.get_thearray());
	    if (labn_read != n_frames) {
		fprintf(stderr, "%s: At sentence %lu in input pfile, "
			"only read %lu frames when should have read %lu.\n",
			program_name, (unsigned long) (*srit),
			(unsigned long) labn_read, (unsigned long) n_frames);
		error("Aborting.");
	    }


	    // Get largest class label so that we can determine if we need
	    // to resize any of our data structures.
	    int max_class_lab = lab_buf.max();

	    // Keep track of actual number of classes
	    if(max_class_lab >= nClasses)
		nClasses = max_class_lab+1; 

	    // see if we exceeded our guess about the number of classes
	    if(max_class_lab >= nClassesGuess) {
		size_t oldnClasses = nClassesGuess;
		nClassesGuess = max_class_lab+1;
		// resize the vector of class priors
		//bjoern (Wed Dec  4 12:01:58 2002)
		//prior_counts.reserve(nClassesGuess);
		prior_counts.resize(nClassesGuess);
		// resize the vector of class means
		//bjoern (Wed Dec  4 12:02:11 2002)
		//new_ftr_means.reserve(nClassesGuess);
		new_ftr_means.resize(nClassesGuess);
		for(i=oldnClasses;i<nClassesGuess;i++)
		    new_ftr_means[i].resize(n_ftrs);
		// resize the correlation matrix
		//bjoern (Wed Dec  4 12:04:50 2002)
		//new_ftr_cor.reserve(nClassesGuess);
		new_ftr_cor.resize(nClassesGuess);
		for(i=oldnClasses;i<nClassesGuess;i++)
		    new_ftr_cor[i].resize(n_ftrs*n_ftrs);
		// resize the covariance matrix
		//bjoern (Wed Dec  4 12:03:41 2002)
		//new_ftr_cov.reserve(nClassesGuess);
		new_ftr_cov.resize(nClassesGuess);
		for(i=oldnClasses;i<nClassesGuess;i++)
		    new_ftr_cov[i].resize(n_ftrs*n_ftrs);
	    }

	    // Loop over the frames for this sentence, updating all of the
	    // counts.
	    for(i=0;i<n_frames;i++){
		unsigned curlab = lab_buf[i];

		// update the prior count for this class
		prior_counts[curlab]++;

		// get features for this frame
		icsiarray<float> frame_feats(&ftr_buf[i*n_ftrs],n_ftrs);

		// Accumulate the features and the feature correlations
		// for this class.
		for(j=0;j<n_ftrs;j++) {
		    new_ftr_means[curlab][j] += static_cast<double>(frame_feats[j]);
		    for(int k=j;k<n_ftrs;k++) {
			new_ftr_cor[curlab][j*n_ftrs+k] += 
			    static_cast<double>(frame_feats[j]*frame_feats[k]);
		    }
		}
	    }
	    total_frames += n_read;
	} // done going through input pfile

	double total_frames_inv = 1.0/total_frames;

#define SAMPMEAN 1
#ifdef SAMPMEAN
	// Multiply the covariances by this factor to convert from
	// the population variance to the sample variance. 
	// Matlab uses sample variance in its cov() function, so if we want to
	// get covariance numbers equal to matlab's, then we need 
	// to multiply each covariance by N/(N-1).
	double cov_conver_factor=1.0;
	if(total_frames > 1)
	    cov_conver_factor = static_cast<double>(total_frames)/
		(static_cast<double>(total_frames)-1.0);
#endif


	// The global means
	icsiarray<double> globalMeans(n_ftrs);
	// The within-class covariance matrix
	icsiarray<double> wccm(n_ftrs*n_ftrs);
	// The class priors
	icsiarray<double> priors(nClasses);

	// Compute the class-dependent means, priors and covariances.
	// At the same time, compute the global means and the 
	// within-class covariance matrix.
	for(i=0;i<nClasses;i++) {

	    // compute class priors
	    priors[i] = static_cast<double>(prior_counts[i]) /
		static_cast<double>(total_frames);

	    // multiply all of the mean counts for this class by the 
	    // class inverse prior counts to get the actual means
	    double invpri = 1.0;
	    if(prior_counts[i] > 0)
		invpri = 1.0/static_cast<double>(prior_counts[i]);
	    new_ftr_means[i].mul(invpri);

	    // accumulate for the global means
	    globalMeans.add(new_ftr_means[i]);

	    // calculate the class-dependent covariance matrix
	    for(j=0;j<n_ftrs;j++) {
		for(int k=j;k<n_ftrs;k++) {
		    unsigned idx = j*n_ftrs+k;
		    new_ftr_cov[i][idx] = new_ftr_cor[i][idx] * invpri -
			new_ftr_means[i][j] * new_ftr_means[i][k];

		    // Multiply by this conversion factor so that the covariances
		    // will match those produced by the matlab 'cov()' function.
		    // Basically, this converts from 1/N to 1/(N-1).
		    new_ftr_cov[i][idx] *= cov_conver_factor;

		    // fill lower half of cov matrix
		    if(j!=k)		// don't do unless we have to
			new_ftr_cov[i][k*n_ftrs+j] = new_ftr_cov[i][idx];
		}
	    }

	    // Add this class cov matrix to the within-class cov matrix 
	    // (weighted by the class prior).
	    wccm.add_prod(new_ftr_cov[i], priors[i]);
	}
	// finish computing the global means (the global means will be used 
	// to compute the between-class covariance matrix).
	globalMeans.div(nClasses);

	// Compute the between-class covariance matrix
	icsiarray<double> bccm(n_ftrs*n_ftrs);
	for(i=0;i<nClasses;i++){
	    icsiarray<double> tmp(n_ftrs*n_ftrs);
	    //icsiarray<double> diff = new_ftr_means[i] - globalMeans;
	    // too strong for gcc-2.95
	    icsiarray<double> diff = new_ftr_means[i];
	    diff.sub(globalMeans);
	    linalg::outerProd(tmp,diff,diff);
	    bccm.add_prod(tmp, priors[i]);
	}
	bccm.div(nClasses);

	// Compute inv(A)*B using LU decomposition and backsubstitution.
	// Warning: this will destroy the A matrix (wccm in this case)
	icsiarray<double> G(n_ftrs*n_ftrs);
	linalg::invAxB(G,wccm,bccm,n_ftrs);

	// now compute the eigen vectors and values
	new_ftr_eigenvecs.resize(n_ftrs*n_ftrs);
	new_ftr_eigenvals.resize(n_ftrs);
	linalg::eigenanalysis(G,n_ftrs,new_ftr_eigenvals,new_ftr_eigenvecs);

	// save it
	if (out_st_fp != NULL) 
	    writeStats(out_st_fp,n_ftrs,ascii,new_ftr_eigenvecs,new_ftr_eigenvals);

	delete(ftr_cor);
    } else {
	// read in the matrix containing the data.
	new_ftr_eigenvecs.resize(n_ftrs*n_ftrs);
	new_ftr_eigenvals.resize(n_ftrs);
	readStats(in_st_fp,n_ftrs,ascii,new_ftr_eigenvecs,new_ftr_eigenvals);

	// God knows why, but if the user
	// ask for this, save it
	if (out_st_fp != NULL) 
	    writeStats(out_st_fp,n_ftrs,ascii,new_ftr_eigenvecs,new_ftr_eigenvals);

    }

    if (out_streamp != NULL) {
	printf("Writing transformed output pfile..\n");
      
	// allocate space performing the transformation
	if (max_n_frames > 0)
	    buf_size = max_n_frames;

	icsiarray<float> oftr_buf(buf_size * frrng.length());
	float *oftr_buf_p;
	icsiarray<double> ftr_dbuf_src(buf_size*n_ftrs);
	icsiarray<double> ftr_dbuf_dst(buf_size*n_ftrs);
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
	    ftr_buf.resize(n_frames * n_ftrs);
	    lab_buf.resize(n_frames * n_labs);
	    oftr_buf.resize(n_frames * frrng.length());
	    ftr_dbuf_src.resize(n_frames*n_ftrs);
	    ftr_dbuf_dst.resize(n_frames*n_ftrs);

	    const QN_SegID seg_id = in_streamp->set_pos((*srit), 0);

	    if (seg_id == QN_SEGID_BAD) {
		fprintf(stderr,
			"%s: Couldn't seek to start of sentence %lu "
			"in input pfile.",
			program_name, (unsigned long) (*srit));
		error("Aborting.");
	    }

	    const size_t n_read =
		  in_streamp->read_ftrs(n_frames, ftr_buf.get_thearray());


	    if (n_read != n_frames){
		fprintf(stderr, "%s: At sentence %lu in input pfile, "
			"only read %lu frames when should have read %lu.\n",
			program_name, (unsigned long) (*srit),
			(unsigned long) n_read, (unsigned long) n_frames);
		error("Aborting.");
	    }

	    // Convert the features to doubles 
	    for(i=0;i<n_frames*n_ftrs;i++)
		ftr_dbuf_src[i] = static_cast<double>(ftr_buf[i]);

	    // Transform the features with matrix multiply.
	    // mul_mdmd_md(const int M, const int K, const int N, 
	    //             const double *const A, const double *const B, 
	    //             double *const C, 
	    //             const int Astride, const int Bstride, const int Cstride)
	    mul_mdmd_md(n_frames,n_ftrs,n_ftrs,
			ftr_dbuf_src.get_thearray(),
			new_ftr_eigenvecs.get_thearray(),
			ftr_dbuf_dst.get_thearray(),
			n_ftrs,n_ftrs,n_ftrs);

	    ftr_dbuf_dst_p = ftr_dbuf_dst.get_thearray();
	    oftr_buf_p = oftr_buf.get_thearray();
	    for (i=0;i<n_read;i++) {
		for (Range::iterator frit=frrng.begin();
		     !frit.at_end(); ++frit) {
		    *oftr_buf_p++ = (float)ftr_dbuf_dst_p[*frit];
		}
		ftr_dbuf_dst_p += n_ftrs;
	    }

	    // Write output.
	    out_streamp->write_ftrs(n_frames, oftr_buf.get_thearray()); 
	    out_streamp->doneseg((QN_SegID) seg_id);

	}

    }
    printf("..done\n");
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

main(int argc, const char *argv[])
{
    //////////////////////////////////////////////////////////////////////
    // TODO: Argument parsing should be replaced by something like
    // ProgramInfo one day, with each extract box grabbing the pieces it
    // needs.
    //////////////////////////////////////////////////////////////////////

    const char *input_fname = 0;   // Input features pfile name.
    const char *input_lfname = 0;   // Input labels pfile name.
    const char *output_fname = 0;   // Output pfile name.
    const char *output_st_fname = 0;   // Output stats file name.    
    const char *input_st_fname = 0;   // input stats file name.    

    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string
    Range *fr_rng;

    int debug_level = 0;
    bool ascii=false;

    program_name = *argv++;
    argc--;

    while (argc--) {
	char buf[BUFSIZ];
	const char *argp = *argv++;

	if (strcmp(argp, "-help")==0) {
	    usage();
	} else if (strcmp(argp, "-a")==0){
	    ascii = true;
	}else if (strcmp(argp, "-i")==0){
	    // Input file name.
	    if (argc>0){
		// Next argument is input file name.
		input_fname = *argv++;
		argc--;
	    } else
		usage("No input features filename given.");
	}else if (strcmp(argp, "-il")==0){
	    // Input file name.
	    if (argc>0){
		// Next argument is input file name.
		input_lfname = *argv++;
		argc--;
	    } else
		usage("No input labels filename given.");
	} else if (strcmp(argp, "-o")==0){
	    // Output file name.
	    if (argc>0) {
		// Next argument is input file name.
		output_fname = *argv++;
		argc--;
	    } else
		usage("No output filename given.");
	} else if (strcmp(argp, "-os")==0) {
	    // Output file name.
	    if (argc>0){
		// Next argument is input file name.
		output_st_fname = *argv++;
		argc--;
	    } else
		usage("No output stat filename given.");
	} else if (strcmp(argp, "-is")==0) {
	    // Output file name.
	    if (argc>0) {
		// Next argument is input file name.
		input_st_fname = *argv++;
		argc--;
	    } else
		usage("No input stat filename given.");
	} else if (strcmp(argp, "-debug")==0) {
	    if (argc>0) {
		// Next argument is debug level.
		debug_level = (int) parse_long(*argv++);
		argc--;
	    } else
		usage("No debug level given.");
	} else if (strcmp(argp, "-sr")==0) {
	    if (argc>0) {
		sr_str = *argv++;
		argc--;
	    } else
		usage("No range given.");
	}else if (strcmp(argp, "-fr")==0) {
	    if (argc>0) {
		fr_str = *argv++;
		argc--;
	    } else
		usage("No range given.");
	} else {
	    sprintf(buf,"Unrecognized argument (%s).",argp);
	    usage(buf);
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    // An input pfile name must be suppied.
    if (input_fname==0)
        usage("No input pfile name supplied.");
    
    // If no label file is supplied and we need one, use the pfile
    FILE *in_lfp = NULL;
    if (input_lfname==0 && input_st_fname == 0) {
	input_lfname = input_fname;
    }
    if (input_lfname!=0) {
	in_lfp = fopen(input_lfname, "r");
	  //printf("opening labels file\n");
	if (in_lfp==NULL)
	    error("Couldn't open input labels pfile for reading.");
    }
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

    QN_InLabStream* in_labstreamp=NULL;
    if (input_lfname!=0) {
	  //printf("opening labels stream\n");
	  in_labstreamp
		= new QN_InFtrLabStream_PFile(debug_level, "", in_lfp, 1);
	  // check if feature and label file have same number of segments
	  if(in_labstreamp->num_segs()!=in_streamp->num_segs()) {
		error("Error: Label file and feature files have differing number of segments.\n");
	  }
    }
    QN_OutFtrLabStream* out_streamp;
    if (out_fp != NULL)
	  out_streamp = new QN_OutFtrLabStream_PFile(debug_level, "",
						     out_fp, 
						     fr_rng->length(), 
						     0, // who wants labels?
						     1);
    else
	  out_streamp = NULL;
	
    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////
    pfile_lda(in_streamp,
	      in_labstreamp,
	      out_streamp,
	      in_st_fp,
	      out_st_fp,
	      *sr_rng,*fr_rng,
	      ascii);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in_streamp;
	delete in_labstreamp;
    delete out_streamp;
    delete sr_rng;
    delete fr_rng;
    //if (in_fp && fclose(in_fp))
    //    error("Couldn't close input features pfile.");
    if (in_lfp && fclose(in_lfp))
	  error("Couldn't close input labels pfile.");
	if (out_fp && fclose(out_fp))
        error("Couldn't close output file.");
    if (in_st_fp && fclose(in_st_fp))
        error("Couldn't close input stat file.");
    if (out_st_fp && fclose(out_st_fp))
        error("Couldn't close output stat file.");

    return EXIT_SUCCESS;
}


template<class T> void print_hdrs(vector<icsiarray<T> >& v, const char* pref)
{
    for (int i = 0; i < v.size(); i++)
    {
	cerr << "[" << i << "]: ";
	v[i].print_hdr(pref);
    }
}
