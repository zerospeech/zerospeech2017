//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <ctype.h>
#include <math.h>
#include "util.h"
#include "score_matches.h"

#define MAXLINE 1024
#define EMPTY 255

inline int dot(struct signature *sig1, struct signature *sig2, float thr)
{
   return (approximate_cosine(sig1,sig2) > thr);
}

int dist1(int i, int j, int start1, int start2, int n1, int n2, struct signature *feats1, struct signature *feats2, int N1, int N2, float thr, unsigned char *distances)
{
  if(i >= n1 || j >= n2) return 0;
  int ii = start1 + i;
  int jj = start2 + j;
  unsigned char *d = &distances[j * n1 + i];
  if(*d != EMPTY) return *d;
  unsigned char d0 = dot(&feats1[ii], &feats2[jj], thr) + dist1(i+1, j+1, start1, start2, n1, n2, feats1, feats2, N1, N2, thr, distances);
  unsigned char d1 = dist1(i, j+1, start1, start2, n1, n2, feats1, feats2, N1, N2, thr, distances );
  unsigned char d2 = dist1(i+1, j, start1, start2, n1, n2, feats1, feats2, N1, N2, thr, distances);
  *d=0;
  if(d0 > *d) *d=d0;
  if(d1 > *d) *d=d1;
  if(d2 > *d) *d=d2;
  return *d;
}

int dist(struct signature *feats1, struct signature *feats2, int start1, int end1, int start2, int end2, int N1, int N2, float thr )
{
  int n1 = end1-start1;
  int n2 = end2-start2;
  unsigned char *distances = (unsigned char *)MALLOC(sizeof(unsigned char) * n1 * n2);
  if(!distances) fatal("malloc failed");
  for ( int i = 0; i < n1*n2; i++ ) {
     distances[i] = EMPTY;
  }
  int result = dist1(0, 0, start1, start2, n1, n2, feats1, feats2, N1, N2, thr, distances);
  FREE(distances);
  return result;
}

double pointdist(int x0, int y0, int x1, int y1, int x2, int y2)
{
  int dx = x2 - x1;
  int dy = y2 - y1;
  return abs((double)(dx * (y1 - y0) - (x1 - x0) * dy))/sqrt((double)(dx*dx + dy*dy));
}
  
int getbin(int x0, int y0, int n1, int n2)
{
  int d = pointdist(x0, y0, 0, 0, n1, n2);
  int maxd1 = pointdist(n1, 0, 0, 0, n1, n2);
  int maxd2 = pointdist(0, n2, 0, 0, n1, n2);
  int maxd = (maxd1 > maxd2) ? maxd1 : maxd2;
  if (maxd == 0) maxd = 1;
  int bin = NBINS * d / maxd;
  if(bin >= NBINS) bin = NBINS-1;
  if(bin < 0) bin=0;
  return bin;
}

void score_match(struct signature *feats1, struct signature *feats2, 
		 int N1, int N2, int start1, int end1, int start2, int end2, float T, 
		 int *d, int *dots, int *bins)
{
   memset(bins, 0, NBINS*sizeof(int));

   if (end1 - start1 >= EMPTY ) end1 = start1+EMPTY-1;
   if (end2 - start2 >= EMPTY ) end2 = start2+EMPTY-1;

   int n1 = end1 - start1;
   int n2 = end2 - start2;


   *d = dist(feats1, feats2, start1, end1, start2, end2, N1-1, N2-1, T );
   *dots = 0;

   int i,j;
   for ( j = end2-1; j >= start2; j-- ) {
      for ( i = start1; i < end1; i++ ) {	 
	 if ( dot(&feats1[i], &feats2[j], T) ) {
	    bins[getbin(i-start1,j-start2,n1,n2)]++;
	    (*dots)++;
	 }
      }
   }
}

float dtw_score(struct signature *feats1, struct signature *feats2, 
	       frameind N1, frameind N2, frameind start1, frameind end1, frameind start2, frameind end2)
{
   int n1 = end1 - start1 + 1;
   int n2 = end2 - start2 + 1;
   
   float simmx[n1*n2];

   for ( int i = 0; i < n1; i++ ) {
      for ( int j = 0; j < n2; j++ ) {
	    simmx[i*n2+j] = (1-approximate_cosine(feats1+start1+i,
						  feats2+start2+j))/2;
      }
   }

   for ( int i = 1; i < n1; i++ ) {
      simmx[i*n2] += simmx[(i-1)*n2];
   }

   for ( int j = 1; j < n2; j++ ) {
      simmx[j] += simmx[j-1];
   }

   for ( int i = 1; i < n1; i++ ) {
      for ( int j = 1; j < n2; j++ ) {
	 float dcost = simmx[(i-1)*n2+(j-1)] + simmx[i*n2+j];
	 float vcost = simmx[(i-1)*n2+j] + simmx[i*n2+j];
	 float hcost = simmx[i*n2+(j-1)] + simmx[i*n2+j];

	 simmx[i*n2+j] = MIN( dcost, MIN(vcost, hcost) );
      }
   } 

   return simmx[n1*n2-1]/(n1+n2);
}

void chop(char *str)
{
  for( ; *str; str++)
    if(*str == '\n') *str = 0;
}

void tr(char *str, char from, char to)
{
  for( ; *str; str++)
    if(*str == from) *str = to;
}

