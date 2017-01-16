//
// Copyright 2011-2012  Johns Hopkins University 
// Authors: Ben Van Durme, Aren Jansen
//

#ifndef SIGNATURE_H
#define SIGNATURE_H

#define SPHW 250 // Second pass maximum halfwidth

#include <tgmath.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <search.h>
#include <stdbool.h>
#include "util.h"

#ifdef INDEXMODE
#include "dotkws.h"
#else
#include "dot.h"
#endif

typedef unsigned char byte;

int numComparisons;

int SIG_NUM_BYTES; // number of bytes in the signatures
int *PERMUTE_; // permutation array for pleb search

// based on http://infolab.stanford.edu/~manku/bitcount/bitcount.html
const static int BITS_IN_[256] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8};

/* 
   (not based on any 3rd party)
   Indexes a byte value to a value from 0 to 255, when sorted lexicographically
   on its bit contents, left to right, with a sign bit.
   00000000 = 0, 00000001 = 1, ..., 11111110 = 254, 11111111 = 255, 
*/

const static int LEX_RANK_[256] = {0,128,64,192,32,160,96,224,16,144,80,208,48,176,112,240,8,136,72,200,40,168,104,232,24,152,88,216,56,184,120,248,4,132,68,196,36,164,100,228,20,148,84,212,52,180,116,244,12,140,76,204,44,172,108,236,28,156,92,220,60,188,124,252,2,130,66,194,34,162,98,226,18,146,82,210,50,178,114,242,10,138,74,202,42,170,106,234,26,154,90,218,58,186,122,250,6,134,70,198,38,166,102,230,22,150,86,214,54,182,118,246,14,142,78,206,46,174,110,238,30,158,94,222,62,190,126,254,1,129,65,193,33,161,97,225,17,145,81,209,49,177,113,241,9,137,73,201,41,169,105,233,25,153,89,217,57,185,121,249,5,133,69,197,37,165,101,229,21,149,85,213,53,181,117,245,13,141,77,205,45,173,109,237,29,157,93,221,61,189,125,253,3,131,67,195,35,163,99,227,19,147,83,211,51,179,115,243,11,139,75,203,43,171,107,235,27,155,91,219,59,187,123,251,7,135,71,199,39,167,103,231,23,151,87,215,55,183,119,247,15,143,79,207,47,175,111,239,31,159,95,223,63,191,127,255};

struct signature_index {
    int SIG_NUM_BYTES;
    int Nfiles;
    int *Nchars;
    char **files;
    frameind Ntot;
    int *Narr;
    struct signature *allfeats;
    int P;
    int **PERMUTE;
    frameind **order;
};

#ifdef INDEXMODE
struct signature {
    int id;
    byte *byte_;
    byte query;
    int fid;
    frameind indexid;
};
#else
struct signature {
    int id;
    byte *byte_;
    byte query;
};
#endif

// Initializes the referenced signature
// sig->byte_ is initialized based on the current value of SIG_NUM_BYTES
void new_signature (struct signature *sig, int id);

// returns the number of bits that differ between the byte_ arrays of x and y
int hamming (struct signature* x, struct signature* y);

void free_signatures ( struct signature *sig, int nsig );

// reads in a binary file of byte arrays of length SIG_NUM_BYTES
// at return, n records the number of signatures read from the file
struct signature* read_signatures (char *filename, int *n);

// reads in a binary file of byte arrays of length SIG_NUM_BYTES
// at return, n records the number of signatures read from the file
struct signature* readsigs_file (char *filename, int *fA, int *fB, int *n);

// initializes PERMUTE_
void initialize_permute ();

// shuffles the value of PERMUTE_
// (may 10, 2011: currently will give SIG_NUM_BYTES 
//  unique permutations, then will cycle)
void permute ();

// comparator that says, e.g., 11111111 > 00000000
int signature_greater (const void *x, const void *y);

// comparator for pointers to signatures
int signature_ptr_greater (const void *x, const void *y);

// returns true when all bytes have the value 0
bool signature_is_zeroed (const struct signature* x);

double approximate_cosine (struct signature* x, struct signature* y);

int pleb( struct signature *x_sig_, int x_size, 
	  struct signature *y_sig_, int y_size, 
	  int diffspeech, int P, int B, float T, int D, Dot *dotlist );

int plebkws( struct signature *x_sig_, int x_size, 
	     struct signature *y_sig_, int y_size, 
	     int diffspeech, int P, int B, float T, int D, Dot *dotlist );

#ifdef INDEXMODE
int plebindex( struct signature_index *index, struct signature *y_sig_, int y_size,
	       int P, int B, float T, int D, Dot *dotlist );

int diagonal_probe_index( struct signature* x_sig_, frameind x_size, 
			  struct signature* y_sig_, frameind y_size, 
			  frameind x, frameind y, float T, int D, 
			  int dotcnt, Dot *dotlist);
#endif

int diagonal_probe( struct signature* x_sig_, int x_size, 
		    struct signature* y_sig_, int y_size, 
		    int x, int y, float T, int D, 
		    bool self_comparison, int dotcnt, Dot *dotlist);

void sig_castpath( struct signature *feats1, int N1, 
		   struct signature *feats2, int N2, 
		   int dir, int xM, int yM, float castthr, float trimthr, 
		   int R, int *xE, int *yE );

void sig_kwspass( Match *matchlist, int matchcnt, 
		  struct signature *feats1, frameind N1, 
		  struct signature *feats2, frameind N2, 
		  int R );

void sig_secondpass( Match *matchlist, int matchcnt, 
		     struct signature *feats1, int N1, 
		     struct signature *feats2, int N2, 
		     int R, float castthr, float trimthr);

void print_lex_rank(struct signature* sig);

#endif
