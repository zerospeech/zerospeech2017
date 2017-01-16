//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "util.h"
#include "icsilog.h"

#define MAXCOST 1e10
#define MAXFRAMES 500
#define MAXCHAR 200

char *matchlist = NULL;
char *file1 = NULL;
char *file2 = NULL;
float simmx[MAXFRAMES*MAXFRAMES];
float costmx[MAXFRAMES*MAXFRAMES];
int aacntmx[MAXFRAMES*MAXFRAMES];
int lenmx[MAXFRAMES*MAXFRAMES];
int D = 39;
int maxrun = INT_MAX;
int kldiv = 0;
int sym = 0;
int wmvn = 0;
float *LOOKUP_TABLE = NULL;
int nbits_log = 14;
int sakoe = 0;

void usage()
{
  printf("usage: univ_rescore_dtw -matchlist <str> (REQUIRED)]\
\n\t[-file1 <str> (REQUIRED)]\
\n\t[-file2 <str> (REQUIRED)]\
\n\t[-D <n> (defaults to 39)]\
\n\t[-maxrun <n> (defaults to INT_MAX)]\
\n\t[-sakoe <n> (defaults to 0)]\
\n\t[-kldiv <n> (defaults to 0=cosine)]\
\n\t[-sym <n> (defaults to 0=not symmetrized)]\
\n\t[-wmvn <n> (defaults to 0=not word-level mvn)]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if ( strcmp(argv[i], "-matchlist") == 0 ) matchlist = argv[++i];
     else if ( strcmp(argv[i], "-file1") == 0 ) file1 = argv[++i];
     else if ( strcmp(argv[i], "-file2") == 0 ) file2 = argv[++i];
     else if ( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-maxrun") == 0 ) maxrun = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-sakoe") == 0 ) sakoe = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-kldiv") == 0 ) kldiv = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-sym") == 0 ) sym = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-wmvn") == 0 ) wmvn = atoi(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !matchlist ) {
     usage();
     fatal("\nERROR: matchlist arg is required");
  }

  if ( !file1 ) {
     usage();
     fatal("\nERROR: file1 not specified");
  }

  if ( !file2 ) {
     usage();
     fatal("\nERROR: file2 not specified");
  }

}

float dtw( float *X1, int N1, float *X2, int N2, int D )
{  
   float norm1[D][2];
   float norm2[D][2];
   for ( int d = 0; d < D; d++ ) {
      norm1[d][0] = 0;
      norm1[d][1] = 1;
      norm2[d][0] = 0;
      norm2[d][1] = 1;
   }
   
   if ( wmvn ) {
      for ( int d = 0; d < D; d++ ) {
	 norm1[d][1] = 0;
	 for ( int j = 0; j < N1; j++ ) {
	    float f = X1[j*D+d];
	    norm1[d][0] += f;
	    norm1[d][1] += f*f;
	 }
	 norm1[d][0] = norm1[d][0]/N1;
	 norm1[d][1] = sqrt(norm1[d][1]/N1 - norm1[d][0]*norm1[d][0]);

	 norm2[d][1] = 0;
	 for ( int j = 0; j < N2; j++ ) {
	    float f = X2[j*D+d];
	    norm2[d][0] += f;
	    norm2[d][1] += f*f;
	 }
	 norm2[d][0] = norm2[d][0]/N2;
	 norm2[d][1] = sqrt(norm2[d][1]/N2 - norm2[d][0]*norm2[d][0]);

      }
   }

   if ( kldiv ) {
      if ( sym ) {
	 for ( int i = 0; i < N1; i++ ) {
	    for ( int j = 0; j < N2; j++ ) {
	       simmx[i*N2+j] = 0;
	       for ( int d = 0; d < D; d++ ) {
		  simmx[i*N2+j] += 
		     0.5*X1[i*D+d]*icsi_log(X1[i*D+d]/X2[j*D+d],
					LOOKUP_TABLE,nbits_log) +
		     0.5*X2[j*D+d]*icsi_log(X2[j*D+d]/X1[i*D+d],
					LOOKUP_TABLE,nbits_log);
	       }
	    }
	 } 
      } else {
	 for ( int i = 0; i < N1; i++ ) {
	    for ( int j = 0; j < N2; j++ ) {
	       simmx[i*N2+j] = 0;
	       for ( int d = 0; d < D; d++ ) {
		  simmx[i*N2+j] += 
		     X1[i*D+d]*icsi_log(X1[i*D+d]/X2[j*D+d],
					LOOKUP_TABLE,nbits_log);
	       }
	    }
	 } 
      }
   } else {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    float lenx = 0;
	    float leny = 0;
	    simmx[i*N2+j] = 0;
	    for ( int d = 0; d < D; d++ ) {
	       float x = (X1[i*D+d]-norm1[d][0])/norm1[d][1];
	       float y = (X2[j*D+d]-norm2[d][0])/norm2[d][1];
	       simmx[i*N2+j] += x*y;
	       lenx += x*x;
	       leny += y*y;
	    }
	    simmx[i*N2+j] = 1-simmx[i*N2+j]/sqrt(lenx*leny);
	 }
      } 
   }

   N1++;
   N2++;
   for ( int i = 0; i < N1; i++ ) {
      for ( int j = 0; j < N2; j++ ) {
	 aacntmx[i*N2+j] = 0;
	 costmx[i*N2+j] = 0;
	 lenmx[i*N2+j] = 0;
      }
   }

   for ( int i = 0; i < N1; i++ ) {
      aacntmx[i*N2] = i;
      lenmx[i*N2] = i;
      if ( i == 0 )
	 costmx[i*N2] = 0;
      else 
	 costmx[i*N2] = MAXCOST;
   }

   for ( int j = 0; j < N2; j++ ) {
      aacntmx[j] = j;
      lenmx[j] = j;
      if ( j == 0 )
	 costmx[j] = 0;      
      else
	 costmx[j] = MAXCOST;
   }

   if ( !sakoe ) {
      for ( int i = 1; i < N1; i++ ) {
	 for ( int j = 1; j < N2; j++ ) {
	    float dcost = costmx[(i-1)*N2+(j-1)] + simmx[(i-1)*(N2-1)+j-1];
	    float vcost = costmx[(i-1)*N2+j] + simmx[(i-1)*(N2-1)+j-1];
	    float hcost = costmx[i*N2+(j-1)] + simmx[(i-1)*(N2-1)+j-1];
	    
	    if ( aacntmx[(i-1)*N2+j] + 1 >= maxrun )
	       vcost = MAXCOST;
	    
	    if ( aacntmx[i*N2+(j-1)] + 1 >= maxrun )
	       hcost = MAXCOST;
	    
	    costmx[i*N2+j] = MIN( dcost, MIN(vcost, hcost) );
	    
	    if ( dcost <= vcost && dcost <= hcost ) {
	       lenmx[i*N2+j] = lenmx[(i-1)*N2+(j-1)] + 1;
	    } else if ( vcost <= hcost ) {
	       lenmx[i*N2+j] = lenmx[(i-1)*N2+j] + 1;
	    } else {
	       lenmx[i*N2+j] = lenmx[i*N2+(j-1)] + 1;
	    }
	    
	    if ( vcost < dcost || hcost < dcost ) {
	       if ( vcost < hcost ) {
		  aacntmx[i*N2+j] = aacntmx[(i-1)*N2+j] + 1;
	       } else {
		  aacntmx[i*N2+j] = aacntmx[i*N2+(j-1)] + 1;
	       }
	    } 
	 }
      } 
   } else {
      for ( int i = 1; i < N1; i++ ) {
	 for ( int j = 1; j < N2; j++ ) {
	    if ( i == 1 || j == 1 ) {
	       float dcost = costmx[(i-1)*N2+(j-1)] + simmx[(i-1)*(N2-1)+j-1];
	       float vcost = costmx[(i-1)*N2+j] + simmx[(i-1)*(N2-1)+j-1];
	       float hcost = costmx[i*N2+(j-1)] + simmx[(i-1)*(N2-1)+j-1];
	       
	       costmx[i*N2+j] = MIN( dcost, MIN(vcost, hcost) );
	    } else {
	       float dcost = costmx[(i-1)*N2+(j-1)] + 2*simmx[(i-1)*(N2-1)+j-1];
	       float vcost = costmx[(i-2)*N2+(j-1)] + 2*simmx[(i-2)*N2+(j-1)] 
		  + simmx[(i-1)*(N2-1)+j-1];
	       float hcost = costmx[(i-1)*N2+(j-2)] + 2*simmx[(i-1)*N2+(j-2)] + 
		  simmx[(i-1)*(N2-1)+j-1];
	       
	       costmx[i*N2+j] = MIN( dcost, MIN(vcost, hcost) );
	    }
	 }
      } 
   }

   return costmx[N1*N2-1]/(N1+N2-2);
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   LOOKUP_TABLE = (float*) MALLOC(((int) pow(2,nbits_log))*sizeof(float));
   fill_icsi_log_table(nbits_log,LOOKUP_TABLE); 

   int Nmatch = file_line_count( matchlist );
   fprintf(stderr,"Total matches to rescore: %d\n",  Nmatch);

   // Read in the matchlist
   fprintf(stderr,"Reading matchlist: "); tic();

   int *xA = (int *) MALLOC( Nmatch*sizeof(int) );
   int *xB = (int *) MALLOC( Nmatch*sizeof(int) );
   int *yA = (int *) MALLOC( Nmatch*sizeof(int) );
   int *yB = (int *) MALLOC( Nmatch*sizeof(int) );

   float *oldscore = (float *) MALLOC( Nmatch*sizeof(float) );
   float *rho = (float *) MALLOC( Nmatch*sizeof(float) );
   
   assert_file_exist( matchlist );
   FILE *fptr = fopen(matchlist, "r");
   int wcnt = 0;

   while ( wcnt < Nmatch && 
	   fscanf(fptr, "%d%d%d%d%f%f",
		  &xA[wcnt], &xB[wcnt], &yA[wcnt], &yB[wcnt],
		  &oldscore[wcnt], &rho[wcnt]) != EOF ) {
      wcnt++;
   }
   fclose(fptr);
   fprintf(stderr, "%f s\n",toc());

   // Read in the features
   float *feats1 = NULL;
   float *feats2 = NULL;
   
   int N1, N2;

   int m1 = -1;
   int m2 = -1;
   assert_file_exist( file1 );	 
   feats1 = readfeats_file(file1, D, &m1, &m2, &N1);
   fprintf(stderr,"Read %s (%d frames)\n", file1, N1);

   m1 = -1;
   m2 = -1;
   assert_file_exist( file2 );
   feats2 = readfeats_file(file2, D, &m1, &m2, &N2);
   fprintf(stderr,"Read %s (%d frames)\n", file2, N2);

   // Rescore the matches
   fprintf(stderr,"Rescoring the matchfile: %s\n", matchlist); tic();
   for ( int i = 0; i < Nmatch; i++ ) {
      
      xA[i] = MAX(xA[i],1);
      yA[i] = MAX(yA[i],1);

      if (xB[i] > N1+5) continue;

      int dur1 = MIN(N1-xA[i]+1, xB[i]-xA[i]+1);
      int dur2 = MIN(N2-yA[i]+1, yB[i]-yA[i]+1);

      if ( dur1 > MAXFRAMES-1 || dur2 > MAXFRAMES-1 )
	 continue;


      float dtwdist = dtw( &feats1[(xA[i]-1)*D], dur1, 
			   &feats2[(yA[i]-1)*D], dur2, D );      

      if ( !kldiv && dtwdist > 2 ) continue;

      if ( kldiv ) dtwdist /= 10;

      printf("%d %d %d %d %f %f %f\n", 
	     xA[i], xB[i], 
	     yA[i], yB[i], 1-dtwdist/2, rho[i], oldscore[i]);      	 
   }
   fprintf(stderr, " %f s\n",toc());
   
   // FREE everything that was malloc-d
   FREE(feats1);
   FREE(feats2);

   FREE(xA);
   FREE(xB);
   FREE(yA);
   FREE(yB);

   FREE(oldscore);
   FREE(rho);

   FREE(LOOKUP_TABLE);

   return 0;
}
