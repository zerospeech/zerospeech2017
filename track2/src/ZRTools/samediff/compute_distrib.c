//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "util.h"

char *distlist = NULL;
char *outfile = NULL;

void usage()
{
  printf("usage: compute_distrib -distlist <str> (REQUIRED)]\
\n\t[-outfile <str> (REQUIRED)]\n");
}

void parse_args(int argc, char **argv)
{
  int i;
  for( i = 1; i < argc; i++ ) 
  {
     if ( strcmp(argv[i], "-distlist") == 0 ) distlist = argv[++i];
     else if ( strcmp(argv[i], "-outfile") == 0 ) outfile = argv[++i];
     else {
       fprintf(stderr, "unknown arg: %s\n", argv[i]);
       usage();
     }
  }

  if ( !distlist ) {
     usage();
     fatal("\nERROR: distlist arg is required");
  }

  if ( !outfile ) {
     usage();
     fatal("\nERROR: outfile arg is required");
  }
}

void dist_normalize( double *x, double *P )
{
   int sum = 0;
   for ( int i = 0; i < 100; i++ ) {
      sum += P[i];
   }
   for ( int i = 0; i < 100; i++ ) {
      P[i] /= (sum*x[2]-x[1]);
   }
}

struct distance
{
      float dist;
      int sw;
};

int distance_cmp (const void *x1, const void *x2)
{
   const struct distance *c1 = (const struct distance *) x1;
   const struct distance *c2 = (const struct distance *) x2;

   if ( c1->dist > c2->dist ) {
      return 1;
   } else {
      if ( c1->dist < c2->dist ) {
	 return -1;
      } else {
	 return 0;
      }
   }
}

float average_precision_overall( float *dist, int *sw, int N )
{
   struct distance *distlist = 
      (struct distance *) MALLOC( sizeof(struct distance)*N );

   for ( int i = 0; i < N; i++ ) {
      distlist[i].dist = dist[i];
      distlist[i].sw = sw[i];
   }

   qsort( distlist, N, sizeof(struct distance), distance_cmp );

   int rel_r = 0;
   int total_rel = 0;
   float AveP = 0;
   for ( int i = 0; i < N; i++ ) {
      total_rel += distlist[i].sw;
      rel_r += distlist[i].sw;
      AveP += distlist[i].sw * ((float)rel_r) / (i+1);
   }
   AveP /= total_rel;
   
   FREE(distlist);

   return AveP;
} 

float average_precision_samespkr( float *dist, int *sw, int *sp, int N )
{
   int Nsp = 0;
   for ( int i = 0; i < N; i++ ) Nsp += sp[i];

   struct distance *distlist = 
      (struct distance *) MALLOC( sizeof(struct distance)*Nsp );

   int cnt = 0;
   for ( int i = 0; i < N; i++ ) {
      if ( sp[i] ) {
	 distlist[cnt].dist = dist[i];
	 distlist[cnt].sw = sw[i];
	 cnt++;
      }
   }

   qsort( distlist, Nsp, sizeof(struct distance), distance_cmp );

   int rel_r = 0;
   int total_rel = 0;
   float AveP = 0;
   for ( int i = 0; i < Nsp; i++ ) {
      total_rel += distlist[i].sw;
      rel_r += distlist[i].sw;
      AveP += distlist[i].sw * ((float)rel_r) / (i+1);
   }
   AveP /= total_rel;
   
   FREE(distlist);

   return AveP;
} 

float average_precision_diffspkr( float *dist, int *sw, int *sp, int N )
{
   int Ndp = 0;
   for ( int i = 0; i < N; i++ ) Ndp += !sp[i];

   struct distance *distlist = 
      (struct distance *) MALLOC( sizeof(struct distance)*Ndp );

   int cnt = 0;
   for ( int i = 0; i < N; i++ ) {
      if ( !sp[i] ) {
	 distlist[cnt].dist = dist[i];
	 distlist[cnt].sw = sw[i];
	 cnt++;
      }
   }

   qsort( distlist, Ndp, sizeof(struct distance), distance_cmp );

   int rel_r = 0;
   int total_rel = 0;
   float AveP = 0;
   for ( int i = 0; i < Ndp; i++ ) {
      total_rel += distlist[i].sw;
      rel_r += distlist[i].sw;
      AveP += distlist[i].sw * ((float)rel_r) / (i+1);
   }
   AveP /= total_rel;
   
   FREE(distlist);

   return AveP;
} 

float average_precision_sid( float *dist, int *sw, int *sp, int N )
{
   int Nsw = 0;
   for ( int i = 0; i < N; i++ ) Nsw += sw[i] || !sp[i];

   struct distance *distlist = 
      (struct distance *) MALLOC( sizeof(struct distance)*Nsw );

   int cnt = 0;
   for ( int i = 0; i < N; i++ ) {
      if ( sw[i] || !sp[i] ) {
	 distlist[cnt].dist = dist[i];
	 distlist[cnt].sw = sp[i];
	 cnt++;
      }
   }

   qsort( distlist, Nsw, sizeof(struct distance), distance_cmp );

   int rel_r = 0;
   int total_rel = 0;
   float AveP = 0;
   for ( int i = 0; i < Nsw; i++ ) {
      total_rel += distlist[i].sw;
      rel_r += distlist[i].sw;
      AveP += distlist[i].sw * ((float)rel_r) / (i+1);
   }
   AveP /= total_rel;
   
   FREE(distlist);

   return AveP;
} 

float falsealarm_at_recall( float *dist, int *sw, int N, float recall )
{
   struct distance *distlist = 
      (struct distance *) MALLOC( sizeof(struct distance)*N );

   int total_rel = 0;
   for ( int i = 0; i < N; i++ ) {
      distlist[i].dist = dist[i];
      distlist[i].sw = sw[i];
      total_rel += sw[i];
   }

   qsort( distlist, N, sizeof(struct distance), distance_cmp );

   float rel_r = 0;
   float fa_r = 0;
   for ( int i = 0; i < N; i++ ) {
      rel_r += (float) distlist[i].sw;
      fa_r += (float) !distlist[i].sw;
      if ( rel_r / total_rel >= recall ) break;      
   }

   FREE(distlist);

   return fa_r/(N-total_rel);
} 

int main(int argc, char **argv)
{ 
   parse_args(argc, argv);

   int Nlines = file_line_count( distlist );
   fprintf(stderr,"Total distance instances: %d\n",  Nlines);

   // Read in the distlist
   fprintf(stderr,"Reading distance list: "); tic();

   float *dist = (float *) MALLOC( Nlines*sizeof(float) );
   int *sw = (int *) MALLOC( Nlines*sizeof(int) );
   int *sp = (int *) MALLOC( Nlines*sizeof(int) );

   FILE *fptr = fopen(distlist, "r");
   int lcnt = 0;

   while ( lcnt < Nlines && 
	   fscanf(fptr, "%f%d%d",
		  &dist[lcnt], &sw[lcnt], &sp[lcnt] ) != EOF ) {
      //fprintf(stderr, "%f %d %d\n", dist[lcnt], sw[lcnt], sp[lcnt]);
      lcnt++;
   }
   fclose(fptr);
   fprintf(stderr, "%f s\n",toc());

   // Find the maximum distance value
   float maxdist = 0;
   for ( int i = 0; i < Nlines; i++ ) {
      if ( dist[i] > maxdist ) 
	 maxdist = dist[i];
   }

   // Estimate the four distributions
   fprintf(stderr,"Estimating distributions: "); tic();
   double kwidth = 0.05*maxdist;
   double x[100];
   double P_swsp[100];
   double P_swdp[100];
   double P_dwsp[100];
   double P_dwdp[100];

   for ( int i = 0; i < 100; i++ ) {
      x[i] = i*maxdist/100;
      P_swsp[i] = 0;
      P_swdp[i] = 0;
      P_dwsp[i] = 0;
      P_dwdp[i] = 0;
   }

   for ( int i = 0; i < Nlines; i++ ) {
      double *P_ptr;
      if ( sw[i] && sp[i] ) // SWSP
	 P_ptr = P_swsp;
      else if ( sw[i] && !sp[i] ) // SWDP
	 P_ptr = P_swdp;
      else if ( !sw[i] && sp[i] ) // DWSP
	 P_ptr = P_dwsp;
      else //DWDP 
	 P_ptr = P_dwdp;

      int jA = MAX(0,floor((dist[i]-kwidth)/(maxdist/100)));
      int jB = MIN(100,ceil((dist[i]+kwidth)/(maxdist/100)));

      for ( int j = jA; j < jB; j++ ) P_ptr[j]++;
   }

   dist_normalize( x, P_swsp );
   dist_normalize( x, P_swdp );
   dist_normalize( x, P_dwsp );
   dist_normalize( x, P_dwdp );
   fprintf(stderr, "%f s\n",toc());

   // Compute the precision-recall values
   fprintf(stderr,"Computing precision and recall: "); tic();
   double prec[100];
   double rec_swsp[100];
   double rec_swdp[100];

   for ( int i = 0; i < 100; i++ ) {
      prec[i] = 0;
      rec_swsp[i] = 0;
      rec_swdp[i] = 0;
   }

   int tot_swsp = 0;
   int tot_swdp = 0;
   for ( int i = 0; i < Nlines; i++ ) {
      if ( sw[i] && sp[i] )
	 tot_swsp++;

      if ( sw[i] && !sp[i] )
	 tot_swdp++;
      
      for ( int k = 99; k >= 0; k-- ) {
	 if ( dist[i] > x[k] ) break;

	 if ( !sw[i] ) {
	    prec[k]++;
	 }

	 if ( sw[i] && sp[i] ) rec_swsp[k]++;
	 if ( sw[i] && !sp[i] ) rec_swdp[k]++;
      }
   }

   for ( int i = 0; i < 100; i++ ) {
      prec[i] = (rec_swsp[i]+rec_swdp[i])/(rec_swsp[i]+rec_swdp[i]+prec[i]+1e-20);
      rec_swsp[i] = rec_swsp[i]/tot_swsp;
      rec_swdp[i] = rec_swdp[i]/tot_swdp;
   }
   
   fprintf(stderr, "%f s\n",toc());

   // Write the distributions and prec-rec values to outfile
   fprintf(stderr,"Writing distributions: "); tic();
   fptr = fopen(outfile, "w");
   for ( int i = 0; i < 100; i++ ) {
      fprintf(fptr, "%f %f %f %f %f %f %f %f \n", 
	      x[i], P_swsp[i], P_swdp[i], P_dwsp[i], P_dwdp[i], 
	      prec[i], rec_swsp[i], rec_swdp[i]);
   }
   fclose(fptr);
   fprintf(stderr, "%f s\n",toc());

   // Compute representation quality measures
   fprintf(stderr,"Compute quality measures: "); tic();
   double mu1 = 0; int c1 = 0;
   double mu2 = 0; int c2 = 0;
   double mu12 = 0; int c12 = 0;
   double mu34 = 0; int c34 = 0;
   
   for ( int i = 0; i < Nlines; i++ ) {
      if ( sw[i] && sp[i] ) {// SWSP
	 mu1 += dist[i];
	 c1++;
      }
      
      if ( sw[i] && !sp[i] ) {// SWDP
	 mu2 += dist[i];
	 c2++;
      }
      
      if ( sw[i] ) {// SW
	 mu12 += dist[i];
	 c12++;
      } else { // DW 
	 mu34 += dist[i];
	 c34++;
      }      
   }

   mu1 /= c1;
   mu2 /= c2;
   mu12 /= c12;
   mu34 /= c34;

   double var1 = 0;
   double var2 = 0;
   double var12 = 0; 
   double var34 = 0; 

   for ( int i = 0; i < Nlines; i++ ) {
      if ( sw[i] && sp[i] ) {// SWSP
	 var1 += pow(dist[i]-mu1,2)/(c1-1);
      }
      
      if ( sw[i] && !sp[i] ) {// SWDP
	 var2 += pow(dist[i]-mu2,2)/(c2-1);
      }
      
      if ( sw[i] ) {// SW
	 var12 += pow(dist[i]-mu12,2)/(c12-1);
      } else { // DW 
	 var34 += pow(dist[i]-mu34,2)/(c34-1);
      }      
   }
   
   double S_1_2 = pow(mu1-mu2,2)/(var1+var2);
   double S_12_34 = pow(mu12-mu34,2)/(var12+var34);
   double R = S_12_34/S_1_2;
   
   double PRB1 = 0;
   double PRB2 = 0;
   double mindiff1 = 1;
   double mindiff2 = 1;

   for ( int i = 0; i < 100; i++ ) {
      if ( rec_swsp[i] > 0.01 && fabs(prec[i]-rec_swsp[i]) < mindiff1 ) {
	 mindiff1 = fabs(prec[i]-rec_swsp[i]);
	 PRB1 = 0.5*(prec[i]+rec_swsp[i]);
      }

      if ( rec_swdp[i] > 0.01 && fabs(prec[i]-rec_swdp[i]) < mindiff2 ) {
	 mindiff2 = fabs(prec[i]-rec_swdp[i]);
	 PRB2 = 0.5*(prec[i]+rec_swdp[i]);
      }
   }
   fprintf(stderr, "%f s\n",toc());

   fprintf(stderr,"Compute average precisions: "); tic();
   float AveP = average_precision_overall( dist, sw, Nlines );
   float AveP_sp = average_precision_samespkr( dist, sw, sp, Nlines );
   float AveP_dp = average_precision_diffspkr( dist, sw, sp, Nlines );
   float sid_AveP = average_precision_sid( dist, sw, sp, Nlines );
   float FAatRecall = falsealarm_at_recall( dist, sw, Nlines, 0.5 );
   fprintf(stderr, "%f s\n",toc());

//   fprintf(stderr,"mu1=%f\nmu2=%f\nsigma1=%f\nsigma2=%f\n",mu1,mu2,sqrtf(var1),sqrtf(var2));
//   fprintf(stderr,"mu12=%f\nmu34=%f\nsigma12=%f\nsigma34=%f\n",mu12,mu34,sqrtf(var12),sqrtf(var34));

   fprintf(stderr,"----------------Results----------------\n");
   fprintf(stderr,"File: %s\n",distlist);
   fprintf(stderr,"S_1_2 = %3.3f\n", S_1_2);
   fprintf(stderr,"S_12_34 = %3.3f\n", S_12_34);
   fprintf(stderr,"R = %3.3f\n", R);
   fprintf(stderr,"PRB_swsp = %3.3f\n", PRB1);
   fprintf(stderr,"PRB_swdp = %3.3f\n", PRB2);
   fprintf(stderr,"---------------------------------------\n");
   fprintf(stderr,"same_ap = %3.3f\n", AveP_sp);
   fprintf(stderr,"diff_ap = %3.3f\n", AveP_dp);
   fprintf(stderr,"ave_prec = %3.3f\n", AveP);
   fprintf(stderr,"---------------------------------------\n");
   fprintf(stderr,"sid_ap = %3.3f\n", sid_AveP);
   fprintf(stderr,"---------------------------------------\n");
   fprintf(stderr,"FA_at_Recall50 = %3.3f\n", FAatRecall);
   fprintf(stderr,"---------------------------------------\n");

   // FREE everything that was malloc-d
   FREE(dist);
   FREE(sw);
   FREE(sp);

   return 0;
}
