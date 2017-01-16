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

// -----------------------------------
// Index structure
// -----------------------------------
// SIG_NUM_BYTES: 1 int
// Nfiles: 1 int
// for i = 1 to Nfiles:
//    nchars[i]: 1 int (includes \0)
//    files[i]: nchars[i] char
// Ntot: 1 frameind
// Narr: Nfiles int
// for i = 1 to Nfiles:
//    for j = 1 to Narr[i]:
//       feats[i][j]->id: 1 int
//       feats[i][j]->byte_: SIG_NUM_BYTES byte (byte is unsigned char)
//       feats[i][j]->fid: 1 int
// P: 1 int
// for i = 1 to P
//    PERMUTE_: SIG_NUM_BYTES int
//    for j = 1 to Ntot
//       allfeats[j]->indexid: 1 frameind
// -----------------------------------

#define MAXCHAR 256

// PLEB parameters
int P = 4;
int S = 32;

// Everything else
int maxframes = 0;
char *filelist = NULL;
char *indexfile = NULL;

void usage()
{
  fatal("usage: build_index [-filelist <str> (REQUIRED)]\
\n\t[-indexfile <str> (REQUIRED)]\
\n\t[-P <n> (defaults to 4)]\
\n\t[-S <n> (defaults to 32)]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if( strcmp(argv[i], "-filelist") == 0 ) filelist = argv[++i];
     else if ( strcmp(argv[i], "-indexfile") == 0 ) indexfile = argv[++i];
     else if ( strcmp(argv[i], "-P") == 0 ) P = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-S") == 0 ) S = atoi(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !filelist ) {
     usage();
     fatal("\nERROR: filelist arg is required");
  }

  if ( !indexfile ) {
     usage();
     fatal("\nERROR: indexfile arg is required");
  }

  fprintf(stderr, "\nRun Parameters\n--------------\n \
filelist = %s, \n \
indexfile = %s, \n \
P = %d, S = %d,\n\n ",
	  filelist, indexfile, P, S);

  SIG_NUM_BYTES = S/8;
}

void randperm(struct signature **allfeats, frameind Ntot) 
{
   struct signature *save;

   for ( frameind i = 0; i < Ntot-1; i++ ) {
      frameind j = Ntot*rand()/RAND_MAX;
      save = allfeats[i];
      allfeats[i] = allfeats[j];
      allfeats[j] = save;
   }
}


int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   // Open the output index file
   FILE *fout = fopen(indexfile, "w");

   // Read the filelist
   fprintf(stderr,"Processing file list: \n"); tic();

   int Nfiles = file_line_count( filelist );
   fprintf(stderr,"Total files to index: %d\n",  Nfiles);
   char **files = (char **) MALLOC( sizeof(char*)*Nfiles );

   assert_file_exist( filelist );
   FILE *fptr = fopen(filelist, "r");
   for ( int i = 0; i < Nfiles; i++ )
      files[i] = (char *) MALLOC( MAXCHAR*sizeof(char) );

   int wcnt = 0;
   while ( wcnt < Nfiles && fscanf(fptr, "%s", files[wcnt]) != EOF ) {
      wcnt++;
   }
   fclose(fptr);
   fprintf(stderr, "Finished. %f s\n",toc());

   // Write number of bytes per signature to file
   fwrite( &SIG_NUM_BYTES, sizeof(int), 1, fout );

   // Write input file info to index
   fprintf(stderr,"Dumping filenames to index: "); tic();
   fwrite( &Nfiles, sizeof(int), 1, fout );
   for ( int i = 0; i < Nfiles; i++ ) {
      int nchars = strlen(files[i])+1;
      fwrite( &nchars, sizeof(int), 1, fout );
      fwrite( files[i], sizeof(char), strlen(files[i])+1, fout );
   }
   fprintf(stderr, "%f s\n",toc());

   // Read the signature files
   fprintf(stderr,"Reading the signature files: \n"); tic();

   frameind Ntot = 0;
   int *Narr = (int *) MALLOC( sizeof(int)*Nfiles );
   struct signature **feats = 
      (struct signature **) MALLOC( sizeof(struct signature *)*Nfiles );
   
   for ( int i = 0; i < Nfiles; i++ ) {
      int xA = -1, xB = -1;
      feats[i] = 
	 (struct signature *)readsigs_file(files[i], &xA, &xB, &Narr[i]);

      for ( int j = 0; j < Narr[i]; j++ ) {
	 feats[i][j].fid = i;
	 feats[i][j].indexid = Ntot+j;
      }
      Ntot += Narr[i];
      fprintf(stderr, "file = %s; N = %d frames\n", files[i], Narr[i]);
   }
   fprintf(stderr, "Finished. %f s\n",toc());

   fprintf(stderr, "Total signatures to index: %d\n",Ntot);
      
   // Write input file lengths (in frames) and signatures to index
   fprintf(stderr,"Dumping file lengths and signatures to index: "); tic();
   fwrite( &Ntot, sizeof(frameind), 1, fout );
   fwrite( Narr, sizeof(int), Nfiles, fout );

   for ( int i = 0; i < Nfiles; i++ ) {
      for ( int j = 0; j < Narr[i]; j++ ) {
	 struct signature *sptr = &feats[i][j];
	 fwrite( &sptr->id, sizeof(int), 1, fout );
	 fwrite( sptr->byte_, sizeof(byte), SIG_NUM_BYTES, fout );
	 fwrite( &sptr->fid, sizeof(int), 1, fout );
      }
   }   
   fprintf(stderr, "%f s\n",toc());

   fprintf(stderr,"Constructing the pointer array: "); tic();
   struct signature **allfeats = 
      (struct signature **) MALLOC( sizeof(struct signature *)*Ntot );
   int pos = 0;
   for ( int i = 0; i < Nfiles; i++ ) {
      for ( int j = 0; j < Narr[i]; j++ ) {
	 allfeats[pos++] = &feats[i][j];
      }
   }
   fprintf(stderr, "%f s\n",toc());

   // Initialize the pleb permutations
   initialize_permute();

   // Build the index and write to file on the fly
   fprintf(stderr,"Constructing and writing the sorted lists: \n"); tic();

   fwrite( &P, sizeof(int), 1, fout ); // Write P to file
   for (int i = 0; i < P; i++) {
      //randperm(allfeats, Ntot);

      // Write ith permutation to file
      fwrite( PERMUTE_, sizeof(int), SIG_NUM_BYTES, fout ); 
      
      // Sort the pointer array
      qsort(allfeats, Ntot, sizeof(struct signature*), 
	    signature_ptr_greater);

      for ( int j = 0; j < Ntot; j++ ) {
	 fwrite( &allfeats[j]->indexid, sizeof(frameind), 1, fout );
      }
      permute();
   }
   fprintf(stderr, "Finished. %f s\n",toc());

   // Close the index file
   fclose(fout);

   // Free the heap
   FREE(PERMUTE_);

   FREE(allfeats);

   for ( int i = 0; i < Nfiles; i++ ) free_signatures(feats[i],Narr[i]);
   FREE(feats);
   FREE(Narr);

   for ( int i = 0; i < Nfiles; i++ ) FREE(files[i]);
   FREE(files);


   int mc = get_malloc_count();
   if(mc != 0) fprintf(stderr,"WARNING: %d malloc'd items not free'd\n", mc);

   return 0;
}
