//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#define M_PI           3.14159265358979323846  /* pi */

int same_vectors(int *vec1, int n1, int *vec2, int n2);
void make_cumhist(int *cumhist, int *hist, int n);

void *MALLOC(size_t sz);
void FREE(void *ptr);
int get_malloc_count( void );
long closest_prime( long base );
int min(int a, int b);
int max(int a, int b);
void fatal(char *msg);
void threshold_vector(float *vec, int n, double T);

float *readfeats_file(char *fn, int D, int *fA, int *fB, int *N);
float *readfeats_file2(char *fn, int fA, int fB, int *N, int *D);

int *readtokens_file(char *fn, int fA, int fB, int *N);

char *mmap_file(char *fn, int *len);

void tic(void);
float toc(void);

int file_line_count( char * file );

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#endif
