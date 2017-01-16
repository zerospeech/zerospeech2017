//
// Copyright 2011-2015  Johns Hopkins University (Author: Aren Jansen)
//

#ifndef DOT_H
#define DOT_H

#include "util.h"

typedef struct Dot {
  int xp, yp;
  float val;
} Dot;

int dot_compare(const void *A, const void *B);
int dedup_dotlist(Dot *dotlist, int N);
int filter_overlap( Dot *dotlist, int N, Segment *segs1, Segment *segs2, float olapthr );
void dump_matchlist( Dot *dotlist, int dotcnt, Segment *segs1, Segment *segs2 );

#endif
