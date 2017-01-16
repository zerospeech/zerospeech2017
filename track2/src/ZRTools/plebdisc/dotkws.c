//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "util.h"
#include "dotkws.h"

#define RHOSKIP 50
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

int radix_sorty(Dot *dotlist, int dotcnt, DotXV *sorted_dots, frameind *cumsum, frameind *cumsumind)
{
   // Sort the dots by yp
   qsort(dotlist, dotcnt, sizeof(Dot), &Dot_compare);
   
   // Loop over the dots, fill sorted_dots and maintain counts for each row
   int yp_count = 0;
   frameind lastyp = -1;
   for(int ind = 0; ind<dotcnt; ind++) {
      if ( dotlist[ind].yp != lastyp ) {
	 cumsumind[yp_count] = dotlist[ind].yp;
	 lastyp = dotlist[ind].yp;
	 yp_count++;
      }

      cumsum[yp_count]++;
      sorted_dots[ind].xp = dotlist[ind].xp;
      sorted_dots[ind].val = dotlist[ind].val;
   }
   
   for ( int i = 1; i <= yp_count; i++ )
      cumsum[i] += cumsum[i-1];
   
   return yp_count;
}

void quick_sortx(DotXV *radixdots, int dotcnt, frameind *cumsum, frameind *cumsumind, int ncumsum )
{
   for ( int i = 0; i < ncumsum; i++ ) {
      qsort(&radixdots[cumsum[i]], cumsum[i+1]-cumsum[i], sizeof(DotXV), &DotXV_compare);
   }
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

int compress_dotlistXV(DotXV *dotlist, int prime, float thr)
{
   int wpos = 0;
   int rpos = 1;

   for(; wpos<prime; wpos++) {
      if(wpos==rpos) rpos++;

      if(dotlist[wpos].val<=thr) {
	 while(rpos<prime && dotlist[rpos].val<=thr) rpos++;
	 if(rpos==prime) return wpos;

	 dotlist[wpos].val = dotlist[rpos].val;
	 dotlist[wpos].xp = dotlist[rpos].xp;
	 dotlist[rpos].val = 0;
      }
   } 

   return wpos;
}

int dot_dedup(DotXV *dotlist, frameind *cumsum, frameind *cumsumind, int ncumsum, float thr)
{
   frameind initcnt = cumsum[ncumsum];
   int *rowcnt = (int *) MALLOC( sizeof(int)*ncumsum );
   for ( int i = 0; i < ncumsum; i++ )
   {
      rowcnt[i] = cumsum[i+1]-cumsum[i];
      frameind start = (i == 0) ? 0 : cumsum[i];
      for ( frameind j = start+1; j < cumsum[i+1]; j++ ) {
	 if ( dotlist[j].xp == dotlist[j-1].xp ) {
	    dotlist[j].val = -1e6;
	    rowcnt[i]--;
	 }	 
      }
   }

   for ( frameind i = 0; i < ncumsum; i++ )
   {
      cumsum[i+1] = cumsum[i]+rowcnt[i];
   }

   FREE(rowcnt);

   frameind wpos = 0;
   frameind rpos = 1;

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

int count_dots_before(frameind pos, DotXV *arr, int n)
{
   // Do binary search down to a range of 10 or less dots
   if(pos < arr[0].xp) return 0;
   if(pos > arr[n-1].xp) return n;
   int low = 0;
   int high = n;
   while(high - low > 10) {
      int mid = low + (high - low)/2;
      frameind p = arr[mid].xp;
      if(pos == p) return mid;
      else if(p < pos) low = mid;
      else high = mid;
   }
   
   // Do linear pass through remaining dots
   while(low < high) {
      frameind p = arr[low].xp;
      if(pos == p) return low;
      else if(p < pos) low++;
      else return low;
   }
      
   return high;
}

int count_dots_interval(frameind start, frameind end, DotXV *arr, int cnt)
{
   return count_dots_before(end, arr, cnt) - count_dots_before(start, arr, cnt);
}

int careful_add_dots(frameind xp1, frameind xp2, frameind yp, Dot *outlist, int sumthr, int dx, DotXV *arr, int cnt)
{
   Dot *o = outlist;
   for ( frameind i = xp1; i < xp2; i++ ) {
      if ( count_dots_interval(i-dx, i+dx+1, arr, cnt) > sumthr ) {
	 o->val = 1;
	 o->xp = i;
	 o->yp = yp;
	 o++;
      }
   }
   
   return o - outlist;
}
    
int median_filtx(frameind Ntest, frameind Nq, DotXV *inlist, int incnt, 
		 frameind *cumsum, frameind *cumsumind, int ncumsum, int dx, float medthr, Dot *outlist)
{
   int window = 2*dx+1;
   int sumthr = medthr*window/2;
   
   int outcnt = 0;
   
   for ( int i = 0; i < ncumsum; i++ ) {
      int done_so_far = 0;
      DotXV *arr = inlist + cumsum[i];
      DotXV *end = inlist + cumsum[i+1];
      frameind yp = cumsumind[i];
      int cnt = end - arr;
      DotXV *head = arr-1;
      DotXV *tail = arr;
      while(head < end-1 && head - tail < sumthr) {
	 head++;
	 while(head - tail >= sumthr) {
	    if(head->xp - tail->xp <= window) {
	       int newcnt = careful_add_dots(MAX(MAX(tail->xp-dx-1, done_so_far), Ntest-yp+1), 
					     MIN(MAX(done_so_far+1, head->xp+dx+2), 2*Nq-yp+Ntest-1), 
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

int hough_gaussy(frameind N, int dotcnt, frameind *cumsum, frameind *cumsumind, int ncumsum, int dy, float *hough, frameind *houghind)
{
   if ( dy == 0 ) {
      for ( int i = 0; i < ncumsum; i++ ) {
	 hough[i] = cumsum[i+1] - cumsum[i];
	 houghind[i] = cumsumind[i];
      }
      return ncumsum;
   } else {      
      float *Fy = (float *)MALLOC((2*dy+1)*sizeof(float));
      for(int n=0; n<2*dy+1; n++) {
	 float y = 3*((float) n-dy)/dy;
	 Fy[n] = (3.0/dy)*(1/sqrt(2*M_PI))*exp(-y*y/2);
      }

      int *rowcnt = (int *) MALLOC(ncumsum*sizeof(int));
      for ( int i = 0; i < ncumsum; i++ ) 
	 rowcnt[i] = cumsum[i+1] - cumsum[i];

      // Initialize elements for all rows that will have nonzero hough
      int nhough = 0;
      for ( int i = 0; i < ncumsum; i++ ) {
	 for ( int yp = MAX(0,cumsumind[i]-dy); yp <= MIN( N-1, cumsumind[i]+dy ); yp++ ) {
	    if ( nhough == 0 || yp > houghind[nhough-1] ) {
	       houghind[nhough++] = yp;
	    }
	 }
      }

      // For each row with nonzero hough, compute the transform value
      int laststartind = 0;
      for ( int i = 0; i < nhough; i++ ) {
	 hough[i] = 0;
	 for ( int yp = MAX(0,houghind[i]-dy); yp <= MIN( N-1, houghind[i]+dy); yp++ ) {
	    for ( int j = laststartind; j < ncumsum && cumsumind[j] <= yp; j++ ) {
	       if ( cumsumind[j] == yp ) {
		  hough[i] += rowcnt[j]*Fy[yp-houghind[i]+dy];
		  if ( yp == MAX(0,houghind[i]-dy) )
		     laststartind = j;
		  break;
	       } 
	    }
	 }
      }

      FREE(rowcnt);
      FREE(Fy);
      return nhough;
   }
}

int count_rholist( float *hough, frameind *houghind, int nhough, int rhothr ) 
{
   if ( nhough == 0 ) return 0;
   if ( nhough == 1 ) return hough[0] > rhothr;

   int rhocnt = 0;

   if ( hough[0] > rhothr && 
	( houghind[1] > houghind[0]+1 || hough[0] >= hough[1] ) )
      rhocnt++;
   
   for ( int i = 1; i < nhough-1; i++ ) {
      if ( hough[i] > rhothr && 
	   ( houghind[i+1] > houghind[i]+1 || hough[i] >= hough[i+1] ) &&
	   ( houghind[i] > houghind[i-1]+1 || hough[i] >= hough[i-1] ) )
	 rhocnt++;
   }

   if ( hough[nhough-1] > rhothr && 
	( houghind[nhough-1] > houghind[nhough-2]+1 || hough[nhough-1] >= hough[nhough-2] ) )
      rhocnt++;

   return rhocnt;
}

int compute_rholist(float *hough, frameind *houghind, int nhough, int rhothr, frameind *rholist, float *rhoampl )
{ 
   if ( nhough == 0 ) return 0;
   if ( nhough == 1 ) {
      if ( hough[0] > rhothr ) {
	 rholist[0] = houghind[0];
	 rhoampl[0] = hough[0];
	 return 1;
      } else {
	 return 0;
      }
   }

   int rhocnt = 0;

   if ( hough[0] > rhothr && 
	( houghind[1] > houghind[0]+1 || hough[0] >= hough[1] ) ) {
      rholist[rhocnt] = houghind[0];
      rhoampl[rhocnt++] = hough[0];
   }
   
   for ( int i = 1; i < nhough-1; i++ ) {
      if ( hough[i] > rhothr && 
	   ( houghind[i+1] > houghind[i]+1 || hough[i] >= hough[i+1] ) &&
	   ( houghind[i] > houghind[i-1]+1 || hough[i] >= hough[i-1] ) ) {
	 rholist[rhocnt] = houghind[i];
	 rhoampl[rhocnt++] = hough[i];
      }
   }

   if ( hough[nhough-1] > rhothr && 
	( houghind[nhough-1] > houghind[nhough-2]+1 || hough[nhough-1] >= hough[nhough-2] ) ) {
      rholist[rhocnt] = houghind[nhough-1];
      rhoampl[rhocnt++] = hough[nhough-1];
   }

   return rhocnt;
}

int compute_matchlist_simple(frameind Ntest, frameind Nq, DotXV *dotlist, int dotcnt, 
			     frameind *cumsum, frameind *cumsumind, int ncumsum,
			     frameind *rholist, float *rhoampl, int rhocnt, int dx, Match *matchlist)
{
  int matchcnt = 0;
  int currind = 0;

  for(int r=0; r<rhocnt; r++ ) {
     frameind yp = rholist[r];
     while ( cumsumind[currind] != yp ) currind++; 

     DotXV *arr = &dotlist[cumsum[currind]];
     int cnt = cumsum[currind+1]-cumsum[currind];
   
     int runlen = 0;
     frameind indstart = 0;

     for(int ind = 1; ind <= cnt; ind++) {
	if (ind < cnt && arr[ind].xp == arr[ind-1].xp+1) {
	   if(runlen == 0) indstart = ind-1;
	   runlen++;
	}
	else {
	   if ( runlen > dx/2 ) {
	      frameind xpA = arr[indstart].xp;
	      frameind xpB = arr[ind-1].xp;
	      
	      frameind xA = (xpA-yp+Ntest)/2;
	      frameind xB = (xpB-yp+Ntest)/2;
	      frameind yA = (xpA+yp-Ntest)/2;
	      frameind yB = (xpB+yp-Ntest)/2;
	      
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

int compute_matchlist_sparse( frameind Ntest, frameind Nq, DotXV *dotlist, int dotcnt, 
			      frameind *cumsum, frameind *cumsumind, int ncumsum,
			      frameind *rholist, float *rhoampl, int rhocnt, 
			      int dx, int dy, Match *matchlist)
{
   if (rhocnt==0) return 0;
   if ( dy == 0 ) return compute_matchlist_simple( Ntest, Nq, dotlist, dotcnt, 
						   cumsum, cumsumind, ncumsum, rholist, 
						   rhoampl, rhocnt, dx, matchlist );
   
   int ywindow = 2*dy+1;
   double funky_constant = (3.0/dy)*(1/sqrt(2*M_PI));
   
   float *Fy = (float *)MALLOC(ywindow*sizeof(float));
   for(int n = 0; n<ywindow; n++) {
      float y = 3*((float) n-dy)/dy;
      Fy[n] = funky_constant*exp(-y*y/2);
   }

   int matchcnt = 0;
   for ( int r = 0; r < rhocnt; r++ ) {
      frameind y0 = rholist[r];

      frameind ypA = MAX(y0-dy,0);
      frameind ypB = MIN(Ntest+Nq-1,y0+dy);

      // Collect all the dots in the band of rows of height 2*dy+1,
      // weighted by the kernel Fy(distance from y0)

      int indA = 0;
      while ( cumsumind[indA] < ypA ) indA++; 
      int indB = indA;
      while ( indB < ncumsum && cumsumind[indB] <= ypB ) indB++; 
      
      int totaldots = cumsum[indB]-cumsum[indA];
      DotXV *banddots = (DotXV *) MALLOC( sizeof(DotXV)*totaldots );

      int banddotcnt = 0;
      int currind = 0;
      for ( frameind yp = ypA; yp <= ypB; yp++ ) {
	 while ( currind < ncumsum && cumsumind[currind] < yp ) currind++; 
	 if ( cumsumind[currind] != yp ) continue;

	 int cnt = cumsum[currind+1]-cumsum[currind];
	 for ( int i = 0; i < cnt; i++ ) {
	    banddots[banddotcnt].xp = dotlist[i+cumsum[currind]].xp;
	    banddots[banddotcnt++].val = Fy[yp-y0+dy]*dotlist[i+cumsum[currind]].val;
	 }
      }

      // Sort all the dots in the band by column (xp)
      qsort(banddots, banddotcnt, sizeof(DotXV), &DotXV_compare);

      // Merge duplicate dots (sum the dot values)
      frameind lastxp = -1;
      frameind lastind = 0;
      for ( int i = 0; i < banddotcnt; i++ ) {
	 if ( banddots[i].xp == lastxp ) {
	    banddots[lastind].val += banddots[i].val;
	    banddots[i].val = 0;
	 } else { 
	    lastxp = banddots[i].xp;
	    lastind = i;
	 }
      }

      // Remove small-valued dots
      banddotcnt = compress_dotlistXV(banddots, banddotcnt, 0.1);

      // Compute the runs
      int runlen = 0;
      frameind indstart = 0;
      for(int ind = 1; ind <= banddotcnt; ind++) {
	 if (ind < banddotcnt && banddots[ind].xp == banddots[ind-1].xp+1) {
	    if(runlen == 0) indstart = ind;
	    runlen++;
	 }
	 else {
	    if ( runlen > dx/2 ) {
	       frameind xpA = banddots[indstart].xp;
	       frameind xpB = banddots[ind-1].xp;
	       
	       frameind xA = (xpA-y0+Ntest)/2;
	       frameind xB = (xpB-y0+Ntest)/2;
	       frameind yA = (xpA+y0-Ntest)/2;
	       frameind yB = (xpB+y0-Ntest)/2;
	       
	       if ( yA > yB ) yA = 0;

	       matchlist[matchcnt].xA = xA;
	       matchlist[matchcnt].xB = xB;
	       matchlist[matchcnt].yA = max(0,yA);
	       matchlist[matchcnt].yB = yB;
	       matchlist[matchcnt].rhoampl = rhoampl[r];
	       matchcnt++;
	    }
	    runlen = 0;
	 }
      }
      FREE(banddots);
   }
   
   FREE(Fy);
   
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

void dump_matchlist( char **files, Match *matchlist, int matchcnt, frameind *xOffsets, frameind yOffset, char *queryfile, char *querytype )
{
   for(int n=0; n<matchcnt; n++) {
      int fid = 0;
      while ( xOffsets[fid++] < matchlist[n].xA );
      fid-=2;

      fprintf(stdout,"%s %s %s %d %d %d %d %f %f\n",
	      querytype, files[fid], queryfile,
	      matchlist[n].xA-xOffsets[fid],
	      matchlist[n].xB-xOffsets[fid], 
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
   if ( dtwscore < 0.85 ) 
      return 0;

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
