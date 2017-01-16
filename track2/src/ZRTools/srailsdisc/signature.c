//
// Copyright 2011-2015  Johns Hopkins University 
// Authors: Ben Van Durme, Aren Jansen
//

#include "signature.h"
#include "util.h"

void new_signature (struct signature *sig, int id) {
  sig->id = id;
  sig->byte_ = (byte*) MALLOC(sizeof(byte) * SIG_NUM_BYTES);
}

int hamming (struct signature* x, struct signature* y) {
  int diff = 0;
  for (int i = 0; i < SIG_NUM_BYTES; i++)
    diff += BITS_IN_[x->byte_[i] ^ y->byte_[i]];
  return diff;
}

int numComparisons = 0;

double approximate_cosine (struct signature* x, struct signature* y) {
  numComparisons++;
  return cos(hamming(x,y) * 3.1415926535897932384626433832795029/ (SIG_NUM_BYTES*8));
}

void free_signatures ( struct signature *sig, int nsig ) 
{
   for ( int n = 0; n < nsig; n++ ) {
      FREE(sig[n].byte_);
   }

   FREE(sig);
}

struct signature* read_signatures (char *filename, int *n) {
    struct signature sig;
    new_signature(&sig,0);
    int bytes_read = 0;

    // Read through file once to establish how many sigs there are
    (*n) = 0;
    assert_file_exist( filename );
    FILE *fp = fopen(filename, "r");
    while (! feof(fp)) {
      if (SIG_NUM_BYTES == (bytes_read = fread(sig.byte_, 1, SIG_NUM_BYTES, fp)))
	(*n)++;
    }
    fclose(fp);

    FREE(sig.byte_);

    // Initialize the array of signatures, now that we know how many there are
    struct signature *sig_ = (struct signature*) MALLOC((*n) * sizeof(struct signature));

    // Read in the signatures to the array
    assert_file_exist( filename );
    fp = fopen(filename, "r");
    for (int i = 0; i < *n; i++) {
      new_signature(&(sig_[i]), i);
      if (SIG_NUM_BYTES > (bytes_read = fread(sig_[i].byte_, 1, SIG_NUM_BYTES, fp))) {
	fprintf(stderr, "ERROR: in reading signature file, expected (%d) bytes but got (%d)\n",
		SIG_NUM_BYTES, bytes_read);
	exit(-1);
      }
    }
    fclose(fp);

    return sig_;
}

struct signature* readsigs_file (char *filename, int *fA, int *fB, int *n) {
    struct signature sig;
    new_signature(&sig,0);

    int bytes_read = 0;
    int maxframes = *n;

    // Read through file once to establish how many sigs there are
    (*n) = 0;
    assert_file_exist( filename );
    FILE *fp = fopen(filename, "r");
    while (! feof(fp) ) {
      if (SIG_NUM_BYTES == (bytes_read = fread(sig.byte_, 1, SIG_NUM_BYTES, fp)))
	(*n)++;
    }
    fclose(fp);
    
    if ( maxframes == 0 )
       maxframes = *n;

    FREE(sig.byte_);

    // Initialize the array of signatures, now that we know how many there are
    struct signature *sig_ = (struct signature*) MALLOC((*n) * sizeof(struct signature));

    // Read in the signatures to the array
    assert_file_exist( filename );
    fp = fopen(filename, "r");
    
    if ( *fA == -1 || *fA < 0 ) *fA = 0;
    if ( *fA >= maxframes ) {
      fprintf(stderr, "ERROR: fA is larger than total number of frames (or maxframes)\n");
      exit(1);
    }
    
    if ( *fB == -1 || *fB > (*n)-1 ) *fB = (*n)-1;
    if ( *fB < *fA ) {
      fprintf(stderr, "ERROR: fB is less than fA\n");
      exit(1);
    }
    
    *n = *fB - *fA + 1;
    if ( (*n) > maxframes ) {
       *fB = *fA + maxframes - 1;
       *n = *fB - *fA + 1;
    }
    
    int offset = SIG_NUM_BYTES*(*fA);
    if(fseek(fp, offset, SEEK_SET) == EOF) fprintf(stderr,"seek failed");

    for (int i = 0; i < (*n); i++) {
      new_signature(&(sig_[i]), i);
      if (SIG_NUM_BYTES > (bytes_read = fread(sig_[i].byte_, 1, SIG_NUM_BYTES, fp))) {
	fprintf(stderr, "ERROR: in reading signature file, expected (%d) bytes but got (%d)\n",
		SIG_NUM_BYTES, bytes_read);
	exit(-1);
      }
    }
    fclose(fp);

    return sig_;
}

void initialize_permute () {
  PERMUTE_ = (int*) MALLOC(sizeof(int) * SIG_NUM_BYTES);
  for (int i = 0; i < SIG_NUM_BYTES; i++)
    PERMUTE_[i] = i;
}

void permute () {
  int tmp;

//  for (int i = 0; i < SIG_NUM_BYTES; i++)
//    fprintf(stderr,"%d ",PERMUTE_[i]);
//  fprintf(stderr,"(%f s)\n",toc());

  // Reverse
  for (int i = 0; i < SIG_NUM_BYTES/2; i++) {
    tmp = PERMUTE_[i];
    PERMUTE_[i] = PERMUTE_[SIG_NUM_BYTES-1-i];
    PERMUTE_[SIG_NUM_BYTES-1-i] = tmp;
  }

  // If first item is odd then return
  if (PERMUTE_[0] % 2 == 1)
    return;

  // Otherwise double permute
  for (int i = 0; i < SIG_NUM_BYTES; i+= 2) {
    tmp = PERMUTE_[i];
    PERMUTE_[i] = PERMUTE_[i+1];
    PERMUTE_[i+1] = tmp;
  }

  for (int i = 1; i < SIG_NUM_BYTES -1; i+= 2) {
    tmp = PERMUTE_[i];
    PERMUTE_[i] = PERMUTE_[i+1];
    PERMUTE_[i+1] = tmp;
  }
  tmp = PERMUTE_[0];
  PERMUTE_[0] = PERMUTE_[SIG_NUM_BYTES -1];
  PERMUTE_[SIG_NUM_BYTES -1] = tmp;
}

int signature_greater (const void *x, const void *y) {
  int diff;
  byte* x_sig = ((struct signature*)x)->byte_;
  byte* y_sig = ((struct signature*)y)->byte_;
  for (int i = 0; i < SIG_NUM_BYTES; i++) {
    diff = LEX_RANK_[x_sig[PERMUTE_[i]]] - LEX_RANK_[y_sig[PERMUTE_[i]]];
    if (diff > 0)
      return 1;
    else if (diff < 0)
      return -1;
  }
  return 0;
}

int signature_equal (const void *x, const void *y) {
  int diff;
  byte* x_sig = ((struct signature*)x)->byte_;
  byte* y_sig = ((struct signature*)y)->byte_;
  for (int i = 0; i < SIG_NUM_BYTES; i++) {
    diff = LEX_RANK_[x_sig[PERMUTE_[i]]] - LEX_RANK_[y_sig[PERMUTE_[i]]];
    if (diff != 0)
      return 0;
  }
  return 1;
}

// Should have same effect as above, at some point need to test relative efficiency
int signature_greater_shift (const void *x, const void *y) {
  byte mask;
  byte* x_sig = ((struct signature*)x)->byte_;
  byte* y_sig = ((struct signature*)y)->byte_;
  int i, j;
  for (i = 0; i < SIG_NUM_BYTES; i++) {
    for (j = 0; j < 8; j++) {
      mask = 1 << j;
      if ((x_sig[PERMUTE_[i]] & mask) != (y_sig[PERMUTE_[i]] & mask)) {
	if (x_sig[PERMUTE_[i]] & mask)
	  return 1;
	else
	  return -1;
      }
    }
  }
  return 0;
}


int signature_ptr_greater (const void *x, const void *y) {
  return signature_greater((*(struct signature**)x),(*(struct signature**)y));
}

int signature_ptr_equal (const void *x, const void *y) {
  return signature_equal((*(struct signature**)x),(*(struct signature**)y));
}

bool signature_is_zeroed (const struct signature* x) {
  for (int i = 0; i < SIG_NUM_BYTES; i++)
    if (x->byte_[i] != 0)
      return false;
  //printf("zeroed ");
  return true;
}

int pleb( struct signature *x_sig_, int x_size, struct signature *y_sig_, int y_size,
	  int diffspeech, int P, int B, float T, Dot *dotlist ) 
{
   int dotcnt = 0;
   
   // Are we comparing a set of signatures to itself?
   bool self_comparison = !diffspeech;

   struct signature **x_sig_ptr_ = (struct signature**) MALLOC(sizeof(struct signature*) * x_size);
   for (int i = 0; i < x_size; i++) {
      x_sig_ptr_[i] = &(x_sig_[i]);
   }
   
   struct signature **y_sig_ptr_;
   if (self_comparison) {
      y_sig_ptr_ = x_sig_ptr_;
   } else {
      y_sig_ptr_ = (struct signature**) MALLOC(sizeof(struct signature*) * y_size);
      for (int i = 0; i < y_size; i++)
	 y_sig_ptr_[i] = &(y_sig_[i]);
   }
   
   int y_current;  
   double cosine;
   int gap = B/2;
   int start;
   for (int i = 0; i < P; i++) {
      // Sort the pointer arrays
      qsort(x_sig_ptr_, x_size, sizeof(struct signature*), signature_ptr_greater);

      if (! self_comparison)
	 qsort(y_sig_ptr_, y_size, sizeof(struct signature*), signature_ptr_greater);
      
      // Find nearest neighbors
      y_current = 0;
      for (int x = 0; x < x_size; x++) {
	 if (! signature_is_zeroed(x_sig_ptr_[x]) ) {
	    
	    // Increment y_current until x fits into the sort of Y
	    if (signature_greater(x_sig_ptr_[x], y_sig_ptr_[y_current]) > 0) {
	       y_current++;
	       while ((y_current < y_size) && (signature_greater(x_sig_ptr_[x], y_sig_ptr_[y_current]) > 0))
		  y_current++;
	       start = y_current;
	       // if there is a run of equal elements, then center y_current within those elements
	       while ((y_current < y_size) && (signature_greater(x_sig_ptr_[x], y_sig_ptr_[y_current]) == 0))
		  y_current++;
	       if (y_current == y_size)
		  y_current--;
	       y_current = (y_current + start) / 2;
	    }

	    for (int y = MAX(0,y_current-gap); y < MIN(y_size, y_current + gap); y++) {
	       cosine = approximate_cosine(x_sig_ptr_[x], y_sig_ptr_[y]);
	       
	       if ( self_comparison && x_sig_ptr_[x]->id >= y_sig_ptr_[y]->id ) 
		  continue;

	       if (cosine > T && !signature_is_zeroed(x_sig_ptr_[x]) 
		   && !signature_is_zeroed(y_sig_ptr_[y]) ) {
		  dotlist[dotcnt].val = cosine;
		  dotlist[dotcnt].xp = x_sig_ptr_[x]->id;
		  dotlist[dotcnt++].yp = y_sig_ptr_[y]->id;
	       }
	    }
	 }
      }

      // Shuffle PERMUTE_
      permute();
   }

   FREE(PERMUTE_);
   FREE(x_sig_ptr_);
   if ( diffspeech )
      FREE(y_sig_ptr_);
   return dotcnt;
}

void fprintf_signature( FILE * f, struct signature * s, int num_bytes ) 
{
   for ( int i = 0; i < num_bytes; i++ ) {
      fprintf(stderr,"%d ", s->byte_[i]);
   }
}

void print_lex_rank (struct signature* sig) {
  for (int i = 0; i < SIG_NUM_BYTES; i++)
    printf("%d ", LEX_RANK_[sig->byte_[i]]);
}

