/* $Id: lsqsolve.c,v 1.2 1996/02/27 19:48:11 bedk Exp $
 *
 * $Log: lsqsolve.c,v $
 * Revision 1.2  1996/02/27 19:48:11  bedk
 * Added lsqsolve tag to determinant reporting
 *
 * Revision 1.1  1995/10/20 20:15:42  bedk
 * Initial revision
 *
 *
 * Least-squares solver.  See the create_mapping man page for
 * a description of this program's purpose in life.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* useful macros */
#define MAX(a,b) (( a > b ) ? a : b )
#define MIN(a,b) (( a > b ) ? b : a )

/* Bloody broken header files... */
int scanf(const char *, ...);
int fprintf(FILE *, const char *, ...);

/* declare all the routines in this file */
void init();
void readAmatrix();
void eliminate(const unsigned int, const unsigned int,
               double * const, const unsigned int);

/* declare all the routines in other files */
void mul_mdmd_md(const int, const int, const int,
                 const double * const, const double * const,
                 double * const, const int, const int, const int); 

/* here are the data structures we need */
int ncrit, radius, nframes, njah, maxncoeff;
FILE *mapFile;
double *a_startp, *at_startp, *bt_startp, *r_startp;
double *a_endp, *at_endp, *bt_endp, *r_endp;
float cleanJah;
char mapFileName[1024];

int
main() {
  unsigned int i, j, col, loCol, hiCol, nxtLoCol, nxtHiCol;
  double *alp, *ahp, *atlp, *athp, *btp, *rp, *p, *q;
  float curJah;
  double t;

  init();
  /* main loop:  get a Jah value.
   *             if it is the clean Jah value, print the identity
   *                 matrix 
   *             otherwise read in the critical band values then
   *                 set up and solve the least-squares problems
   *                 and print the results to mapFile.
   */
  while (scanf("%f", &curJah) != EOF) {
    fprintf(mapFile, "%g\n\n", curJah);
    if (curJah != cleanJah) {
      readAmatrix();
      loCol = 0;
      hiCol = MIN(radius,(ncrit - 1));
      btp = bt_startp;
      alp = a_startp;
      ahp = a_startp + hiCol + 1;
      atlp = at_startp;
      athp = at_startp + nframes * (hiCol + 1);
      p = athp + nframes;
      for(col = 0; col < ncrit; col++) {
        nxtLoCol = (col > radius) ? (col - radius) : 0;
        nxtHiCol = MIN((col + radius),(ncrit - 1));
        if (nxtLoCol != loCol) {
          loCol = nxtLoCol;
          alp++;
          atlp += nframes;
        }
        if (nxtHiCol != hiCol) {
          hiCol = nxtHiCol;
          /* move 1's in A and At */
          q = ahp + 1;
          for (i = 0; i < nframes; i++) {
            t = *p;
            *p++ = 1.0;
            *athp++ = t;
            *ahp = t;
            *(ahp + 1) = 1.0;
            ahp += (ncrit + 1);
          }
          ahp = q;
        }
        rp = r_startp + (hiCol - loCol + 2);
        
        /* compute At * A and put result in tableau */
        mul_mdmd_md(hiCol - loCol + 2, nframes, hiCol - loCol + 2,
                    atlp, alp, r_startp, nframes, ncrit + 1,
                    maxncoeff + 1);

        /* compute At * b and put result in tableau */
        mul_mdmd_md(hiCol - loCol + 2, nframes, 1, atlp, btp, rp,
                    nframes, 1, maxncoeff + 1);

        /* do Gaussian elimination to find inv(At * A) * (At * b) */
        eliminate(hiCol - loCol + 2, hiCol - loCol + 3, r_startp,
                  maxncoeff + 1);

        /* print result to mapFile */
        for (j = 0; j < loCol; j++) fprintf(mapFile, "0\n");
        for (j = loCol; j <= hiCol; j++) {
          fprintf(mapFile, "%g\n", *rp);
          rp += (maxncoeff + 1);
        }
        for (j = hiCol + 1; j < ncrit; j++) fprintf(mapFile, "0\n");
        fprintf(mapFile, "%g\n\n", *rp);

        btp += nframes;
      }
    }
    else {
      /* print the identity matrix to mapFile and continue */
      for (i = 0; i < ncrit; i++) {
        for (j = 0; j < i; j++) fprintf(mapFile, "0\n");
        fprintf(mapFile, "1\n");
        for (j = i + 1; j <= ncrit; j++) fprintf(mapFile, "0\n");
        fprintf(mapFile, "\n");
      }
    }
  }
  return 0;
}

/* initialization routine */
void
init() {
  unsigned int i;
  float fb;
  double *p, *q;

  /* read in initial data */
  scanf("%d%d%d%d%g%1024s", &ncrit, &radius, &nframes, &njah,
        &cleanJah, mapFileName);
  maxncoeff = MIN((ncrit + 1),((radius + 1) << 1));
  if ((mapFile = fopen(mapFileName, "w")) == NULL) {
    fprintf(stderr, "lsqsolve: unable to open file %s for output.\n",
            mapFileName);
    exit(-1);
  }

  /* allocate space for the arrays */
  if ((a_startp = (double *)malloc(sizeof(double) *
                                   nframes * (ncrit + 1)))
      == NULL) {
    fprintf(stderr, "lsqsolve: out of heap space.\n");
    exit(-1);
  }
  a_endp = a_startp + nframes * (ncrit + 1);

  if ((at_startp = (double *)malloc(sizeof(double) *
                                   nframes * (ncrit + 1)))
      == NULL) {
    fprintf(stderr, "lsqsolve: out of heap space.\n");
    exit(-1);
  }
  at_endp = at_startp + nframes * (ncrit + 1);

  if ((bt_startp = (double *)malloc(sizeof(double) * nframes * ncrit))
      == NULL) {
    fprintf(stderr, "lsqsolve: out of heap space.\n");
    exit(-1);
  }
  bt_endp = bt_startp + nframes * ncrit;

  if ((r_startp = (double *)malloc(sizeof(double) *
                                   maxncoeff * (maxncoeff + 1)))
      == NULL) {
    fprintf(stderr, "lsqsolve: out of heap space.\n");
    exit(-1);
  }
  r_endp = r_startp + maxncoeff * (maxncoeff + 1);

  /* read in clean Jah critical band values.  The matrix is stored in
     transposed form because it will be accessed column-by-column   */
  q = bt_startp;
  for (i = 0; i < nframes; i++) {
    p = q++;
    while (p < bt_endp) {
      scanf("%g", &fb);
      *p = (double) fb;
      p += nframes;
    }
  }

  /* print header info to the mapping file */
  fprintf(mapFile, "%d %d %d\n\n", njah, ncrit, ncrit+1);
}

/* read in matrix of jah values.  Produces both the matrix and its
   transpose since both are needed later.  A column of 1s is inserted
   into both matrices because it is needed later for the least-squares
   solution */
void
readAmatrix() {
  unsigned int col = 0;
  float fb;
  double *ap, *atp, *atstp;
  ap = a_startp;
  atp = at_startp;
  atstp = at_startp;
  while (ap < a_endp) {
    if (col == radius + 1) {
      *ap++ = 1.0;
      *atp = 1.0;
    }
    else {
      scanf("%g", &fb);
      *ap++ = (double) fb;
      *atp = (double) fb;
    }
    atp += nframes;
    if (atp >= at_endp) atp = ++atstp;
    col = (col == ncrit) ? 0 : col + 1;
  }
}

/* Do Gaussian elimination on an M x N tableau A (N >= M).
   Function works in place (A is destroyed) and can handle different
   strides for A.

   Elimination is done with partial, implicit pivoting.  Implicit
   pivoting, means that pivoting is done as if each row in A were
   scaled so that the absolute value of the largest element in the
   first M columns was 1.0 */
void
eliminate(const unsigned int M, const unsigned int N,
          double * const A, const unsigned int strideA) {
  unsigned int i, j;
  double *scale_startp, *scale_endp, *row_startp, *p, *q, *r;
  double *pivotpt, *pivotrow, *pivotcol, *A_endp, *pivotscale_p;
  double mx, t, pivot, pivotsign, pivotscale, logdet;
  double tpivot, tpivotsign, tscale, f;

  logdet = 0.0;
  A_endp = A + M * strideA;

  /* allocate and initialize scale vector */
  if ((scale_startp = (double *)malloc(sizeof(double) * M))
      == NULL) {
    fprintf(stderr, "lsqsolve: out of heap space.\n");
    exit(-1);
  }
  scale_endp = scale_startp + M;
  row_startp = A;
  p = scale_startp;
  while (p != scale_endp) {
    mx = 0.0;
    q = row_startp;
    while (q != row_startp + M) {
      t = *q++;
      mx = MAX(mx,fabs(t));
    }
    row_startp += strideA;
    *p++ = mx;
  }

  /* Now do the elimination */
  pivotpt = A;
  pivotcol = A;
  for (i = 0; i < M; i++) {
    pivotrow = pivotpt;
    if ((pivot = *pivotpt) < 0.0) {
      pivot = -pivot;
      pivotsign = -1.0;
    }
    else {
      pivotsign = 1.0;
    }
    q = scale_startp + i;
    pivotscale_p = q;
    pivotscale = *q++;

    /* find the pivot */
    for (p = pivotpt + strideA; p < A_endp; p += strideA) {
      if ((tpivot = *p) < 0.0) {
        tpivot = -tpivot;
        tpivotsign = -1.0;
      }
      else {
        tpivotsign = 1.0;
      }
      tscale = *q;
      if (pivotscale * tpivot > tscale * pivot) {
        pivot = tpivot;
        pivotsign = tpivotsign;
        pivotscale = tscale;
        pivotscale_p = q;
        pivotrow = p;
      }
      q++;
    }
    if (pivot == 0.0) {
      fprintf(stderr, "lsqsolve: singular matrix\n");
      exit(-1);
    }
    logdet += log10(pivot);

    /* scale the pivot row, swapping rows if necessary */
    f = pivotsign / pivot;
    if (pivotrow != pivotpt) {
      *pivotrow = *pivotpt;
      *pivotpt = 1.0;
      p = pivotrow + 1;
      q = pivotpt + 1;
      for (j = i + 1; j < N; j++) {
        t = *p * f;
        *p++ = *q;
        *q++ = t;
      }
      /* also swap implicit pivoting scale factors */
      t = *(scale_startp + i);
      *(scale_startp + i) = *pivotscale_p;
      *pivotscale_p = t;
    }
    else {
      *pivotpt = 1.0;
      q = pivotpt + 1;
      for (j = i + 1; j < N; j++) {
        *q++ *= f;
      }
    }

    /* subtract the pivot row from the other rows */
    p = pivotcol;
    while (p < A_endp) {
      if (p != pivotpt) {
        f = *p;
        *p = 0.0;
        q = p + 1;
        r = pivotpt + 1;
        for (j = i + 1; j < N; j++) {
          *q++ -= f * *r++;
        }
      }
      p += strideA;
    }

    /* advance the pointers */
    pivotpt += (strideA + 1);
    pivotcol++;
  }

  /* clean up */
  free((char *)scale_startp);
  fprintf(stderr, "lsqsolve:  log10 | det A | = %g\n", logdet);
}
