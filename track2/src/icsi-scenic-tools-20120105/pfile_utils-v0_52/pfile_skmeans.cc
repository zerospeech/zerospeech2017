/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_skmeans.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
  
    This program computes utterance specific segmental k-means.
     
    That is, for each segment of the same utterance (e.g., word),
    N segments of equal length are defined and the k-means 
    algorithm is computed on each of them.
    The resulting means and variances are printed as output.

    This program was hacked together very quickly so there is currently
    little C-code optimization performed.
*/

#include "pfile_utils.h"

#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_WAIT_H
#  include <wait.h>
#else 
#  include <sys/wait.h>
#endif

#include "QN_PFile.h"
#include "error.h"
#include "Range.H"
#include "rand.h"

RAND myrand(true);
unsigned short seedv[3];

#define MIN(a,b) ((a)<(b)?(a):(b))

#define SENTS_PER_PRINT 1

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
	    "-o <file-name>  output file ('-' for stdout)\n"
	    "-b              Binary rather than ASCII output\n"
	    "-k #            number of clusters (i.e., K)\n"
	    "-r #            Number of epoch random restarts to take best of\n"
	    "-m #            Maximum number of re-inits before giving up\n"
	    "-x #            Min number of samples/cluster for a re-init to occur\n"
	    "-a #            Convergence threshold (default 0.0)\n"
            "-u              Uniform segmental k-means\n"
            "Options under -u\n"
	    "   -n #            Total number of different words\n"
	    "   -f <file-name>  input utterance-ID file\n"
	    "   -s #            number of segments/words\n"
            "-v              Viterbi segmental k-means\n"
            "Options under -v\n"
	    "   -n #            Maximum number of labels (range [0:n-1])\n"
	    "   -f <file-name>  input label pfile\n"
	    "-pr range       per-sentence frame range\n"
	    "-prefetch       Prefetch next sentence at each iteration\n"
	    "-q              quite mode\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


/////////////////////////////////////////////////////////////////////////////
// SentIdStream_File returns a stream of sentence id strings from a file.
// TOOD: Make separate abstract base class.
/////////////////////////////////////////////////////////////////////////////

class SentIdStream_File
{
public:
  SentIdStream_File(FILE *fp,const int nw);
  ~SentIdStream_File();

  void rewind() { if (fp != NULL) ::rewind(fp); }
  const char *next();         // Returns pointer to next sentence id, or
                              // 0 if at end of file or error.

  operator char*() { return buf; }
  operator size_t() { return index; }

  size_t number_read() { return nread; }
private:
  FILE *fp;
  size_t buf_size;
  char *buf;
  size_t nread;
  size_t index;
  int num_words;
};

SentIdStream_File::SentIdStream_File(FILE *fp_arg,const int nw)
    : fp(fp_arg),num_words(nw)
{
    buf_size = 128;
    buf = new char[buf_size];
    index = 0;
    ::sprintf(buf,"0");
}

SentIdStream_File::~SentIdStream_File()
{
    delete buf;
}

const char*
SentIdStream_File::next()
{

  // TODO: Doesn't distinguish file I/O errors from end of file condition.
  if (fp == NULL) {
    // if no file, return infinite stream of 0's
    return buf;
  }

  if (fgets(buf, buf_size, fp)==NULL)
    {
      error("Error: EOF in labels file, or I/O error. nread=%ld",
	    nread);
      return 0;
    }
  else
    {
      char *p = strchr(buf,'\n'); // Get pos. of newline character.
      if (p==NULL)
        {
	  error("Sentence id too long.");
        }
      else
        {
	  *p = '\0';          // Trim off newline character.
        }   
      char *ptr;
      int tmp = ::strtol(buf,&ptr,10);
      if (ptr == buf) 
	error("No integer at position %d in file.",nread);
      if (tmp >= num_words) 
	error("Word id (%d) > num_words (%d).",index,num_words);
      index = (size_t)tmp;
      nread++;
      return buf;
    }
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


/////////////////////////////////////////////////////////////////////////////
// pfile_skmeans: compute segmental kmeans
/////////////////////////////////////////////////////////////////////////////


static void
pfile_uniform_skmeans(QN_InFtrLabStream& in_stream,FILE *out_fp,
		      SentIdStream_File& sid_stream,
		      const int num_words, const int num_segments, 
		      const int num_clusters,const int maxReInits,
		      const int minSamplesPerCluster,
		      const int numRandomReStarts,
		      const char*pr_str,
		      const bool binary,const bool quiet_mode,
		      const float conv_thres,
		      const bool prefetch)
{

    // Feature and label buffers are dynamically grown as needed.

    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_labs = in_stream.num_labs();
    const size_t n_ftrs = in_stream.num_ftrs();

    float *ftr_buf = new float[buf_size * n_ftrs];
    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];

    kmeans::kmeans_k = num_clusters;
    kmeans::kmeans_vl = n_ftrs;

    kmeans *kms = new kmeans[num_words*num_segments];
    size_t sent_no;

    double bestVarianceSum=HUGE;
    
    for (int epoch=0;epoch<numRandomReStarts;epoch++) {

      printf("Epoch %d\n",epoch);

      int i,j,iter=0;
      int reInits = 0;
    
      float max_dist = 0;

      for (i=0;i<num_words*num_segments;i++) {
	kms[i].randomAssignment = true;
	kms[i].done = false;
      }

      do {
	iter++;


	sid_stream.rewind();
	for (i=0;i<num_words*num_segments;i++) {
	  if (!kms[i].done)
	    kms[i].initNew();
	}

	pid_t pid = (pid_t)1;
	for (sent_no=0;sent_no<in_stream.num_segs();sent_no++) {

	  if (prefetch && pid != 0 && sent_no > 0) {
	    // pid != 0 means this is the parent
	    // sent_no > 0 means there must have been a child since
	    // this is not the first iteration.
	    wait(0); // wait for the child to complete
	  }

	  const size_t n_frames = in_stream.num_frames((sent_no));
	  sid_stream.next();
	  size_t word_id = (size_t) sid_stream;
	
	  if (word_id >= (size_t)num_words) {
	    error("Invalid word_id (%d) at position %d\n",
		  word_id,sent_no+1);
	  }

	  if (!quiet_mode) {
	    if ((sent_no) % SENTS_PER_PRINT == 0)
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

	  
	  if (prefetch) {
	    // this is a hack to pre-fetch the next sentence
	    // and optimize Solaris's disk cache strategy.
	    if (pid != (pid_t)0) {
	      // then this is the parent process
	      if ((sent_no+1)<in_stream.num_segs()) {
		// then there is a next iteration.
		pid = fork();
	      }
	    } else {
	      // then this is the child process
	      // who just read in the next iteration's data into disk cache.
	      exit(0);
	    }
	  
	    if (pid == (pid_t)0) {
	      // then this is the new child,
	      // continue with the next iteration.
	      continue;
	    }
	  }
	  
	  Range prrng(pr_str,0,n_frames);
	  const int frames_per_segment = prrng.length()/num_segments;
	  int cur_seg_no = 0;
	  int segs_so_far = 0;
	  for (Range::iterator prit=prrng.begin();
	       !prit.at_end() ; ++prit) {

	    if (!kms[word_id*num_segments + cur_seg_no].done) {
	      // compute a pointer to the current buffer.
	      const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	      if (kms[word_id*num_segments + cur_seg_no].randomAssignment)
		kms[word_id*num_segments + cur_seg_no].add2newRand(ftr_buf_p);
	      else
		kms[word_id*num_segments + cur_seg_no].add2new(ftr_buf_p);
	    }

	    segs_so_far++;
	    if (segs_so_far >= frames_per_segment 
		&& (cur_seg_no+1 < num_segments)) {
	      segs_so_far = 0;
	      cur_seg_no++;
	    }
	  }
	}

	for (i=0;i<num_words*num_segments;i++) {
	  if (!kms[i].done)
	    kms[i].randomAssignment = false;
	}

	// make sure each kmeans had some data
	max_dist = 0;
	int num_active=0;
	int num_reinits = 0;
	for (i=0;i<num_words*num_segments;i++) {
	  if (!kms[i].done) {
	    if (kms[i].zeroCounts()) {
	      // This shouldn't happen with uniform skmeans. All kmeans
	      // objects should be getting some data.
	      error("kms[%d] was given no input. Probably an command line argument error");
	    } else if (kms[i].someClusterHasLessThanNEntries(minSamplesPerCluster)) {
	      // fprintf(stderr,
	      //       "Warning: kms word %d seg %d, some clusters have < %d entries. Re-initializing.\n",i/num_segments,i % num_segments,minSamplesPerCluster);
	    
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
		// printf("Iter %d: kms word %d seg %d converged\n",iter,
		//       i/num_segments, i % num_segments);
	      }
	    }
	    num_active++;
	  }
	}
	if (num_reinits)
	  reInits++;
	printf("Iter %d: max_dist = %e, cur num_reinits = %d, tot itr re_init = %d, num_active = %d\n",iter,max_dist,num_reinits,reInits,num_active);
	fflush(stdout);

      } while (max_dist > conv_thres && reInits <= maxReInits);

      if (reInits > maxReInits) {
	error("Error. %d re-inits and convergence didn't occur.", reInits);
      }

      // Do one more pass over file to compute the variances.
      sid_stream.rewind();
      for (sent_no=0;sent_no<in_stream.num_segs();sent_no++) {

	const size_t n_frames = in_stream.num_frames((sent_no));
	sid_stream.next();
	size_t word_id = (size_t) sid_stream;
	
	if (word_id >= (size_t)num_words) {
	  error("Invalid word_id (%d) at position %d\n",
		word_id,sent_no+1);
	}

	if (!quiet_mode) {
	  if ((sent_no) % SENTS_PER_PRINT == 0)
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
	const int frames_per_segment = prrng.length()/num_segments;
	int cur_seg_no = 0;
	int segs_so_far = 0;
	for (Range::iterator prit=prrng.begin();
	     !prit.at_end() ; ++prit) {

	  // compute a pointer to the current buffer.
	  const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	  kms[word_id*num_segments + cur_seg_no].computeVariances(ftr_buf_p);

	  segs_so_far++;
	  if (segs_so_far >= frames_per_segment 
	      && (cur_seg_no+1 < num_segments)) {
	    segs_so_far = 0;
	    cur_seg_no++;
	  }
	}
      }

    
      kmeans *kmsp = kms;
      double sumVariances=0;
      for (i=0;i<num_words;i++) {
	for (j=0;j<num_segments;j++) {
	  sumVariances+=kmsp->finishVariances();
	  kmsp++;
	}
      }
      
      printf("End of Epoch %d of %d, variance sum = %e, best = %e\n",epoch,
	     numRandomReStarts,
	     sumVariances,bestVarianceSum);

      if (sumVariances < bestVarianceSum) {
	bestVarianceSum = sumVariances;
	kmsp = kms;
	for (i=0;i<num_words;i++) {
	  for (j=0;j<num_segments;j++) {
	    kmsp->save();
	    kmsp++;
	  }
	}
      }
    }
    
    kmeans *kmsp = kms;
    for (int i=0;i<num_words;i++) {
      for (int j=0;j<num_segments;j++) {
	fprintf(out_fp,"word %d, seg %d:\n",i,j);
	kmsp->printSaved(out_fp);
	kmsp++;
      }
    }

    delete [] ftr_buf;
    delete [] lab_buf;
    delete [] kms;
}




static void
pfile_viterbi_skmeans(QN_InFtrLabStream& in_stream,FILE *out_fp,
		      QN_InFtrLabStream* in_lstreamp,
		      const int num_labels,
		      const int num_clusters,const int maxReInits,
		      const int minSamplesPerCluster,
		      const int numRandomReStarts,
		      const char*pr_str,
		      const bool binary,const bool quiet_mode,
		      const float conv_thres,
		      const bool prefetch)
{

    // Feature and label buffers are dynamically grown as needed.

    size_t buf_size = 300;      // Start with storage for 300 frames.
    const size_t n_ftrs = in_stream.num_ftrs();
    const size_t n_labs = (in_lstreamp==NULL
			   ? in_stream.num_labs()
			   : in_lstreamp->num_labs());
    if (n_labs != 1)
      error("pfile containing labels must have exactly 1 label per frame.");

    if (n_ftrs == 0)
      error("pfile must have more than 0 features.");

    float *ftr_buf = new float[buf_size * n_ftrs];
    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];

    kmeans::kmeans_k = num_clusters;
    kmeans::kmeans_vl = n_ftrs;

    kmeans *kms = new kmeans[num_labels];
    size_t sent_no;

    double bestVarianceSum=HUGE;

    if (in_lstreamp != NULL) {
      if (in_lstreamp->num_segs() != in_stream.num_segs())
	error("Feature and label pfile have differing number of sentences.");
    }
    
    for (int epoch=0;epoch<numRandomReStarts;epoch++) {

      printf("Epoch %d\n",epoch);

      int i,iter=0;
      int reInits = 0;
    
      float max_dist = 0;

      for (i=0;i<num_labels;i++) {
	kms[i].randomAssignment = true;
	kms[i].done = false;
      }

      do {
	iter++;

	for (i=0;i<num_labels;i++) {
	  if (!kms[i].done)
	    kms[i].initNew();
	}

	pid_t pid = (pid_t)1;
	for (sent_no=0;sent_no<in_stream.num_segs();sent_no++) {

	  if (prefetch && pid != 0 && sent_no > 0) {
	    // pid != 0 means this is the parent
	    // sent_no > 0 means there must have been a child since
	    // this is not the first iteration.
	    wait(0); // wait for the child to complete
	  }

	  const size_t n_frames = in_stream.num_frames((sent_no));
	  if (in_lstreamp != NULL) {
	    if (in_lstreamp->num_frames(sent_no) != n_frames)
	      error("Feature and label pfile have differing number of frames at sentence %d.",sent_no);
	  }

	  if (!quiet_mode) {
	    if ((sent_no) % 1 == 0)
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
	  if (in_lstreamp != NULL) {
	    const QN_SegID seg_id = in_lstreamp->set_pos((sent_no), 0);
	    if (seg_id == QN_SEGID_BAD)
	      {
		fprintf(stderr,
			"%s: Couldn't seek to start of sentence %lu "
			"in label pfile.",
			program_name, (unsigned long) (sent_no));
		error("Aborting.");
	      }
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

	  if (in_lstreamp != NULL) {
	    const size_t n_read =
	      in_lstreamp->read_labs(n_frames, lab_buf);
	    if (n_read != n_frames) {
	      fprintf(stderr, "%s: At sentence %lu in label pfile, "
		      "only read %lu frames when should have read %lu.\n",
		      program_name, (unsigned long) (sent_no),
		      (unsigned long) n_read, (unsigned long) n_frames);
	      error("Aborting.");
	    }
	  }

	  if (prefetch) {
	    // this is a hack to pre-fetch the next sentence from disk
	    // and optimize any disk cache strategy.
	    if (pid != (pid_t)0) {
	      // then this is the parent process
	      if ((sent_no+1)<in_stream.num_segs()) {
		// then there is a next iteration.
		pid = fork();
	      }
	    } else {
	      // then this is the child process
	      // who just read in the next iteration's data into disk cache.
	      exit(0);
	    }
	  
	    if (pid == (pid_t)0) {
	      // then this is the new child,
	      // continue with the next iteration.
	      continue;
	    }
	  }

	  Range prrng(pr_str,0,n_frames);
	  for (Range::iterator prit=prrng.begin();
	       !prit.at_end() ; ++prit) {

	    const size_t curLab = lab_buf[(*prit)];
	    if ((int)curLab >= num_labels)
	      error("Label at sentence %d, frame %d is %d and is >= %d.",
		    sent_no,(*prit),curLab,num_labels);

	    if (!kms[curLab].done) {
	      // compute a pointer to the current buffer.
	      const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	      if (kms[curLab].randomAssignment)
		kms[curLab].add2newRand(ftr_buf_p);
	      else
		kms[curLab].add2new(ftr_buf_p);
	    }

	  }
	}

	for (i=0;i<num_labels;i++) {
	  if (!kms[i].done)
	    kms[i].randomAssignment = false;
	}


	// make sure each kmeans had some data
	max_dist = 0;
	int num_active=0;
	int num_reinits = 0;
	for (i=0;i<num_labels;i++) {
	  if (!kms[i].done) {
	    if (kms[i].zeroCounts()) {
	      // this is ok, this label perhaps doesn't exist in the file.
	    } else if (kms[i].someClusterHasLessThanNEntries(minSamplesPerCluster)) {
	      // fprintf(stderr,
	      //       "Warning: kms label %d, some clusters have < %d entries. Re-initializing.\n",i,minSamplesPerCluster);
	      
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
		printf("Iter %d: label %d converged\n",iter,i);
	      }
	    }
	  }
	  num_active++;
	}
	if (num_reinits)
	  reInits++;
	printf("Iter %d: max_dist = %e, cur num_reinits = %d, tot itr re_init = %d, num_active = %d\n",iter,max_dist,num_reinits,reInits,num_active);
	fflush(stdout);

      } while (max_dist > conv_thres && reInits <= maxReInits);

      if (reInits > maxReInits) {
	error("Error. %d re-inits and convergence didn't occur.", reInits);
      }

      // Do one more pass over file to compute the variances.
      for (sent_no=0;sent_no<in_stream.num_segs();sent_no++) {

	const size_t n_frames = in_stream.num_frames((sent_no));
	if (in_lstreamp != NULL) {
	  if (in_lstreamp->num_frames(sent_no) != n_frames)
	    error("Feature and label pfile have differing number of frames at sentence %d.",sent_no);
	}

	if (!quiet_mode) {
	  if ((sent_no) % SENTS_PER_PRINT == 0)
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
	if (seg_id == QN_SEGID_BAD) {
	  fprintf(stderr,
		  "%s: Couldn't seek to start of sentence %lu "
		  "in input pfile.",
		  program_name, (unsigned long) (sent_no));
	  error("Aborting.");
	}
	if (in_lstreamp != NULL) {
	  const QN_SegID seg_id = in_lstreamp->set_pos((sent_no), 0);
	  if (seg_id == QN_SEGID_BAD)
	    {
	      fprintf(stderr,
		      "%s: Couldn't seek to start of sentence %lu "
		      "in label pfile.",
		      program_name, (unsigned long) (sent_no));
	      error("Aborting.");
	    }
	}
	const size_t n_read =
	  in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);

	if (n_read != n_frames) {
	  fprintf(stderr, "%s: At sentence %lu in input pfile, "
		  "only read %lu frames when should have read %lu.\n",
		  program_name, (unsigned long) (sent_no),
		  (unsigned long) n_read, (unsigned long) n_frames);
	  error("Aborting.");
	}
	if (in_lstreamp != NULL) {
	  const size_t n_read =
	    in_lstreamp->read_labs(n_frames, lab_buf);
	  if (n_read != n_frames) {
	    fprintf(stderr, "%s: At sentence %lu in label pfile, "
		    "only read %lu frames when should have read %lu.\n",
		    program_name, (unsigned long) (sent_no),
		    (unsigned long) n_read, (unsigned long) n_frames);
	    error("Aborting.");
	  }
	}


	Range prrng(pr_str,0,n_frames);
	for (Range::iterator prit=prrng.begin();
	     !prit.at_end() ; ++prit) {

	  const size_t curLab = lab_buf[(*prit)];
	  // compute a pointer to the current buffer.
	  const float *const ftr_buf_p = ftr_buf + (*prit)*n_ftrs;
	  kms[curLab].computeVariances(ftr_buf_p);
	}
      }

    
      kmeans *kmsp = kms;
      double sumVariances=0;
      for (i=0;i<num_labels;i++) {
	if (!kmsp->zeroCounts())
	  sumVariances+=kmsp->finishVariances();
	kmsp++;
      }
      

      if (sumVariances < bestVarianceSum) {
	bestVarianceSum = sumVariances;
	kmsp = kms;
	for (i=0;i<num_labels;i++) {
	  if (!kmsp->zeroCounts())
	    kmsp->save();
	  kmsp++;
	}
      }
      printf("End of Epoch %d of %d, variance sum = %e, best = %e\n",epoch,
	     numRandomReStarts,
	     sumVariances,bestVarianceSum);


    }

    kmeans *kmsp = kms;
    for (int i=0;i<num_labels;i++) {
      if (!kmsp->zeroCounts()) {
	fprintf(out_fp,"label %d:\n",i);
	kmsp->printSaved(out_fp);
      }
      kmsp++;
    }

    delete [] ftr_buf;
    delete [] lab_buf;
    delete [] kms;
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

    const char *input_fname = 0;   // Input pfile name.
    int num_words = 0;
    const char *input_uname = 0;   // Input utterance/label file name.
    const char *output_fname = 0;   // Input ASCII name.
    int num_segments = 0;
    int num_clusters = 0;
    int maxReInits = 50;
    int numRandomReStarts = 1;
    int minSamplesPerCluster = 1;
    float conv_thres = 0.0;

    const char *pr_str = 0;   // per-sentence range string

    bool binary = false;
    int debug_level = 0;
    bool quiet_mode = false;


    // if not true, we do viterbi k-means
    bool uniform_skm = true;

    bool prefetch = false;

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
        else if (strcmp(argp, "-f")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is input file name.
	        input_uname = *argv++;
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
        else if (strcmp(argp, "-n")==0)
        {
            if (argc>0)
            {
	        num_words = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No -n n, value given.");
        }
        else if (strcmp(argp, "-s")==0)
        {
            if (argc>0)
            {
	        num_segments = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No -s n, value given.");
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
        else if (strcmp(argp, "-b")==0)
        {
	  binary = true;
        }
        else if (strcmp(argp, "-u")==0)
        {
	  uniform_skm = true;
        }
        else if (strcmp(argp, "-prefetch")==0)
        {
	  prefetch = true;
        }
        else if (strcmp(argp, "-v")==0)
        {
	  uniform_skm = false;
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


    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////
      
    if (uniform_skm) {
      FILE *ut_fp=NULL;;
      if (input_uname != NULL) {
	ut_fp = fopen(input_uname, "r");
	if (ut_fp==NULL)
	  error("Couldn't open input utternace file for reading.");
      } else {
	// all utterances are assumed the same.
      }
      SentIdStream_File* sid_streamp
	= new SentIdStream_File(ut_fp,num_words);
      pfile_uniform_skmeans(*in_streamp,out_fp,*sid_streamp,
			    num_words,num_segments,num_clusters,maxReInits,
			    minSamplesPerCluster,
			    numRandomReStarts,
			    pr_str,
			    binary,quiet_mode,conv_thres,
			    prefetch);
      if (ut_fp) {
	if (fclose(ut_fp))
	  error("Couldn't close utterance file.");
      }
    } else {
      FILE *vp_fp=NULL; 
      QN_InFtrLabStream* in_lstreamp = NULL;
     
      if (input_uname != NULL) {
	vp_fp = fopen(input_uname, "r");
	if (vp_fp==NULL)
	  error("Couldn't open input label pfile for reading.");
        in_lstreamp = new QN_InFtrLabStream_PFile(debug_level, "", vp_fp, 1);
      } else {
	// labels must be in the feature pfile.
	if (in_streamp->num_labs() != 1)
	  error("No labels specified and input pfile is labelless");
      }      
      pfile_viterbi_skmeans(*in_streamp,out_fp,in_lstreamp,
			    num_words,num_clusters,maxReInits,
			    minSamplesPerCluster,
			    numRandomReStarts,
			    pr_str,
			    binary,quiet_mode,conv_thres,
			    prefetch);

      if (in_lstreamp != NULL)
	delete in_lstreamp;
      
      if (vp_fp) {
	if (fclose(vp_fp))
	  error("Couldn't close utterance file.");
      }
    }

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

