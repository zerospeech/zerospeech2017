//
// Copyright 2015  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "feat.h"
#include "util.h"
#include "dot.h"
#include "signature.h"

// PLEB parameters
int P = 8;
int B = 10;
float T = 0.95;
int S = 64;

// Everything else
char *sigfile1 = NULL;
char *sigfile2 = NULL;
char *seglist1 = NULL;
char *seglist2 = NULL;
int diffspeech = 0;

void usage()
{
  fatal("usage: srails_disc [-sigfile1 <str> (REQUIRED)]\
\n\t[-seglist1 <str> (REQUIRED)]\
\n\t[-sigfile2 <str>]\
\n\t[-seglist2 <str>]\
\n\t[-P <n> (defaults to 8)]\
\n\t[-B <n> (defaults to 10)]\
\n\t[-T <n> (defaults to 0.95)]\
\n\t[-S <n> (defaults to 64)]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if ( strcmp(argv[i], "-sigfile1") == 0 ) sigfile1 = argv[++i];
     else if ( strcmp(argv[i], "-sigfile2") == 0 ) sigfile2 = argv[++i];
     else if ( strcmp(argv[i], "-seglist1") == 0 ) seglist1 = argv[++i];
     else if ( strcmp(argv[i], "-seglist2") == 0 ) seglist2 = argv[++i];
     else if ( strcmp(argv[i], "-P") == 0 ) P = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-B") == 0 ) B = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-T") == 0 ) T = atof(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !sigfile1 || !seglist1) {
     usage();
     fatal("\nERROR: sigfile1 and seglist1 args are required");
  }

  if ( sigfile2 && strcmp(sigfile2,sigfile1) != 0 ) {
     diffspeech = 1;
  } else {
     if ( !sigfile2 ) {
	sigfile2 = sigfile1;
     }

     if ( !seglist2 ) {
	seglist2 = seglist1;
     }
  }

  fprintf(stderr, "\nRun Parameters\n--------------\n \
sigfile1 = %s, seglist1 = %s\n \
sigfile2 = %s, seglist2 = %s\n \
P = %d, B = %d, T = %f, S = %d\n\n",
	  sigfile1, seglist1, sigfile2, seglist2,
	  P, B, T, S);

  SIG_NUM_BYTES = S/8;
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   int N1 = 0;
   int N2 = 0;
   int NS1 = 0;
   int NS2 = 0;
   int xA = -1; 
   int xB = -1; 

   // Read the signature and seglist files
   struct signature *sigs1 = (struct signature *)readsigs_file(sigfile1, &xA, &xB, &N1);
   Segment *segs1 =  readseglist_file(seglist1, &NS1);
   fprintf(stderr, "sigfile1 = %s; N1 = %d signatures\n", sigfile1, N1);
   fprintf(stderr, "seglist1 = %s; NS1 = %d segments\n", seglist1, NS1);
   assert( N1 == NS1 );

   struct signature *sigs2 = NULL;
   Segment *segs2 = NULL;
   if ( diffspeech ) {
      xA = -1; xB = -1;
      sigs2 = (struct signature *)readsigs_file(sigfile2, &xA, &xB, &N2);
      segs2 =  readseglist_file(seglist2, &NS2);
      fprintf(stderr, "sigfile2 = %s; N2 = %d signatures\n", sigfile2, N2);
      fprintf(stderr, "seglist2 = %s; NS2 = %d segments\n", seglist2, NS2);
      assert( N2 == NS2 );
   } else {
      sigs2 = sigs1;
      segs2 = segs1;
      N2 = N1;
   }

   int Nmax = max(N1,N2);

   // Initialize the pleb permutations
   initialize_permute();

   // Compute the segmental dot plot
   long int maxdots = P*B*(Nmax);

   fprintf(stderr,"Computing segment dotplot (Max Dots = %ld) ...\n", 
	   maxdots); 
   tic();

   Dot *dotlist = (Dot *) MALLOC(maxdots*sizeof(Dot));
   memset(dotlist, 0, maxdots*sizeof(Dot));

   int dotcnt = pleb( sigs1, N1, sigs2, N2, diffspeech, 
		      P, B, T, dotlist );

   fprintf(stderr, "    Total dots above threshold: %d\n", dotcnt);
   fprintf(stderr, "Finished: %f sec.\n",toc());

   // Sort the dotlist to facilitate deduping
   fprintf(stderr, "Sorting the dotlist: "); tic();
   qsort(dotlist, dotcnt, sizeof(Dot), &dot_compare);
   fprintf(stderr, "%f s\n",toc());

   // Dedup the dotlist
   fprintf(stderr, "Deduping the dotlist: "); tic();
   dotcnt = dedup_dotlist( dotlist, dotcnt );
   fprintf(stderr, "%f s\n",toc());

   // Remove overlapping segments
   if ( !diffspeech ) {
      fprintf(stderr, "Deduping the dotlist: "); tic();
      dotcnt = filter_overlap( dotlist, dotcnt, segs1, segs2, 0.0 );
      fprintf(stderr, "%f s\n",toc());
   }

   // Generating the matchlist
   fprintf(stderr,"Dumping %d matches\n",dotcnt);
   dump_matchlist(dotlist, dotcnt, segs1, segs2);

   // Free the heap
   FREE(dotlist);

   free_signatures(sigs1,N1);
   FREE(segs1);

   if ( diffspeech ) {
      free_signatures(sigs2,N2);
      FREE(segs2);
   }

   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
