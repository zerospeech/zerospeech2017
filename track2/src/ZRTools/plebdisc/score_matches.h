//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#ifndef SCORE_MATCHES_H
#define SCORE_MATCHES_H

#include "signature.h"

#define NBINS 20

void score_match(struct signature *feats1, struct signature *feats2, 
		 int N1, int N2, int start1, int end1, int start2, int end2, float T,
		 int *d, int *dots, int *bins);

float dtw_score(struct signature *feats1, struct signature *feats2, 
		frameind N1, frameind N2, frameind start1, frameind end1, 
		frameind start2, frameind end2);
#endif
