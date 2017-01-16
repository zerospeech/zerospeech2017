//
// $Header: /u/drspeech/repos/pfile_utils/MixBiNormal.cc,v 1.2 2012/01/05 20:32:01 dpwe Exp $
// 
// A mixture of bi-variate normal class.
//   Includes the EM algorithm with loop iteration controlled and
//   data provided by outside of the class.
//
// Written by: 
//       Jeff Bilmes <bilmes@icsi.berkeley.edu>
// 
// For full documentation, 
//     see ICSI TR-97-021, May 1997.
//     ftp://ftp.icsi.berkeley.edu/pub/techreports/1997/tr-97-021.ps
// 

#include "pfile_utils.h"

#include "rand.h"
#include "MixBiNormal.h"
#include "error.h"

// Read the logic.  If you want ieeefp.h, define HAVE_IEEEFP_H
// (or, better, use autoconf to set it for you)
#ifdef HAVE_SYS_IEEEFP_H
#  include <sys/ieeefp.h>
#else
#  ifdef HAVE_IEEEFP_H
#    include <ieeefp.h>
#  endif
#endif

// ======================================================================
// ======================================================================
// ----------  Interface to external optimized C routines. --------------
// ======================================================================
// ======================================================================

extern "C" {
#ifdef HAVE_MVEC
void vexp_(int *n, double *x, int *stridex, double  *y,  int
     *stridey);
void vlog_(int *n, double *x, int *stridex, double  *y,  int
     *stridey);
#endif
void pg_c1(double *x0,double *x1,
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
		);

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
		   const double norm);
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
      const double norm);

}

inline void
vexp(int n,double *x, double *y)
{
#ifdef HAVE_MVEC
  static int one = 1;
  vexp_(&n,x,&one,y,&one);
#else
  double *x_endp = x+n;
  while (x != x_endp) {
    *y++ = exp(*x++);
  }
#endif
}

// What we call log(~0). If log-likelihoods get 
// smaller than this (which is very unlikely), an error could occur.
#define LOG_ZERO (-1e250)

// define this to be 1 if, e.g., we want to check for denormals
// occuring. This should be necesary if we can turn on automatic 
// hardware FPU denormal truncation to zero.
#define CHECK_FOR_POST_THRESHOLD 0
// posterior prob threshold. If a posterior gets smaller than
// this, we don't bother using it for the accumulation.
#define POST_THRESHOLD 1e-100

inline void
vlog(int n,double *x, double *y)
{
#ifdef HAVE_MVEC
  static int one = 1;
  vlog_(&n,x,&one,y,&one);
#else
  double *x_endp = x+n;
  while (x != x_endp) {
    *y++ = log(*x++);
  }
#endif
}



// ======================================================================
// ----------  MixBiNormal static members -------------------------------
// ======================================================================


//
// The default number of components if
// none is specified.
int
MixBiNormal::GlobalNumComps = 1;

int
MixBiNormal::reRandsPerMixCompRedux = 5;

bool
MixBiNormal::reRandomizeOnlyOneComp = false;

bool
MixBiNormal::noReRandomizeOnDrop = false;

double
MixBiNormal::varianceFloor = 0.0;

double
MixBiNormal::detFloor = DBL_MIN;

// The implicit assumption here is that there
// will never be more than 1000 mixture components
// to keep any coef > DBL_MIN.
double
MixBiNormal::mixtureCoeffVanishNumerator = 1000.0*DBL_MIN;

double*
MixBiNormal::scratch1 = 0;
double*
MixBiNormal::scratch2 = 0;
double*
MixBiNormal::scratch3 = 0;
double*
MixBiNormal::scratch4 = 0;
int MixBiNormal::scratch_len = 0;

MixBiNormal::MixBiNormal(int ncomps)
  : OrigNumMixComps(ncomps)
{

  if (OrigNumMixComps < 1)
    error("Number of mixing components must be >= 1.");

  if (OrigNumMixComps > ((1 << sizeof(unsigned char)*8)-1))
    error("Number of mixing components must be <= %d",
	  ((1 << sizeof(unsigned char)*8)-1));

  NumMixComps = OrigNumMixComps;
  cur_alphas = new double[NumMixComps];
  cur_means = new Mean[NumMixComps];
  cur_covars = new CoVar[NumMixComps];
  cur_covars_help = new CoVarHelp[NumMixComps];
  cur_chol = NULL;

  next_alphas = new double[NumMixComps];
  next_means = new Mean[NumMixComps];
  next_covars = new CoVar[NumMixComps];
  likelihoods = new double[NumMixComps];
  bitmask = bm_initState;

  llikelihood = LOG_ZERO;
  totalCurrentReRands = 0;

  randomizeCurMixtures();
  prepareCurrent();

  setActive();
  setDirty();

}


MixBiNormal::~MixBiNormal()
{
  delete [] cur_alphas;
  delete [] cur_means;
  delete [] cur_covars;
  delete [] cur_covars_help;
  delete [] next_alphas;
  delete [] next_means;
  delete [] next_covars;
  delete [] likelihoods;
  if (cur_chol != NULL)
    delete [] cur_chol;
  if (scratch1 != 0) {
    delete [] scratch1;
    scratch1 = 0;
  }
  if (scratch2 != 0) {
    delete [] scratch2;  
    scratch2 = 0;  
  }
  if (scratch3 != 0) {
    delete [] scratch3;  
    scratch3 = 0;  
  }
  if (scratch4 != 0) {
    delete [] scratch4;
    scratch4 = 0;
  }
  scratch_len = 0;
}




// ======================================================================
// ----------  Miscelaneous routines ------------------------------------
// ======================================================================


void
MixBiNormal::randomizeCurMixture(int l)
{
  // None of the mixture coefficients should be zero.
  cur_alphas[l] = 1.0;

  // The means can be anything really.
  cur_means[l].u[0] = 2*drand48()-1;
  cur_means[l].u[1] = 2*drand48()-1;

  // We must have s0*s2 - s1*s1 > 0 and s0>0 and s2>0
  // for positive definiteness.
  cur_covars[l].s[0] = 1.0+drand48()+2.0*varianceFloor + 2.0*detFloor;
  cur_covars[l].s[1] = drand48()+varianceFloor + detFloor;
  cur_covars[l].s[2] = 1.0+drand48()+2.0*varianceFloor + 2.0*detFloor;
}

void
MixBiNormal::normalizeAlphas()
{
  // Normalize alphas.
  int l;
  double tmp = 0.0;
  for (l=0;l<NumMixComps;l++) {
    tmp += cur_alphas[l];
  }  
  const double inv_tmp = 1.0/tmp;
  for (l=0;l<NumMixComps;l++) {
    cur_alphas[l] *= inv_tmp;
  }
}


void
MixBiNormal::normalizeAlphasWithNoise()
{
  // Normalize alphas.
  int l;
  double tmp = 0.0;
  for (l=0;l<NumMixComps;l++) {
    tmp += cur_alphas[l];
  }  

  // added to each one.
  double noise = tmp/NumMixComps;
  tmp += tmp;

  const double inv_tmp = 1.0/tmp;
  for (l=0;l<NumMixComps;l++) {
    cur_alphas[l] += noise;
    cur_alphas[l] *= inv_tmp;
  }
}

void
MixBiNormal::randomizeCurMixtures()
{
  int l;
  for (l=0;l<NumMixComps;l++)
    randomizeCurMixture(l);
  normalizeAlphas();
}

void
MixBiNormal::eliminateMixture(int l)
{
  // Shift all mixtures greater than l over, 
  // reduce NumMixComps,
  // and renormalize the alphas.

  int i;
  for (i=l;i<(NumMixComps-1);i++) {
    cur_alphas[i] = cur_alphas[i+1];
    cur_means[i] = cur_means[i+1];
    cur_covars[i] = cur_covars[i+1];
  }
  NumMixComps--;
  normalizeAlphas();
}




void
MixBiNormal::setScratchSize(const int n, 
			    const int numComps)
{
  if (n > scratch_len) {
    delete [] scratch1;
    delete [] scratch2;  
    delete [] scratch3;  
    delete [] scratch4;  
    scratch1 = new double[n*numComps];
    scratch2 = new double[n*numComps];
    scratch3 = new double[n*numComps];
    scratch4 = new double[n*numComps];
    scratch_len = n;
  }
}


// Return the relative different (in %) between the 
// the current and the previous log-likelihood of the
// data. Note that if this routine changes, we'll potentially need to
// change all the expliict settings of prev_llikelihood and llikelihood
// in this file.
double
MixBiNormal::llPercDiff()
{
  if (fabs(prev_llikelihood) < DBL_MIN)
    error("Can't compute llPercDiff, prev_llikelihood = %e\n",prev_llikelihood);
  return 100.0*( (llikelihood - prev_llikelihood)/fabs(llikelihood));
}


void
MixBiNormal::swapNextWithCur()
{
  // swap the old with the new, so the 'cur' becomes the next guess
  // and 'next' holds the previous ones.
  double *t_alphas;
  Mean  *t_means;
  CoVar *t_covars;
  t_alphas=cur_alphas;cur_alphas=next_alphas;next_alphas=t_alphas;
  t_means=cur_means;cur_means=next_means;next_means=t_means;
  t_covars=cur_covars;cur_covars=next_covars;next_covars=t_covars;

}

void
MixBiNormal::copyCurToNext()
{
  int l;
  for (l=0;l<NumMixComps;l++) {
    next_alphas[l] = cur_alphas[l];
    next_means[l] = cur_means[l];
    next_covars[l] = cur_covars[l];
  }
}


void
MixBiNormal::copyNextToCur()
{
  int l;
  for (l=0;l<NumMixComps;l++) {
    cur_alphas[l] = next_alphas[l];
    cur_means[l] = next_means[l];
    cur_covars[l] = next_covars[l];
  }
}

// ======================================================================
// ----------  Probability Calculation of various forms -----------------
// ======================================================================


// Return the value of P(x[0]|l,G) without the normalizing
// constant.
double
MixBiNormal::pgx0(double x0,int l)
{
  const double u0 = cur_means[l].u[0];
  const double d0 = x0 - u0;
  const double inv_sig = cur_chol[l].c[3];
  const double n_half_inv_sqsig = cur_chol[l].c[4];  

  // return
  // (1/sigma)exp(-0.5(X-U)^2/sigma^2)

  return 
    inv_sig*exp(d0*d0*n_half_inv_sqsig);

}

double
MixBiNormal::pgx1(double x1,int l)
{
  const double u1 = cur_means[l].u[1];
  const double d1 = x1 - u1;
  const double inv_sig = cur_chol[l].c[5];
  const double n_half_inv_sqsig = cur_chol[l].c[6];

  // return
  // (1/sigma)exp(-0.5(X-U)^2/sigma^2)

  return 
    inv_sig*exp(d1*d1*n_half_inv_sqsig);
}

double
MixBiNormal::pgx0(double x0)
{
  double tmp = 0.0;
  for (int l=0;l<NumMixComps;l++) 
    tmp += cur_alphas[l]*pgx0(x0,l);
  return tmp;
}

double
MixBiNormal::pgx1(double x1)
{
  double tmp = 0.0;
  for (int l=0;l<NumMixComps;l++) 
    tmp += cur_alphas[l]*pgx1(x1,l);
  return tmp;
}


//
// return p(x0)
// for all len elements in x0 and put result in
// res. All arrays must be length 'len'.
void
MixBiNormal::pgx0(double *x0,int len,
		  double *scratch1,
		  double *scratch2,
		  double *res)
{
  double *scratch_p;
  double *scratch1_endp = scratch1 + len;
  double *scratch2_endp = scratch2 + len;
  double *res_p;
  
  ::memset(res,0,len*sizeof(double));

  for (int l = 0;l<NumMixComps;l++) {
      const double u0 = cur_means[l].u[0];
      const double inv_sig = cur_chol[l].c[3];
      const double n_half_inv_sqsig = cur_chol[l].c[4];
      const double alpha = cur_alphas[l];

      // put the diffs into scratch
      double *x0_p = x0;
      scratch_p = scratch1;
      do {
	const double d0 = *x0_p++ - u0;
	*scratch_p++ = d0*d0*n_half_inv_sqsig;
      } while (scratch_p != scratch1_endp);
      vexp(len,scratch1,scratch2);

      res_p = res;
      scratch_p = scratch2;
      do {
	*res_p++  += alpha*inv_sig * (*scratch_p++);
      } while (scratch_p != scratch2_endp);
  }
}

// same as above but for x1
void
MixBiNormal::pgx1(double *x1,int len,
		  double *scratch1,
		  double *scratch2,
		  double *res)
{
  double *scratch_p;
  double *scratch1_endp = scratch1 + len;
  double *scratch2_endp = scratch2 + len;
  double *res_p;
  
  ::memset(res,0,len*sizeof(double));

  for (int l = 0;l<NumMixComps;l++) {
      const double u1 = cur_means[l].u[1];
      const double inv_sig = cur_chol[l].c[5];
      const double n_half_inv_sqsig = cur_chol[l].c[6];
      const double alpha = cur_alphas[l];

      // put the diffs into scratch
      double *x1_p = x1;
      scratch_p = scratch1;
      do {
	const double d1 = *x1_p++ - u1;
	*scratch_p++ = d1*d1*n_half_inv_sqsig;
      } while (scratch_p != scratch1_endp);
      vexp(len,scratch1,scratch2);

      res_p = res;
      scratch_p = scratch2;
      do {
	*res_p++  += alpha*inv_sig * (*scratch_p++);
      } while (scratch_p != scratch2_endp);
  }
}





// return the value of P(X|l,G) without the normalizing
// constant.
double
MixBiNormal::pg(double x0,double x1,int l)
{
  const double u0 = cur_means[l].u[0];
  const double u1 = cur_means[l].u[1];
  const double s0 = cur_covars[l].s[0];
  // const double s1 = cur_covars[l].s[1];
  const double s2 = cur_covars[l].s[2];
  // negative twice s1
  const double ntw_s1 = cur_covars_help[l].h[0];
  const double d0 = x0 - u0;
  const double d1 = x1 - u1;

  // determinant values.
  // Negative of half of inverse determinant.
  const double n_inv_det_2 = cur_covars_help[l].h[1];
  // sqrt of inverse determinant.
  const double sqrt_inv_det = cur_covars_help[l].h[2];

  // return
  // (1/det(A)^0.5)exp(-0.5(X-U)^T E^(-1) (X-U))
  // where E is the covariance matrix.

  return 
    sqrt_inv_det*
    exp(n_inv_det_2*(d0*(d0*s2 + d1*ntw_s1) + d1*d1*s0));

}



// return the value of P(X|l,G) without the normalizing
// constant, for each of the pairs 
// (x0,x1[0]),(x0,x1[1]), ... (x0,x1[n-1])
// res and scratch must be length >= n
void
MixBiNormal::pg(double x0,double *x1,int l,int n,double alpha,
		double *scratch,double *res)
{
  const double u0 = cur_means[l].u[0];
  const double u1 = cur_means[l].u[1];
  const double s0 = cur_covars[l].s[0];
  // const double s1 = cur_covars[l].s[1];
  const double s2 = cur_covars[l].s[2];
  // negative twice s1
  const double ntw_s1 = cur_covars_help[l].h[0];
  const double d0 = x0 - u0;

  // determinant values.
  // Negative of half of inverse determinant.
  const double n_inv_det_2 = cur_covars_help[l].h[1];
  // sqrt of inverse determinant.
  const double sqrt_inv_det = cur_covars_help[l].h[2];

  // more pre-computed values.
  const double d0d0s2 = d0*d0*s2;
  const double d0ntw_s1 = d0*ntw_s1;
  const double norm = alpha*sqrt_inv_det;

  // for each pair, return
  // (1/det(A)^0.5)exp(-0.5(X-U)^T E^(-1) (X-U))
  // where E is the covariance matrix.

  double *x1p = x1;
  const double *x1_endp = x1+n;
  double *scratchp = scratch;
  do {
    const double d1 = (*x1p)-u1;
    *scratchp++ = n_inv_det_2*(d0d0s2 + (d0ntw_s1 + d1*s0)*d1);
    x1p++;
  } while (x1p != x1_endp);
    
  vexp(n,scratch,res);

  double *resp = res;
  double *res_endp = res+n;
  do {
    (*resp) *= norm;
    resp ++;
  } while (resp != res_endp);

}



// return the value of P(X|l,G) without the normalizing
// constant, for each of the pairs 
// (x0,x1[0]),(x0,x1[1]), ... (x0,x1[n-1])
// resa and scratch{1,2} must be length >= n
void
MixBiNormal::pg(double x0,
		double *x1,
		int l,
		int n,
		double alpha,
		double *scratch1,
		double *scratch2,
		double *resa  // accumulate result into here.
		)
{
  const double u0 = cur_means[l].u[0];
  const double u1 = cur_means[l].u[1];
  const double s0 = cur_covars[l].s[0];
  // const double s1 = cur_covars[l].s[1];
  const double s2 = cur_covars[l].s[2];
  // negative twice s1
  const double ntw_s1 = cur_covars_help[l].h[0];
  const double d0 = x0 - u0;

  // determinant values.
  // Negative of half of inverse determinant.
  const double n_inv_det_2 = cur_covars_help[l].h[1];
  // sqrt of inverse determinant.
  const double sqrt_inv_det = cur_covars_help[l].h[2];

  // more pre-computed values.
  const double d0d0s2 = d0*d0*s2;
  const double d0ntw_s1 = d0*ntw_s1;
  const double norm = alpha*sqrt_inv_det;

  // for each pair, return
  // (1/det(A)^0.5)exp(-0.5(X-U)^T E^(-1) (X-U))
  // where E is the covariance matrix.

  double *x1p = x1;
  const double *x1_endp = x1+n;
  double *scratch1p = scratch1;
  do {
    const double d1 = (*x1p)-u1;
    *scratch1p++ = n_inv_det_2*(d0d0s2 + (d0ntw_s1 + d1*s0)*d1);
    x1p++;
  } while (x1p != x1_endp);
    
  vexp(n,scratch1,scratch2);

  double *resap = resa;
  double *resa_endp = resa+n;
  double *scratch2p = scratch2;
  do {
    double tmp = (*scratch2p)*norm;
    (*resap) += tmp;
    resap ++; scratch2p++;
  } while (resap != resa_endp);

}


// return the value of P(X|l,G) without the normalizing
// constant, for each of the pairs 
// (x0[0],x1[0]),(x0[s],x1[s]), ... (x0[(n-1)*s],x1[(n-1)*s])
// where s is the stride.
// res{,a} and scratch must be length >= n
void
MixBiNormal::pg(double *x0,double *x1, // set of n samples
		int l, // the mixture component
		int n, // size of x0 and x1
		double alpha, // mixture coeficient
		double *scratch, // scratch arrays of size >= n
		double *res,       // result (size >= n) to place result
		double *resa       // result (size >= n) to accumulate result
		)
{
  const double u0 = cur_means[l].u[0];
  const double u1 = cur_means[l].u[1];
  const double s0 = cur_covars[l].s[0];
  // const double s1 = cur_covars[l].s[1];
  const double s2 = cur_covars[l].s[2];
  // negative twice s1
  const double ntw_s1 = cur_covars_help[l].h[0];


  // determinant values.
  // Negative of half of inverse determinant.
  const double n_inv_det_2 = cur_covars_help[l].h[1];
  // sqrt of inverse determinant.
  const double sqrt_inv_det = cur_covars_help[l].h[2];

  // more pre-computed values.
  const double norm = alpha*sqrt_inv_det;

  // for each pair, return
  // (1/det(A)^0.5)exp(-0.5(X-U)^T E^(-1) (X-U))
  // where E is the covariance matrix.

  double *x1p = x1;
  double *x0p = x0;
  const double *x1_endp = x1+n;
  double *scratchp = scratch;
  do {
    const double d1 = (*x1p)-u1;
    const double d0 = (*x0p)-u0;
    *scratchp++ = n_inv_det_2*(d0*d0*s2 + (d0*ntw_s1 + d1*s0)*d1);
    x0p ++; x1p ++;
  } while (x1p != x1_endp);
    
  vexp(n,scratch,res);

  double *resp = res;
  double *resap = resa;
  double *res_endp = res+n;
  do {
    double tmp = (*resp) * norm;
    // accumulate result into resa
    (*resap) += tmp;
    // place result into res
    (*resp) = tmp;
    resap++;
    resp++;
  } while (resp != res_endp);

}



// return the value of P(X|G) without the normalizing
// constant, for each of the pairs 
// (x0[0],x1[0]),(x0[1],x1[1]), ... (x0[(n-1)],x1[(n-1)])
// res{,a} and scratch must be length >= n
void
MixBiNormal::pg(double *x0,double *x1, // set of n samples
		int n, // size of x0 and x1
		double *scratch1, // scratch arrays of size >= n
		double *scratch2, // scratch arrays of size >= n
		double *res       // result (size >= n) to place result
		)
{
  int l;

  ::memset(res,0,n*sizeof(double));

  for (l=0;l<NumMixComps;l++) {

    const double u0 = cur_means[l].u[0];
    const double u1 = cur_means[l].u[1];
    const double s0 = cur_covars[l].s[0];
    const double s2 = cur_covars[l].s[2];
    // negative twice s1
    const double ntw_s1 = cur_covars_help[l].h[0];


    // determinant values.
    // Negative of half of inverse determinant.
    const double n_inv_det_2 = cur_covars_help[l].h[1];
    // sqrt of inverse determinant.
    const double sqrt_inv_det = cur_covars_help[l].h[2];

    const double alpha = cur_alphas[l];

    // more pre-computed values.
    const double norm = alpha*sqrt_inv_det;

    // for each pair, return
    // (1/det(A)^0.5)exp(-0.5(X-U)^T E^(-1) (X-U))
    // where E is the covariance matrix.

    double *x1p = x1;
    double *x0p = x0;
    const double *x1_endp = x1+n;
    double *scratch1p = scratch1;
    do {
      const double d1 = (*x1p)-u1;
      const double d0 = (*x0p)-u0;
      *scratch1p++ = n_inv_det_2*(d0*d0*s2 + (d0*ntw_s1 + d1*s0)*d1);
      x0p ++; x1p ++;
    } while (x1p != x1_endp);
    
    vexp(n,scratch1,scratch2);

    double *scratch2_p = scratch2;
    double *res_p = res;
    double *res_endp = res+n;
    do {
      double tmp = (*scratch2_p) * norm;
      // accumulate result into res
      (*res_p) += tmp;
      res_p++;
      scratch2_p ++;
    } while (res_p != res_endp);
  }
}





// return the value of P(X|G) without the normalizing
// constant
double
MixBiNormal::pg(double x0,double x1)
{
  double tmp = 0.0;
  for (int l=0;l<NumMixComps;l++) 
    tmp += cur_alphas[l]*pg(x0,x1,l);
  return tmp;
}


// return the value of P(X|G) without the normalizing
// constant.
// res and scratch{1,2} must be length >= n
void
MixBiNormal::pg(double x0,double *x1, int n, 
		double *scratch1, double* scratch2, double *res)
{
  // do l=0 case, setting res.
  int l=0;

  // assign first mixture component
  {
    // Instead of the call:
    //    pg(x0,x1,l,n,cur_alphas[l],scratch1,res);
    // do the following instead.

    const double u0 = cur_means[l].u[0];
    const double u1 = cur_means[l].u[1];
    const double s0 = cur_covars[l].s[0];
    // const double s1 = cur_covars[l].s[1];
    const double s2 = cur_covars[l].s[2];
    // negative twice s1
    const double ntw_s1 = cur_covars_help[l].h[0];
    const double d0 = x0 - u0;
    
    // determinant values.
    // Negative of half of inverse determinant.
    const double n_inv_det_2 = cur_covars_help[l].h[1];
    // sqrt of inverse determinant.
    const double sqrt_inv_det = cur_covars_help[l].h[2];

    // more pre-computed values.
    const double d0d0s2 = d0*d0*s2;
    const double d0ntw_s1 = d0*ntw_s1;
    const double norm = cur_alphas[l]*sqrt_inv_det;

    pg_c2(x0,x1,l,n,cur_alphas[l],scratch1,res,
	  u0,u1,s0,s2,ntw_s1,d0,n_inv_det_2,sqrt_inv_det,
	  d0d0s2,d0ntw_s1,norm);
  }

  // accumulate in the rest.
  for (l=1;l<NumMixComps;l++) {
    // Instead of the call:
    //   pg(x0,x1,l,n,cur_alphas[l],scratch1,scratch2,res);
    // do the following instead.

    const double u0 = cur_means[l].u[0];
    const double u1 = cur_means[l].u[1];
    const double s0 = cur_covars[l].s[0];
    // const double s1 = cur_covars[l].s[1];
    const double s2 = cur_covars[l].s[2];
    // negative twice s1
    const double ntw_s1 = cur_covars_help[l].h[0];
    const double d0 = x0 - u0;
    
    // determinant values.
    // Negative of half of inverse determinant.
    const double n_inv_det_2 = cur_covars_help[l].h[1];
    // sqrt of inverse determinant.
    const double sqrt_inv_det = cur_covars_help[l].h[2];

    // more pre-computed values.
    const double d0d0s2 = d0*d0*s2;
    const double d0ntw_s1 = d0*ntw_s1;
    const double norm = cur_alphas[l]*sqrt_inv_det;

    pg_c3(x0,x1,l,n,cur_alphas[l],scratch1,scratch2,res,
       u0,u1,s0,s2,ntw_s1,d0,n_inv_det_2,sqrt_inv_det,
       d0d0s2,d0ntw_s1,norm);
  }
}



// ======================================================================
// ----------  EM parameter estimation  algorithm -----------------------
// ======================================================================


// Prepare the helper values for the current mixture parameters,
// and check to make sure the current parameters are valid.
// If not, re-randomize the current mixture parameters.
bool
MixBiNormal::prepareCurrent()
{
  int l=0;
  bool mix_comp_drop = false;
start:
  if (totalCurrentReRands >= reRandsPerMixCompRedux) {
    if (NumMixComps > 1) {
      totalCurrentReRands = 0;
      mix_comp_drop = true;

      if (noReRandomizeOnDrop) {
	// Either l has been set from below, or
	// l == 0
	eliminateMixture(l);
      } else {
	NumMixComps--;
	// randomize with the new num mixture components.
	randomizeCurMixtures();
      }

      // Make sure ll difference is very big. This forces us
      // to continue (i.e., this mixture has not converged).
      prev_llikelihood = 2*LOG_ZERO;
      llikelihood = LOG_ZERO;
    } else {
      // We've reached the total number of rerandomziations and we
      // only have a single component. We assume this must be caused by
      // a singular (or degenerate) distribution (i.e., one of the
      // dimensions linearly predicts the other). We set up a simple, valid, 
      // one-component large-MI distribution.

      // We must have s0*s2 - s1*s1 > 0 and s0>0 and s2>0
      // For now, use a covariance matrix that
      // produces a very large MI values, i.e., 7.6471 bits for
      // a grid of 250 and variance range of 2.4
      // Such values should be ignored.

      // Could also uncomment the following but
      // that would strip away info in the .mg file.
      // cur_alphas[0] = 1.0;
      // cur_means[0].u[0] = 0;
      // cur_means[0].u[1] = 0;
      // cur_covars[0].s[0] = cur_covars[0].s[2] = 1.0;
      cur_covars[0].s[1] = 
	sqrt(cur_covars[0].s[0]*cur_covars[0].s[2]*(1.0-1e-10));

      // We're done, so set up the current and previous
      // log likelihoods to say that there has been no change. 
      // I.e., This forces us to stop (i.e., this mixture has "converged").
      // Must set to the same value, and the value can be any 
      // real number other than zero.
      prev_llikelihood = llikelihood = -100;
    }
  }

  for (l=0;l<NumMixComps;l++) {
    // pre-compute some values of the guessed parameters
    const double s0 = cur_covars[l].s[0];
    const double s1 = cur_covars[l].s[1];
    const double s2 = cur_covars[l].s[2];

    // Make various checks to see if thiscomponent is going singular.
    // If so, re-randomize and hope for the best.
    if (s0 <= varianceFloor || s2 <= varianceFloor) {
      if (reRandomizeOnlyOneComp) {
	randomizeCurMixture(l);
	normalizeAlphasWithNoise();
      } else
	randomizeCurMixtures();
      totalCurrentReRands++;
      prev_llikelihood = 2*LOG_ZERO;
      llikelihood = LOG_ZERO;
      goto start;
    }
    const double det = (s0*s2-s1*s1);
    if (det <= detFloor || det <= DBL_MIN) {
      if (NumMixComps > 1) {
	// If there's only 1 comp. left,
	// this might be a dataset with
	// just a linear dependence.
	if (reRandomizeOnlyOneComp) {
	  randomizeCurMixture(l);
	  normalizeAlphasWithNoise();
	} else
	  randomizeCurMixtures();
      }
      totalCurrentReRands++;
      prev_llikelihood = 2*LOG_ZERO;
      llikelihood = LOG_ZERO;
      goto start;
    }
    const double inv_det = 1.0/det;
    if (!finite(inv_det)) {
      // In theory, we don't need this check if we also
      // do the DBL_MIN check above. We keep this here
      // just in case.
      if (reRandomizeOnlyOneComp) {
	randomizeCurMixture(l);
	normalizeAlphasWithNoise();
      } else
	randomizeCurMixtures();
      totalCurrentReRands++;
      prev_llikelihood = 2*LOG_ZERO;
      llikelihood = LOG_ZERO;
      goto start;
    }
    // check if cur_alphs[l] <= mixtureCoeffVanishNumerator/NumMixComps
    if (NumMixComps*cur_alphas[l] <= mixtureCoeffVanishNumerator || cur_alphas[l] <= DBL_MIN) {
      if (reRandomizeOnlyOneComp) {
	randomizeCurMixture(l);
	normalizeAlphasWithNoise();
      } else
	randomizeCurMixtures();
      totalCurrentReRands++;
      prev_llikelihood = 2*LOG_ZERO;
      llikelihood = LOG_ZERO;
      goto start;
    }

    cur_covars_help[l].h[0] = -2.0*s1;
    cur_covars_help[l].h[1] = -0.5*inv_det;
    cur_covars_help[l].h[2] = sqrt(inv_det);
  }
  return mix_comp_drop;
}

void
MixBiNormal::startEpoch()
{
  n_elements = 0;
  // initialize the new 'next' parameters.
  for (int l=0;l<NumMixComps;l++) {
    next_alphas[l] = 0.0;
    next_means[l].u[0] = next_means[l].u[1] = 0.0;
    next_covars[l].s[0] = next_covars[l].s[1] = 
      next_covars[l].s[2] = 0.0;
  }
  prev_llikelihood = llikelihood;
  llikelihood = 0;
}

void
MixBiNormal::addtoEpoch(double x0,double x1)
{
  if (NumMixComps == 1) {
    next_alphas[0] += 1.0;
    next_means[0].u[0] += x0;
    next_means[0].u[1] += x1;

    const double u0 = cur_means[0].u[0];
    const double u1 = cur_means[0].u[1];
    const double d0 = x0 - u0;
    const double d1 = x1 - u1;

    next_covars[0].s[0] += d0*d0;
    next_covars[0].s[1] += d0*d1;
    next_covars[0].s[2] += d1*d1;
    
    double ll = pg(x0,x1,0);
    if (ll < DBL_MIN)
      ll = DBL_MIN;
    llikelihood += log(ll);
  } else {
    // compute likelihoods
    int l;
    double sum_like = 0.0;
    for (l=0;l<NumMixComps;l++) {
      const double tmp = cur_alphas[l]*pg(x0,x1,l);
      likelihoods[l] = tmp;
      sum_like += tmp;
    }
    if (sum_like < DBL_MIN) {
      llikelihood += log(DBL_MIN);
      // Lacking any evidence to the contrary, we assume
      // uniform posteriors for a presumably outlier data point.
      const double post_l = 1.0/NumMixComps;
      for (l=0;l<NumMixComps;l++) {
	next_alphas[l] += post_l;
	next_means[l].u[0] += post_l*x0;
	next_means[l].u[1] += post_l*x1;
	const double u0 = cur_means[l].u[0];
	const double u1 = cur_means[l].u[1];
	const double d0 = x0 - u0;
	const double d1 = x1 - u1;
	next_covars[l].s[0] += post_l*d0*d0;
	next_covars[l].s[1] += post_l*d0*d1;
	next_covars[l].s[2] += post_l*d1*d1;
      }
    } else {
      llikelihood += log(sum_like);
      const double inv_sum_like = 1.0/sum_like;
      for (l=0;l<NumMixComps;l++) {
	// compute the ith posterior
	const double post_l = likelihoods[l]*inv_sum_like;

#if CHECK_FOR_POST_THRESHOLD
	if (post_l > POST_THRESHOLD) {
#endif
	  next_alphas[l] += post_l;
	  next_means[l].u[0] += post_l*x0;
	  next_means[l].u[1] += post_l*x1;

	  const double u0 = cur_means[l].u[0];
	  const double u1 = cur_means[l].u[1];
	  const double d0 = x0 - u0;
	  const double d1 = x1 - u1;
	
	  next_covars[l].s[0] += post_l*d0*d0;
	  next_covars[l].s[1] += post_l*d0*d1;
	  next_covars[l].s[2] += post_l*d1*d1;
#if CHECK_FOR_POST_THRESHOLD
	}
#endif
      }

    }
  }
  n_elements++;
}


void
MixBiNormal::addtoEpoch(double *x0,double *x1,const int n)
{
  int l;
  if (NumMixComps == 1) {

    // Compute likelihoods, since we need to update the
    // global log likelihood.
    double *likelihoods = scratch1;

    double *accum_likelihoods = scratch2;
    const double *accum_likelihoods_endp = scratch2 + n;
    double *accum_likelihoodsp;

    accum_likelihoodsp = accum_likelihoods;
    do {
      *accum_likelihoodsp++ = 0.0;
    } while (accum_likelihoodsp != accum_likelihoods_endp);

    pg_c1(x0,x1,0,n,cur_alphas[0],scratch3,likelihoods,accum_likelihoods,
	    cur_means[0].u[0],cur_means[0].u[1],cur_covars[0].s[0],cur_covars[0].s[2],
	    cur_covars_help[0].h[0],cur_covars_help[0].h[1],cur_covars_help[0].h[2],
	    cur_alphas[0]*cur_covars_help[0].h[2]);

    // make sure none of the likelihoods are zero
    accum_likelihoodsp = accum_likelihoods;
    do {
      if (*accum_likelihoodsp < DBL_MIN)
	*accum_likelihoodsp = DBL_MIN;
      accum_likelihoodsp++;
    } while (accum_likelihoodsp != accum_likelihoods_endp);

    // compute the logs and place into scratch4.
    vlog(n,accum_likelihoods,scratch4);

    double *x0p,*x1p;
    double *scratch4p;
    double *scratch4_endp;
    l=0;
    double n_u0,n_u1,c_u0,c_u1;
    double d0,d1;
    double s0,s1,s2;
    double lll = 0.0; // local log likelihood
    
    n_u0 = next_means[0].u[0];
    n_u1 = next_means[0].u[1];
    c_u0 = cur_means[0].u[0];
    c_u1 = cur_means[0].u[1];
    s0 = next_covars[0].s[0];
    s1 = next_covars[0].s[1];
    s2 = next_covars[0].s[2];
    x0p = x0; x1p = x1;
    scratch4p = scratch4;
    scratch4_endp = scratch4 + n;

    do {
      n_u0 += *x0p;
      n_u1 += *x1p;
      d0 = (*x0p) - c_u0;
      d1 = (*x1p) - c_u1;
      s0 += d0*d0;
      s1 += d0*d1;
      s2 += d1*d1;
      lll += *scratch4p++;
      x0p ++; x1p ++;
    } while (scratch4p != scratch4_endp);
    next_covars[0].s[0] = s0;
    next_covars[0].s[1] = s1;
    next_covars[0].s[2] = s2;
    next_alphas[0] += n;
    next_means[0].u[0] = n_u0;
    next_means[0].u[1] = n_u1;

    // update global log likelihood
    llikelihood += lll;

  } else { // NumMixComps > 1
    // Compute likelihoods
    double *likelihoods = scratch1;
    double *likelihoodsp;

    double *accum_likelihoods = scratch2;
    const double *accum_likelihoods_endp = scratch2 + n;
    double *accum_likelihoodsp;

    accum_likelihoodsp = accum_likelihoods;
    do {
      *accum_likelihoodsp++ = 0.0;
    } while (accum_likelihoodsp != accum_likelihoods_endp);

    likelihoodsp = likelihoods;
    for (l=0;l<NumMixComps;l++) {
      // Instead of the call:
      //    pg(x0,x1,l,n,cur_alphas[l],scratch3,likelihoodsp,accum_likelihoods);
      // do the following instead.
      pg_c1(x0,x1,l,n,cur_alphas[l],scratch3,likelihoodsp,accum_likelihoods,
	    cur_means[l].u[0],cur_means[l].u[1],cur_covars[l].s[0],cur_covars[l].s[2],
	    cur_covars_help[l].h[0],cur_covars_help[l].h[1],cur_covars_help[l].h[2],
	    cur_alphas[l]*cur_covars_help[l].h[2]);
      likelihoodsp += n;
    }


    // Make sure none of the likelihoods are zero, and
    // if they are, set up to create uniform posteriors.
    accum_likelihoodsp = accum_likelihoods;
    likelihoodsp = likelihoods;
    double *scratch3p = scratch3;
    do {
      if (*accum_likelihoodsp < DBL_MIN) {
	double *likelihoodspp = likelihoodsp;
	l=1;
	do {
	  *likelihoodspp = 1.0;
	  likelihoodspp+=n;
	} while (l++ < NumMixComps);
	*accum_likelihoodsp = NumMixComps;
	*scratch3p = DBL_MIN;
      } else
	*scratch3p = *accum_likelihoodsp;
      accum_likelihoodsp++;
      likelihoodsp++;
      scratch3p++;
    } while (accum_likelihoodsp != accum_likelihoods_endp);

    // compute the log of the likelihoods 
    // and place into scratch4 to be accumulated later.
    vlog(n,scratch3,scratch4);

    double *x0p,*x1p;
    double *scratch4p;
    double c_u0,c_u1,n_u0,n_u1;
    double d0,d1;
    double s0,s1,s2;
    double alpha;
    double lll = 0.0; // local log likelihood

    l=0; // index for first iteration.

    // do the first iteration of the loop here, doing the divisions.
    alpha = next_alphas[l];
    n_u0 = next_means[l].u[0];
    n_u1 = next_means[l].u[1];
    c_u0 = cur_means[l].u[0];
    c_u1 = cur_means[l].u[1];
    s0 = next_covars[l].s[0];
    s1 = next_covars[l].s[1];
    s2 = next_covars[l].s[2];
    accum_likelihoodsp = accum_likelihoods;
    likelihoodsp = likelihoods;
    x0p = x0; x1p = x1;
    scratch4p = scratch4;
    do {
      const double inv_acc_like = 1.0/(*accum_likelihoodsp);
      const double post = (*likelihoodsp)*inv_acc_like;
#if CHECK_FOR_POST_THRESHOLD
      if (post > POST_THRESHOLD) {
#endif
	alpha += post;
	n_u0 += post*(*x0p);
	n_u1 += post*(*x1p);
	d0 = (*x0p) - c_u0;
	d1 = (*x1p) - c_u1;
	s0 += post*d0*d0;
	s1 += post*d0*d1;
	s2 += post*d1*d1;
#if CHECK_FOR_POST_THRESHOLD
      }
#endif
      lll += *scratch4p++;
      likelihoodsp++;
      *accum_likelihoodsp++ = inv_acc_like;
      x0p ++; x1p ++;
    } while (accum_likelihoodsp != accum_likelihoods_endp);
    next_alphas[l] = alpha;
    next_means[l].u[0] = n_u0;
    next_means[l].u[1] = n_u1;
    next_covars[l].s[0] = s0;
    next_covars[l].s[1] = s1;
    next_covars[l].s[2] = s2;

    // update global log likelihood
    llikelihood += lll;

    // do the rest of the loop, with the 
    // 1.0/likelihoods already computed.
    for (l=1;l<NumMixComps;l++) {
      alpha = next_alphas[l];
      n_u0 = next_means[l].u[0];
      n_u1 = next_means[l].u[1];
      c_u0 = cur_means[l].u[0];
      c_u1 = cur_means[l].u[1];
      s0 = next_covars[l].s[0];
      s1 = next_covars[l].s[1];
      s2 = next_covars[l].s[2];
      accum_likelihoodsp = accum_likelihoods;
      x0p = x0; x1p = x1;
      do {
	const double inv_acc_like = (*accum_likelihoodsp);
	const double post = (*likelihoodsp)*inv_acc_like;
#if CHECK_FOR_POST_THRESHOLD
	if (post > POST_THRESHOLD) {
#endif
	  alpha += post;
	  n_u0 += post*(*x0p);
	  n_u1 += post*(*x1p);
	  d0 = (*x0p) - c_u0;
	  d1 = (*x1p) - c_u1;
	  s0 += post*d0*d0;
	  s1 += post*d0*d1;
	  s2 += post*d1*d1;
#if CHECK_FOR_POST_THRESHOLD
	}
#endif
	likelihoodsp++;
	accum_likelihoodsp++;
	x0p ++; x1p ++;
      } while (accum_likelihoodsp != accum_likelihoods_endp);
      next_alphas[l] = alpha;
      next_means[l].u[0] = n_u0;
      next_means[l].u[1] = n_u1;
      next_covars[l].s[0] = s0;
      next_covars[l].s[1] = s1;
      next_covars[l].s[2] = s2;
    }
  }
  n_elements += n;
}


bool
MixBiNormal::endEpoch()
{
  int l;

  if (n_elements == 0)
    error("Called endEpoch() after not adding any data samples.");

  const double norm = (1.0/n_elements);
  for (l=0;l<NumMixComps;l++) {

    // check if next_alphs[l] <= mixtureCoeffVanishNumerator/NumMixComps
    // or something worse.
    if (NumMixComps*next_alphas[l] <= mixtureCoeffVanishNumerator

	|| next_alphas[l] <= DBL_MIN) {
      // this component is getting very unlikely, we set things up
      // so that the next beginning of an iteration will surely do a 
      // rerandomization (see prepareCurrent()).

      // use valid means and variances, but with a zero
      // alpha probability, so we essentially eliminate this component.
      next_means[l].u[0]  = 0.0;
      next_means[l].u[1]  = 0.0;

      next_covars[l].s[0] = 1.0;
      next_covars[l].s[1] = 0.0;
      next_covars[l].s[2] = 1.0;

      // this causes the rerandomization in prepareCurrent().
      next_alphas[l] = 0.0;
    } else {
      const double tmp = 1.0/next_alphas[l];
      next_means[l].u[0]  *= tmp;
      next_means[l].u[1]  *= tmp;
      
      // update the new means
      const double d0 = cur_means[l].u[0] - next_means[l].u[0];
      const double d1 = cur_means[l].u[1] - next_means[l].u[1];

      // and variances
      next_covars[l].s[0] = tmp*next_covars[l].s[0] - d0*d0;
      next_covars[l].s[1] = tmp*next_covars[l].s[1] - d0*d1;
      next_covars[l].s[2] = tmp*next_covars[l].s[2] - d1*d1;

      // then, finish with the alphas themselves.    
      next_alphas[l] *= norm;
    }
  }
  swapNextWithCur();
  return prepareCurrent();
}



// ======================================================================
// ----------  I/O routines ---------------------------------------------
// ======================================================================


void
MixBiNormal::readCurParams(FILE *f)
{
  int l;
  int rc;
  rc = fscanf(f,"%d",&NumMixComps);
  if (rc == 0 || rc == EOF)
    error("Error reading parameter input.");
  for (l=0;l<NumMixComps;l++) {
    rc = fscanf(f,"%le %le %le  ",
		&cur_means[l].u[0],
		&cur_covars[l].s[0],
		&cur_covars[l].s[1]);
    if (rc == 0 || rc == EOF)
      error("Error reading parameter input.");
  }
  for (l=0;l<NumMixComps;l++) {
    rc = fscanf(f,"%le %le %le  ",
		&cur_means[l].u[1],
		&cur_alphas[l],
		&cur_covars[l].s[2]);
    if (rc == 0 || rc == EOF)
      error("Error reading parameter input.");
  }
}

void
MixBiNormal::printCurParams(FILE *f)
{
  int l;
  fprintf(f,"%d\n",NumMixComps);
  for (l=0;l<NumMixComps;l++) {
    fprintf(f,"%.10e %.10e %.10e  ",
	   cur_means[l].u[0],
	   cur_covars[l].s[0],
	   cur_covars[l].s[1]);
  }
  fprintf(f,"\n");
  for (l=0;l<NumMixComps;l++) {
    fprintf(f,"%.10e %.10e %.10e  ",
	   cur_means[l].u[1],
	   cur_alphas[l],
	   cur_covars[l].s[2]);
  }
  fprintf(f,"\n");
}

static void swab_dbuf(double *ptr, int n) {
  // Swap bytes in a buffer of doubles, if needed to make them bigendian
#ifndef WORDS_BIGENDIAN
  unsigned int d,e, *dp = (unsigned int*)ptr;
  int i;

  assert(sizeof(double) == 8);
  assert(sizeof(unsigned int) == 4);

  for (i=0; i<n; ++i) {
    d = *dp++;
    e = *dp--;
    *dp++ = ((e & 0x000000FF) << 24) | ((e & 0x0000FF00) << 8) | ((e & 0x00FF0000) >> 8) | (0x000000FF & ((e & 0xFF000000) >> 24));
    *dp++ = ((d & 0x000000FF) << 24) | ((d & 0x0000FF00) << 8) | ((d & 0x00FF0000) >> 8) | (0x000000FF & ((d & 0xFF000000) >> 24));
  }
#endif /* WORDS_BIGENDIAN */
}

static void swab_fbuf(float *ptr, int n) {
  // Swap bytes in a buffer of floats, if needed to make them bigendian
#ifndef WORDS_BIGENDIAN
  unsigned int d, *dp = (unsigned int*)ptr;
  int i;

  assert(sizeof(float) == 4);
  assert(sizeof(unsigned int) == 4);

  for (i=0; i<n; ++i) {
    d = *dp;
    *dp++ = ((d & 0x000000FF) << 24) | ((d & 0x0000FF00) << 8) | ((d & 0x00FF0000) >> 8) | (0x000000FF & ((d & 0xFF000000) >> 24));
  }
#endif /* WORDS_BIGENDIAN */
}


void
MixBiNormal::readCurParamsBin(FILE *f)
{
  int l;
  unsigned char nmc;
  size_t rc;

  if ((rc = fread(&nmc,sizeof(nmc),1,f)) != 1)
    error("Error reading number mixture components.");
  NumMixComps = nmc; 

  if (NumMixComps > OrigNumMixComps) {
    error("NumMixComps(%d) > OrigNumMixComps(%d)\n",NumMixComps,OrigNumMixComps);
  }
  if (NumMixComps < 1)
    error("NumMixComps = %d\n",NumMixComps);

  for (l=0;l<NumMixComps;l++) {
    /*    rc = fread(&cur_means[l].u[0],sizeof(cur_means[l].u[0]),2,f);
	  rc += fread(&cur_covars[l].s[0],sizeof(cur_covars[l].s[0]),3,f);
	  rc += fread(&cur_alphas[l],sizeof(cur_alphas[l]),1,f); */
    double dbuf[6];
    rc = fread(&dbuf[0], sizeof(dbuf[0]), 6, f);
    swab_dbuf(dbuf, 6);
    if (rc != 6)
      error("Error reading parameter input.");
    cur_means[l].u[0] = dbuf[0];    cur_means[l].u[1] = dbuf[1];
    cur_covars[l].s[0] = dbuf[2];   cur_covars[l].s[1] = dbuf[3];
    cur_covars[l].s[2] = dbuf[4];   cur_alphas[l] = dbuf[5];
  }

  // seek over the remaining unused components.
  if (NumMixComps < OrigNumMixComps) {
    const int offset = 
      sizeof(cur_means[0].u[0])*2+
      sizeof(cur_covars[0].s[0])*3+
      sizeof(cur_alphas[0])*1;
    if (::fseek(f,offset*(OrigNumMixComps-NumMixComps),SEEK_CUR) == -1)
      error("Problem seeking over mixture components in file.");
  }
}

void
MixBiNormal::printCurParamsBin(FILE *f)
{
  int l;
  size_t rc;
  const unsigned char nmc = NumMixComps;
  if ((rc = fwrite(&nmc,sizeof(nmc),1,f)) != 1) {
    error("Problem writing number mixture components");
  }
  for (l=0;l<NumMixComps;l++) {
    /* rc = fwrite(&cur_means[l].u[0],sizeof(cur_means[l].u[0]),2,f);
       rc += fwrite(&cur_covars[l].s[0],sizeof(cur_covars[l].s[0]),3,f);
       rc += fwrite(&cur_alphas[l],sizeof(cur_alphas[l]),1,f); */
    double dbuf[6];
    dbuf[0] = cur_means[l].u[0];    dbuf[1] = cur_means[l].u[1];
    dbuf[2] = cur_covars[l].s[0];   dbuf[3] = cur_covars[l].s[1];
    dbuf[4] = cur_covars[l].s[2];   dbuf[5] = cur_alphas[l];
    swab_dbuf(dbuf, 6);
    rc = fwrite(&dbuf[0], sizeof(dbuf[0]), 6, f);
    if (rc != 6)
      error("Error writing parameter output.");
  }
  // seek over the remaining unused components.
  if (NumMixComps < OrigNumMixComps) {
    const int offset = 
      sizeof(cur_means[0].u[0])*2+
      sizeof(cur_covars[0].s[0])*3+
      sizeof(cur_alphas[0])*1;
    if (::fseek(f,offset*(OrigNumMixComps-NumMixComps),SEEK_CUR) == -1)
      error("Problem seeking over mixture components in file.");
  }
}

void
MixBiNormal::seekOverCurParamsBin(FILE *f)
{
  const int offset = 
    sizeof(cur_means[0].u[0])*2+
    sizeof(cur_covars[0].s[0])*3+
    sizeof(cur_alphas[0])*1;
  if (::fseek(f,sizeof(unsigned char) + offset*OrigNumMixComps,SEEK_CUR) == -1)
    error("Problem seeking over mixture components in file.");
}



// ======================================================================
// ----------  MI calculation, square grid method -----------------------
// ======================================================================


double
MixBiNormal::mi(int nbins,double n_std)
{
  double res;
  double *margx = new double[nbins];
  double *margy = new double[nbins];
  double *distxy = new double[nbins*nbins];

  double Hx,Hy,Hxy;
  res = mi(nbins,n_std,margx,margy,distxy,&Hx,&Hy,&Hxy);

  delete [] margx;
  delete [] margy;
  delete [] distxy;
  return res;
}

double
MixBiNormal::mi(int nbins,    // num histogram bins
		double n_std, // number stds.
		double *margx, // array for x margin
		double *margy, // array for y margin
		double *distxy, // array for joint x,y dist.
		double *aHx,
		double *aHy,
		double *aHxy)
{
  double min_x,max_x; // correponds to u0
  double min_y,max_y; // correponds to u1
  
  // set min_x = min(u-n_std*std) over all mixtures.
  // set max_x = max(u+n_std*std) over all mixtures.
  // similar for min_y, max_y

  int l = 0;
  min_x = cur_means[l].u[0] - n_std*sqrt(cur_covars[l].s[0]);
  max_x = cur_means[l].u[0] + n_std*sqrt(cur_covars[l].s[0]);
  min_y = cur_means[l].u[1] - n_std*sqrt(cur_covars[l].s[2]);
  max_y = cur_means[l].u[1] + n_std*sqrt(cur_covars[l].s[2]);

  for (l=1;l<NumMixComps;l++) {
    if (min_x > (cur_means[l].u[0] - n_std*sqrt(cur_covars[l].s[0])))
      min_x = cur_means[l].u[0] - n_std*sqrt(cur_covars[l].s[0]);
    if (max_x < (cur_means[l].u[0] + n_std*sqrt(cur_covars[l].s[0])))
      max_x = cur_means[l].u[0] + n_std*sqrt(cur_covars[l].s[0]);
    if (min_y > (cur_means[l].u[1] - n_std*sqrt(cur_covars[l].s[2])))
      min_y = cur_means[l].u[1] - n_std*sqrt(cur_covars[l].s[2]);
    if (max_y < (cur_means[l].u[1] + n_std*sqrt(cur_covars[l].s[2])))
      max_y = cur_means[l].u[1] + n_std*sqrt(cur_covars[l].s[2]);
  }
  const double step_x = (max_x-min_x)/nbins;
  const double step_y = (max_y-min_y)/nbins;
  
  memset(margy,0,nbins*sizeof(double));
  setScratchSize(nbins,1);



  double x,y;  
  int i,j;
  // create the x and y domain
  for (i=0,x=min_x,y=min_y;i<nbins;i++,x+=step_x,y+=step_y) {
    scratch1[i] = x;
    scratch2[i] = y;
  }

  double *xp;
  double cumsum = 0.0;
  double *distxy_p = distxy;
  double *margx_p = margx;
  double *margy_p;
  for (i=0,xp=scratch1;i<nbins;i++,xp++) {
    pg(*xp,scratch2,nbins,scratch3,scratch4,distxy_p);
    margy_p = margy;
    double *distxy_pp = distxy_p;
    double csum = 0;
    for (j=0;j<nbins;j++) {
      const double tmp = *distxy_pp++;
      csum += tmp;
      *margy_p++ += tmp;
    }
    *margx_p++ = csum;
    cumsum += csum;
    distxy_p += nbins;
  }

  // Compute the inv_cumsum and normalize so that we don't
  // have to worry about the normalizing 2*pi constant
  // which the above p(.) probability calculations do not include.
  const double inv_cumsum = 1.0/cumsum;

  double Hx=0.0,Hy=0.0,Hxy=0.0;
  distxy_p = distxy;
  margx_p = margx;  
  margy_p = margy;
  for (i=0;i<nbins;i++) {
    const double margx = (*margx_p)*inv_cumsum;
    if (margx > 0.0)
      Hx += (margx)*log(margx);
    const double margy = (*margy_p)*inv_cumsum;
    if (margy > 0.0)
      Hy += (margy)*log(margy);
    for (j=0;j<nbins;j++) {
      const double distxy = (*distxy_p)*inv_cumsum;
      if ((distxy) > 0)
	Hxy += (distxy)*log(distxy);
      distxy_p++;
    }
    margx_p++;
    margy_p++;
  }

  static const double inv_log2 = 1.0/log(2.0);
  Hx *= inv_log2;
  Hy *= inv_log2;
  Hxy *= inv_log2;
  *aHx = -Hx;
  *aHy = -Hy;
  *aHxy = -Hxy;

  return (-Hx-Hy+Hxy);
}



// ======================================================================
// ----------  MI calculation, law-of-large-numbers method --------------
// ======================================================================

// randomly choose a mixture component in [0:NumMixComps-1]
int
MixBiNormal::sampleComponent()
{
  double tmp = drand48();
  int i=0;
  double *alpha_p = cur_alphas;
  // do a dumb linear search for now.
  do {
    tmp -= *alpha_p++;
    i++;
  } while (tmp > 0 && i < NumMixComps);
  return i-1;
}


//
// Return a randomly chosen sample from the distribution.
void
MixBiNormal::sample(double*xp, double *yp,const int len)
{

  int i=len;
  while (i--) {
    const int l = sampleComponent();

    const double c0 = cur_chol[l].c[0];
    const double c1 = cur_chol[l].c[1];
    const double c2 = cur_chol[l].c[2];
    const double u0 = cur_means[l].u[0];
    const double u1 = cur_means[l].u[1];
    
    double xu,yu; // uniformly distributed variables.
    do {
      xu = drand48();
    } while (xu == 0.0);
    do {
      yu = drand48();
    } while (yu == 0.0);

    double xn,yn; // independent N(0,1) variables
    xn = inverse_normal_func(xu);
    yn = inverse_normal_func(yu);

    double xjn,yjn; // joint Normal variables
    xjn = c0*xn         + u0;
    yjn = c1*xn + c2*yn + u1;
    
    *xp++ = xjn;
    *yp++ = yjn;
  }
}

//
// Do a cholesky factorization and some other preparatory calculations.
void
MixBiNormal::prepareChol()
{
  if (cur_chol != NULL)
    delete [] cur_chol;
  cur_chol = new CholCoVar[NumMixComps];

  for (int l=0;l<NumMixComps;l++) {
    const double s0 = cur_covars[l].s[0];
    const double s1 = cur_covars[l].s[1];
    const double s2 = cur_covars[l].s[2];

    // the first three are the reall cholesky factorization.
    cur_chol[l].c[0] = sqrt(s0);
    cur_chol[l].c[1] = s1/cur_chol[l].c[0];
    cur_chol[l].c[2] = sqrt(s2 - cur_chol[l].c[1]*cur_chol[l].c[1]);

    // The rest are precomputed values that are used to compute
    // the marginal probabilities.
    cur_chol[l].c[3] = 1.0/cur_chol[l].c[0]; // i.e., 1/sqrt(s0)
    cur_chol[l].c[4] = -0.5/s0;
    cur_chol[l].c[5] = 1/sqrt(s2);
    cur_chol[l].c[6] = -0.5/s2;
  }
}

double
MixBiNormal::mi(int nsamps)
{
  double *hx_buf = new double[nsamps];
  double *hy_buf = new double[nsamps];
  double Hx, Hy, Hxy;
  double res = mi(nsamps,hx_buf,hy_buf,&Hx,&Hy,&Hxy);
  delete [] hx_buf;
  delete [] hy_buf;
  return res;
}

double
MixBiNormal::mi(int nsamps,
		double *hx_buf,
		double *hy_buf,
		double *aHx,
		double *aHy,
		double *aHxy)
{
  

  prepareChol();

  setScratchSize(nsamps,1);  
  
  // compute a set of nsamps samples of the distribution.
  sample(hx_buf,hy_buf,nsamps);
  
  // compute p(x[i],y[i]) place result in scratch1
  pg(hx_buf,hy_buf,nsamps,scratch2,scratch3,scratch1);

  // compute p(x[i]), place result in scratch2
  pgx0(hx_buf,nsamps,scratch3,scratch4,scratch2);

  // compute p(y[i]), place result in scratch3, destroy hx_buf
  pgx1(hy_buf,nsamps,hx_buf,scratch4,scratch3);

  // scratch 1 2 and 3 are in use.

  // compute logs
  vlog(nsamps,scratch1,scratch4);
  vlog(nsamps,scratch2,scratch1);
  vlog(nsamps,scratch3,scratch2);

  // scratch4 now has log(p(x,y))
  // scratch1 now has log(p(x))
  // scratch2 now has log(p(y)) 

  // compute entropies
  double Hx=0.0,Hy=0.0,Hxy=0.0;
  double *Hx_p = scratch1;
  double *Hx_endp = scratch1+nsamps;
  double *Hy_p = scratch2;
  double *Hxy_p = scratch4;

  // assume nsamps > 0
  do {
    Hx += *Hx_p++;
    Hy += *Hy_p++;
    Hxy += *Hxy_p++;
  } while (Hx_p != Hx_endp);


  // Because we're essentially computing
  //    E[log (p(x,y)/(p(x)p(y)) ] 
  //      ~= 1/N sum_i log (p(x_i,y_i)/(p(x_i)p(y_i))
  // where (x_i,y_i) are drawn from the learned distribution,
  // we don't have to worry about the fact that the p(.) probability
  // calculations above do not include the (2*pi)^(d/2) = 2*pi 
  // normalizing constant for the MI calculation.
  
  const double inv_norm = 1.0/(nsamps*log(2.0));
  Hx *= inv_norm;
  Hy *= inv_norm;
  Hxy *= inv_norm;

  // These *should* have the 2*pi normalization constant.
  static const double log_2pi = log(2*M_PI);
  *aHx = -Hx + log_2pi;
  *aHy = -Hy + log_2pi;
  *aHxy = -Hxy + log_2pi;
  
  // return Hx + Hy - Hxy
  return (-Hx-Hy+Hxy);
}



#ifdef MAIN

// ======================================================================
// ----------  TEST main() code  ----------------------------------------
// ======================================================================



// Read the transpose of the matrix on disk.
static float *
readTransposeMatrix(FILE*f,
		    int numCols, // the number of cols of the disk matrix
		    int&cur_len, // number of rows
		    bool ascii)
{
  // Input file is a matrix of size K*numCols
  // where K is some unknown value (i.e., we don't
  // know how big it is until we read the matrix). The
  // matrix consists of numCols signals each of length
  // K. We read them into memory as a numCols*K matrix
  // so that successive signal values occupy consecutive
  // memory locations. 
  

  int allocated_len = 100;
  cur_len = 0;
  float *data = new float[numCols*allocated_len];

  while (1) { 
    if (cur_len >= allocated_len) {
      const int old_len = allocated_len;
      allocated_len *= 2;
      float *tmp = new float[numCols*allocated_len];
      for (int i=0;i<numCols;i++)
	for (int j=0;j<cur_len;j++)
	  tmp[i*allocated_len+j] = data[i*old_len+j];
      delete [] data;
      data = tmp;
    }
      
    for (int i=0;i<numCols;i++) {
      if (!ascii) {
	if (!fread(&data[i*allocated_len+cur_len],
		   sizeof(data[0]),1,f)) {
	  if (feof(f) && i == 0)
	    goto done;
	  else { 
	    fprintf(stderr,"Error. EOF encountered. Must have k*%d els.\n",
		    numCols);
	    exit(-1);
	  }
	}
	swab_fbuf(&data[i*allocated_len+cur_len], 1);
      } else {
	if (!fscanf(f,"%f",&data[i*allocated_len+cur_len])) {
	  if (feof(f) && i == 0)
	    goto done;
	  else { 
	    fprintf(stderr,"Error. EOF encountered. Must have k*%d els.\n",
		    numCols);
	    exit(-1);
	  }
	}
      }
    }
    cur_len ++;
  }

done:

  float *res = new float[numCols*cur_len];
  float *tmp = res;
  for (int i=0;i<numCols;i++)
    for (int j=0;j<cur_len;j++)
      tmp[i*cur_len+j] = data[i*allocated_len+j];
  delete [] data;
  return res;
}


// Read the matrix on disk.
static float *
readMatrix(FILE*f,
	   int numCols, // the number of cols of the disk matrix
	   int& cur_rows,
	   bool ascii)
{
  // Input file is a matrix of size K*numCols
  // where K is some unknown value (i.e., we don't
  // know how big it is until we read the matrix). The
  // matrix consists of numCols columns each of length
  // K -- e.g., it consists of K samples, each of
  // dimension numCols.
  // We read them into memory as a K*numCols matrix
  // so that successive sample values occupy consecutive
  // memory locations. 
  // Returns the number of rows read in.

  int allocated_len = 100;
  cur_rows = 0;
  float *data = new float[allocated_len*numCols];

  while (1) { 
    if (cur_rows >= allocated_len) {
      allocated_len *= 2;
      float *tmp = new float[allocated_len*numCols];
      for (int i=0;i<cur_rows*numCols;i++)
	tmp[i] = data[i];
      delete [] data;
      data = tmp;
    }
      
    if (!ascii) {
      int res = fread(&data[cur_rows*numCols],sizeof(data[0]),numCols,f);
      if (res == 0 && feof(f)) {
	goto done;
      }	else { 
	fprintf(stderr,"Error. EOF encountered. Must have k*%d els (at row %d).\n",
		  numCols, cur_rows);
	exit(-1);
      }
      swab_fbuf(&data[cur_rows*numCols], res);
    } else {
      int i;
      for (i=0;i<numCols;i++) {
	int res = fscanf(f,"%f",&data[cur_rows*numCols+i]);
	if (res == 0 || res == EOF) {
	  if (feof(f) && i == 0)
	    goto done;
	  else { 
	    fprintf(stderr,"Error. EOF encountered. Must have k*%d els.\n",
		    numCols);
	    exit(-1);
	  }
	}
      }
    }
    //    printf("Reading %d %f, %f\n",cur_rows,data[cur_rows*numCols],
    //	   data[cur_rows*numCols+1]);
    cur_rows ++;
  }

done:

  float *res = new float[cur_rows*numCols];
  for (int i=0;i<cur_rows*numCols;i++)
    res[i] = data[i];
  delete [] data;
  return res;
}


main(int argc,char*argv[])
{
  
  int i,j;
  int ncomps = 1;
#ifdef HAVE_IEEE_FLAGS
    char *out;
    ieee_flags("set",
	       "exception",
	       "invalid",
	       &out);
    ieee_flags("set",
	       "exception",
	       "division",
	       &out);
#else
#  ifdef HAVE_FPSETMASK
    fpsetmask(
	      FP_X_INV      /* invalid operation exception */
	      /* | FP_X_OFL */     /* overflow exception */
	      /* | FP_X_UFL */     /* underflow exception */
	      | FP_X_DZ       /* divide-by-zero exception */
	      /* | FP_X_IMP */      /* imprecise (loss of precision) */
	      );
#  else // create a syntax error.
//#    error No way known to trap FP exceptions
#  endif
#endif

  if (argc > 1)
    ncomps = atoi(argv[1]);

  printf("Using %d components\n",ncomps);

  float *arr,*p;
  int rows;
  bool isascii = true;
  arr = readMatrix(stdin,2,rows,isascii);
  p = arr;

  MixBiNormal::GlobalNumComps = ncomps;
  MixBiNormal mbn1;


  i=0;
  do {
    printf("Epoch %d:\n",i);

    mbn1.startEpoch();

    for (j=0,p=arr;j<rows;j++,p+=2)
      mbn1.addtoEpoch(p[0],p[1]);      

    if (mbn1.endEpoch()) 
      printf("Epoch %d: Mixture Component drop occured.\n",i);

    printf("-------- iter %d used cur params ------------\n",i);
    mbn1.printCurParams(stdout);
    printf("----------------------------------\n");


    printf("Epoch %d: diff = %f, mi_grid=%f,mi_lll=%f\n",
	   i,
	   mbn1.llPercDiff(),
	   mbn1.mi(250,3.0),
	   mbn1.mi(250*250));

    i++;
  } while (mbn1.llPercDiff() > 0.1);
  

}



#endif


// ======================================================================
// ----------  END ------------------------------------------------------
// ======================================================================
