/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *			alloc_svec(): allocate an svec structure         *
 *				(short vector and int length) and        *
 *				return a pointer to it                   *
 *                                                                       *
 *			svec_fvec_copy(): copy an svec to an fvec        *
 *                                                                       *
 *			svec_check(): bounds check for svec reference    *
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
#include <math.h>
#include "rasta.h"

/* Allocate svec, and return pointer to it. */
struct svec *alloc_svec( int veclength )
{
	struct svec *vecptr;

	vecptr = (struct svec *)malloc (sizeof( struct svec) );
	if(vecptr == (struct svec *)NULL)
	{
		fprintf(stderr,"Can't allocate %ld bytes for svec\n",
                                sizeof( struct svec) );
		exit(-1);
	}
	vecptr->length = veclength;
	vecptr->values = (short *)malloc ((veclength) * sizeof(short) );
	if(vecptr->values == (short *)NULL)
	{
		fprintf(stderr,"Can't allocate %ld bytes for vector\n",
                                (veclength) * sizeof(short) );
		exit(-1);
	}

    	return (vecptr);
} 

void free_svec(struct svec *vecptr)
{   /* Release the data allocated by alloc_svec */

    free(vecptr->values);
    free(vecptr);
}

/* Copy an svec vector into an equal length fvec (first one into
	second one). Currently kills
	the program if you attempt to copy incompatible length
	vecs. */
void svec_fvec_copy(char *funcname, struct svec *svec_1, struct fvec *fvec_2 )
{
	int i;

	if(svec_1->length != fvec_2->length)
	{
		fprintf(stderr,"Cannot copy an svec into an fvec of ");
		fprintf(stderr,"unequal length\n");
		fprintf(stderr,"\tThis was tried in function %s\n", funcname);
		exit(-1);
	}
	for(i=0; i<svec_1->length; i++)
	{
		fvec_2->values[i] = svec_1->values[i];
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

void svec_check( char *funcname, struct svec *vec, int index )
{
	if((index >= vec->length) || (index < 0))
	{
		fprintf(stderr,"Tried to access %dth elt, array length=%d\n",
			index + 1, vec->length);
		fprintf(stderr,"\tIn routine %s\n", funcname);
		fflush(stderr);
		exit(-1);
	}
}
