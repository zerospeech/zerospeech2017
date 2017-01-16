//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "feat.h"
#include "util.h"
#include "dot.h"
#include "signature.h"

char *projfile = NULL;
char *featfile = NULL;
char *sigfile = NULL;
char *vadfile = NULL;

int D = 39;
int S = 32;


void usage()
{
  fatal("usage: lsh [-projfile <str> (REQUIRED)]\
\n\t[-featfile <str> (REQUIRED)]\
\n\t[-sigfile <str> (REQUIRED)]\
\n\t[-vadfile <str>]\
\n\t[-D <n> (defaults to 39)]\
\n\t[-S <n> (defaults to 32)]");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-projfile") == 0 ) projfile = argv[++i];
     else if ( strcmp(argv[i], "-featfile") == 0 ) featfile = argv[++i];
     else if ( strcmp(argv[i], "-sigfile") == 0 ) sigfile = argv[++i];
     else if ( strcmp(argv[i], "-vadfile") == 0 ) vadfile = argv[++i];
     else if ( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
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

  if ( S != 32 && S != 64 ) {
     usage();
     fatal("\nERROR: unsupported number of bits");
  }

  fprintf(stderr, "\nRun Parameters\n--------------\n\
projfile = %s, \n\
featfile = %s, \n\
sigfile = %s, \n\
vadfile = %s, \n\
S = %d, D = %d\n\n",
	  projfile, featfile, sigfile, vadfile, S, D);
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

   // Read the vad file if it exists
   int *vadA = NULL;
   int *vadB = NULL;
   int vadcnt = 0;

   if ( vadfile ) {
      assert_file_exist( vadfile );
      FILE *fptr = fopen(vadfile,"r");

      // Count entries in vad file
      char c; int lines = 0;
      while ((c = fgetc(fptr)) != EOF) {
	 if ( c == '\n' ) lines++;
      }
      fseek(fptr,-1,SEEK_END);
      if ( fgetc(fptr) != '\n' )
	 lines++;

      // Fill vad arrays
      if ( lines > 0 ) {
	 fseek(fptr,0,SEEK_SET);

	 vadA = (int *) MALLOC( lines*sizeof(int) );
	 vadB = (int *) MALLOC( lines*sizeof(int) );
	 vadcnt = lines;
	 
	 for ( int i = 0; i < lines; i++ ) {
	    fscanf(fptr, "%d%d", &vadA[i], &vadB[i]);
	 }
      } 

      fclose(fptr);
   } 

   // If no vad info was specified process the whole file
   if ( vadcnt == 0 ) {
      vadA = (int *) MALLOC( sizeof(int) );
      vadB = (int *) MALLOC( sizeof(int) );
      vadA[0] = 1;
      vadB[0] = N;
      vadcnt = 1;
   }

   // Compute the LSH signatures and write to file
   if ( S == 32 ) {
      unsigned int *sigs = (unsigned int *) MALLOC( N*sizeof(unsigned int) );

      int currV = 0;
      for ( int i = 0; i < N; i++ ) {
	 sigs[i] = 0;
	 
	 if ( currV < vadcnt && i+1 > vadB[currV] )
	    currV++;
	 
	 if ( currV >= vadcnt || i+1 < vadA[currV] )
	    continue;
	 
	 for ( int b = 0; b < S; b++ ) {
	    float proj = 0;
	    for ( int d = 0; d < D; d++ ) {
	       proj += feats[i*D+d] * T[b*D+d];
	    }
	    sigs[i] += (proj >= 0) << b;
	 }
      }
      
      FILE *fptr = fopen( sigfile, "w" );
      N = fwrite(sigs, sizeof(unsigned int), N, fptr);
      fclose(fptr);
      fprintf(stderr, "Wrote %d signatures to %s\n", N, sigfile);

      FREE(sigs);
   } else if ( S == 64 ) {
      unsigned int *sigs = (unsigned int *) MALLOC( 2*N*sizeof(unsigned int) );

      int currV = 0;
      for ( int i = 0; i < N; i++ ) {
	 sigs[2*i] = 0;
	 sigs[2*i+1] = 0;
	 
	 if ( currV < vadcnt && i+1 > vadB[currV] )
	    currV++;
	 
	 if ( currV >= vadcnt || i+1 < vadA[currV] )
	    continue;
	 
	 for ( int b = 0; b < S/2; b++ ) {
	    float proj = 0;
	    for ( int d = 0; d < D; d++ ) {
	       proj += feats[i*D+d] * T[b*D+d];
	    }
	    sigs[2*i] += (proj >= 0) << b;

	    proj = 0;
	    for ( int d = 0; d < D; d++ ) {
	       proj += feats[i*D+d] * T[(b+S/2)*D+d];
	    }
	    sigs[2*i+1] += (proj >= 0) << b;
	 }
      }
      
      FILE *fptr = fopen( sigfile, "w" );
      N = fwrite(sigs, sizeof(unsigned int), 2*N, fptr);
      fclose(fptr);
      fprintf(stderr, "Wrote %d signatures to %s\n", N/2, sigfile);

      FREE(sigs);
   } else {
      fatal("ERROR: unsupported number of bits\n");
   }
   
   FREE(vadA);
   FREE(vadB);
   FREE(feats);
   FREE(T);

   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
