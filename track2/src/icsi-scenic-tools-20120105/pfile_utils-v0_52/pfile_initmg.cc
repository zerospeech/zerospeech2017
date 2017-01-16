/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_initmg.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
    Create an initializtion .mg file for pfile_mi.cc using
    a simple k-means algorithm.

*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "error.h"
#include "Range.H"
//#include "bp_range.h"
#include "rand.h"

RAND myrand(true);
unsigned short seedv[3];

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
	    "-i <file-name>  input feature pfile\n"
	    "-omg <file-name> output mg file ('-' for stdout)\n"
	    "-sr range       sentence range\n"
	    "-pr range       per-sentence frame range\n"
	    "-fr range       Feature range (not currently used).\n"
	    "-cfr bp-range   Range of past/future context frames to use.\n"
	    "-k #            number of mixtures (i.e., K)\n"
	    "-r #            Number of epoch random restarts to take best of\n"
	    "-x #            Number of samples/cluster below which a random re-init occurs\n"
	    "-m #            Maximum number of random re-inits before bailing out\n"

	    "-a #            Convergence threshold (default 0.0)\n"
	    "-q              quite mode\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


/////////////////////////////////////////////////////////////////////////////
// kmeans support class
/////////////////////////////////////////////////////////////////////////////

class kmeans {

  const int k;
  const int vector_length;

  // k*vl matrices
  float *saved_means;
  float *saved_variances;
  int   *saved_counts;

  float *cur_means;
  float *new_means;  
  int *new_counts;
  
  float *variances;

  float distance(const float *const v1,
		 const float *const v2);
  // add vector v to count against new
  void add2new(const int lk,const float *const v);



public:
  
  static int kmeans_k;
  static int kmeans_vl;

  kmeans(int _k=kmeans_k, int vl=kmeans_vl);
  ~kmeans();

  void initNew();
  void add2new(const float *const v);
  void add2newRand(const float *const v);
  bool someClusterHasZeroEntries();
  bool someClusterHasLessThanNEntries(int n);
  bool zeroCounts();
  void finishNew();

  void save();

  void computeVariances(const float *const v);
  double finishVariances();

  bool done;
  bool randomAssignment;

  void printSaved(FILE *fp);

  // avg distance between new and cur
  float newCurDist();
  // swap the new and current parameters.
  void swapCurNew() { float *tmp=cur_means;cur_means=new_means;new_means=tmp; }

  void writeMgDoubleRecord2D(FILE *stream);

};


int kmeans::kmeans_k = 5;
int kmeans::kmeans_vl = 5;

kmeans::kmeans(int _k,int vl)
  : k(_k), vector_length(vl)
{

  if (k <=0) {
    error("ERROR, can't have K=%d clusters\n",k);
  }

  cur_means = new float[k*vector_length];
  new_means = new float[k*vector_length];
  variances = new float[k*vector_length];
  
  saved_means = new float[k*vector_length];
  saved_variances = new float[k*vector_length];
  saved_counts = new int[k];

  done = false;
  randomAssignment = true;

  new_counts = new int[k];
  float *curp = cur_means;
  float *newp = new_means;
  float *varp = variances;
  for (int i=0;i<k;i++) {
    new_counts[i] = 0;
    for (int j=0;j<vector_length;j++) {
      *curp++ = drand48();
      *newp++ = 0.0;
      *varp++ = 0.0;
    }
  }
}

kmeans::~kmeans()
{
  delete [] cur_means;
  delete [] new_means;
  delete [] new_counts;
  delete [] variances;
  delete [] saved_means;
  delete [] saved_variances;
  delete [] saved_counts;

}

void
kmeans::initNew()
{
  float *newp = new_means;
  float *varp = variances;
  for (int i=0;i<k;i++) {
    new_counts[i] = 0;
    for (int j=0;j<vector_length;j++) {
      *newp++ = 0.0;
      *varp++ = 0.0;
    }
  }
}

void
kmeans::save()
{
  ::memcpy((void*)saved_means,(void*)cur_means,
	   sizeof(float)*k*vector_length);
  ::memcpy((void*)saved_variances,(void*)variances,
	   sizeof(float)*k*vector_length);
  ::memcpy((void*)saved_counts,(void*)new_counts,
	   sizeof(int)*k);
}


inline float
kmeans::distance(const float *const v1,const float *const v2)
{
  float rc = 0;

  // assumes vector_length > 0
  const float *v1p = v1;
  const float *const v1_endp = v1+vector_length;
  const float *v2p = v2;
  do {
    const float tmp = (*v1p++ - *v2p++);
    rc += tmp*tmp;
  } while (v1p != v1_endp);


  // for (int i=0;i<vector_length;i++) {
  // const float tmp = (v1[i] - v2[i]);
  // rc += tmp*tmp;
  // }
  return rc;
}

inline void
kmeans::add2new(const int lk,const float *const v)
{
  new_counts[lk]++;
  float *k_mean = &new_means[lk*vector_length];

  // assumes vector_length > 0
  const float *vp = v;
  const float *const v_endp = v+vector_length;
  float *k_meanp = k_mean;
  do {
    *k_meanp++ += *vp++;
  } while (vp != v_endp);

  // for (int i=0;i<vector_length;i++) {
  // k_mean[i] += v[i];
  // }
}

void
kmeans::add2new(const float *const v)
{
  float *cur_meansp = cur_means;
  float md = distance(cur_meansp,v);
  int inx = 0;

  cur_meansp += vector_length;
  for (int i=1;i<k;i++) {
    const float tmp = distance(cur_meansp,v);
    if (tmp < md) {
      md = tmp;
      inx = i;
    }
    cur_meansp += vector_length;
  }
  add2new(inx,v);
}

void
kmeans::add2newRand(const float *const v)
{
  int inx = myrand.uniform(k-1);
  add2new(inx,v);
}

void
kmeans::computeVariances(const float *const v)
{

  // first compute the mean this vector is closest to:
  float *cur_meansp = cur_means;
  float md = distance(cur_meansp,v);
  int inx = 0;
  int i;
  cur_meansp += vector_length;
  for (i=1;i<k;i++) {
    float tmp = distance(cur_meansp,v);
    if (tmp < md) {
      md = tmp;
      inx = i;
    }
    cur_meansp += vector_length;
  }

  // mean this vector is closest to is inx.
  cur_meansp = &cur_means[inx*vector_length];
  float *variancesp = &variances[inx*vector_length];
  for (i=0;i<vector_length;i++) {
    const float tmp = v[i] - cur_meansp[i];
    variancesp[i] += tmp*tmp;
  }
}


double
kmeans::finishVariances()
{
  
  double sum=0.0;
  // mean this vector is closest to is inx.
  float *variancesp = variances;
  for (int i=0;i<k;i++) {
    const float norm = 1.0/new_counts[i];
    for (int j=0;j<vector_length;j++) {
      variancesp[j] *= norm;
      sum += variancesp[j];
    }
    variancesp += vector_length;
  }
  // return sum of variances.
  return sum;
}


bool
kmeans::someClusterHasLessThanNEntries(int n)
{
  for (int i=0;i<k;i++)
    if (new_counts[i] <n)
      return true;
  return false;
}


bool
kmeans::someClusterHasZeroEntries()
{
  for (int i=0;i<k;i++)
    if (new_counts[i] == 0)
      return true;
  return false;
}




// return true if no samples were 
// given to this kmeans object.
bool
kmeans::zeroCounts()
{
  for (int i=0;i<k;i++)
    if (new_counts[i] != 0)
      return false;
  return true;
}



void
kmeans::finishNew()
{
  float *newp = new_means;
  for (int i=0;i<k;i++) {
    double inv_count = 1.0/new_counts[i];
    for (int j=0;j<vector_length;j++) {
      *newp *= inv_count;
      newp++;
    }
  }
}


float
kmeans::newCurDist()
{

  float *curp = cur_means;
  float *newp = new_means;
  float dist = 0.0;
  for (int i=0;i<k;i++) {
    dist += distance(curp,newp);
    curp += vector_length;
    newp += vector_length;
  }
  return dist;
}


void
kmeans::printSaved(FILE *fp)
{
  float *meansp = saved_means;
  float *variancesp = saved_variances;
  for (int i=0;i<k;i++) {
    int j;
    fprintf(fp,"%d means(%d): ",i,saved_counts[i]);
    for (j=0;j<vector_length;j++) {
      fprintf(fp,"%0.5e ",meansp[j]);
    }
    fprintf(fp,"\n");
    fprintf(fp,"%d varns: ",i);
    for (j=0;j<vector_length;j++) {
      fprintf(fp,"%0.5e ",variancesp[j]);
    }
    fprintf(fp,"\n");

    meansp += vector_length;
    variancesp += vector_length;
  }
}

static void swab_dbuf(double *ptr, int n) {
  // Swap bytes in a buffer of doubles, if needed to make them bigendian
#ifndef WORDS_BIGENDIAN
  unsigned int d,e, *dp = (unsigned int*)ptr;
  int i;

  assert(sizeof(double) == 8);
  assert(sizeof(unsigned int) == 4);

  for (i=0; i<n; ++i) {
    d = *dp++;
    e = *dp--;
    *dp++ = ((e & 0x000000FF) << 24) | ((e & 0x0000FF00) << 8) | ((e & 0x00FF0000) >> 8) | (0x000000FF & ((e & 0xFF000000) >> 24));
    *dp++ = ((d & 0x000000FF) << 24) | ((d & 0x0000FF00) << 8) | ((d & 0x00FF0000) >> 8) | (0x000000FF & ((d & 0xFF000000) >> 24));
  }
#endif /* WORDS_BIGENDIAN */
}

void
kmeans::writeMgDoubleRecord2D(FILE *stream)
{

  // only writes the first 2

  if (vector_length != 2) 
    error("kmeans::writeMgDoubleRecord2D, vl = %d, this function must have length 2 vectors\n",vector_length);

  char char_k = kmeans_k;
  fwrite(&char_k,sizeof(char_k),1,stream);

  int i;
  int total_count = 0;
  for (i=0;i<kmeans_k;i++) {
    total_count += saved_counts[i];
  }
  float *meansp = saved_means;
  float *varsp = saved_variances;

  for (i=0;i<kmeans_k;i++) {
    double tmp;

    // mean x
    tmp = meansp[0];
    swab_dbuf(&tmp, 1);
    fwrite(&tmp,sizeof(tmp),1,stream);

    // mean y
    tmp = meansp[1];
    swab_dbuf(&tmp, 1);
    fwrite(&tmp,sizeof(tmp),1,stream);

    // var x
    tmp = varsp[0];
    swab_dbuf(&tmp, 1);
    fwrite(&tmp,sizeof(tmp),1,stream);

    // cov xy
    tmp = 0.0;
    swab_dbuf(&tmp, 1);
    fwrite(&tmp,sizeof(tmp),1,stream);

    // var y
    tmp = varsp[1];
    swab_dbuf(&tmp, 1);
    fwrite(&tmp,sizeof(tmp),1,stream);

    // coef
    tmp = saved_counts[i]/(double)total_count;
    swab_dbuf(&tmp, 1);
    fwrite(&tmp,sizeof(tmp),1,stream);

    meansp += vector_length;
    varsp += vector_length;
  }
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////



static void
pfile_initmg(QN_InFtrLabStream& in_stream,
	     FILE *out_fp,
	     Range& srrng,
	     Range& frrng,
	     Range& cfrrng,
	     const char *pr_str,
	     const int num_clusters,
	     const int maxReInits,
	     const int minSamplesPerCluster,
	     const int numRandomReStarts,
	     const bool quiet_mode,
	     const float conv_thres)
{

    // Feature and label buffers are dynamically grown as needed.

    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const int n_ftrs = (int)in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];

    kmeans::kmeans_k = num_clusters;
    kmeans::kmeans_vl = 2;

    const int num_kmeans = n_ftrs*(n_ftrs-1)/2;
    kmeans *kms = new kmeans[num_kmeans];
    size_t sent_no;

    double bestVarianceSum=HUGE;
    
    for (int epoch=0;epoch<numRandomReStarts;epoch++) {

      printf("Epoch %d\n",epoch);

      int i,j,iter=0;
      int reInits = 0;
    
      float max_dist = 0;

      for (i=0;i<num_kmeans;i++) {
	kms[i].randomAssignment = true;
	kms[i].done = false;
      }

      do {
	iter++;

	for (i=0;i<num_kmeans;i++) {
	  if (!kms[i].done)
	    kms[i].initNew();
	}

	for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	  sent_no = (*srit);
	  const size_t n_frames = in_stream.num_frames((sent_no));

	  if (!quiet_mode) {
	    if ((sent_no) % 1000 == 0)
	      printf("Processing sentence %zu\n",(sent_no));
	  }

	  if (n_frames == QN_SIZET_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't find number of frames "
		      "at sentence %lu in input pfile.\n",
		      program_name, (unsigned long) (sent_no));
	      error("Aborting.");
	    }

	  // Increase size of buffers if needed.
	  if (n_frames > buf_size)
	    {
	      // Free old buffers.
	      delete [] ftr_buf;
	      delete [] lab_buf;

	      // Make twice as big to cut down on future reallocs.
	      buf_size = n_frames * 2;

	      // Allocate new larger buffers.
	      ftr_buf = new float[buf_size * n_ftrs];
	      lab_buf = new QNUInt32[buf_size * n_labs];
	    }

	  const QN_SegID seg_id = in_stream.set_pos((sent_no), 0);

	  if (seg_id == QN_SEGID_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't seek to start of sentence %lu "
		      "in input pfile.",
		      program_name, (unsigned long) (sent_no));
	      error("Aborting.");
	    }

	  const size_t n_read =
	    in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);

	  if (n_read != n_frames)
	    {
	      fprintf(stderr, "%s: At sentence %lu in input pfile, "
		      "only read %lu frames when should have read %lu.\n",
		      program_name, (unsigned long) (sent_no),
		      (unsigned long) n_read, (unsigned long) n_frames);
	      error("Aborting.");
	    }

	  Range prrng(pr_str,0,n_frames);

	  for (Range::iterator prit=prrng.begin();
	       !prit.at_end() ; ++prit) {
	    const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	    
	    int kmno = 0;
	    float buf2[2];
	    for (i=0;i<n_ftrs;i++) {
	      buf2[0] = ftr_buf_p[i];
	      for (j=i+1;j<n_ftrs;j++) {
		if (!kms[kmno].done) {
		  buf2[1] = ftr_buf_p[j];
		  if (kms[kmno].randomAssignment)
		    kms[kmno].add2newRand(buf2);
		  else 
		    kms[kmno].add2new(buf2);
		}
		kmno++;
	      }
	    }
	  }

	}

	for (i=0;i<num_kmeans;i++) {
	  if (!kms[i].done)
	    kms[i].randomAssignment = false;
	}

	// make sure each kmeans had some data
	max_dist = 0;
	int num_active=0;
	int num_reinits = 0;
	for (i=0;i<num_kmeans;i++) {
	  if (!kms[i].done) {
	    if (kms[i].zeroCounts()) {
	      // This shouldn't happen with uniform skmeans. All kmeans
	      // objects should be getting some data.
	      error("kms[%d] was given no input. Probably an command line argument error",i);
	    } else if (kms[i].someClusterHasLessThanNEntries(minSamplesPerCluster)) {
	      // fprintf(stderr,"Warning: kms %d, some clusters have < %d entries. Re-initializing.\n",i,minSamplesPerCluster);
	    
	      // only re-init this particular kmeans object, not all of them.
	      kms[i].randomAssignment = true;
	      num_reinits++;
	      max_dist = 1.0; // keep this condition from stopping the loop.
	      // break;
	    } else {
	      kms[i].finishNew();
	      float tmp = kms[i].newCurDist();
	      if (tmp > max_dist)
		max_dist = tmp;
	      kms[i].swapCurNew();
	      if (tmp <= conv_thres) {
		kms[i].done = true;
		// printf("Iter %d: kms %d converged\n",iter,i);
	      }
	    }
	    num_active++;
	  }
	}
	if (num_reinits > 0)
	  reInits++;
	printf("Iter %d: max_dist = %e, cur num_reinits = %d, tot itr re_init = %d, num_active = %d\n",iter,max_dist,num_reinits,reInits,num_active);
	fflush(stdout);

      } while (max_dist > conv_thres && reInits <= maxReInits);

      if (reInits > maxReInits) {
	error("Error. %d re-inits and convergence didn't occur.", reInits);
      }

      printf("Epoch %d: Computing Variances\n",epoch);

      // Do one more pass over file to compute the variances.
      for (Range::iterator srit=srrng.begin();!srit.at_end();srit++) {
	sent_no = (*srit);

	const size_t n_frames = in_stream.num_frames((sent_no));

	if (!quiet_mode) {
	  if ((sent_no) % 1000 == 0)
	    printf("Processing sentence %zu\n",(sent_no));
	}

	if (n_frames == QN_SIZET_BAD)
	  {
	    fprintf(stderr,
		    "%s: Couldn't find number of frames "
		    "at sentence %lu in input pfile.\n",
		    program_name, (unsigned long) (sent_no));
	    error("Aborting.");
	  }

	const QN_SegID seg_id = in_stream.set_pos((sent_no), 0);

	if (seg_id == QN_SEGID_BAD)
	  {
	    fprintf(stderr,
		    "%s: Couldn't seek to start of sentence %lu "
		    "in input pfile.",
		    program_name, (unsigned long) (sent_no));
	    error("Aborting.");
	  }

	const size_t n_read =
	  in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);

	if (n_read != n_frames)
	  {
	    fprintf(stderr, "%s: At sentence %lu in input pfile, "
		    "only read %lu frames when should have read %lu.\n",
		    program_name, (unsigned long) (sent_no),
		    (unsigned long) n_read, (unsigned long) n_frames);
	    error("Aborting.");
	  }

	Range prrng(pr_str,0,n_frames);

	for (Range::iterator prit=prrng.begin();
	     !prit.at_end() ; ++prit) {
	  const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	  
	  int kmno = 0;
	  float buf2[2];
	  for (i=0;i<n_ftrs;i++) {
	    buf2[0] = ftr_buf_p[i];
	    for (j=i+1;j<n_ftrs;j++) {
	      buf2[1] = ftr_buf_p[j];
	      kms[kmno].computeVariances(buf2);
	      kmno++;
	    }
	  }
	}
      }

    
      double sumVariances=0;
      for (i=0;i<num_kmeans;i++) {
	sumVariances+=kms[i].finishVariances();
      }

      if (sumVariances < bestVarianceSum) {
	bestVarianceSum = sumVariances;
	for (i=0;i<num_kmeans;i++) {
	    kms[i].save();
	}
      }
      
      printf("End of Epoch %d of %d, variance sum = %e, best = %e\n",epoch,
	     numRandomReStarts,
	     sumVariances,bestVarianceSum);

    }
    
//     kmeans *kmsp = kms;
//     for (int i=0;i<n_ftrs;i++) {
//       for (int j=i+1;j<n_ftrs;j++) {
// 	fprintf(out_fp,"feat %d:%d: \n",i,j);
// 	kmsp->printSaved(out_fp);
// 	kmsp++;
//       }
//     }


    // We must have that to > from
#define KMSPOS(from,to,n) ((from)*(n) - ((from)+1)*((from)+2)/2 + (to))

  for (Range::iterator cnit=cfrrng.begin();
       !cnit.at_end();cnit++) {
    const int l = (*cnit);
    // j-loop is over position in the lagged feature vector, x

    for (int j=0;j<n_ftrs;j++) {
      // k-loop is over position in the current feature vector, y
      if (l != 0) {
	for (int k=0;k<n_ftrs;k++) {
	  if (k == j) {
	    if (k < (n_ftrs-1)) 
	      kms[KMSPOS(j,k+1,n_ftrs)].writeMgDoubleRecord2D(out_fp);
	    else 
	      kms[KMSPOS(j-1,k,n_ftrs)].writeMgDoubleRecord2D(out_fp);
	  } else if (k > j) {
	    kms[KMSPOS(j,k,n_ftrs)].writeMgDoubleRecord2D(out_fp);
	  } else { // k < j
	    kms[KMSPOS(k,j,n_ftrs)].writeMgDoubleRecord2D(out_fp);
	  }
	}
      } else {
	for (int k=(j+1);k<n_ftrs;k++) {
	  kms[KMSPOS(j,k,n_ftrs)].writeMgDoubleRecord2D(out_fp);
	}
      }
    }
  }

  delete [] ftr_buf;
  delete [] lab_buf;
  delete [] kms;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////


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

    const char *input_fname = 0;   // Input pfile name.
    const char *output_fname = 0;   // output mg file name

    const char *sr_str = 0;   // sentence range string
    Range *sr_rng;
    const char *fr_str = 0;   // feature range string    
    Range *fr_rng;
    const char *pr_str = 0;   // per-sentence range string
    Range *cfr_rng = NULL;
    const char *cfr_str = 0;   // per-sentence range string


    int num_clusters = 0;  // i.e., k
    int maxReInits = 100;
    int numRandomReStarts = 1;
    int minSamplesPerCluster = 1;
    float conv_thres = 0.0;

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
        else if (strcmp(argp, "-omg")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
	        output_fname = *argv++;
                argc--;
            }
            else
                usage("No output mg filename given.");
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
        else if (strcmp(argp, "-cfr")==0)
        {
            if (argc>0)
            {
                cfr_str = *argv++;
                argc--;
            }
            else
                usage("No -cfr range given.");
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
        else if (strcmp(argp, "-k")==0)
        {
            if (argc>0)
            {
	        num_clusters = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No -k n, value given.");
        }
        else if (strcmp(argp, "-r")==0)
        {
            if (argc>0)
            {
	        numRandomReStarts = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No -r *n*, value given.");
        }
        else if (strcmp(argp, "-m")==0)
        {
            if (argc>0)
            {
	        maxReInits = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No -m n, value given.");
        }
        else if (strcmp(argp, "-x")==0)
        {
            if (argc>0)
            {
	        minSamplesPerCluster = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No -x n, value given.");
        }
        else if (strcmp(argp, "-a")==0)
        {
            if (argc>0)
            {
                // Next argument is debug level.
                conv_thres = parse_float(*argv++);
                argc--;
            }
            else
                usage("No Convergence threshold level given.");
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

    if (minSamplesPerCluster < 1) {
      error("Minimum samples per cluster (-x) must be >= 1.");
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

    if (cfr_str == NULL) {
      error("Must specify a context frame range.");
    } else {
      cfr_rng = new Range(cfr_str,
			     -(int)in_streamp->num_frames(),
			     (int)in_streamp->num_frames());
      if (cfr_rng->length()==0) {
	  error("Must specify a non-empty context frame range.");
      }
    }


    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////
      
     pfile_initmg(*in_streamp,
		  out_fp,
		  *sr_rng,*fr_rng,*cfr_rng,pr_str,
		  num_clusters,
		  maxReInits,
		  minSamplesPerCluster,
		  numRandomReStarts,
		  quiet_mode,
		  conv_thres);


    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete in_streamp;
    if (fclose(in_fp))
        error("Couldn't close input pfile.");
    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}

