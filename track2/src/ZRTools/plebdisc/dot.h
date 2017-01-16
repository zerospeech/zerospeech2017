//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#ifndef DOT_H
#define DOT_H

typedef int frameind;

typedef struct Dot {
  int xp, yp;
  float val;
} Dot;

typedef struct DotXV {
  int xp;
  float val;
} DotXV;

typedef struct Match {
      int xA, xB, yA, yB;
      float rhoampl;
      float score;
} Match;

int Dot_compare(const void *A, const void *B);

int DotXV_compare(const void *A, const void *B);

void radix_sorty(int N, Dot *dotlist, int dotcnt, DotXV *sorted_dots, int *cumsum);

void quick_sortx(int N, DotXV *radixdots, int dotcnt, int *cumsum);

int compute_dotplot_dense(int N, float *feats, int D, int *cntlist, int **occlist, float *silence, 
			   float distthr, float **M);

int dense_to_sparse(int N, float **M, float distthr, Dot *dotlist);

int compress_dotlist(Dot *dotlist, int prime, float thr);

int dot_dedup(DotXV *dotlist, int *cumsum, int nrows, float thr);

int compute_dotplot_sparse(float *feats1, int N1, float *feats2, int N2, int D, int sildim, float *silence1, float *silence2, int diffspeech, 
			   int *postings1, int *postings1_index, int *postings2, int *postings2_index, int prime, float distthr, Dot *dotlist);


int median_filtx(int N, DotXV *inlist, int incnt, int *cumsum, int dx, float medthr, Dot *outlist);

int brute_median_filtx(int N, DotXV * inlist, int incnt, int * cumsum, int dx, float medthr, Dot * outlist);

void hough_gaussy(int N, int dotcnt, int *cumsum, int dy, int diffspeech, float *hough);

int count_rholist(int N, float *hough, int rhothr);
 
int compute_rholist(int N, float *hough, int rhothr, int *rholist, float *rhoampl);

int compute_matchlist_simple(int N, DotXV *dotlist, int dotcnt, int *cumsum, 
			     int *rholist, float *rhoampl, int rhocnt, int dx, int diffspeech, Match *matchlist);

int compute_matchlist(int N, DotXV *dotlist, int dotcnt, int *cumsum, 
		      int *rholist, float * rhoampl, int rhocnt, int dx, int dy, int diffspeech, Match *matchlist);

int merge_matchlist(Match *matchlist, int matchcnt, int mergetol);

void dump_matchlist(Match *matchlist, int matchcnt, int xOffset, int yOffset);

void hist_feats(int *hist, float *feats, int nframes, int nphonemes, float threshold);

void dump_dotplot(int xmin, int xmax, int ymin, int ymax, int diffspeech, int Nmax, DotXV *dotlist, int *cumsum);

int duration_filter( Match *matchlist, int matchcnt, float durthr );

void compute_path_dotcnt(int D, int sildim, float *silence1, float *silence2, Match * match, 
			 int *postings1, int *postings1_index, int *postings2, int *postings2_index,
			 int * totdots, int * dotcnt, int * pathlen, int * dotvec);

float logreg_score_ken( int totdots, int pathcnt, int * dotvec );

float logreg_score_aren( float dtwscore, int totdots, int pathcnt, int * dotvec );

#endif
