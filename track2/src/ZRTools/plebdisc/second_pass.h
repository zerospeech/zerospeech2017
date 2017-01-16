#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#define SPHW 250 // Second pass maximum halfwidth

/* float ALPHA = 0.5; // Exponential smoothing parameter */

void sig_find_paths( struct signature *feats1, int N1, 
		     struct signature *feats2, int N2, 
		     int dir, int xM, int yM,
		     float castthr, float trimthr, 
		     int R, int *xE, int *yE, int strategy,
		     float alpha);


void secondpass( Match *matchlist, int matchcnt, 
		 struct signature *feats1, int N1, 
		 struct signature *feats2, int N2, 
		 int R, float castthr, float trimthr,
		 int strategy); 

#endif
