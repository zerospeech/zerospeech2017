/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      alloc_fvec(): allocate an fvec structure         *
 *                              (float vector and int length) and        *
 *                              return a pointer to it                   *
 *                                                                       *
 *                      alloc_fmat(): allocate an fmat structure         *
 *                              (float matrix and int nrows and ncols)   *
 *                              and return a pointer to it               *
 *                                                                       *
 *                      fvec_copy(): copy an fvec to an fvec             *
 *                                                                       *
 *                      norm_fvec(): normalize fvec by scaling factor    *
 *                                                                       *
 *                      fmat_x_fvec(): matrix-vector multiply            *
 *                                                                       *
 *                      fvec_check(): bounds check for fvec reference    *
 *                                                                       *
 ************************************************************************/

/***********************************************************************

 (C) 1995 US West and International Computer Science Institute,
     All rights reserved
 U.S. Patent Numbers 5,450,522 and 5,537,647

 Embodiments of this technology are covered by U.S. Patent
 Nos. 5,450,522 and 5,537,647.  A limited license for internal
 research and development use only is hereby granted.  All other
 rights, including all commercial exploitation, are retained unless a
 license has been granted by either US West or International Computer
 Science Institute.

***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "rasta.h"

/* Allocate fvec, and return pointer to it. */
struct fvec *alloc_fvec( int veclength )
{
	struct fvec *vecptr;

	vecptr = (struct fvec *)malloc (sizeof( struct fvec) );
	if(vecptr == (struct fvec *)NULL)
	{
		fprintf(stderr,"Can't allocate %ld bytes for fvec\n",
                                sizeof( struct fvec) );
		exit(-1);
	}
	vecptr->length = veclength;
	vecptr->values = (float *)malloc ((veclength) * sizeof(float) );
	if(vecptr->values == (float *)NULL)
	{
		fprintf(stderr,"Can't allocate %ld bytes for vector\n",
                                (veclength) * sizeof(float) );
		exit(-1);
	}
	/* clear it */
	memset(vecptr->values, 0, veclength*sizeof(float));

    	return (vecptr);
} 

/* Allocate fmat, and return pointer to it. */
struct fmat * alloc_fmat(int nrows, int ncols)
{
        struct fmat *mptr;
        int i;

        mptr = (struct fmat *)malloc(sizeof(struct fmat ));
        if(mptr == (struct fmat *)NULL)
        {
                fprintf(stderr,"Cant malloc an fmat\n");
                exit(-1);
        }

        mptr->values = (float **)malloc(nrows * sizeof(float *));
        if(mptr->values == (float **)NULL)
        {
                fprintf(stderr,"Cant malloc an fmat ptr array\n");
                exit(-1);
        }
	/* I need the rows of the fmat to be contiguous in memory 
	   so that I can use the phipac routines on them.  Anyway, 
	   it's neater.  So allocate space for the whole matrix at
	   once, then construct row pointers into it. */
        mptr->values[0] = (float *)malloc(ncols*nrows*sizeof(float));
        if (mptr->values[0] == NULL) {
	    fprintf(stderr, "Can't malloc an fmat array %dx%d\n", 
		    nrows, ncols);
	    exit(-1);
	}
	/* clear the new space */
	memset(mptr->values[0], 0, nrows*ncols*sizeof(float));
	/* set up pointers to subsequent rows */
        for (i=1; i<nrows; ++i) {
	    mptr->values[i] = mptr->values[0] + i*ncols;
        }

        mptr->nrows = nrows;
        mptr->ncols = ncols;

        return( mptr );
}

/* destroy the structures created by alloc_* above */
void free_fvec(struct fvec* fv) 
{
    free(fv->values);
    free(fv);
}
void free_fmat(struct fmat* fm) 
{
    int i;
    /* only the first row points to allocated space (now) */
    free(fm->values[0]);
    free(fm->values);
    free(fm);
}

/* Multiply a float matrix (in fmat structure) times a
	float vector (in fvec structure) */
void fmat_x_fvec( const struct fmat * mat, const struct fvec * invec,
                        struct fvec *outvec )
{
        int i, j;
        float f;

        if(mat->ncols != invec->length)
        {
                fprintf(stderr,"input veclength neq mat ncols\n");
                exit(-1);
        }
        if(mat->nrows != outvec->length)
        {
                fprintf(stderr,"output veclength neq mat nrows\n");
                exit(-1);
        }

        for(i=0; i<outvec->length; i++)
        {
                f = mat->values[i][0] * invec->values[0];
                for(j=1; j<invec->length; j++)
                {
                        f += mat->values[i][j] * invec->values[j];
                }
                outvec->values[i] = f;
        }
}

/* Normalize a float vector by a scale factor */
void norm_fvec ( struct fvec *fptr, float scale)
{
	int i;

	for(i=0; i<fptr->length; i++)
	{
		fptr->values[i] /= scale;
	}
}

/* Copy a vector into an equal length fvec (first one into
	second one). Currently kills
	the program if you attempt to copy incompatible length
	fvecs. */
void fvec_copy(char *funcname, const struct fvec *fvec_1, struct fvec *fvec_2 )
{
	int i;

	if(fvec_1->length != fvec_2->length)
	{
		fprintf(stderr,"Cannot copy an fvec into one of ");
		fprintf(stderr,"unequal length\n");
		fprintf(stderr,"\tThis was tried in function %s\n", funcname);
		exit(-1);
	}
	for(i=0; i<fvec_1->length; i++)
	{
		fvec_2->values[i] = fvec_1->values[i];
	}
}

/* Routine to check that it is OK to access an array element.
Use this in accordance with your level of paranoia; if truly
paranoid, use it before every array reference. If only moderately
paranoid, use it once per loop with the indices set to the
largest value they will achieve in the loop. You don't need to use this
at all, of course. 

	The routine accesses a global character array that is supposed
to hold the name of the calling function. Of course if you
write a new function and don't update this value, this fature won't work. */

void fvec_check(char *funcname, const struct fvec *vec, int index )
{
	if((index >= vec->length) || (index < 0))
	{
		fprintf(stderr,"Tried to access %dth elt, array length=%d\n",
			index + 1, vec->length);
		fprintf(stderr,"\tIn routine %s\n", funcname);
		fflush(stderr);
		abort();
	}
}
