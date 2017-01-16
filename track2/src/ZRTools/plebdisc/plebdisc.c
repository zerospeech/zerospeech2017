//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "feat.h"
#include "util.h"
#include "dot.h"
#include "score_matches.h"
#include "signature.h"

#define MAXDOTS_MF 50000000
#define MAXMATCHES 100000
#define PRIMEFACT 2.5

// PLEB parameters
int P = 4;
int B = 100;
float T = 0.5;
float Tscore = 0.75;
int D = 10;
int S = 32;

// Everything else
int dy = 3;
int dx = 25;
int maxframes = 0;
char *featfile1 = NULL;
char *featfile2 = NULL;
int xA = -1;
int xB = -1;
int yA = -1;
int yB = -1;
double rhothr = 0.0;
float medthr = 0.5;
int twopass = 1;
int diffspeech = 0;
int compfact = 1;
int dtwscore = 1;
int kws = 0;

float castthr = 7;
int R = 10; 
float trimthr = 0.25;

char *dump_matchlistf = NULL;

void usage()
{
  fatal("usage: plebdisc [-file1 <str> (REQUIRED)]\
\n\t[-file2 <str>]\
\n\t[-P <n> (defaults to 4)]\
\n\t[-B <n> (defaults to 100)]\
\n\t[-T <n> (defaults to 0.5)]\
\n\t[-D <n> (defaults to 10)]\
\n\t[-S <n> (defaults to 32)]\
\n\t[-xA <n> (defaults to 0)]\
\n\t[-xB <n> (defaults to last frame)]\
\n\t[-yA <n> (defaults to 0)]\
\n\t[-yB <n> (defaults to last frame)]\
\n\t[-maxframes <n> (defaults to no limit)]\
\n\t[-medthr <n> (defaults to 0.5)]\
\n\t[-castthr <n> (defaults to 7)]\
\n\t[-trimthr <n> (defaults to 0.25)]\
\n\t[-R <n> (defaults to 10)]\
\n\t[-dx <n> (defaults to 25)]\
\n\t[-dy <n> (defaults to 3)] ]\
\n\t[-rhothr <n> (defalts to 0)]\
\n\t[-twopass <n> (defaults to 1)] ]\
\n\t[-Tscore <n> (defaults to 0.75)] ]\
\n\t[-dtwscore <n> (defaults to 1)] ]\
\n\t[-kws <n> (defaults to 0)] ]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-dy") == 0 ) dy = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-dx") == 0 ) dx = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-maxframes") == 0 ) maxframes = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-rhothr") == 0 ) rhothr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-file1") == 0 ) featfile1 = argv[++i];
     else if ( strcmp(argv[i], "-file2") == 0 ) featfile2 = argv[++i];
     else if ( strcmp(argv[i], "-P") == 0 ) P = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-B") == 0 ) B = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-T") == 0 ) T = atof(argv[++i]);
     else if ( strcmp(argv[i], "-Tscore") == 0 ) Tscore = atof(argv[++i]);
     else if ( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-xA") == 0 ) xA = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-xB") == 0 ) xB = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-yA") == 0 ) yA = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-yB") == 0 ) yB = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-medthr") == 0 ) medthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-castthr") == 0 ) castthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-trimthr") == 0 ) trimthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-R") == 0 ) R = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-twopass") == 0 ) twopass = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-dtwscore") == 0 ) dtwscore = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-kws") == 0 ) kws = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-dump-matchlist") == 0 ) {
       dump_matchlistf = argv[++i];
     }
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !featfile1 ) {
     usage();
     fatal("\nERROR: file1 arg is required");
  }

  if ( featfile2 && strcmp(featfile2,featfile1) != 0 ) {
     diffspeech = 1;
  } else {
     if ( !featfile2 ) {
	featfile2 = featfile1;
     }

     if ( xA != yA || xB != yB ) {
	diffspeech = 1;
     }
  }

  compfact = diffspeech + 1;

  if ( twopass < 0 || twopass > 1 )
     fatal("\nERROR: Invalid value for twopass\n");

  fprintf(stderr, "\nRun Parameters\n--------------\n \
file1 = %s, (xA,xB) = (%d,%d)\n \
file2 = %s, (yA,yB) = (%d,%d)\n \
maxframes = %d, \n \
P = %d, B = %d, T = %f, D = %d, S = %d,\n \
dx = %d, dy = %d, medthr = %f, \n \
castthr = %f, trimthr = %f, R = %d,\n \
rhothr = %f, twopass = %d, dtwscore = %d, Tscore = %f\n\n",
	  featfile1, xA, xB, featfile2, 
	  yA, yB, maxframes,   
	  P, B, T, D, S,
	  dx, dy, medthr, castthr, trimthr, R, rhothr, 
	  twopass, dtwscore, Tscore);

  dx *= 2;
  dy *= 2;

  SIG_NUM_BYTES = S/8;
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   int N1, N2, Nmax;
   if ( maxframes > 0 ) N1 = maxframes;
   else N1 = 0;

   struct signature *feats1 = (struct signature *)readsigs_file(featfile1, &xA, &xB, &N1);
   fprintf(stderr, "featfile1 = %s; N1 = %d frames\n", featfile1, N1);

   struct signature *feats2 = feats1;
   if ( diffspeech ) {
      if ( maxframes > 0 ) N2 = maxframes;
      else N2 = 0;
      feats2 = (struct signature *)readsigs_file(featfile2, &yA, &yB, &N2);
      fprintf(stderr, "featfile2 = %s; N2 = %d frames\n", featfile2, N2);
   } else {
      N2 = N1;
      yA = xA;
      yB = xB;
   }
   Nmax = max(N1,N2);

   // Initialize the pleb permutations
   initialize_permute();

   // Compute the rotated dot plot
   long maxdots;
   if ( kws ) 
      maxdots = P*B*N2*(D+1);
   else
      maxdots = P*B*(Nmax)*(2*D+1);

   fprintf(stderr,"Computing sparse dot plot (Max Dots = %ld) ...\n", maxdots); tic();

   Dot *dotlist = (Dot *) MALLOC(maxdots*sizeof(Dot));
   memset(dotlist, 0, maxdots*sizeof(Dot));

   int dotcnt;
   if ( kws ) {
      dotcnt = plebkws( feats1, N1, feats2, N2, diffspeech, 
			P, B, T, D, dotlist );
   } else {
      dotcnt = pleb( feats1, N1, feats2, N2, diffspeech, 
		     P, B, T, D, dotlist );
   }

   fprintf(stderr, "    Total elements in thresholded sparse: %d\n", dotcnt);
   fprintf(stderr, "Finished: %f sec.\n",toc());

   // Sort dots by row
   fprintf(stderr, "Applying radix sort of dotlist: "); tic();
   DotXV *radixdots = (DotXV *)MALLOC( dotcnt*sizeof(DotXV));
   int *cumsum = (int*)MALLOC((compfact*Nmax+1)*sizeof(int));
   radix_sorty(compfact*Nmax, dotlist, dotcnt, radixdots, cumsum);
   fprintf(stderr, "%f s\n",toc());

   // Sort rows by column
   fprintf(stderr, "Applying qsort of radix bins: "); tic();
   quick_sortx(compfact*Nmax, radixdots, dotcnt, cumsum);
   fprintf(stderr, "%f s\n",toc());

   // Remove duplicate dots in each row
   fprintf(stderr, "Removing duplicate dots from radix bins: "); tic();
   dotcnt = dot_dedup(radixdots, cumsum, compfact*Nmax, T);
   fprintf(stderr, "%f s\n",toc());
   fprintf(stderr, "    Total elements after dedup: %d\n", dotcnt);

   // Apply the median filter in the X direction
   fprintf(stderr, "Applying median filter to sparse matrix: "); tic();
   Dot *dotlist_mf = (Dot *) MALLOC(MAXDOTS_MF*sizeof(DotXV));
   dotcnt = median_filtx(compfact*Nmax, radixdots, dotcnt, cumsum, dx, medthr, dotlist_mf);
   fprintf(stderr, "%f s\n",toc());
   fprintf(stderr, "    Total elements in filtered sparse: %d\n", dotcnt);

   // Sort mf dots by row
   fprintf(stderr,"Applying radix sort of dotlist_mf: "); tic();
   DotXV *radixdots_mf = (DotXV *)MALLOC(dotcnt*sizeof(DotXV));
   int *cumsum_mf = (int *)MALLOC((compfact*Nmax+1)*sizeof(int));
   radix_sorty(compfact*Nmax, dotlist_mf, dotcnt, radixdots_mf, cumsum_mf);
   fprintf(stderr, "%f s\n",toc());

   // Sort mf rows by column
   fprintf(stderr,"Applying qsort of radix_mf bins: "); tic();
   quick_sortx(compfact*Nmax, radixdots_mf, dotcnt, cumsum_mf);
   fprintf(stderr, "%f s\n",toc());

   // Compute the Hough transform
   fprintf(stderr,"Computing hough transform: "); tic();
   float *hough = (float*)MALLOC(compfact*Nmax*sizeof(float));
   hough_gaussy(compfact*Nmax, dotcnt, cumsum_mf, dy, diffspeech, hough);
   fprintf(stderr, "%f s\n",toc());
   
   // Compute rho list
   fprintf(stderr, "Computing rholist: "); tic();
   int rhocnt = count_rholist(compfact*Nmax,hough,rhothr);
   int *rholist = (int *)MALLOC(rhocnt*sizeof(int));
   float *rhoampl = (float *)MALLOC(rhocnt*sizeof(float));
   rhocnt = compute_rholist(compfact*Nmax,hough,rhothr,rholist,rhoampl);
   fprintf(stderr, "%f s\n",toc());

   // Compute the matchlist
   fprintf(stderr, "Computing matchlist: "); tic();
   Match * matchlist = (Match*) MALLOC(MAXMATCHES*sizeof(Match));
   int matchcnt = compute_matchlist( Nmax, radixdots_mf, dotcnt, cumsum_mf, rholist, rhoampl, rhocnt, dx, dy, diffspeech, matchlist );
   fprintf(stderr, "%f s\n",toc());

   if ( dump_matchlistf ) {
     fprintf(stderr, "Writing matchlist: "); tic();
     FILE *matchlist_fileptr;
     matchlist_fileptr = fopen(dump_matchlistf, "wb");
     for (int i = 0; i < matchcnt; i++ ) {
       fwrite(&matchlist[i].xA, sizeof(int), 1, matchlist_fileptr);
       fwrite(&matchlist[i].xB, sizeof(int), 1, matchlist_fileptr);
       fwrite(&matchlist[i].yA, sizeof(int), 1, matchlist_fileptr);
       fwrite(&matchlist[i].yB, sizeof(int), 1, matchlist_fileptr);
       fwrite(&matchlist[i].rhoampl, sizeof(float), 1,matchlist_fileptr);
       fwrite(&matchlist[i].score, sizeof(float), 1,matchlist_fileptr);
     }
     fclose(matchlist_fileptr);
   }

   int lastmc = matchcnt;
   fprintf(stderr,"    Found %d matches in first pass\n",lastmc);

   fprintf(stderr, "Filtering by first-pass duration: "); tic();
   lastmc = duration_filter(matchlist, lastmc, 0.);
   fprintf(stderr, "%f s\n",toc());
   fprintf(stderr,"    %d matches left after duration filter\n",lastmc);

   // Run the second pass DTW search
   if ( twopass ) {
      fprintf(stderr, "Applying second pass: "); tic();
      if ( kws )
	 sig_kwspass(matchlist, lastmc, feats1, N1, feats2, N2, R);
      else
	 sig_secondpass(matchlist, lastmc, feats1, N1, feats2, N2, R, castthr, trimthr);
      fprintf(stderr, "%f s\n",toc());
      
      fprintf(stderr, "Filtering by second-pass duration: "); tic();
      lastmc = duration_filter(matchlist, lastmc, 0.);
      fprintf(stderr, "%f s\n",toc());
      fprintf(stderr,"    %d matches left after duration filter\n",lastmc);
   } else {
      for ( int n = 0; n < lastmc; n++ ) {
	 if (matchlist[n].xA < 0) matchlist[n].xA = 0;
	 if (matchlist[n].yA < 0) matchlist[n].yA = 0;

	 if (matchlist[n].xB >= N1) matchlist[n].xB = N1-1;
	 if (matchlist[n].yB >= N2) matchlist[n].yB = N2-1;
      }
   }

   for ( int n = 0; n < lastmc; n++ )
   {
      if ( dtwscore ) {
	 matchlist[n].score = 1.0 - 
	    dtw_score(feats1, feats2, N1, N2,  
		      matchlist[n].xA, matchlist[n].xB, 
		      matchlist[n].yA, matchlist[n].yB );
      } else {
	 int totdots;
	 int pathcnt;
	 int dotvec[20];
	 
	 score_match( feats1, feats2, N1, N2, 
		      matchlist[n].xA, matchlist[n].xB, matchlist[n].yA, 
		      matchlist[n].yB, Tscore, &pathcnt, &totdots, dotvec);
	 matchlist[n].score = logreg_score_ken( totdots, pathcnt, dotvec );
	 
      }
   }
   
   fprintf(stderr,"    Dumping %d matches\n",lastmc);
   if ( xA == -1 ) xA = 0;
   if ( yA == -1 ) yA = 0;
   dump_matchlist(matchlist, lastmc, xA, yA);

   // Free the heap
   free_signatures(feats1,N1);

   if ( diffspeech ) {
      free_signatures(feats2,N2);
   }

   FREE(dotlist);
   FREE(radixdots);
   FREE(cumsum);
   FREE(dotlist_mf);
   FREE(radixdots_mf);
   FREE(cumsum_mf);

   FREE(hough);

   FREE(rholist);
   FREE(rhoampl);
   FREE(matchlist);

   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
