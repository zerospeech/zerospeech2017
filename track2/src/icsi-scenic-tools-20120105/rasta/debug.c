/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      show_args(): show set elements of param          *
 *                                    structure                          *
 *                                                                       *
 *                      show_param(): show computed elements of param    *
 *                                    structure                          *
 *                                                                       *
 *                      show_vec(): show fvec structure                  *
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

#include <stdio.h>
#include <math.h>
#include "rasta.h"

/******************************************************/

void show_args( struct param * pptr)
{
	fprintf(stderr,"window size = %f msec\n", pptr->winsize );
        fprintf(stderr,"step size = %f msec\n", pptr->stepsize );
        fprintf(stderr,"sample freq = %d Hz\n", pptr->sampfreq );
	fprintf(stderr,"effective nyq frq = %f Hz\n", pptr->nyqhz );
        fprintf(stderr,"number of critical band filters = %d \n", 
		pptr->nfilts ); 
        fprintf(stderr,"pole position = %f \n", pptr->polepos );
        fprintf(stderr,"upper cutoff frq = %f Hz\n", pptr->fcupper );
        fprintf(stderr,"lower cutoff frq = %f Hz\n", pptr->fclower );
        fprintf(stderr,"model order = %d \n", pptr->order );
        fprintf(stderr,"number of params = %d \n", pptr->nout );
        fprintf(stderr,"liftering exponent = %f \n", pptr->lift );
        fprintf(stderr,"windowing coefficient = %f \n", pptr->winco );
        fprintf(stderr,"fraction of rasta = %f \n", pptr->rfrac );
        fprintf(stderr,"Jah value = %g \n", pptr->jah );
        fprintf(stderr,"gainflag = %d \n", pptr->gainflag );
        fprintf(stderr,"log rasta flag = %d \n", pptr->lrasta );
        fprintf(stderr,"jah rasta flag = %d \n", pptr->jrasta );
        fprintf(stderr,"input file name = %s \n", pptr->infname?pptr->infname:"(none)");
        fprintf(stderr,"output file name = %s \n", pptr->outfname?pptr->outfname:"(none)");
        fprintf(stderr,"ascii input flag = %d \n", pptr->ascin );
        fprintf(stderr,"ascii output flag = %d \n", pptr->ascout );
        fprintf(stderr,"espsin = %d \n", pptr->espsin );
        fprintf(stderr,"espsout = %d \n", pptr->espsout );
        fprintf(stderr,"smallmask flag = %d \n", pptr->smallmask );
        fprintf(stderr,"online flag = %d \n", pptr->online );
        fprintf(stderr,"debug flag = %d \n", pptr->debug );
}

void show_param( struct fvec *vptr, struct param *pptr)
{
        fprintf(stderr,"Nyquist freq in barks = %f\n", pptr->nyqbar );
        fprintf(stderr,"first good filters is = %d\n", pptr->first_good );
        fprintf(stderr,"window size = %d points\n", pptr->winpts );
        fprintf(stderr,"step size = %d points\n", pptr->steppts );
        fprintf(stderr,"n frames is = %d \n", pptr->nframes );
        fprintf(stderr,"frame length = %d points\n", vptr->length );
}

void show_vec ( const struct param *pptr, struct fvec *sptr)
{
        fprintf(stderr,"\nLength of vector is %d\n", 
		sptr->length);

        print_vec(pptr, stderr, sptr, 8 );

        printf("\n");
}
