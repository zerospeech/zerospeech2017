/*
**
**
** $Header: /u/drspeech/repos/pfile_utils/MixBiNormal_chelp.c,v 1.2 2012/01/05 21:47:22 dpwe Exp $
** 
**   C langauge helper functions for the C++ class MixBiNormal.cc
**   A mixture of bi-variate normal class.
**   Includes the EM algorithm with loop iteration controlled and
**   data provided by outside of the class.
**   This file contains functions from MixBiNormal.cc, but written in
**   C, that correspond to the inner-most loops and can be compiled
**   by, potentially, a more aggressive optimizing C (rather than C++) compiler.
**
**   $Header: /u/drspeech/repos/pfile_utils/MixBiNormal_chelp.c,v 1.2 2012/01/05 21:47:22 dpwe Exp $
** 
** For documentation, see ICSI TR-97-021, May 1997.
*/

#include "pfile_utils.h"

#include <float.h> /* for DBL_MIN */

static int one = 1;
#ifdef HAVE_MVEC
extern void vexp_(int *n, double *x, int *stridex, double  *y,  int
     *stridey);
#define MYVEXP vexp_
#else
void MYVEXP(int *n, double *x, int *stridex, double *y, int *stridey)
{
  double *x_endp = x+(*n);
  while (x != x_endp) {
    *y++ = exp(*x++);
  }  
}
#endif


/* define this to be 1 if, e.g., we want to check for denormals
** occuring. This should be necesary if we can turn on automatic 
** hardware FPU denormal truncation to zero.
*/
#define CHECK_FOR_MIN_EXP 0
/* The value of the minimum result of exp() that we
**  care about. 
*/
#define MIN_EXP_RESULT (1e-200)

/*
** return the value of P(X|l,G) without the normalizing
** constant, for each of the pairs 
** (x0[0],x1[0]),(x0[s],x1[s]), ... (x0[(n-1)*s],x1[(n-1)*s])
** where s is the stride.
** res{,a} and scratch must be length >= n
** This function corresponds to:
** void MixBiNormal::pg(double *x0,double *x1,int l,int n,double alpha,
**                      double *scratch,double *res,double *resa);
**
*/
void
pg_c1(double *x0,double *x1,
		int l,
		int n,
		double alpha,
		double *scratch,
		double *res,
		double *resa,
		const double u0,
		const double u1,
		const double s0,
		const double s2,
		const double ntw_s1,
		const double n_inv_det_2,
		const double sqrt_inv_det,
		const double norm
		)
{

  /* const double u0 = g_means[l].u[0];
     const double u1 = g_means[l].u[1];
     const double s0 = g_covars[l].s[0];
     const double s1 = g_covars[l].s[1];
     const double s2 = g_covars[l].s[2];
     negative twice s1
     const double ntw_s1 = g_covars_help[l].h[0];
     */

  /*
    determinant values.
    // Negative of half of inverse determinant.
    const double n_inv_det_2 = g_covars_help[l].h[1];
    // sqrt of inverse determinant.
    const double sqrt_inv_det = g_covars_help[l].h[2];
    // more pre-computed values.
    const double norm = alpha*sqrt_inv_det;
    
    // for each pair, return
    // (1/det(A)^0.5)exp(-0.5(X-U)^T E^(-1) (X-U))
    // where E is the covariance matrix.
    */

  double *x1p = x1;
  double *x0p = x0;
  const double *x1_endp = x1+n;
  double *scratchp = scratch;
  double *resp;
  double *resap;
  double *res_endp;


  do {
    const double d1 = (*x1p)-u1;
    const double d0 = (*x0p)-u0;
    *scratchp++ = n_inv_det_2*(d0*d0*s2 + (d0*ntw_s1 + d1*s0)*d1);
    x0p ++; x1p ++;
  } while (x1p != x1_endp);

  /* fpsetmask( fpgetmask() & ~ FP_X_UFL ); */
  MYVEXP(&n,scratch,&one,res,&one);
  /* fpsetmask( fpgetmask() | FP_X_UFL ); */

  resp = res;
  resap = resa;
  res_endp = res+n;
  do {
#if CHECK_FOR_MIN_EXP
    double tmp;
    if (*resp <= MIN_EXP_RESULT) 
      tmp = 0.0;
    else 
      tmp = (*resp) * norm;
#else
    double tmp = (*resp) * norm;
#endif
    /* accumulate result into resa */
    (*resap) += tmp;
    /* place result into res */
    (*resp) = tmp;
    resap++;
    resp++;
  } while (resp != res_endp);
}




/*
** This function corresponds to:
** void MixBiNormal::pg(double x0,double *x1,int l,int n,double alpha,
**                      double *scratch,double *res);
*/
void
pg_c2(double x0,double *x1,int l,int n,double alpha,
		   double *scratch,double *res,
		   const double u0,
		   const double u1,
		   const double s0,
		   const double s2,
		   const double ntw_s1,
		   const double d0,
		   const double n_inv_det_2,
		   const double sqrt_inv_det,
		   const double d0d0s2,
		   const double d0ntw_s1,
		   const double norm)
{

  double *x1p = x1;
  const double *x1_endp = x1+n;
  double *scratchp = scratch;
  double *resp;
  double *res_endp;
  do {
    const double d1 = (*x1p)-u1;
    *scratchp++ = n_inv_det_2*(d0d0s2 + (d0ntw_s1 + d1*s0)*d1);
    x1p++;
  } while (x1p != x1_endp);
    
  /* fpsetmask( fpgetmask() & ~ FP_X_UFL ); */
  MYVEXP(&n,scratch,&one,res,&one);
  /* fpsetmask( fpgetmask() | FP_X_UFL ); */

  resp = res;
  res_endp = res+n;
  do {
#if CHECK_FOR_MIN_EXP
    if (*resp <= MIN_EXP_RESULT) 
      *resp = 0;
    else 
      (*resp) *= norm;
#else
    (*resp) *= norm;
#endif
    resp ++;
  } while (resp != res_endp);

}


/*
** This function corresponds to:
** void MixBiNormal::pg(double x0,double *x1,int l,int n,double alpha,
**    	                double *scratch1,double *scratch2,double *resa);
*/
void
pg_c3(double x0,
      double *x1,
      int l,
      int n,
      double alpha,
      double *scratch1,
      double *scratch2,
      double *resa,
      const double u0,
      const double u1,
      const double s0,
      const double s2,
      const double ntw_s1,
      const double d0,
      const double n_inv_det_2,
      const double sqrt_inv_det,
      const double d0d0s2,
      const double d0ntw_s1,
      const double norm)
{

  double *x1p = x1;
  const double *x1_endp = x1+n;
  double *scratch1p = scratch1;
  double *resap = resa;
  double *resa_endp = resa+n;
  double *scratch2p = scratch2;
  do {
    const double d1 = (*x1p)-u1;
    *scratch1p++ = n_inv_det_2*(d0d0s2 + (d0ntw_s1 + d1*s0)*d1);
    x1p++;
  } while (x1p != x1_endp);
    
  /* fpsetmask( fpgetmask() & ~ FP_X_UFL ); */
  MYVEXP(&n,scratch1,&one,scratch2,&one);
  /* fpsetmask( fpgetmask() | FP_X_UFL ); */

  resap = resa;
  resa_endp = resa+n;
  scratch2p = scratch2;
  do {
#if CHECK_FOR_MIN_EXP
    double tmp;
    if (*scratch2p <= MIN_EXP_RESULT)
      tmp = 0;
    else 
      tmp = (*scratch2p)*norm;
#else
    double tmp = (*scratch2p)*norm;
#endif

    (*resap) += tmp;
    resap ++; scratch2p++;
  } while (resap != resa_endp);

}

