//
// Copyright 2011-2015  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "util.h"

// Name of output file
char *projfile = NULL;
char dfile[100];

// dimension of the feature vectors
int D = 39;

// Number of bits in the resulting signature
int S = 32;

// Random seed
int seed = -1;

void usage()
{
   fprintf(stderr,"usage: genproj [-projfile <str> (defaults to proj_S#xD#_{date|seed})]\
\n\t[-D <n> (defaults to 39)]\
\n\t[-S <n> (defaults to 32)]\
\n\t[-seed <n> (defaults to date)]\n");
   exit(1);
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-projfile") == 0 ) projfile = argv[++i];
     else if ( strcmp(argv[i], "-h") == 0 ) usage();
     else if ( strcmp(argv[i], "-seed") == 0 ) seed = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-?") == 0 ) usage();
     else if ( strcmp(argv[i], "--help") == 0 ) usage();
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !projfile ) {
     if ( seed >= 0 ) {
	sprintf(dfile, "proj_b%dxd%d_seed%d", S, D, seed ); 
	srand(seed);
     } else {
	time_t timeval = time(NULL);
	struct tm *now = localtime(&timeval);
	srand( timeval );
	
	sprintf(dfile, "proj_b%dxd%d_%02d%02d%02d_%02d%02d%02d", S, D, 
		now->tm_year-100, now->tm_mon+1, now->tm_mday,
		now->tm_hour, now->tm_min, now->tm_sec);
     }
     projfile = dfile;
  }

  fprintf(stderr, "\nRun Parameters\n--------------\n\
projfile = %s, D = %d, S = %d\n\n",
	  projfile, D, S);
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   // Draw uniform random numbers between
   float randmat[S*D];

   for ( int i = 0; i < S*D; i++ ) {
      randmat[i] = 0.0;
      for ( int j = 0; j < 12; j++ ) {
	 randmat[i] += 12.0*(((float)rand())/RAND_MAX-0.5);
      }
      randmat[i] /= 12.0;
   }

   FILE *fptr = fopen(projfile,"w");
   int nw = fwrite(randmat, sizeof(float), S*D, fptr);

   if ( nw != S*D ) {
      fprintf(stderr,"ERROR: Write unsuccessful\n");
      exit(1);
   }
   fclose(fptr);

   return 0;
}
