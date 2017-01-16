//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "util.h"
#include "dot.h"

#define RHOSKIP 50
#define MAXMATCHES 100000
#define PSILTHR 0.5

int Dot_compare(const void *A, const void *B) 
{
   if( ((Dot *)A)->yp > ((Dot *)B)->yp ) return 1;
   else if( ((Dot *)A)->yp < ((Dot *)B)->yp ) return -1;
   else if( ((Dot *)A)->xp > ((Dot *)B)->xp ) return 1;
   else if( ((Dot *)A)->xp < ((Dot *)B)->xp ) return -1;
   else return 0;
}

int DotXV_compare(const void *A, const void *B)
{
   if( ((DotXV *)A)->xp > ((DotXV *)B)->xp) return 1;
   else if(((DotXV *)A)->xp < ((DotXV *)B)->xp) return -1;
   else return 0;
}

void hist_feats(int *hist, float *feats, int nframes, int nphonemes, float threshold)
{
  for(int frame=0;frame<nframes;frame++)
    for(int phoneme=0; phoneme<nphonemes; phoneme++, feats++)
      if(*feats > threshold) hist[phoneme]++;
}

void radix_sorty(int N, Dot *dotlist, int dotcnt, DotXV *sorted_dots, int *cumsum)
{
  int *rowcnts = (int *)MALLOC(N*sizeof(int));
  memset(rowcnts, 0, sizeof(*rowcnts) * N);

  for(int ind = 0; ind<dotcnt; ind++)
    rowcnts[dotlist[ind].yp]++;

  make_cumhist_signed(cumsum, rowcnts, N);

  memset(rowcnts, 0, sizeof(*rowcnts) * N);

  Dot *start = dotlist;
  Dot *end = dotlist + dotcnt;

  for(; start < end; start++) {
    int yp = start->yp;
    DotXV *d = &sorted_dots[cumsum[yp]+rowcnts[yp]];
    d->xp = start->xp;
    d->val = start->val;
    rowcnts[yp]++;
  }
  FREE(rowcnts);
}

void quick_sortx(int N, DotXV *radixdots, int dotcnt, int *cumsum )
{
   for (int yp=0; yp<N; yp++)
      qsort(&radixdots[cumsum[yp]], cumsum[yp+1]-cumsum[yp], sizeof(DotXV), &DotXV_compare);
}

int compress_dotlist(Dot *dotlist, int prime, float thr)
{
   int wpos = 0;
   int rpos = 1;

   for(; wpos<prime; wpos++) {
      if(wpos==rpos) rpos++;

      if(dotlist[wpos].val<=thr) {
	 while(rpos<prime && dotlist[rpos].val<=thr) rpos++;
	 if(rpos==prime) return wpos;

	 dotlist[wpos].val = 1;
	 dotlist[wpos].xp = dotlist[rpos].xp;
	 dotlist[wpos].yp = dotlist[rpos].yp;
	 dotlist[rpos].val = 0;
      }
   } 

   return wpos;
}

int dot_dedup(DotXV *dotlist, int *cumsum, int nrows, float thr)
{
   int initcnt = cumsum[nrows];

   int *rowcnt = (int *) MALLOC( sizeof(int)*nrows );

   for ( int i = 0; i < nrows; i++ )
   {
      //fprintf(stderr,"i: %d, rowcnt: \n", i);
      rowcnt[i] = cumsum[i+1]-cumsum[i];
      //fprintf(stderr,"i: %d, rowcnt: %d ", i, rowcnt[i]);
      int start = (i == 0) ? 0 : cumsum[i];
      for ( int j = start+1; j < cumsum[i+1]; j++ ) {
	 if ( dotlist[j].xp == dotlist[j-1].xp ) {
	    dotlist[j].val = -1e6;
	    rowcnt[i]--;
	 }	 
      }
   }

   for ( int i = 0; i < nrows; i++ )
   {
      cumsum[i+1] = cumsum[i]+rowcnt[i];
   }

   FREE(rowcnt);

   int wpos = 0;
   int rpos = 1;

   for(; wpos<initcnt; wpos++) {
      if(wpos==rpos) rpos++;

      if(dotlist[wpos].val < thr) {
	 while(rpos < initcnt && dotlist[rpos].val < thr) rpos++;
	 if(rpos==initcnt) return wpos;

	 dotlist[wpos].val = dotlist[rpos].val;
	 dotlist[wpos].xp = dotlist[rpos].xp;
	 dotlist[rpos].val = -1e6;
      }
   } 
   
   return wpos;
}

int compute_dotplot_sparse(float *feats1, int N1, float *feats2, int N2, int D, int sildim, float *silence1, float *silence2, int diffspeech, 
			   int *postings1, int *postings1_index, int *postings2, int *postings2_index, int prime, float distthr, Dot *dotlist)
{
   int Nmax = max(N1,N2);
   double M = 1e6;
   int steps = 0, adds = 0, news = 0;

   for(int d=0;d<D;d++) {
      if ( d == sildim )
	 continue;

      int nd1 = postings1_index[d+1] - postings1_index[d];
      int nd2 = postings2_index[d+1] - postings2_index[d];
      
      fprintf(stderr,"\tFeat %d (%d,%d occurences); adds = %5.2fM, steps = %5.2fM, news = %5.2fM, ratio = %5.2f\n",
	      d, nd1, nd2, adds/M, steps/M, news/M, steps/(adds+0.01));
      
      int *p1 = postings1 + postings1_index[d];
      int *p2 = postings2 + postings2_index[d];
      
      for(int n=0;n<nd1;n++) {
	 int x = p1[n];
	 if(silence1[x] > PSILTHR) continue; /* silence */
	 
	 int startm = n+1;
	 if ( diffspeech )
	    startm = 0;
	 
	 for(int m=startm; m<nd2; m++) {
	    int y = p2[m];
	    if(silence2[y] > PSILTHR) continue; /* silence */
	    
	    int xp =  x+y;
	    int yp = -x+y+diffspeech*Nmax;
	    float val = feats1[x*D+d]*feats2[y*D+d];
	    
	    int h = ((long)yp*2*Nmax+xp) % prime;
	    adds++;
	    
	    while(h < 0) h += prime;
	    while(h >= prime) h -= prime;
	    
	    int loop = 0;
	    for(int ind=h;;ind++) {
	       steps++;
	       if(dotlist[ind].val == 0) {
		  dotlist[ind].xp = xp;
		  dotlist[ind].yp = yp;
		  dotlist[ind].val = val;
		  news++;
		  break;
	       }
	       else if (dotlist[ind].xp == xp && dotlist[ind].yp == yp) {
		  dotlist[ind].val += val;
		  break;
	       }
	       
	       if(ind == prime-1) { loop++; ind = -1; }
	    }
	 }
      }
   }

   return compress_dotlist(dotlist, prime, 1-2*distthr);   
}

int count_dots_before(int pos, DotXV *arr, int n)
{
   // Do binary search down to a range of 10 or less dots
   if(pos < arr[0].xp) return 0;
   if(pos > arr[n-1].xp) return n;
   int low = 0;
   int high = n;
   while(high - low > 10) {
      int mid = low + (high - low)/2;
      int p = arr[mid].xp;
      if(pos == p) return mid;
      else if(p < pos) low = mid;
      else high = mid;
   }
   
   // Do linear pass through remaining dots
   while(low < high) {
      int p = arr[low].xp;
      if(pos == p) return low;
      else if(p < pos) low++;
      else return low;
   }
      
   return high;
}

int count_dots_interval(int start, int end, DotXV *arr, int cnt)
{
   return count_dots_before(end, arr, cnt) - count_dots_before(start, arr, cnt);
}

int careful_add_dots(int xp1, int xp2, int yp, Dot *outlist, int sumthr, int dx, DotXV *arr, int cnt)
{
   Dot *o = outlist;
   for(int i=xp1; i<xp2; i++) {
      if ( count_dots_interval(i-dx, i+dx+1, arr, cnt) > sumthr ) {
	 o->val = 1;
	 o->xp = i;
	 o->yp = yp;
	 o++;
      }
   }
   
   return o - outlist;
}
    
int median_filtx(int N, DotXV *inlist, int incnt, int *cumsum, int dx, float medthr, Dot *outlist)
{
   int window = 2*dx+1;
   int sumthr = medthr*window/2;

   int outcnt = 0;
   
  for(int yp=0;yp<N;yp++) {
    int done_so_far = 0;
    DotXV *arr = inlist + cumsum[yp];
    DotXV *end = inlist + cumsum[yp+1];
    int cnt = end - arr;
    DotXV *head = arr-1;
    DotXV *tail = arr;
    while(head < end-1 && head - tail < sumthr) {
      head++;
      while(head - tail >= sumthr) {
	if(head->xp - tail->xp <= window) {
	   int newcnt = careful_add_dots(MAX(tail->xp-dx-1, done_so_far), 
					 MAX(done_so_far + 1, head->xp + dx + 2), 
					 yp, outlist + outcnt, sumthr, dx, arr, cnt);
	  outcnt += newcnt;
	  done_so_far = head->xp + dx + 2;
	}
	tail++;
      }
    }
  }

  return outcnt;
}

int brute_median_filtx(int N, DotXV * inlist, int incnt, int * cumsum, int dx, float medthr, Dot * outlist)
{
   int window = 2*dx+1;
   int sumthr = medthr*window/2;

   int * row = (int *) MALLOC( 2*N*sizeof(int) );
   int outcnt = 0;
   for ( int yp = 0; yp < N; yp++ ) {
      for ( int xp = yp; xp < 2*N-yp; xp++ ) {
	 row[xp] = 0;
      }
      
      int rowcnt;
      if ( yp == N-1 ) rowcnt = incnt-cumsum[N-1];
      else rowcnt = cumsum[yp+1]-cumsum[yp];

      DotXV *arr = &inlist[cumsum[yp]];

      for ( int n = 0; n < rowcnt; n++ ) {
	 int x0 = arr[n].xp;
	 int xA = (x0-dx > 0) ? x0-dx : 0;
	 int xB = (x0+dx < 2*N) ? x0+dx+1 : 2*N;

	 for ( int xp = xA; xp < xB; xp++ ) {
	    row[xp]++;
	 }
      }
      
      for ( int xp = yp; xp < 2*N-yp; xp++ ) {
	 if ( row[xp] > sumthr ) {
	    outlist[outcnt].xp = xp;
	    outlist[outcnt].yp = yp;
	    outlist[outcnt].val = 1;
	    outcnt++;
	 }
      }
   }
   FREE(row);

   return outcnt;
}

void hough_gaussy(int N, int dotcnt, int *cumsum, int dy, int diffspeech, float *hough)
{
   int * rowcnt = (int*) MALLOC(N*sizeof(int));
   rowcnt[N-1] = dotcnt-cumsum[N-1];
   for(int yp=0;yp<N-1;yp++)
      rowcnt[yp] = cumsum[yp+1]-cumsum[yp];

   if (dy==0) {
      for(int yp=0;yp<N;yp++) {
	 int cyp = yp;
	 if ( diffspeech ) cyp = abs(yp-N/2);

	 if(cyp < RHOSKIP) hough[yp]=0;
	 else hough[yp] = rowcnt[yp];
      }
   } 
   else {      
      float *Fy = (float *)MALLOC((2*dy+1)*sizeof(float));
      for(int n=0; n<2*dy+1; n++) {
	 float y = 3*((float) n-dy)/dy;
	 Fy[n] = (3.0/dy)*(1/sqrt(2*M_PI))*exp(-y*y/2);
      }
      
      for(int y0=0; y0<N; y0++) {
	 hough[y0] = 0;

	 int cy0 = y0;
	 if ( diffspeech ) cy0 = abs(y0-N/2);

	 if (!diffspeech && cy0 < RHOSKIP) continue;
	 
	 int yA = (y0-dy > 0) ? y0-dy : 0;
	 int yB = (y0+dy < N) ? y0+dy+1 : N;
	
	 for(int yp=yA; yp<yB; yp++)
	    hough[y0] += rowcnt[yp]*Fy[yp-yA];
      }

      FREE(Fy);
   }

   FREE(rowcnt);
}

int count_rholist( int N, float *hough, int rhothr ) 
{
   int rhocnt = 0;
   for ( int yp = 1; yp < N-1; yp++ ) {
      if ( hough[yp] > rhothr && 
	   hough[yp] > hough[yp-1] && 
	   hough[yp] > hough[yp+1] )
	 rhocnt++;
   }
   return rhocnt;
}

int compute_rholist(int N, float *hough, int rhothr, int *rholist, float *rhoampl )
{ 
   int rhocnt = 0;
   for(int yp=1; yp<N-1; yp++) {
      if ( hough[yp] > rhothr && 
	   hough[yp] > hough[yp-1] && 
	   hough[yp] > hough[yp+1] )
      {
	 rholist[rhocnt] = yp;
	 rhoampl[rhocnt++] = hough[yp];
      }	   
   }

   return rhocnt;
}

int compute_matchlist_simple(int N, DotXV *dotlist, int dotcnt, int *cumsum, 
			     int *rholist, float *rhoampl, int rhocnt, int dx, int diffspeech, Match *matchlist)
{
  int matchcnt = 0;

  for(int r=0; r<rhocnt; r++ ) {
    int yp = rholist[r];
    int cyp = yp - diffspeech*N;

    DotXV *arr = &dotlist[cumsum[yp]];
    int cnt = cumsum[yp+1]-cumsum[yp];
   
    int runlen = 0;
    int indstart = 0;
    for(int ind=1; ind<=cnt; ind++) {
      if (ind < cnt && arr[ind].xp == arr[ind-1].xp+1) {
	if(runlen== 0) indstart = ind;
	runlen++;
      }
      else {
	if ( runlen > dx/2) {
	  int xpA = arr[indstart].xp;
	  int xpB = arr[ind-1].xp;
	       
	  int xA = (xpA-cyp)/2;
	  int xB = (xpB-cyp)/2;
	  int yA = (xpA+cyp)/2;
	  int yB = (xpB+cyp)/2;

	  matchlist[matchcnt].xA = xA;
	  matchlist[matchcnt].xB = xB;
	  matchlist[matchcnt].yA = yA;
	  matchlist[matchcnt].yB = yB;
	  matchlist[matchcnt].rhoampl = rhoampl[r];
	  matchcnt++;
	}
	runlen = 0;
      }
    } 
  }

  return matchcnt;
}

int compute_matchlist(int N, DotXV *dotlist, int dotcnt, int *cumsum, 
		      int *rholist, float *rhoampl, int rhocnt, int dx, int dy, int diffspeech, Match *matchlist)
{
  if (rhocnt==0) return 0;
  if ( dy == 0 ) return compute_matchlist_simple( N, dotlist, dotcnt, cumsum, rholist, rhoampl, rhocnt, dx, diffspeech, matchlist );

  int N2 = 2*N;
  int ywindow = 2*dy+1;
  double funky_constant = (3.0/dy)*(1/sqrt(2*M_PI));

  float *Fy = (float *)MALLOC(ywindow*sizeof(float));
  for(int n = 0; n<ywindow; n++) {
    float y = 3*((float) n-dy)/dy;
    Fy[n] = funky_constant*exp(-y*y/2);
  }

  float *act = (float *)MALLOC(N2*sizeof(float)); 
  int matchcnt = 0;
  for(int r=0; r<rhocnt; r++) {
    int y0 = rholist[r];
    int cy0 = y0 - diffspeech*N;
  
    int ypA = (y0-dy < 0) ? 0 : y0-dy;
    int ypB = (y0+dy < (diffspeech+1)*N) ? y0+dy+1 : (diffspeech+1)*N;

    memset(act, 0, N2*sizeof(*act)); 
      
    for(int yp=ypA; yp<ypB; yp++) {
      DotXV *arr = &dotlist[cumsum[yp]];
      int cnt = cumsum[yp+1]-cumsum[yp];
      for(int ind=0; ind<cnt; ind++) {
	int xp = arr[ind].xp;
	act[xp] += Fy[yp-y0+dy];
      }
    }

    threshold_vector(act, N2, 0.1);
      
    int runlen = act[0];
    int indstart = 0;
    for(int xp=1; xp<=N2; xp++) {
       if(xp<N2 && act[xp]) {
	  if(runlen==0) indstart=xp;
	  runlen++;
       }
       else {
	  if(runlen > dx/2) {
	     int xpA = indstart;
	     int xpB = xp-1;
	     
	     int xA = (xpA-cy0)/2;
	     int xB = (xpB-cy0)/2;
	     int yA = (xpA+cy0)/2;
	     int yB = (xpB+cy0)/2;
	     
	     if(matchcnt < MAXMATCHES) {
		matchlist[matchcnt].xA = xA;
		matchlist[matchcnt].xB = xB;
		matchlist[matchcnt].yA = yA;
		matchlist[matchcnt].yB = yB;
		matchlist[matchcnt].rhoampl = rhoampl[r];
		matchcnt++;
	     }
	  }
	  runlen = 0;
       }
    }
  }
  
  FREE(Fy);
  FREE(act);

  return matchcnt;
}

int compress_matchlist(Match *matchlist, int matchcnt)
{
   for(int wpos=0, rpos=1; wpos<matchcnt; wpos++) {
      if(wpos==rpos) rpos++;

      if(matchlist[wpos].xA == -1) {
	 while(rpos < matchcnt && matchlist[rpos].xA == -1) rpos++;
	 if(rpos==matchcnt) return wpos;

	 matchlist[wpos].xA = matchlist[rpos].xA;
	 matchlist[wpos].xB = matchlist[rpos].xB;
	 matchlist[wpos].yA = matchlist[rpos].yA;
	 matchlist[wpos].yB = matchlist[rpos].yB;
	 matchlist[wpos].rhoampl = matchlist[rpos].rhoampl;
	 matchlist[rpos].xA = -1;
      }
   } 

   return matchcnt;
}

// This function just does one pass--must iterate to do complete merge
int merge_matchlist(Match *matchlist, int matchcnt, int mergetol)
{
  for(int n=0; n<matchcnt-1; n++) {
    if(matchlist[n].xA == -1) continue;

    for(int m=n+1; m<matchcnt; m++) {
      if (matchlist[m].xA == -1) continue;

      int l1x = matchlist[n].xB - matchlist[n].xA;
      int l1y = matchlist[n].yB - matchlist[n].yA;
      int l2x = matchlist[m].xB - matchlist[m].xA;
      int l2y = matchlist[m].yB - matchlist[m].yA;
      int lmx = max(matchlist[n].xB,matchlist[m].xB) - min(matchlist[n].xA,matchlist[m].xA);
      int lmy = max(matchlist[n].yB,matchlist[m].yB) - min(matchlist[n].yA,matchlist[m].yA);

      if ( lmx < l1x+l2x+mergetol && lmy < l1y+l2y+mergetol ) {
	matchlist[n].xA = min(matchlist[n].xA,matchlist[m].xA);
	matchlist[n].xB = max(matchlist[n].xB,matchlist[m].xB);
	matchlist[n].yA = min(matchlist[n].yA,matchlist[m].yA);
	matchlist[n].yB = max(matchlist[n].yB,matchlist[m].yB);
	matchlist[n].rhoampl = matchlist[n].rhoampl+matchlist[m].rhoampl;

	matchlist[m].xA = -1;
      }	 
    }
  }
  return compress_matchlist(matchlist, matchcnt);
}

void dump_matchlist( Match *matchlist, int matchcnt, int xOffset, int yOffset )
{
   for(int n=0; n<matchcnt; n++) {
      fprintf(stdout,"%d %d %d %d %f %f\n",
	      matchlist[n].xA+xOffset,
	      matchlist[n].xB+xOffset, 
	      matchlist[n].yA+yOffset,
	      matchlist[n].yB+yOffset,
	      matchlist[n].score,
	      matchlist[n].rhoampl);
   }
}

void dump_dotplot( int xmin, int xmax, int ymin, int ymax, int diffspeech, int Nmax, DotXV *dotlist, int *cumsum )
{
   int xpA = xmin+ymin;
   int ypA = ymin-xmax;
   int xpB = xmax+ymax;
   int ypB = ymax-xmin;

   if ( !diffspeech && ypA < 0 ) ypA = 0;
   
   if ( diffspeech ) {
      ypA += Nmax;
      ypB += Nmax;
   }

   int * rowcnt = (int*) MALLOC(ypB*sizeof(int));
   for(int yp=ypA;yp<ypB;yp++) {
      rowcnt[yp] = cumsum[yp+1]-cumsum[yp];
   }

   for ( int yp = ypA; yp < ypB; yp++ )
   {
      DotXV *arr = &dotlist[cumsum[yp]];
      int pos = 0;
      while ( pos < rowcnt[yp] && arr[pos].xp < xpA ) pos++;

      for ( int xp = xpA; xp <= xpB; xp++ )
      {
	 if ( pos < rowcnt[yp] && arr[pos].xp == xp )
	 {
	    printf("%f ",arr[pos].val);
	    pos++;
	 }
	 else
	 {
	    printf("0 ");
	 }
      }
      printf("\n");
   }

   FREE(rowcnt);
}

int duration_filter( Match *matchlist, int matchcnt, float durthr )
{
   for(int n = 0; n < matchcnt; n++) {
      if ( matchlist[n].xB-matchlist[n].xA < durthr*100 ||
	   matchlist[n].yB-matchlist[n].yA < durthr*100 )
	 matchlist[n].xA = -1;
   }

   return compress_matchlist(matchlist, matchcnt);
}


void compute_path_dotcnt(int D, int sildim, float *silence1, float *silence2, Match * match, 
			 int *postings1, int *postings1_index, int *postings2, int *postings2_index, 
			 int * totdots, int * pathcnt, int * pathlen, int * dotvec)
{
   int xA = match->xA;
   int xB = match->xB;
   int yA = match->yA;
   int yB = match->yB;

   int N1 = xB-xA+1;
   int N2 = yB-yA+1;

   char * dotplot = (char *) MALLOC( sizeof(char)*N1*N2 );
   memset(dotplot, 0, sizeof(char) * N1 * N2);

   memset(dotvec, 0, sizeof(int)*20);
   *totdots = 0;
   for(int d=0;d<D;d++) {
      if ( d == sildim )
	 continue;
      
      int nd1 = postings1_index[d+1] - postings1_index[d];
      int nd2 = postings2_index[d+1] - postings2_index[d];
      
      int *p1 = postings1 + postings1_index[d];
      int *p2 = postings2 + postings2_index[d];
      
      for(int n = 0; n < nd1; n++) {
	 int x = p1[n];
	 if(silence1[x] > PSILTHR || x < xA ) continue; /* silence */
	 if ( x > xB-1 ) break;
	 x -= xA;

	 for(int m=0; m<nd2; m++) {
	    int y = p2[m];
	    if(silence2[y] > PSILTHR || y < yA) continue; /* silence */
	    if(y > yB-1) break;
	    y -= yA;

	    if ( dotplot[N2*x+y] == 0 ) {	       
	       int dvind = min(19,floor(20.0*fabs(x-y)/min(N1,N2)));
	       dotvec[dvind]++;
	       (*totdots)++;
	    }

	    dotplot[N2*x+y] = 1;
	 }
      }
   }

   int * cost = (int *) MALLOC( sizeof(int)*(N1+1)*(N2+1) );
   int * pathmat = (int *) MALLOC( sizeof(int)*(N1+1)*(N2+1) );
   int MAXINT = 500;
   int R = 20;

   for ( int n = 0; n < N1+1; n++ ) {
      for ( int m = 0; m < N2+1; m++ ) {
	 if ( n == 0 ) {
	    cost[m] = (m < R) ? 0 : MAXINT;
	    pathmat[m] = m;
	 } else if ( m == 0 ) {
	    cost[n*(N2+1)] = (n < R) ? 0 : MAXINT;
	    pathmat[n*(N2+1)] = n;
	 } else {
	    int u = cost[(n-1)*(N2+1)+m];
	    int l = cost[n*(N2+1)+(m-1)];
	    int d = cost[(n-1)*(N2+1)+(m-1)];

	    if ( abs(n-m) > R && !( n == N1 && n <= m) && !( m == N2 && m <= n ) ) {
	       cost[n*(N2+1)+m] = MAXINT;
	       pathmat[n*(N2+1)+m] = 0;
	    } else {
	       cost[n*(N2+1)+m] = (1-dotplot[N2*(n-1)+(m-1)]) + min( d, min(u,l) );
	       if ( d <= l && d <= u )
		  pathmat[n*(N2+1)+m] = pathmat[(n-1)*(N2+1)+(m-1)]+1;
	       else if ( l <= d ) pathmat[n*(N2+1)+m] = pathmat[n*(N2+1)+(m-1)]+1;
	       else pathmat[n*(N2+1)+m] = pathmat[(n-1)*(N2+1)+m]+1;
	    }
	 }
      }
   }
   
   *pathlen = pathmat[(N1+1)*(N2+1)-1];
   *pathcnt = *pathlen - cost[(N1+1)*(N2+1)-1];
   
   FREE(pathmat);
   FREE(cost);
   FREE(dotplot);

   return;
}

float mlog( float arg )
{
   return arg <= 10 ? log(10) : log(arg);
}

float logreg_score_aren( float dtwscore, int totdots, int pathcnt, int * dotvec )
{
   float params[] = {32.9903,
		     -28.0827,
		     -3.5700,
		     -0.1067,
		     0.2485,
		     0.9177,
		     0.0142,
		     -0.0622,
		     -0.2242,
		     -0.1991,
		     0.3158,
		     0.1314,
		     0.0683,
		     -0.0029,
		     -0.2071,
		     0.0646,
		     0.4886,
		     -0.0684,
		     -0.1257,
		     -0.0754,
		     0.2565,
		     -0.0815,
		     1.0162,
		     -1.1894};


   float z = params[0] + dtwscore*params[1] + 
      mlog((float)pathcnt)*params[2] + mlog((float)totdots)*params[3];
   for ( int n = 0; n < 20; n++ ) {
      z += mlog(dotvec[n])*params[n+4];
   }
   
   return 1.0-1.0/(1.0+exp(-z));
   //return z;
}

float logreg_score_ken( int totdots, int pathcnt, int * dotvec )
{
   float params[] = {-12.7,9.56,-1.66,
		     -1.28,-0.549,-0.421,-0.214,-0.0987,-0.0484,-0.0207};

   
   float z = params[0] + mlog((float)pathcnt)*params[1] + 
      mlog((float)totdots)*params[2];
   for ( int n = 0; n < 7; n++ ) {
      z += mlog(dotvec[n])*params[n+3];
   }
   
   return 1.0/(1.0+exp(-z));
}
