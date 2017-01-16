//
// Copyright 2011-2015  Johns Hopkins University (Author: Aren Jansen)
//

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "util.h"
#include "dot.h"

int dot_compare(const void *A, const void *B) 
{
   if( ((Dot *)A)->xp > ((Dot *)B)->xp ) return 1;
   else if( ((Dot *)A)->xp < ((Dot *)B)->xp ) return -1;
   else if( ((Dot *)A)->yp > ((Dot *)B)->yp ) return 1;
   else if( ((Dot *)A)->yp < ((Dot *)B)->yp ) return -1;
   else return 0;
}

int dedup_dotlist( Dot *dotlist, int N )
{
   if ( N <= 1 )
      return N;

   int A = 0;
   int B = 1;

   while ( A < N ) {
      B = A + 1;
      while ( B < N && 
	      dotlist[A].xp == dotlist[B].xp &&
	      dotlist[A].yp == dotlist[B].yp ) {
	 dotlist[B].xp = -1;
	 B++;
      }
   
      A = B;
   }

   int wpos = 0;
   int rpos = 1;
   for (wpos = 0; wpos < N; wpos++) {
      if ( wpos == rpos ) rpos++;

      if ( dotlist[wpos].xp == -1 ) {
	 while ( rpos < N && dotlist[rpos].xp == -1 ) 
	    rpos++;

	 if ( rpos == N ) return wpos;

	 dotlist[wpos].val = dotlist[rpos].val;
	 dotlist[wpos].xp = dotlist[rpos].xp;
	 dotlist[wpos].yp = dotlist[rpos].yp;
	 dotlist[rpos].xp = -1;
      }
   } 

   return wpos;
}

int filter_overlap( Dot *dotlist, int N, Segment *segs1, Segment *segs2, float olapthr )
{
   int wpos = 0;
   int rpos = 0;
   for (wpos = 0; wpos < N; wpos++) {
      int num = segs1[dotlist[wpos].xp].xB-segs1[dotlist[wpos].xp].xA + 
	 segs2[dotlist[wpos].yp].xB-segs2[dotlist[wpos].yp].xA;
      int den = MAX(segs1[dotlist[wpos].xp].xB,segs2[dotlist[wpos].yp].xB) - 
	 MIN(segs1[dotlist[wpos].xp].xA,segs2[dotlist[wpos].yp].xA);
      float folap = MAX(0,((float) num)/((float) den)-1);
      
      if ( folap <= olapthr )
	 continue;


      rpos = wpos + 1;

      while ( rpos < N ) {
	 num = segs1[dotlist[rpos].xp].xB-segs1[dotlist[rpos].xp].xA + 
	    segs2[dotlist[rpos].yp].xB-segs2[dotlist[rpos].yp].xA;
	 den = MAX(segs1[dotlist[rpos].xp].xB,segs2[dotlist[rpos].yp].xB) - 
	    MIN(segs1[dotlist[rpos].xp].xA,segs2[dotlist[rpos].yp].xA);
	 folap = MAX(0,((float) num)/((float) den)-1);
	 
	 if ( folap <= olapthr )
	    break;

	 rpos++;
      }

      if ( rpos == N ) return wpos;

      dotlist[wpos].val = dotlist[rpos].val;
      dotlist[wpos].xp = dotlist[rpos].xp;
      dotlist[wpos].yp = dotlist[rpos].yp;
      dotlist[rpos].yp = dotlist[rpos].xp;
   } 

   return wpos;
}

void dump_matchlist( Dot *dotlist, int dotcnt, Segment *segs1, Segment *segs2 )
{
   for ( int n = 0; n < dotcnt; n++ ) {
//      int num = segs1[dotlist[n].xp].xB-segs1[dotlist[n].xp].xA + 
//	 segs2[dotlist[n].yp].xB-segs2[dotlist[n].yp].xA;
//      int den = MAX(segs1[dotlist[n].xp].xB,segs2[dotlist[n].yp].xB) - 
//	 MIN(segs1[dotlist[n].xp].xA,segs2[dotlist[n].yp].xA);
//      float folap = MAX(0,((float) num)/((float) den)-1);

      fprintf( stdout,"%d %d %d %d %d %d %f\n",
//      fprintf( stdout,"%d %d %d %d %f\n",
//      fprintf( stdout,"%d %d %d %d %f %f\n",
	       dotlist[n].xp+1,dotlist[n].yp+1,
	       segs1[dotlist[n].xp].xA,
	       segs1[dotlist[n].xp].xB,
	       segs2[dotlist[n].yp].xA,
	       segs2[dotlist[n].yp].xB,
	       dotlist[n].val );
//	       dotlist[n].val, folap );
   }
}
