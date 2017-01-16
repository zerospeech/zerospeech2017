//
// Copyright 2011-2015  Johns Hopkins University (Author: Aren Jansen)
//

#ifndef FEAT_H
#define FEAT_H

int feat_readfile(char * featfile, float * feats, int D, int maxframes);
int feat_downsample(int N, float * feats, int D, int dsfact);
void feat_gettraj(int N, float * feats, int D, int d, float * traj);
int feat_postings(int N, float * feats, int D, float pthr, int * cntlist, int ** occlist);
void feat_normalize(float *feats, int N, int D, float *nfeats);

#endif
