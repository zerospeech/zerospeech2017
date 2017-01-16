/*
 * $Header: /u/drspeech/repos/pfile_utils/eig.h,v 1.1.1.1 2002/11/08 19:47:06 bdecker Exp $
 * Eigenanalysis
 *
 * Converted to "C" by
 *  Jeff Bilmes <bilmes@icsi.berkeley.edu>
 *
 */


#ifndef EIG_H
#define EIG_H

extern void
eigenanalyasis(int n,
	       double *cor, /* nxn real symmetric covariance matrix */
	       double *vals,   /* n-vector, space for eigenvalues */
	       double *vecs); 

#endif
