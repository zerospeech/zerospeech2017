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

char *statsfile = NULL;
char *infile = NULL;
char *outfile = NULL;
char *vadfile = NULL;

int D = 39;


void usage()
{
  fatal("usage: standfeat [-statsfile <str>]\
\n\t[-infile <str> (REQUIRED)]\
\n\t[-outfile <str> (REQUIRED)]\
\n\t[-vadfile <str>]\
\n\t[-D <n> (defaults to 39)]");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-statsfile") == 0 ) statsfile = argv[++i];
     else if ( strcmp(argv[i], "-infile") == 0 ) infile = argv[++i];
     else if ( strcmp(argv[i], "-outfile") == 0 ) outfile = argv[++i];
     else if ( strcmp(argv[i], "-vadfile") == 0 ) vadfile = argv[++i];
     else if ( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !infile ) {
     usage();
     fatal("\nERROR: infile arg is required");
  }

  if ( !outfile ) {
     usage();
     fatal("\nERROR: outfile arg is required");
  }

  fprintf(stderr, "\nRun Parameters\n--------------\n\
statsfile = %s, \n\
vadfile = %s, \n\
infile = %s, \n\
outfile = %s, \n\
D = %d\n\n",
	  statsfile, vadfile, infile, outfile, D);
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   // Read the feature matrix
   int defnegA = -1;
   int defnegB = -1;
   int N;
   float *feats = readfeats_file(infile, D, &defnegA, &defnegB, &N);
   fprintf(stderr, "infile = %s; N = %d frames\n", infile, N);

   // Extract the mean and stdev vectors
   int framecnt=0;
   float std[D];
   float mean[D];

   // If statsfile provided, get the sample mean and variance from it
   // Else get if from the data
   if ( statsfile ) {
      defnegA = -1;
      defnegB = -1;
      int nlines;
      double *stats = readfeats_file_d( statsfile, D, &defnegA, &defnegB, 
				        &nlines );
      if ( nlines != 80 ) {
	 fprintf(stderr,"Improperly formatted stats file\n");
	 exit(1);
      }
      
      for ( int d = 0; d < D; d++ ) {
	 std[d] = (float) sqrt(stats[d*80+d]);
	 mean[d] = (float) stats[d*80+D];
      }

      FREE(stats);
   } else {
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
      } else {
	 vadA = (int *) MALLOC( sizeof(int) );
	 vadB = (int *) MALLOC( sizeof(int) );
	 vadcnt = 1;
	 vadA[0] = 0;
	 vadB[0] = 100000000;
      }

      for ( int d = 0; d < D; d++ ) {
	 mean[d] = 0;
	 std[d] = 0;
      }

      for ( int v = 0; v < vadcnt; v++ ) {
	 for ( int n = vadA[v]; n < MIN(N,vadB[v]+1); n++ ) {
	    framecnt++;
	    for ( int d = 0; d < D; d++ ) {
	       mean[d] += feats[n*D+d];
	       std[d] += feats[n*D+d]*feats[n*D+d];
	    }
	 }
      }
	       
      for ( int d = 0; d < D; d++ ) {
	 mean[d] /= framecnt;
	 std[d] /= framecnt;
	 std[d] = sqrtf(std[d] - mean[d]*mean[d]);
      }

      FREE(vadA);
      FREE(vadB);	 
   } 

   // Standardize features and write to file
   for ( int n = 0; n < N; n++ ) {
      for ( int d = 0; d < D; d++ ) {
	 feats[n*D+d] = (feats[n*D+d]-mean[d])/std[d];
      }
   }

   FILE *fptr = fopen( outfile, "w" );

   N = fwrite(feats, sizeof(float), N*D, fptr);
   fclose(fptr);
   fprintf(stderr, "Wrote %d frames to %s\n", N/D, outfile);

   FREE(feats);

   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
