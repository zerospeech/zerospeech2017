//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "feat.h"
#include "util.h"

int feat_readfile(char *featfile, float *feats, int D, int maxframes)
{
   assert_file_exist( featfile );
   FILE* fptr = fopen(featfile,"r");
   if (NULL == fptr) fatal("Feature file does not exist!\n");
   
   int c = 0;
   while (!feof(fptr) && c < maxframes*D) {
      fscanf(fptr,"%f",&feats[c]);
      c++;
   }

   if(feof(fptr)) c--;

   fclose(fptr);

   if (c % D != 0) fatal("Incorrect feature geometry!\n");
   
   return c/D;
}

int feat_downsample(int N, float * feats, int D, int dsfact)
{
   for ( int n = 0; n < N/dsfact; n++ )
   {
      for ( int d = 0; d < D; d++ )
      {
	 feats[n*D+d] = feats[2*n*D+d];
      }
   }

   return N/dsfact;
}

void feat_gettraj(int N, float *feats, int D, int d, float *traj)
{
   for(int n=0; n<N; n++)
     traj[n] = feats[n*D+d];
}

int feat_postings(int N, float *feats, int D, float pthr, int *cntlist, int **occlist)
{
  memset((void *)cntlist, 0, sizeof(*cntlist) * D);
   
  for(int n=0; n<N; n++)
    for(int d=0; d<D; d++)
      if (feats[n*D+d] > pthr )
	occlist[d][cntlist[d]++] = n;

  int maxdots = 0;
  for(int d=0; d<D; d++)
    maxdots += cntlist[d]*(cntlist[d]-1)/2;

  return maxdots;
}

void feat_normalize(float *feats, int N, int D, float *nfeats)
{
  float *f = feats;
  float *nf = nfeats;

  for (int n=0; n<N; n++) {
    double sum_of_squares = 0;
    for(int d=0; d<D; d++, f++)
      sum_of_squares += *f * *f;

    double norm = sqrt(sum_of_squares);

    float *fend = f;
    f -= D;
    for(; f<fend;nf++,f++) *nf = *f/norm;
  }
}
