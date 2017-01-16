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

#define NDIV 15
#define MAXCOST 1e10
#define MAXFRAMES 800
#define MAXCHAR 30
#define N_3D 104

char *wordlist = NULL;
char *filedir = NULL;
char *filedir2 = NULL;
float simmx[MAXFRAMES*MAXFRAMES];
float costmx[MAXFRAMES*MAXFRAMES];
int pathmx[MAXFRAMES*MAXFRAMES];
float costmx3D[N_3D*MAXFRAMES*MAXFRAMES];
int pathmx3D[N_3D*MAXFRAMES*MAXFRAMES];
int aacntmx[MAXFRAMES*MAXFRAMES];
int subset = 0;
int printinds = 0;
int maxrun = INT_MAX;
int token = 0;
int euclidean = 0;
int kldiv = 0;
int sym = 0;
int normalize = 1;
int dtw_dur_norm = 1;
int mit = 0;
int usedtw3D = 0;
int segnorm = 0;
float distthr = 1e30;
float delta = 1.0;
float *LOOKUP_TABLE = NULL;
int nbits_log = 14;
float spvec[N_3D];

void usage()
{
  printf("usage: wordsim -wordlist <str> (REQUIRED)]\
\n\t[-filedir <str> (REQUIRED)]\
\n\t[-filedir2 <str>]\
\n\t[-subset <n> (defaults to no splitting)]\
\n\t[-maxrun <n> (defaults to INT_MAX)]\
\n\t[-distthr <n> (defaults to FLOAT_MAX)]\
\n\t[-delta <n> (defaults to 1.0)]\
\n\t[-token <n> (defaults to 0)]\
\n\t[-euclidean <n> (defaults to 0=cosine)]\
\n\t[-kldiv <n> (defaults to 0=cosine)]\
\n\t[-sym <n> (defaults to 0=not symmetrized)]\
\n\t[-normalize <n> (defaults to 1, unless kldiv=1)]\
\n\t[-mit <n> (defaults to 0)]\
\n\t[-3D <n> (defaults to 0)]\
\n\t[-segnorm <n> (defaults to 0)]\
\n\t[-dtw_dur_norm <n> (defaults to 1)]\
\n\t[-printinds <n> (defaults to 0)]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if ( strcmp(argv[i], "-wordlist") == 0 ) wordlist = argv[++i];
     else if ( strcmp(argv[i], "-filedir") == 0 ) filedir = argv[++i];
     else if ( strcmp(argv[i], "-filedir2") == 0 ) filedir2 = argv[++i];
     else if ( strcmp(argv[i], "-subset") == 0 ) subset = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-maxrun") == 0 ) maxrun = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-distthr") == 0 ) distthr = atof(argv[++i]);
     else if ( strcmp(argv[i], "-delta") == 0 ) delta = atof(argv[++i]);
     else if ( strcmp(argv[i], "-token") == 0 ) token = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-euclidean") == 0 ) euclidean = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-kldiv") == 0 ) kldiv = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-sym") == 0 ) sym = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-normalize") == 0 ) normalize = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-mit") == 0 ) mit = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-3D") == 0 ) usedtw3D = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-segnorm") == 0 ) segnorm = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-dtw_dur_norm") == 0 ) dtw_dur_norm = atoi(argv[++i]);
     else if ( strcmp(argv[i], "-printinds") == 0 ) printinds = atoi(argv[++i]);
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !wordlist ) {
     usage();
     fatal("\nERROR: wordlist arg is required");
  }

  if ( !filedir ) {
     usage();
     fatal("\nERROR: filedir arg is required");
  }

  if ( subset > NDIV*NDIV )
     fatal("\nERROR: Maximum subset is NDIV*NDIV");

  if ( mit ) {
     normalize = 0;
     euclidean = 0;
     sym = 0;
     kldiv = 0;
  }     
}

float dtw3D( float *X1, int N1, float *X2, int N2, int D, int dump )
{  
   for ( int i = 0; i < N1; i++ ) {
      for ( int j = 0; j < N2; j++ ) {
	 simmx[i*N2+j] = 0;
	 for ( int d = 0; d < D; d++ ) {
	    simmx[i*N2+j] += X1[i*D+d]*X2[j*D+d];
	 }
	 simmx[i*N2+j] = (1-simmx[i*N2+j])/2;
	 if ( simmx[i*N2+j] > delta )
	    simmx[i*N2+j] = 1.0e6;
      } 
   }

   for ( int i = 0; i < N1; i++ ) {
      for ( int j = 0; j < N2; j++ ) {
	 for ( int k = 0; k < N_3D; k++ ) {
	    costmx3D[k*N1*N2+i*N2+j] = 100;
	 }
      }
   }

   // Initialize edges of the cube
   costmx3D[0] = spvec[0]*simmx[0];

   for ( int i = 1; i < N1; i++ ) {
      costmx3D[i*N2] = costmx3D[(i-1)*N2] + spvec[0]*simmx[i*N2];
   }

   for ( int j = 1; j < N2; j++ ) {
      costmx3D[j] = costmx3D[j-1] + spvec[0]*simmx[j];
   }

   for ( int k = 1; k < N_3D; k++ ) {
      costmx3D[k*N1*N2] = 100; // Illegal to increment k without incrementing i or j
   }

   // Initialize i-j size of the cube
   for ( int i = 1; i < N1; i++ ) {
      for ( int j = 1; j < N2; j++ ) {
	 float dcost = costmx3D[(i-1)*N2+(j-1)] + spvec[0]*simmx[i*N2+j];
	 float vcost = costmx3D[(i-1)*N2+j] + spvec[0]*simmx[i*N2+j];
	 float hcost = costmx3D[i*N2+(j-1)] + spvec[0]*simmx[i*N2+j];

	 costmx3D[i*N2+j] = MIN(dcost, MIN(hcost, vcost));

	 if ( dcost <= vcost && dcost <= hcost )
	    pathmx3D[i*N2+j] = 4;
	 else if ( hcost <= vcost )
	    pathmx3D[i*N2+j] = 5;
	 else
	    pathmx3D[i*N2+j] = 6;
      }
   }

   // Initialize i-k size of the cube
   for ( int k = 1; k < N_3D; k++ ) {
      for ( int i = 1; i < N1; i++ ) {
	 float dcost = costmx3D[(k-1)*N1*N2+(i-1)*N2] + spvec[k]*simmx[i*N2];
	 float hcost = costmx3D[k*N1*N2+(i-1)*N2] + spvec[k]*simmx[i*N2];

	 costmx3D[k*N1*N2+i*N2] = MIN(hcost, dcost);

	 if ( dcost <= hcost )
	    pathmx3D[k*N1*N2+i*N2] = 3;
	 else
	    pathmx3D[k*N1*N2+i*N2] = 6;
      }
   }

   // Initialize j-k size of the cube
   for ( int k = 1; k < N_3D; k++ ) {
      for ( int j = 1; j < N2; j++ ) {
	 float dcost = costmx3D[(k-1)*N1*N2+(j-1)] + spvec[k]*simmx[j];
	 float hcost = costmx3D[k*N1*N2+(j-1)] + spvec[k]*simmx[j];

	 costmx3D[k*N1*N2+j] = MIN(dcost, hcost);

	 if ( dcost <= hcost )
	    pathmx3D[k*N1*N2+j] = 2;
	 else
	    pathmx3D[k*N1*N2+j] = 5;
      }
   }

     
   for ( int k = 1; k < N_3D; k++ ) {
      for ( int i = 1; i < N1; i++ ) {
	 for ( int j = 1; j < N2; j++ ) {
	    float dcostA = costmx3D[(k-1)*N1*N2+(i-1)*N2+(j-1)] + spvec[k]*simmx[i*N2+j];
	    float vcostA = costmx3D[(k-1)*N1*N2+(i-1)*N2+j] + spvec[k]*simmx[i*N2+j];
	    float hcostA = costmx3D[(k-1)*N1*N2+i*N2+(j-1)] + spvec[k]*simmx[i*N2+j];

	    float dcostB = costmx3D[k*N1*N2+(i-1)*N2+(j-1)] + spvec[k]*simmx[i*N2+j];  
	    float vcostB = costmx3D[k*N1*N2+(i-1)*N2+j] + spvec[k]*simmx[i*N2+j];
	    float hcostB = costmx3D[k*N1*N2+i*N2+(j-1)] + spvec[k]*simmx[i*N2+j];

	    costmx3D[k*N1*N2+i*N2+j] = MIN( MIN( dcostA, MIN(vcostA, hcostA) ), 
					    MIN( dcostB, MIN(vcostB, hcostB) ) );

	    if ( MIN( dcostA, MIN(vcostA, hcostA)) <= MIN( dcostB, MIN(vcostB, hcostB) ) ) {
	       if ( dcostA <= vcostA && dcostA <= hcostA )
		  pathmx3D[k*N1*N2+i*N2+j] = 1;
	       else if ( hcostA <= vcostA )
		  pathmx3D[k*N1*N2+i*N2+j] = 2;
	       else
		  pathmx3D[k*N1*N2+i*N2+j] = 3;
	    } else {
	       if ( dcostB <= vcostB && dcostB <= hcostB )
		  pathmx3D[k*N1*N2+i*N2+j] = 4;
	       else if ( hcostB <= vcostB )
		  pathmx3D[k*N1*N2+i*N2+j] = 5;
	       else
		  pathmx3D[k*N1*N2+i*N2+j] = 6;	       
	    }
	 }
      }
   } 

   int curri = N1-1;
   int currj = N2-1;
   int currk = N_3D-1;
   float sum1=0;
   float sum2=0;
   while ( curri && currj && currk ) {
      int p = pathmx3D[currk*N1*N2+curri*N2+currj];
      if ( p == 1 ) {
	 currk--; curri--; currj--;
      } else if ( p == 2 ) {
	 currk--; currj--;
      } else if ( p == 3 ) {
	 currk--; curri--; 
      } else if ( p == 4 ) {
	 curri--; currj--;
      } else if ( p == 5 ) {
	 currj--;
      } else if ( p == 6 ) {
	 curri--;
      }
      if ( dump ) {
	 fprintf(stderr,"%d %d %d\n",curri, currj, (int) spvec[currk]);
	 exit(1);
      }
      if ( curri > N1/2 )
	 sum2 += spvec[currk]; 
      else
	 sum1 += spvec[currk];
   }
   sum1+=1e-3; sum2+=1e-3;
   float p = sum1/(sum1+sum2);
   float entcorr = -p*log2(p) - (1-p)*log2(1-p);

   return costmx3D[N_3D*N1*N2-1]/((N_3D-1)/2)/entcorr;
}

float dtw( float *X1, int N1, float *X2, int N2, int D, float *fact1, float *fact2 )
{   
   if ( euclidean ) {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    simmx[i*N2+j] = 0;
	    for ( int d = 0; d < D; d++ ) {
	       simmx[i*N2+j] += powf(X1[i*D+d]-X2[j*D+d],2);
	    }
	    simmx[i*N2+j] = sqrt(simmx[i*N2+j]);
	 }
      } 
   } else if ( kldiv ) {
      if ( sym ) {
	 for ( int i = 0; i < N1; i++ ) {
	    for ( int j = 0; j < N2; j++ ) {
	       simmx[i*N2+j] = 0;
	       for ( int d = 0; d < D; d++ ) {
		  simmx[i*N2+j] += 
		     0.5*(X1[i*D+d]-X2[j*D+d])*icsi_log(X1[i*D+d]/X2[j*D+d],
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
   } else if ( mit ) {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    simmx[i*N2+j] = 0;
	    for ( int d = 0; d < D; d++ ) {
	       simmx[i*N2+j] += X1[i*D+d]*X2[j*D+d];
	    }
	    simmx[i*N2+j] = -icsi_log(simmx[i*N2+j],
				      LOOKUP_TABLE,nbits_log);
	 }
      } 
   } else {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    simmx[i*N2+j] = 0;
	    for ( int d = 0; d < D; d++ ) {
	       simmx[i*N2+j] += X1[i*D+d]*X2[j*D+d];
	    }
	    simmx[i*N2+j] = (1-simmx[i*N2+j])/2;
	    if ( simmx[i*N2+j] > delta )
	       simmx[i*N2+j] = 1.0e6;	    
	 }
      } 
   }

   N1++;
   N2++;
   for ( int i = 0; i < N1; i++ ) {
      for ( int j = 0; j < N2; j++ ) {
	 aacntmx[i*N2+j] = 0;
	 costmx[i*N2+j] = 0;
	 pathmx[i*N2+j] = 0;
      }
   }

   for ( int i = 0; i < N1; i++ ) {
      aacntmx[i*N2] = i;
      if ( i == 0 )
	 costmx[i*N2] = 0;
      else {
	 costmx[i*N2] = MAXCOST;
	 pathmx[i*N2] = 0;
      }
   }

   for ( int j = 0; j < N2; j++ ) {
      aacntmx[j] = j;
      if ( j == 0 )
	 costmx[j] = 0;      
      else {
	 costmx[j] = MAXCOST;
	 pathmx[j] = 0;
      }      
   }
      
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

	 if ( vcost < dcost || hcost < dcost ) {
	    if ( vcost < hcost ) {
	       aacntmx[i*N2+j] = aacntmx[(i-1)*N2+j] + 1;
	    } else {
	       aacntmx[i*N2+j] = aacntmx[i*N2+(j-1)] + 1;
	    }
	 } 

	 if ( dcost <= vcost && dcost <= hcost )
	    pathmx[i*N2+j] = 1;
	 else if ( hcost <= vcost )
	    pathmx[i*N2+j] = 2;
	 else
	    pathmx[i*N2+j] = 3;
      }
   } 

   if ( 0 ) {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    printf( "%d ", pathmx[i*N2+j] );
	 }
	 printf("\n");
      } 
      fatal("");
   }

   if ( 0 ) {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    printf( "%f ", costmx[i*N2+j] );
	 }
	 printf("\n");
      } 
      fatal("");
   }

   if ( segnorm ) {
      int curri = N1-1;
      int currj = N2-1;
      float num = 0;
      float den = 0;
      float pathlen = 0;
      while ( curri && currj ) {
	 pathlen++;
	 num += simmx[(curri-1)*(N2-1)+(currj-1)]*sqrt(powf(fact1[curri-1],2)+powf(fact2[currj-1],2)); 
	 den += sqrt(powf(fact1[curri-1],2)+powf(fact2[currj-1],2)); 

	 int p = pathmx[curri*N2+currj];
	 if ( p == 1 ) {
	    curri--; currj--;
	 } else if ( p == 2 ) {
	    currj--;
	 } else if ( p == 3 ) {
	    curri--; 
	 } else {
	    fatal("Invalid pathmx element");
	 }
      }
      return (num/den)*(pathlen/(N1+N2-2));
   }

   if ( dtw_dur_norm )      
      return costmx[N1*N2-1]/(N1+N2-2);
   else
      return costmx[N1*N2-1];
}

float stredit( int *X1, int N1, int *X2, int N2 )
{   
   N1++;
   N2++;
   for ( int i = 0; i < N1; i++ ) {
      for ( int j = 0; j < N2; j++ ) {
	 aacntmx[i*N2+j] = 0;
	 costmx[i*N2+j] = 0;
      }
   }

   for ( int i = 0; i < N1; i++ ) {
      aacntmx[i*N2] = i;
      if ( i == 0 )
	 costmx[i*N2] = 0;
      else 
	 costmx[i*N2] = MAXCOST;
   }

   for ( int j = 0; j < N2; j++ ) {
      aacntmx[j] = j;
      if ( j == 0 )
	 costmx[j] = 0;      
      else
	 costmx[j] = MAXCOST;
   }
      
   for ( int i = 1; i < N1; i++ ) {
      for ( int j = 1; j < N2; j++ ) {
	 float dcost = costmx[(i-1)*N2+(j-1)] + (X1[i] != X2[j]);
	 float vcost = costmx[(i-1)*N2+j] + (X1[i] != X2[j]);
	 float hcost = costmx[i*N2+(j-1)] + (X1[i] != X2[j]);

	 if ( aacntmx[(i-1)*N2+j] + 1 >= maxrun )
	    vcost = MAXCOST;

	 if ( aacntmx[i*N2+(j-1)] + 1 >= maxrun )
	    hcost = MAXCOST;

	 costmx[i*N2+j] = MIN( dcost, MIN(vcost, hcost) );

	 if ( vcost < dcost || hcost < dcost ) {
	    if ( vcost < hcost ) {
	       aacntmx[i*N2+j] = aacntmx[(i-1)*N2+j] + 1;
	    } else {
	       aacntmx[i*N2+j] = aacntmx[i*N2+(j-1)] + 1;
	    }
	 } 
      }
   } 

   if ( 0 ) {
      for ( int i = 0; i < N1; i++ ) {
	 for ( int j = 0; j < N2; j++ ) {
	    printf( "%f ", costmx[i*N2+j] );
	 }
	 printf("\n");
      } 
      fatal("");
   }
   
   return costmx[N1*N2-1]/(N1+N2-2);
}

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   //fprintf(stderr,"spvec: ");
   for ( int k = 0; k < N_3D; k++ ) {
      spvec[k] = ((k+1) % 5) == 0;
      //fprintf(stderr,"%d ",  (int)spvec[k]);
      //spvec[k] = k % 2;
   }
   //fprintf(stderr,"\n");

   LOOKUP_TABLE = (float*) MALLOC(((int) pow(2,nbits_log))*sizeof(float));
   fill_icsi_log_table(nbits_log,LOOKUP_TABLE); 

   int Nwords = file_line_count( wordlist );
   fprintf(stderr,"Total word instances: %d\n",  Nwords);

   int rA = 0;
   int rB = Nwords;
   int cA = 0;
   int cB = Nwords;

   if ( subset > 0 ) {
      int n_per_div = ceil(((float) Nwords)/NDIV);
      int i = (subset-1) / NDIV;
      int j = (subset-1) % NDIV;
      rA = n_per_div*i;
      rB = MIN(Nwords,n_per_div*(i+1));
      cA = n_per_div*j;
      cB = MIN(Nwords,n_per_div*(j+1));
      fprintf(stderr,"Evaluating subset %d: (%d,%d) x (%d,%d)\n",
	      subset,rA,rB-1,cA,cB-1);
   }

   // Read in the wordlist
   fprintf(stderr,"Reading word example list: "); tic();

   int *spkr = (int *) MALLOC( Nwords*sizeof(int) );
   char **words = (char **) MALLOC( Nwords*sizeof(char *) );
   for ( int i = 0; i < Nwords; i++ )
      words[i] = (char *) MALLOC( MAXCHAR*sizeof(char) );
   FILE *fptr = fopen(wordlist, "r");
   int wcnt = 0;
   int int_ign;
   char str_ign[10];

   while ( wcnt < Nwords && 
	   fscanf(fptr, "%s%d%s%d%d%d",
		  words[wcnt], &int_ign, str_ign,
		  &spkr[wcnt], &int_ign, &int_ign ) != EOF ) {
      wcnt++;
   }
   fclose(fptr);
   fprintf(stderr, "%f s\n",toc());

   // Either process word examples as feature vectors or tokens
   if ( !token ) {
      // Read in the example features
      fprintf(stderr,"Reading word example features: "); tic();
      int D;
      int *N = (int *) MALLOC( Nwords*sizeof(int *) );
      float **examples = (float **) MALLOC( Nwords*sizeof(float *) );
      memset(examples,0,Nwords*sizeof(float *));

      float **snfacts = (float **) MALLOC( Nwords*sizeof(float *) );
      memset(snfacts,0,Nwords*sizeof(float *));

      float **examples2 = NULL;
      int *N2 = NULL;
      if ( filedir2 ) {
	 N2 = (int *) MALLOC( Nwords*sizeof(int *) );
	 examples2 = (float **) MALLOC( Nwords*sizeof(float *) );
	 memset(examples2,0,Nwords*sizeof(float *));
      }

      for ( int i = 0; i < Nwords; i++ ) {
	 if ( !( i >= rA && i < rB ) && !( i >= cA && i < cB ) )
	    continue;
	 
	 char featbase[20];
	 char featpref[20];
	 sprintf( featbase, "%06d.binary", i+1 );
	 sprintf( featpref, "%06d.binary", i+1 );
	 featpref[3] = '\0';
	 
	 char featfile[200];
	 sprintf( featfile, "%s/%s/%s", filedir, featpref, featbase );
	 
	 examples[i] = readfeats_file2( featfile, -1, -1, &N[i], &D );

	 snfacts[i] = (float *) MALLOC( N[i]*sizeof(float) );
	 for ( int j = 0; j < N[i]; j++ ) {
	    snfacts[i][j] = 0;
	    for ( int d = D/3; d < 2*D/3; d++ ) {
	       snfacts[i][j] += examples[i][j*D+d]*examples[i][j*D+d];
	    }
	    snfacts[i][j] = powf(snfacts[i][j],0.5);
	 }

	 char featfile2[200];
	 if ( examples2 ) {
	    sprintf( featfile2, "%s/%s/%s", filedir2, featpref, featbase );
	    examples2[i] = readfeats_file2( featfile2, -1, -1, &N2[i], &D );
	 }

	 if ( !kldiv && normalize ) {
	    for ( int j = 0; j < N[i]; j++ ) {
	       float norm = 0;
	       for ( int d = 0; d < D; d++ ) {
		  if ( fabs(examples[i][j*D+d]) < 1e-10 ) examples[i][j*D+d] = 0.0;
		  norm += examples[i][j*D+d]*examples[i][j*D+d];
	       }
	       
	       for ( int d = 0; d < D; d++ )
		  examples[i][j*D+d] /= sqrt(norm);
	    }
	    
	    if ( examples2 ) {
	       for ( int j = 0; j < N2[i]; j++ ) {
		  float norm = 0;
		  for ( int d = 0; d < D; d++ ) {
		     if ( fabs(examples2[i][j*D+d]) < 1e-10 ) examples2[i][j*D+d] = 0.0;
		     norm += examples2[i][j*D+d]*examples2[i][j*D+d];
		  }
		  
		  for ( int d = 0; d < D; d++ )
		     examples2[i][j*D+d] /= sqrt(norm);
	       }
	    }	    
	 }

	 if ( N[i] > MAXFRAMES-1 ) {
	    fprintf(stderr,
		    "\nWARNING: %s (%d frames) exceeds MAXFRAMES=%d",
		    featfile, N[i], MAXFRAMES); 
	    N[i] = MAXFRAMES-1;
	 }
	 
	 if ( N2 && N2[i] > MAXFRAMES-1 ) {
	    fprintf(stderr,
		    "\nWARNING: %s (%d frames) exceeds MAXFRAMES=%d",
		    featfile, N2[i], MAXFRAMES); 
	    N2[i] = MAXFRAMES-1;
	 }
      }
      fprintf(stderr, "%f s\n",toc());
      fprintf(stderr,"Feature dimension: %d\n", D); tic();
      
      // Compute the example similarities
      fprintf(stderr,"Computing word example similarities: "); tic();
      
      int pctstep = (rB-rA)/10;
      for ( int i = rA; i < rB; i++ ) {
	 if ( ((i-rA+1) % pctstep) == 0 )
	    fprintf(stderr,".");

	 for ( int j = cA; j < cB; j++ ) {
	    if ( j <= i ) 
	       continue;

	    float *fact1 = snfacts[i];
	    float *fact2 = snfacts[j];

	    float dtwdist;

	    if ( NULL == examples2 ) {
	       if ( usedtw3D )
		  dtwdist = dtw3D( examples[i], N[i], examples[j], N[j], D, 0 );
	       else 
		  dtwdist = dtw( examples[i], N[i], examples[j], N[j], D, fact1, fact2 );
	    } else {
	       dtwdist = dtw( examples[i], N[i], examples2[j], N2[j], D, fact1, fact2 );
	    }
	    
	    if ( dtwdist > 1000 ) 
	       dtwdist = 1+(((float)rand())/RAND_MAX)*0.001;

	    if ( dtwdist > distthr )
	       continue;
	    
	    int sw = !strcmp( words[i], words[j] );
	    int sp = spkr[i] == spkr[j];
	    if ( printinds )
	       printf("%f %d %d %d %d\n", dtwdist, sw, sp, i+1, j+1);
	    else
	       printf("%f %d %d\n", dtwdist, sw, sp);
	 }
      }
      fprintf(stderr, " %f s\n",toc());
      FREE(N);
      if (N2) FREE(N2);

      for ( int i = 0; i < Nwords; i++ ) {
	 if ( examples[i] )
	    FREE(examples[i]);
	 if ( snfacts[i] )
	    FREE(snfacts[i]);
	 if ( examples2 && examples2[i] )
	    FREE(examples2[i]);	    
      }
      FREE(examples);
      if ( examples2 )
	 FREE(examples2);
      FREE(snfacts);
   } else { // This is the token=1 case 
      // Read in the example tokens
      fprintf(stderr,"Reading word example tokens: "); tic();
      int *N = (int *) MALLOC( Nwords*sizeof(int *) );
      int **examples = (int **) MALLOC( Nwords*sizeof(int *) );
      memset(examples,0,Nwords*sizeof(int *));

      for ( int i = 0; i < Nwords; i++ ) {
	 if ( !( i >= rA && i < rB ) && !( i >= cA && i < cB ) )
	    continue;
	 
	 char featbase[20];
	 char featpref[20];
	 sprintf( featbase, "%06d.binary", i+1 );
	 sprintf( featpref, "%06d.binary", i+1 );
	 featpref[3] = '\0';
	 
	 char featfile[100];
	 sprintf( featfile, "%s/%s/%s", filedir, featpref, featbase );
	 
	 examples[i] = readtokens_file( featfile, -1, -1, &N[i] );

	 if ( N[i] > MAXFRAMES-1 ) {
	    fprintf(stderr,
		    "\nWARNING: %s (%d frames) exceeds MAXFRAMES=%d",
		    featfile, N[i], MAXFRAMES); 
	    N[i] = MAXFRAMES-1;
	 }
      }
      fprintf(stderr, "%f s\n",toc());
      
      // Compute the example similarities
      fprintf(stderr,"Computing word example similarities: "); tic();
      
      int pctstep = (rB-rA)/10;
      for ( int i = rA; i < rB; i++ ) {
	 if ( ((i-rA+1) % pctstep) == 0 )
	    fprintf(stderr,".");

	 for ( int j = cA; j < cB; j++ ) {
	    if ( j <= i ) 
	       continue;
	    
	    float editdist = stredit( examples[i], N[i], examples[j], N[j] );
	    
	    int sw = !strcmp( words[i], words[j] );
	    int sp = spkr[i] == spkr[j];
	    if ( printinds )
	       printf("%f %d %d %d %d\n", editdist, sw, sp, i+1, j+1 );
	    else
	       printf("%f %d %d\n", editdist, sw, sp);
	 }
      }
      fprintf(stderr, " %f s\n",toc());
      FREE(N);
      for ( int i = 0; i < Nwords; i++ ) {
	 if ( examples[i] )
	    FREE(examples[i]);
      }
      FREE(examples);
   }

   fprintf(stderr, "Completed subset %03d\n", subset);

   // FREE everything else that was malloc-d
   FREE(spkr);
   for ( int i = 0; i < Nwords; i++ )
      FREE(words[i]);
   FREE(words);
   FREE(LOOKUP_TABLE);

   return 0;
}
