//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "feat.h"
#include "util.h"
#include "dotkws.h"
#include "score_matches.h"
#include "signature.h"

#define MAXDOTS_MF 150000000
#define MAXMATCHES 1000000
#define PRIMEFACT 2.5

// PLEB parameters
int P = 4;
int B = 100;
float T = 0.5;
float Tscore = 0.25;
int D = 10;
int S = 64;

// Everything else
int dy = 3;
int dx = 25;
int qA = -1;
int qB = -1;
char *querylist = NULL;
char *queryfile = NULL;
char *indexfile = NULL;
double rhothr = 0.0;
float medthr = 0.5;
int twopass = 1;
int matchfeat = 0;
int dtwscore = 1;
int submatch = 0;
int singlequery = 0;

float castthr = 7;
int R = 10; 
float trimthr = 0.25;

void usage()
{
  fatal("usage: plebkws [-querylist <str>]\
\n\t[-queryfile <str>]\
\n\t[-indexfile <str> (REQUIRED)]\
\n\t[-P <n> (defaults to 4)]\
\n\t[-B <n> (defaults to 100)]\
\n\t[-T <n> (defaults to 0.5)]\
\n\t[-D <n> (defaults to 10)]\
\n\t[-S <n> (defaults to 64)]\
\n\t[-qA <n> (defaults to 0)]\
\n\t[-qB <n> (defaults to last frame)]\
\n\t[-medthr <n> (defaults to 0.5)]\
\n\t[-castthr <n> (defaults to 7)]\
\n\t[-trimthr <n> (defaults to 0.25)]\
\n\t[-R <n> (defaults to 10)]\
\n\t[-dx <n> (defaults to 25)]\
\n\t[-dy <n> (defaults to 3)] ]\
\n\t[-rhothr <n> (defalts to 0)]\
\n\t[-twopass <n> (defaults to 1)] ]\
\n\t[-dtwscore <n> (defaults to 1)] ]\
\n\t[-submatch <n> (defaults to 0)] ]\
\n\t[-matchfeat <n> (defaults to 0)] ]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-dy") == 0 ) dy = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-dx") == 0 ) dx = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-rhothr") == 0 ) rhothr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-querylist") == 0 ) querylist = argv[++i];
     else if ( strcmp(argv[i], "-queryfile") == 0 ) queryfile = argv[++i];
     else if ( strcmp(argv[i], "-indexfile") == 0 ) indexfile = argv[++i];
     else if ( strcmp(argv[i], "-qA") == 0 ) qA = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-qB") == 0 ) qB = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-P") == 0 ) P = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-B") == 0 ) B = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-T") == 0 ) T = atof(argv[++i]);
     else if ( strcmp(argv[i], "-D") == 0 ) D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-medthr") == 0 ) medthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-castthr") == 0 ) castthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-trimthr") == 0 ) trimthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-R") == 0 ) R = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-twopass") == 0 ) twopass = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-dtwscore") == 0 ) dtwscore = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-submatch") == 0 ) submatch = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-matchfeat") == 0 ) matchfeat = atoi(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !querylist && !queryfile ) {
     fprintf(stderr,"\nERROR: querylist OR queryfile arg is required\n");
  }

  if ( querylist && queryfile ) {
     fprintf(stderr,"\nWARNING: querylist AND queryfile specified\n");
     usage();
  }

  if ( queryfile ) {
     singlequery = 1;
  }

  if ( !indexfile ) {
     fprintf(stderr,"\nERROR: indexfile arg is required\n");
     usage();
  }

  if ( twopass < 0 || twopass > 1 )
     fatal("\nERROR: Invalid value for twopass\n");

  if ( !singlequery ) {
     fprintf(stderr, "\nRun Parameters\n--------------\n\
querylist = %s, \n\
indexfile = %s, \n\
P = %d, B = %d, T = %f, D = %d, S = %d,\n\
dx = %d, dy = %d, medthr = %f, \n\
castthr = %f, trimthr = %f, R = %d,\n\
rhothr = %f, twopass = %d,\n\
dtwscore = %d, submatch = %d\n\n",
	  querylist, indexfile, 
	  P, B, T, D, S,
	  dx, dy, medthr, castthr, trimthr, R, rhothr, 
	  twopass, dtwscore, submatch);
  } else {
     fprintf(stderr, "\nRun Parameters\n--------------\n\
queryfile = %s, qA = %d, qB = %d,\n\
indexfile = %s, \n\
P = %d, B = %d, T = %f, D = %d, S = %d,\n\
dx = %d, dy = %d, medthr = %f, \n\
castthr = %f, trimthr = %f, R = %d,\n\
rhothr = %f, twopass = %d,\n\
dtwscore = %d, submatch = %d\n\n",
	     queryfile, qA, qB, indexfile, 
	     P, B, T, D, S,
	     dx, dy, medthr, castthr, trimthr, R, rhothr, 
	     twopass, dtwscore, submatch);
  }
  
  dx *= 2;
  dy *= 2;

  SIG_NUM_BYTES = S/8;
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   // Read the index
   tic();
   assert_file_exist( indexfile );
   FILE *fptr = fopen(indexfile,"r");
   struct signature_index index;
   
   fprintf(stderr, "indexfile = %s; ", indexfile);
   fread(&index.SIG_NUM_BYTES, sizeof(int), 1, fptr);
   fprintf(stderr, "SIG_NUM_BYTES = %d, ", index.SIG_NUM_BYTES);
   fread(&index.Nfiles, sizeof(int), 1, fptr);
   fprintf(stderr, "Nfiles = %d, ", index.Nfiles);
   
   if ( index.SIG_NUM_BYTES != SIG_NUM_BYTES ) {
      fatal("\nERROR: Requested signature size does not match index");
   }
   
   index.Nchars = (int *) MALLOC( sizeof(int)*index.Nfiles );
   index.files = (char **) MALLOC( sizeof(char *)*index.Nfiles );
   for ( int i = 0; i < index.Nfiles; i++ ) {
      fread(&index.Nchars[i], sizeof(int), 1, fptr);
      index.files[i] = (char *) MALLOC( sizeof(char)*index.Nchars[i] );
      fread(index.files[i], sizeof(char), index.Nchars[i], fptr);
   }
   
   fread(&index.Ntot, sizeof(frameind), 1, fptr);
   fprintf(stderr, "Ntot = %d, ", index.Ntot);
   index.Narr = (int *) MALLOC( sizeof(int)*index.Nfiles );
   fread(index.Narr, sizeof(int), index.Nfiles, fptr);
   
   index.allfeats = 
      (struct signature *) MALLOC( sizeof(struct signature)*index.Ntot );
   for ( int i = 0; i < index.Ntot; i++ ) {
      fread(&index.allfeats[i].id, sizeof(int), 1, fptr);
      index.allfeats[i].byte_ = 
	 (byte *) MALLOC( sizeof(byte)*SIG_NUM_BYTES );
      fread(index.allfeats[i].byte_, sizeof(byte), SIG_NUM_BYTES, fptr);
      fread(&index.allfeats[i].fid, sizeof(int), 1, fptr);
   }
   
   fread(&index.P, sizeof(int), 1, fptr);
   if ( P > index.P ) {
      fatal("\nERROR: Requested permutations exceeds amount in index");
   }
   
   fprintf(stderr, "P = %d", index.P);
   index.PERMUTE = (int **) MALLOC( sizeof(int *)*P );
   index.order = 
      (frameind **) MALLOC( sizeof(frameind *)*P );
   for ( int i = 0; i < P; i++ ) {
      index.PERMUTE[i] = (int *) MALLOC( sizeof(int)*index.SIG_NUM_BYTES );
      fread(index.PERMUTE[i], sizeof(int), index.SIG_NUM_BYTES, fptr);
      index.order[i] = 
	 (frameind *) MALLOC( sizeof(frameind)*index.Ntot );
      fread(index.order[i], sizeof(frameind), index.Ntot, fptr);
   }
   
   frameind *fileranges = (frameind*)MALLOC((index.Nfiles+1)*sizeof(frameind));
   make_cumhist( fileranges, index.Narr, index.Nfiles );
   
   fclose(fptr);
   fprintf(stderr, " (Load time: %f sec)\n",toc());
   if ( P < index.P ) {
      fprintf(stderr,"WARNING: Using first %d permutations from %d total in index.\n",P,index.P);
   }
   
   // Process the queries
   int numqueries = 0;
   FILE *fptr_qlist = NULL;

   if ( singlequery ) {
      numqueries = 1;
   } else {
      numqueries = file_line_count( querylist );
      assert_file_exist( querylist );
      fptr_qlist = fopen(querylist,"r");
      queryfile = (char *) MALLOC( 255*sizeof(char) );
   }

   fprintf(stderr, "\nProcessing %d queries:\n", numqueries);

   for ( int iq = 0; iq < numqueries; iq++ ) {
      // Initialize the pleb permutations
      initialize_permute();

      // If not single query, read the query and the query signatures
      char querytype[100];
      strcpy(querytype,"UNKNOWN");
      float scorefactor = 1;

      if ( !singlequery ) {
	 fscanf(fptr_qlist,"%s %s %d %d",querytype,queryfile,&qA,&qB);

	 int c = fgetc(fptr_qlist);
	 ungetc(c, fptr_qlist);
	 
	 if ( c == '\n' )
	    fprintf(stderr, "\nQuery %d/%d: %s %s %d %d\n", iq+1, numqueries, querytype, queryfile, qA, qB);
	 else {
	    fscanf(fptr_qlist,"%f",&scorefactor);
	    fprintf(stderr, "\nQuery %d/%d: %s %s %d %d %f\n", iq+1, numqueries, querytype, queryfile, qA, qB, scorefactor);
	 }
      } else {
	    fprintf(stderr, "\nQuery %d/%d: %s %s %d %d\n", iq+1, numqueries, querytype, queryfile, qA, qB);
      }
      
      int Nq = 0;
      struct signature *queryfeats = (struct signature *)readsigs_file(queryfile, &qA, &qB, &Nq);
      
      // Compute the rotated dot plot
      unsigned long maxdots = ((unsigned long)P)*B*Nq*(2*D+1);
      fprintf(stderr,"Computing sparse dot plot (Max Dots = %ld = P*B*Nq*(D+1)) ...\n", maxdots); tic();
      
      Dot *dotlist = (Dot *) CALLOC(maxdots,sizeof(Dot));
      unsigned long dotcnt = plebindex( &index, queryfeats, Nq, P, B, T, D, dotlist );
      unsigned long cslen = dotcnt;

      fprintf(stderr, "    Total elements in thresholded sparse: %ld\n", dotcnt);
      fprintf(stderr, "Finished: %f sec.\n",toc());
      
      // Sort dots by row
      fprintf(stderr, "Applying radix sort of dotlist: "); tic();
      DotXV *radixdots = (DotXV *)MALLOC( dotcnt*sizeof(DotXV));
      frameind *cumsum = (frameind*)CALLOC(cslen,sizeof(frameind));
      frameind *cumsumind = (frameind*)MALLOC(cslen*sizeof(frameind));
      int ncumsum = radix_sorty(dotlist, dotcnt, radixdots, cumsum, cumsumind);
      fprintf(stderr, "%f s\n",toc());
      
      // Sort rows by column
      fprintf(stderr, "Applying qsort of radix bins: "); tic();
      quick_sortx(radixdots, dotcnt, cumsum, cumsumind, ncumsum);
      fprintf(stderr, "%f s\n",toc());
      
      // Remove duplicate dots in each row
      fprintf(stderr, "Removing duplicate dots from radix bins: "); tic();
      dotcnt = dot_dedup(radixdots, cumsum, cumsumind, ncumsum, T);
      fprintf(stderr, "%f s\n",toc());
      fprintf(stderr, "    Total elements after dedup: %ld\n", dotcnt);
      
      // Apply the median filter in the X direction
      fprintf(stderr, "Applying median filter to sparse matrix: "); tic();
      Dot *dotlist_mf = (Dot *) MALLOC(MAXDOTS_MF*sizeof(Dot));
      dotcnt = median_filtx(index.Ntot, Nq, radixdots, dotcnt, cumsum, cumsumind, ncumsum, dx, medthr, dotlist_mf);
      fprintf(stderr, "%f s\n",toc());
      fprintf(stderr, "    Total elements in filtered sparse: %ld\n", dotcnt);
      
      // Sort mf dots by row
      fprintf(stderr,"Applying radix sort of dotlist_mf: "); tic();
      DotXV *radixdots_mf = (DotXV *)MALLOC(dotcnt*sizeof(DotXV));
      memset(cumsum, 0, sizeof(frameind) * cslen);
      memset(cumsumind, 0, sizeof(frameind) * cslen);
      ncumsum = radix_sorty(dotlist_mf, dotcnt, radixdots_mf, cumsum, cumsumind);
      fprintf(stderr, "%f s\n",toc());
      FREE(dotlist);
      
      // Sort mf rows by column
      fprintf(stderr,"Applying qsort of radix_mf bins: "); tic();
      quick_sortx(radixdots_mf, dotcnt, cumsum, cumsumind, ncumsum);
      fprintf(stderr, "%f s\n",toc());

      // Compute the Hough transform
      fprintf(stderr,"Computing hough transform: "); tic();
      float *hough = (float*)MALLOC(dotcnt*(2*dy+1)*sizeof(float));
      frameind *houghind = (frameind*)MALLOC(dotcnt*(2*dy+1)*sizeof(frameind));
      int nhough = hough_gaussy(index.Ntot+Nq, dotcnt, cumsum, cumsumind, ncumsum, dy, hough, houghind);
      fprintf(stderr, "%f s\n",toc());
      
      // Compute rho list
      fprintf(stderr, "Computing rholist: "); tic();
      int rhocnt = count_rholist(hough,houghind,nhough,rhothr);
      frameind *rholist = (frameind *)MALLOC(rhocnt*sizeof(frameind));
      float *rhoampl = (float *)MALLOC(rhocnt*sizeof(float));
      rhocnt = compute_rholist(hough,houghind,nhough,rhothr,rholist,rhoampl);
      fprintf(stderr, "%f s\n",toc());

      // Compute the matchlist
      fprintf(stderr, "Computing matchlist: "); tic();
      Match * matchlist = (Match*) MALLOC(MAXMATCHES*sizeof(Match));
      int matchcnt = compute_matchlist_sparse( index.Ntot, Nq, radixdots_mf, dotcnt, 
					       cumsum, cumsumind, ncumsum,
					       rholist, rhoampl, rhocnt, dx, dy, matchlist );
      fprintf(stderr, "%f s\n",toc());
      
      int lastmc = matchcnt;
      fprintf(stderr,"    Found %d matches in first pass\n",lastmc);
      
      fprintf(stderr, "Filtering by first-pass duration: "); tic();
      lastmc = duration_filter(matchlist, lastmc, 0.);
      fprintf(stderr, "%f s\n",toc());
      fprintf(stderr,"    %d matches left after duration filter\n",lastmc);
      
      
      // Run the second pass DTW search
      if ( twopass ) {
	 if ( submatch ) {
	    fprintf(stderr, "Applying submatch second pass: "); tic();
	    sig_secondpass(matchlist, lastmc, index.allfeats, (int) index.Ntot, queryfeats, Nq, R, castthr, trimthr);
	    fprintf(stderr, "%f s\n",toc());
	 } else {
	    fprintf(stderr, "Applying kws second pass: "); tic();
	    sig_kwspass(matchlist, lastmc, index.allfeats, index.Ntot, queryfeats, Nq, R);
	    fprintf(stderr, "%f s\n",toc());
	 }
	 
	 fprintf(stderr, "Filtering by second-pass duration: "); tic();
	 lastmc = duration_filter(matchlist, lastmc, 0.);
	 fprintf(stderr, "%f s\n",toc());
	 fprintf(stderr,"    %d matches left after duration filter\n",lastmc);      
      }
      
      // Score the matches
      fprintf(stderr, "Computing scores: "); tic();
      for ( int n = 0; n < lastmc; n++ )
      {
	 matchlist[n].score = 1.0 - 
	    dtw_score(index.allfeats, queryfeats, index.Ntot, Nq,  
		      matchlist[n].xA, matchlist[n].xB, 
		      matchlist[n].yA, matchlist[n].yB );
	 if ( scorefactor > 0 )
	    matchlist[n].rhoampl = scorefactor;
	 if ( !dtwscore ) {
	    int totdots;
	    int pathcnt;
	    int dotvec[20];
	    score_match( index.allfeats, queryfeats, index.Ntot, Nq, 
			 matchlist[n].xA, matchlist[n].xB, matchlist[n].yA, 
			 matchlist[n].yB, Tscore, &pathcnt, &totdots, dotvec);
	    matchlist[n].score = logreg_score_ken( totdots, pathcnt, dotvec );
	 }
      }
      fprintf(stderr, "%f s\n",toc());
      
      fprintf(stderr,"    Dumping %d matches\n",lastmc);
      dump_matchlist(index.files, matchlist, lastmc, fileranges, 
		     qA, queryfile, querytype);

      // Free the query-specific heap
      free_signatures(queryfeats,Nq);
      
      FREE(radixdots);
      FREE(cumsum);
      FREE(cumsumind);
      FREE(dotlist_mf);
      FREE(radixdots_mf);
      
      FREE(hough);
      FREE(houghind);
      
      FREE(rholist);
      FREE(rhoampl);
      FREE(matchlist);
   }

   if ( !singlequery ) {
      fclose(fptr_qlist);
      FREE(queryfile);
   }


   fprintf(stderr, "Freeing index: "); tic();
   // Free the index
   FREE(index.Nchars);
   for ( int i = 0; i < index.Nfiles; i++ ) {
      FREE(index.files[i]);
   }
   FREE(index.files);
   
   FREE(index.Narr);
   
   for ( int i = 0; i < index.Ntot; i++ ) {
      FREE(index.allfeats[i].byte_);
   }
   FREE(index.allfeats);
   
   for ( int i = 0; i < P; i++ ) {
      FREE(index.PERMUTE[i]);
      FREE(index.order[i]);
   }
   FREE(index.PERMUTE);
   FREE(index.order);
   FREE(fileranges);

   fprintf(stderr, "%f s\n",toc());

   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
