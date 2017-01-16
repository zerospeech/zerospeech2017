//
// Copyright 2011-2012  Johns Hopkins University 
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

  for (int i = 0; i < SIG_NUM_BYTES; i++)
    fprintf(stderr,"%d ",PERMUTE_[i]);
  fprintf(stderr,"(%f s)\n",toc());

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
	  int diffspeech, int P, int B, float T, int D, Dot *dotlist ) 
{
   int Nmax = max(x_size,y_size);
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
	       // We're either not self comparing,
	       // or we are ignoring similar points that have nearby IDs
	       if ((!self_comparison) ||
		   ((abs(x_sig_ptr_[x]->id - y_sig_ptr_[y]->id) > 50))) {
		  
		  cosine = approximate_cosine(x_sig_ptr_[x], y_sig_ptr_[y]);

		  if (cosine > T) {
		     int xp, yp;		     
		     if (self_comparison) {
			if (x_sig_ptr_[x]->id < y_sig_ptr_[y]->id) {
			   xp =  x_sig_ptr_[x]->id + y_sig_ptr_[y]->id;
			   yp = -x_sig_ptr_[x]->id + y_sig_ptr_[y]->id;
			} else {
			   xp =  y_sig_ptr_[y]->id + x_sig_ptr_[x]->id;
			   yp = -y_sig_ptr_[y]->id + x_sig_ptr_[x]->id;
			}
		     } else {
			xp =  x_sig_ptr_[x]->id + y_sig_ptr_[y]->id;
			yp = -x_sig_ptr_[x]->id + y_sig_ptr_[y]->id + Nmax;
		     }

		     if ( ! signature_is_zeroed(x_sig_ptr_[x]) && ! signature_is_zeroed(y_sig_ptr_[y]) ) {
			dotlist[dotcnt].val = cosine;
			dotlist[dotcnt].xp = xp;
			dotlist[dotcnt++].yp = yp;
			
			if (D > 0) {
			   dotcnt = diagonal_probe(x_sig_, x_size, y_sig_, y_size,
						   x_sig_ptr_[x]->id, y_sig_ptr_[y]->id, T, D, 
						   self_comparison, dotcnt, dotlist);
			}
		     }
		  }
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


int plebkws( struct signature *x_sig_, int x_size, struct signature *y_sig_, int y_size,
	     int diffspeech, int P, int B, float T, int D, Dot *dotlist ) 
{
   int Nmax = max(x_size,y_size);
   int dotcnt = 0;
   
   struct signature **both_sig_ptr_ = (struct signature**) MALLOC(sizeof(struct signature*) * (x_size + y_size));
   for (int i = 0; i < y_size; i++) {
      both_sig_ptr_[i] = &(y_sig_[i]);
      both_sig_ptr_[i]->query = 1;
   }

   for (int i = 0; i < x_size; i++) {
      both_sig_ptr_[i+y_size] = &(x_sig_[i]);
      both_sig_ptr_[i+y_size]->query = 0;
   }
   
   int gap = B/2;

   for (int i = 0; i < P; i++) {
      // Sort the pointer array
      qsort(both_sig_ptr_, x_size+y_size, sizeof(struct signature*), signature_ptr_greater);

      // Proxy for query field: ( both_sig_ptr_[i] >= y_sig_ && both_sig_ptr_[i] < y_sig_+y_size )

      for ( int y = 0; y < y_size+x_size; y++ ) {
	 if ( !both_sig_ptr_[y]->query || signature_is_zeroed(both_sig_ptr_[y]) )
	    continue;
	 
	 for (int x = MAX(0,y-gap); x < MIN(x_size+y_size, y+gap); x++) {
	    if ( both_sig_ptr_[x]->query || signature_is_zeroed(both_sig_ptr_[x]) )
	       continue;
	    
	    double cosine = approximate_cosine(both_sig_ptr_[x], both_sig_ptr_[y]);
	    if (cosine > T) {
	       int xp, yp;		     
	       xp =  both_sig_ptr_[x]->id + both_sig_ptr_[y]->id;
	       yp = -both_sig_ptr_[x]->id + both_sig_ptr_[y]->id + Nmax;
	       	       
	       dotlist[dotcnt].val = cosine;
	       dotlist[dotcnt].xp = xp;
	       dotlist[dotcnt++].yp = yp;

	       if (D > 0) {
		  dotcnt = diagonal_probe(x_sig_, x_size, y_sig_, y_size,
					  both_sig_ptr_[x]->id, both_sig_ptr_[y]->id, T, D, 
					  0, dotcnt, dotlist);
	       }
	    }
	 }
      }

      // Shuffle PERMUTE_
      permute();
   }
//   exit(1);
   FREE(PERMUTE_);
   FREE(both_sig_ptr_);

   return dotcnt;
}

void fprintf_signature( FILE * f, struct signature * s, int num_bytes ) 
{
   for ( int i = 0; i < num_bytes; i++ ) {
      fprintf(stderr,"%d ", s->byte_[i]);
   }
}

#ifdef INDEXMODE
int binary_search( struct signature *q, struct signature_index *index, int p, 
		   frameind index_start, frameind index_length ) 
{
   if ( index_length == 1 ) {
      return index_start;
   }

   frameind midpt = index_start+index_length/2;
   int diff = signature_greater(q, &index->allfeats[index->order[p][midpt-1]] );

   if ( diff < 0 )
      return binary_search( q, index, p, index_start, midpt-index_start );
   else if ( diff > 0 )
      return binary_search( q, index, p, midpt, index_start+index_length-midpt );
   else
      return midpt-1;
}

int plebindex( struct signature_index *index, struct signature *y_sig_, int y_size,
	       int P, int B, float T, int D, Dot *dotlist ) 
{
   int dotcnt = 0;
   int gap = B/2;

   for (int p = 0; p < P; p++) {

      // install new permutation array
      for ( int i = 0; i < index->SIG_NUM_BYTES; i++ ) {
	 PERMUTE_[i] = index->PERMUTE[p][i];
      }

      // loop over query signatures
      for ( int y = 0; y < y_size; y++ ) {
	 // skip if query signature is silence
	 if ( signature_is_zeroed(&y_sig_[y]) ) 
	    continue;

	 // find the i-th signature in the p-th index
	 frameind pos = binary_search( &y_sig_[y], index, p, 0, index->Ntot );

	 // Loop over beam in index
	 for (int x = MAX(0,pos-gap); x <= MIN(index->Ntot, pos+gap); x++) {
	    double cosine = approximate_cosine(&index->allfeats[index->order[p][x]], &y_sig_[y]);
	    if (cosine > T) {
	       dotlist[dotcnt].val = cosine;
	       dotlist[dotcnt].xp = index->order[p][x] + y_sig_[y].id;
	       dotlist[dotcnt++].yp = -index->order[p][x] + y_sig_[y].id + index->Ntot;
	       
	       if (D > 0) {
		  dotcnt = diagonal_probe_index(index->allfeats, index->Ntot, y_sig_, y_size,
						index->order[p][x], y_sig_[y].id, T, D, 
						dotcnt, dotlist);
	       }
	    } 
	 }
      }
   }      
   FREE(PERMUTE_);

   return dotcnt;
}

int diagonal_probe_index( struct signature* x_sig_, frameind x_size,
			  struct signature* y_sig_, frameind y_size,
			  frameind x, frameind y, float T, int D, 
			  int dotcnt, Dot *dotlist ) 
{    
   int d = 1;
   double cosine;

   while (d < D && y >= d && x >= d) {
      if ( signature_is_zeroed(&x_sig_[x-d]) || signature_is_zeroed(&y_sig_[y-d]) ) {
	 d++;
	 continue;
      }

      cosine = approximate_cosine(&x_sig_[x-d], &y_sig_[y-d]);
      if (cosine > T) {
	 dotlist[dotcnt].val = cosine;
	 dotlist[dotcnt].xp = (x-d) + y_sig_[y-d].id;
	 dotlist[dotcnt++].yp = -(x-d) + y_sig_[y-d].id + x_size;
      }
      d++;
   }
   
   d = 1;
   while ((d < D) &&
	  (y + d < y_size) &&
	  (x + d < x_size)) {
      if ( signature_is_zeroed(&x_sig_[x+d]) || signature_is_zeroed(&y_sig_[y+d]) ) {
	 d++;
	 continue;
      }

      cosine = approximate_cosine(&x_sig_[x+d], &y_sig_[y+d]);
      if (cosine > T) {
	 dotlist[dotcnt].val = cosine;
	 dotlist[dotcnt].xp = (x+d) + y_sig_[y+d].id;
	 dotlist[dotcnt++].yp = -(x+d) + y_sig_[y+d].id + x_size;
      }
      d++;
   }
   
   return dotcnt;
}
#endif

int diagonal_probe( struct signature* x_sig_, int x_size,
		    struct signature* y_sig_, int y_size,
		    int x, int y, float T, int D, bool self_comparison, 
		    int dotcnt, Dot *dotlist ) 
{    
   int Nmax = max(x_size,y_size);
   int d = 1;
   double cosine;
   while ((d < D) &&
	  (y - d >= 0) &&
	  (x - d >= 0)) {
      if ( signature_is_zeroed(&x_sig_[x-d]) || signature_is_zeroed(&y_sig_[y-d]) ) {
	 d++;
	 continue;
      }

      cosine = approximate_cosine(&x_sig_[x-d], &y_sig_[y-d]);
      if (cosine > T) {
	 int xp, yp;
	 if (self_comparison) {
	    if (x_sig_[x].id < y_sig_[y].id) {
	       xp = x_sig_[x-d].id + y_sig_[y-d].id;
	       yp = -x_sig_[x-d].id + y_sig_[y-d].id;
	    } else {
	       xp = y_sig_[y-d].id + x_sig_[x-d].id;
	       yp = -y_sig_[y-d].id + x_sig_[x-d].id;
	    }
	 } else {
	    xp = x_sig_[x-d].id + y_sig_[y-d].id;
	    yp = -x_sig_[x-d].id + y_sig_[y-d].id + Nmax;
	 }
	 dotlist[dotcnt].val = cosine;
	 dotlist[dotcnt].xp = xp;
	 dotlist[dotcnt++].yp = yp;
      }
      d++;
   }
   
   d = 1;
   while ((d < D) &&
	  (y + d < y_size) &&
	  (x + d < x_size)) {
      if ( signature_is_zeroed(&x_sig_[x+d]) || signature_is_zeroed(&y_sig_[y+d]) ) {
	 d++;
	 continue;
      }

      cosine = approximate_cosine(&x_sig_[x+d], &y_sig_[y+d]);
      if (cosine > T) {
	 int xp, yp;
	 if (self_comparison) {
	    if (x_sig_[x].id < y_sig_[y].id) {
	       xp = x_sig_[x+d].id + y_sig_[y+d].id;
	       yp = -x_sig_[x+d].id + y_sig_[y+d].id;
	    } else {
	       xp = y_sig_[y+d].id + x_sig_[x+d].id;
	       yp = -y_sig_[y+d].id + x_sig_[x+d].id;
	    }
	 } else {
	    xp = x_sig_[x+d].id + y_sig_[y+d].id;
	    yp = -x_sig_[x+d].id + y_sig_[y+d].id + Nmax;
	 }
	 dotlist[dotcnt].val = cosine;
	 dotlist[dotcnt].xp = xp;
	 dotlist[dotcnt++].yp = yp;
      }
      d++;
   }
   
   return dotcnt;
}

void sig_castpath( struct signature *feats1, int N1, 
		   struct signature *feats2, int N2, 
		   int dir, int xM, int yM, float castthr, float trimthr, 
		   int R, int *xE, int *yE )
{
   float bound = 1e10;
   float scr[SPHW+1][SPHW+1];
   char path[SPHW+1][SPHW+1];
      
   for ( int i = 0; i < SPHW+1; i++ ) {
      for ( int j = 0; j < SPHW+1; j++ ) {
	 scr[i][j] = bound;
	 path[i][j] = 0;
      }
   }

   scr[0][0] = 0;
   *xE = 0;
   *yE = 0;

   for (int i = 1; i < SPHW+1; i++ ) {
      int cont = 0;
      for ( int j = max(1,i-R); j <= min(SPHW,i+R); j++ ) {

	 int x = dir*(i-1)+xM;
	 int y = dir*(j-1)+yM;

	 if ( x < 0 || x >= N1 || y < 0 || y >= N2 )
	    continue;

	 float subst_cost = 0;;

	 if ( signature_is_zeroed(&feats1[x]) || 
	      signature_is_zeroed(&feats2[y]) ) {
	    subst_cost = -1;//1;
	 } else {
	    subst_cost = approximate_cosine(&feats1[x], &feats2[y]);
	 }
	 subst_cost = (1-subst_cost)/2;

	 if ( scr[i-1][j-1] <= scr[i-1][j] && scr[i-1][j-1] <= scr[i][j-1] ) {
	    scr[i][j] = scr[i-1][j-1] + subst_cost;
	    path[i][j] = 1;
	 } else if ( scr[i-1][j] <= scr[i][j-1] ) {
	    scr[i][j] = scr[i-1][j] + subst_cost;
	    path[i][j] = 2;
	 } else {
	    scr[i][j] = scr[i][j-1] + subst_cost;
	    path[i][j] = 3;
	 }

	 if ( scr[i][j] > castthr )
	    scr[i][j] = bound;
	 else {
	    cont = 1;
	    if ( (i+1)*(i+1)+(j+1)*(j+1) > (*xE)*(*xE)+(*yE)*(*yE) ) {
	       *xE = i+1;
	       *yE = j+1;
	    }
	 }
      }
      
      if ( cont == 0 )
	 break;
   } 

   // Trim the loose ends
   while( *xE > 0 && *yE > 0 ) {
      float last = scr[*xE-1][*yE-1];
      int s = path[*xE-1][*yE-1];
      *xE = *xE - (s==2 || s==1);
      *yE = *yE - (s==3 || s==1);
      if ( s == 0 || last-scr[*xE-1][*yE-1] < trimthr )
	 break;
   }
}


void sig_secondpass( Match *matchlist, int matchcnt, 
		     struct signature *feats1, int N1, 
		     struct signature *feats2, int N2, 
		     int R, float castthr, float trimthr) 
{
   for(int n = 0; n < matchcnt; n++) {
      int xM = 0.5*(matchlist[n].xA+matchlist[n].xB);
      int yM = 0.5*(matchlist[n].yA+matchlist[n].yB);
      
      int xA = max(0,xM-SPHW);
      int xB = min(N1-1,xM+SPHW);
      int yA = max(0,yM-SPHW);
      int yB = min(N2-1,yM+SPHW);
      
      sig_castpath(feats1, N1, feats2, N2, -1, xM-1, yM-1, castthr, trimthr, R, &xA, &yA );
      xA = xM-xA+1;
      yA = yM-yA+1;
      
      sig_castpath(feats1, N1, feats2, N2, 1, xM-1, yM-1, castthr, trimthr, R, &xB, &yB );
      xB = xM+xB-1;
      yB = yM+yB-1;
      
      matchlist[n].xA = max(xA,0);
      matchlist[n].xB = min(xB,N1-1);
      matchlist[n].yA = max(yA,0);
      matchlist[n].yB = min(yB,N2-1);
   }

   return;
}

void sig_kwspath( struct signature *feats1, frameind N1, 
		  struct signature *feats2, frameind N2, 
		  int dir, frameind xM, frameind yM, 
		  int R, frameind *xE, frameind *yE )
{
   frameind Nkw = abs(*yE-yM);
   float bound = 1e10;
   float scr[SPHW+1][Nkw+1];
   char path[SPHW+1][Nkw+1];

   R = 0.5*Nkw;

   for ( int i = 0; i < SPHW+1; i++ ) {
      for ( int j = 0; j < Nkw+1; j++ ) {
	 scr[i][j] = bound;
	 path[i][j] = 0;
      }
   }
   
   scr[0][0] = 0;
   
   for (int i = 1; i < SPHW+1; i++ ) {
      for ( int j = max(1,i-R); j <= min(Nkw,i+R); j++ ) {
	 
	 frameind x = dir*(i-1)+xM;
	 frameind y = dir*(j-1)+yM;

	 if ( x < 0 || x >= N1 || y < 0 || y >= N2 )
	    continue;

	 float subst_cost = 0;;

	 if ( signature_is_zeroed(&feats1[x]) || 
	      signature_is_zeroed(&feats2[y]) ) {
	    subst_cost = -1;//1;
	 } else {
	    subst_cost = approximate_cosine(&feats1[x], &feats2[y]);
	 }
	 subst_cost = (1-subst_cost)/2;

	 if ( scr[i-1][j-1] <= scr[i-1][j] && scr[i-1][j-1] <= scr[i][j-1] ) {
	    scr[i][j] = scr[i-1][j-1] + subst_cost;
	    path[i][j] = 1;
	 } else if ( scr[i-1][j] <= scr[i][j-1] ) {
	    scr[i][j] = scr[i-1][j] + subst_cost;
	    path[i][j] = 2;
	 } else {
	    scr[i][j] = scr[i][j-1] + subst_cost;
	    path[i][j] = 3;
	 }
      }
   } 

   float scoreopt = bound;
   for (int i = 1; i < SPHW+1; i++ ) {
      if ( scr[i][Nkw] + 0*abs(i-Nkw) < scoreopt ) {
	 scoreopt = scr[i][Nkw] + 0*abs(i-Nkw);
	 *xE = i+1;
      }
   }
}

void sig_kwspass( Match *matchlist, int matchcnt, 
		  struct signature *feats1, frameind N1, 
		  struct signature *feats2, frameind N2, 
		  int R ) 
{
   for(int n = 0; n < matchcnt; n++) {
      frameind xM = 0.5*(matchlist[n].xA+matchlist[n].xB);
      frameind yM = 0.5*(matchlist[n].yA+matchlist[n].yB);
      
      frameind xA = max(0,xM-SPHW);
      frameind xB = min(N1-1,xM+SPHW);

      frameind yA = 0;
      frameind yB = N2-1;
      
      sig_kwspath(feats1, N1, feats2, N2, -1, xM-1, yM-1, R, &xA, &yA );
      xA = xM-xA+1;
      
      sig_kwspath(feats1, N1, feats2, N2, 1, xM-1, yM-1, R, &xB, &yB );
      xB = xM+xB-1;
      
      matchlist[n].xA = max(xA-1,0);
      matchlist[n].xB = min(xB-1,N1-1);
      matchlist[n].yA = yA;
      matchlist[n].yB = yB;
   }

   return;
}

void print_lex_rank (struct signature* sig) {
  for (int i = 0; i < SIG_NUM_BYTES; i++)
    printf("%d ", LEX_RANK_[sig->byte_[i]]);
}

