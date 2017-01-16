//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#ifndef DOT_H
#define DOT_H

typedef unsigned int frameind;

typedef struct Dot {
  frameind xp, yp;
  float val;
} Dot;

typedef struct DotXV {
  frameind xp;
  float val;
} DotXV;

typedef struct Match {
      frameind xA, xB, yA, yB;
      float rhoampl;
      float score;
} Match;

int Dot_compare(const void *A, const void *B);

int DotXV_compare(const void *A, const void *B);

int radix_sorty(Dot *dotlist, int dotcnt, DotXV *sorted_dots, frameind *cumsum, frameind *cumsumind);

void quick_sortx(DotXV *radixdots, int dotcnt, frameind *cumsum, frameind *cumsumind, int ncumsum);

int compress_dotlist(Dot *dotlist, int prime, float thr);

int dot_dedup(DotXV *dotlist, frameind *cumsum, frameind *cumsumind, int ncumsum, float thr);

int median_filtx(frameind Ntest, frameind Nq, DotXV *inlist, int incnt, 
		 frameind *cumsum, frameind *cumsumind, int ncumsum, int dx, float medthr, Dot *outlist);

int hough_gaussy(frameind N, int dotcnt, frameind *cumsum, frameind *cumsumind, 
		 int ncumsum, int dy, float *hough, frameind *houghind);

int count_rholist( float *hough, frameind *houghinds, int nhough, int rhothr );


int compute_rholist(float *hough, frameind *houghind, int nhough, int rhothr, frameind *rholist, float *rhoampl );

int compute_matchlist_simple(frameind Ntest, frameind Nq, DotXV *dotlist, int dotcnt, 
			     frameind *cumsum,frameind *cumsumind, int ncumsum, 
			     frameind *rholist, float *rhoampl, int rhocnt, int dx, Match *matchlist);

//int compute_matchlist(frameind Ntest, frameind Nq, DotXV *dotlist, int dotcnt, frameind *cumsum, 
//		      frameind *rholist, float *rhoampl, int rhocnt,  
//		      int dx, int dy, Match *matchlist);

int compute_matchlist_sparse(frameind Ntest, frameind Nq, DotXV *dotlist, int dotcnt, 
			     frameind *cumsum, frameind *cumsumind, int ncumsum,
			     frameind *rholist, float *rhoampl, int rhocnt,  
			     int dx, int dy, Match *matchlist);

void dump_matchlist(char **files, Match *matchlist, int matchcnt, frameind *xOffsets, frameind yOffset, char *queryfile, char *querytype);

void hist_feats(int *hist, float *feats, int nframes, int nphonemes, float threshold);

void dump_dotplot(int xmin, int xmax, int ymin, int ymax, int diffspeech, int Nmax, DotXV *dotlist, int *cumsum);

int duration_filter( Match *matchlist, int matchcnt, float durthr );

void compute_path_dotcnt(int D, int sildim, float *silence1, float *silence2, Match * match, 
			 int *postings1, int *postings1_index, int *postings2, int *postings2_index,
			 int * totdots, int * dotcnt, int * pathlen, int * dotvec);

float logreg_score_ken( int totdots, int pathcnt, int * dotvec );

float logreg_score_aren( float dtwscore, int totdots, int pathcnt, int * dotvec );

#endif
