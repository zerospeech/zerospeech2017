// Authors: Ben Van Durme, Aren Jansen, Roland Thiolliere

#include "dot.h"
#include "signature.h"
#include "second_pass.h"

// strategy:
// 0: original one (threshold on sum)
// 1: exponential smoothing
// 2: normalized dtw

void sig_find_paths( struct signature *feats1, int N1, 
		     struct signature *feats2, int N2, 
		     int dir, int xM, int yM,
		     float castthr, float trimthr, 
		     int R, int *xE, int *yE, int strategy,
		     float alpha)
{
  int nbest = 10;

  typedef struct coordinates {
    int x, y;
    float norm;
  } coordinates;

  typedef struct store_array {
    int len;
    coordinates data[10];
    int min_idx;
    float min_val;
  } store_array;

  store_array new_store_array(int nbest) {
    store_array arr;
    arr.len = nbest;
    /* arr.data = (coordinates*)MALLOC(nbest * sizeof(coordinates)); */
    arr.min_idx = 0;
    arr.min_val = 0.;
    for (int i; i < nbest; i++) {
      arr.data[i] = (coordinates){0, 0, 0.};
    }
    return arr;
  }


  void insert(int xE, int yE, store_array arr) {
    float normE = xE*xE + yE*yE;
    if (normE < arr.min_val) {
      arr.data[arr.min_idx] = (coordinates){xE, yE, normE};
      float min_val = arr.data[0].norm;
      int min_idx = 0;
      for (int i=1; i< arr.len; i++) {
	if (arr.data[i].norm < min_val) {
	  min_val = arr.data[i].norm;
	  min_idx = i;
	}
      }
      arr.min_idx = min_idx;
      arr.min_val = min_val;
    }
  }

  
   float bound = 1e10;
   float scr[SPHW+1][SPHW+1];
   char path[SPHW+1][SPHW+1];
   float prev_cost;
   int path_lengths[SPHW+1][SPHW+1];

   store_array best = new_store_array(nbest);
      
   for ( int i = 0; i < SPHW+1; i++ ) {
      for ( int j = 0; j < SPHW+1; j++ ) {
	 scr[i][j] = bound;
	 path[i][j] = 0;
      }
   }

   scr[0][0] = 0;
   path_lengths[0][0] = 0;
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
	   prev_cost = scr[i-1][j-1];
	   path[i][j] = 1;
	   if (strategy == 2)
	     path_lengths[i][j] = path_lengths[i-1][j-1] + 1;
	 } else if ( scr[i-1][j] <= scr[i][j-1] ) {
	   prev_cost = scr[i-1][j];
	   path[i][j] = 2;
	   if (strategy == 2)
	     path_lengths[i][j] = path_lengths[i-1][j] + 1;
	 } else {
	   prev_cost = scr[i][j-1];
	   path[i][j] = 3;
	   if (strategy == 2)
	     path_lengths[i][j] = path_lengths[i][j-1] + 1;
	 }

	 if (strategy == 0)
	   scr[i][j] = prev_cost + subst_cost;
	 else if (strategy == 1)
	   scr[i][j] = alpha * prev_cost + (1 - alpha) * subst_cost;
	 else if (strategy == 2)
	   scr[i][j] = (prev_cost + subst_cost) / path_lengths[i][j];

	 if ( scr[i][j] > castthr )
	   scr[i][j] = bound;
	 else {
	    cont = 1;
	    insert(i+1, j+1, best);
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


void secondpass( Match *matchlist, int matchcnt, 
		 struct signature *feats1, int N1, 
		 struct signature *feats2, int N2, 
		 int R, float castthr, float trimthr,
		 int strategy) 
{
  float ALPHA = 0.5;
   for(int n = 0; n < matchcnt; n++) {
      int xM = 0.5*(matchlist[n].xA+matchlist[n].xB);
      int yM = 0.5*(matchlist[n].yA+matchlist[n].yB);
      
      int xA = max(0,xM-SPHW);
      int xB = min(N1-1,xM+SPHW);
      int yA = max(0,yM-SPHW);
      int yB = min(N2-1,yM+SPHW);
      
      sig_find_paths(feats1, N1, feats2, N2, -1, xM-1, yM-1, castthr, trimthr, R, &xA, &yA, strategy, ALPHA);
      xA = xM-xA+1;
      yA = yM-yA+1;
      
      sig_find_paths(feats1, N1, feats2, N2, 1, xM-1, yM-1, castthr, trimthr, R, &xB, &yB, strategy, ALPHA );
      xB = xM+xB-1;
      yB = yM+yB-1;
      
      matchlist[n].xA = max(xA,0);
      matchlist[n].xB = min(xB,N1-1);
      matchlist[n].yA = max(yA,0);
      matchlist[n].yB = min(yB,N2-1);
   }

   return;
}


