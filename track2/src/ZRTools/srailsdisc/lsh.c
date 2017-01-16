//
// Copyright 2011-2015  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include "feat.h"
#include "util.h"
#include "signature.h"

char *projfile = NULL;
char *featfile = NULL;
char *sigfile = NULL;

int D = 39;
int S = 32;
int NBYTES = 4;
float sigma = 0;
float normthr = 0;

void usage()
{
  fatal("usage: lsh [-projfile <str> (REQUIRED)]\
\n\t[-featfile <str> (REQUIRED)]\
\n\t[-sigfile <str> (REQUIRED)]\
\n\t[-D <n> (defaults to 39)]\
\n\t[-S <n> (defaults to 32)]\
\n\t[-normthr <n> (defaults to 0 = no threshold)]\
\n\t[-sigma <f> (defaults to 0)]");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-projfile") == 0 ) projfile = argv[++i];
     else if ( strcmp(argv[i], "-featfile") == 0 ) featfile = argv[++i];
     else if ( strcmp(argv[i], "-sigfile") == 0 ) sigfile = argv[++i];
     else if ( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-sigma") == 0 ) sigma = atof(argv[++i]);
     else if ( strcmp(argv[i], "-normthr") == 0 ) normthr = atof(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !projfile ) {
     usage();
     fatal("\nERROR: projfile arg is required");
  }

  if ( !featfile ) {
     usage();
     fatal("\nERROR: featfile arg is required");
  }

  if ( !sigfile ) {
     usage();
     fatal("\nERROR: sigfile arg is required");
  }

  if ( S < 8 || S % 8 != 0 ) {
     usage();
     fatal("\nERROR: unsupported number of bits");
  }

  NBYTES = S/8;
  
  fprintf(stderr, "\nRun Parameters\n--------------\n\
projfile = %s, \n\
featfile = %s, \n\
sigfile = %s, \n\
S = %d, D = %d, normthr = %f, sigma = %f\n\n",
	  projfile, featfile, sigfile, S, D, normthr, sigma);

  srand(time(NULL));
}

float randn( void ) 
{
   float r = 0.0;
   for ( int j = 0; j < 12; j++ ) {
      r += 12.0*(((float)rand())/RAND_MAX-0.5);
   }
   return r/12.0;
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   // Read the feature matrix
   int defnegA = -1;
   int defnegB = -1;
   int N;
   float *feats = readfeats_file(featfile, D, &defnegA, &defnegB, &N);
   fprintf(stderr, "featfile = %s; N = %d frames\n", featfile, N);

   // Read the projection matrix
   defnegA = -1;
   defnegB = -1;
   int Sfile;
   float *T = readfeats_file(projfile, D, &defnegA, &defnegB, &Sfile);
   if ( S != Sfile ) {
      fprintf(stderr,"Sfile = %d inconsistent with projfile length\n", Sfile);
      exit(1);
   }
   fprintf(stderr, "projfile = %s; D = %d, S = %d bits\n", projfile, D, Sfile);

   // Compute the LSH signatures and write to file
   byte *sigs = (byte *) MALLOC( NBYTES*N*sizeof(byte) );
   memset(sigs, NBYTES*N, sizeof(byte));

   for ( int i = 0; i < N; i++ ) {

      if ( normthr > 0 ) {
	 float norm = 0;
	 for ( int d = 0; d < D; d++ ) {
	    norm += feats[i*D+d]*feats[i*D+d];
	 }
	 norm = sqrt(norm);

	 if ( norm < normthr ) {
	    continue;
	 }
      }

      if ( sigma > 0 ) {
	 for ( int d = 0; d < D; d++ ) {
	    feats[i*D+d] += randn()*sigma;
	 }
      }

      for ( int B = 0; B < NBYTES; B++ ) {
	 sigs[NBYTES*i+B] = 0;
	 for ( int b = 0; b < 8; b++ ) {
	    float proj = 0;
	    for ( int d = 0; d < D; d++ ) {
	       proj += feats[i*D+d] * T[(B*8+b)*D+d];
	    }
	    sigs[NBYTES*i+B] += (proj >= 0) << b;
	 }
      }
   }
   
   FILE *fptr = fopen( sigfile, "w" );
   N = fwrite(sigs, sizeof(byte), NBYTES*N, fptr);
   fclose(fptr);
   fprintf(stderr, "Wrote %d signatures to %s\n", N/NBYTES, sigfile);
   
   FREE(sigs);
   FREE(feats);
   FREE(T);

   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
