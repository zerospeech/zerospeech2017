//
//
//
// $Header: /u/drspeech/repos/pfile_utils/MixBiNormal.h,v 1.1.1.1 2002/11/08 19:47:05 bdecker Exp $
// 
// A mixture of bi-variate normal class.
//   Includes the EM algorithm with loop iteration controlled and
//   data provided by outside of the class.



#ifndef MIXBINORMAL_H
#define MIXBINORMAL_H

#include <stdlib.h>
#include <math.h>
#include <assert.h>


class MixBiNormal {

  const int OrigNumMixComps;
  int NumMixComps;

  struct Mean {
    double u[2];
  };

  struct CoVar {
    // s[0] = var(x[0]), s[1] = cov(x[0],x[1]), s[2] = var(x[1])
    // i.e., covar matrix is:
    //
    //  s[0] s[1]
    //  s[1] s[2]
    double s[3];
  };

  struct CholCoVar {
    // Cholesky factorization of CoVar
    // i.e., Above covariance matrix is Z, then
    // this is C'C = Z where C is an upper trangular 
    // matrix. 
    // and where 
    // C  = c[0]  c[1]
    //       0    c[2]
    // and 
    // c[0] = sqrt(s0)
    // c[1] = s1/c[0] 
    // c[2] = (s2 - c[1]^2)^(1/2)
    //
    // This means that C'x+u is a sample with covar Z if x is
    // bi-variate normal N(0,1).
    // Helper variables for marginal computation.
    // c[3] = 1/sqrt(s0);
    // c[4] = -0.5*/s0
    // c[5] = 1/sqrt(s2);
    // c[6] = -0.5*/s2
    double c[7];
  };


  struct CoVarHelp {
    // 
    // The following are precomputed for computational reasons.
    // where s[i] is from the CoVar structure above.
    // h[0] = -2.0*s[1]
    // Assuming the inverse determinant is:
    //     inv_det = 1.0/(s0*s2-s1*s1),
    // then
    // h[1] = -0.5*inv_det;
    // h[2] = (sqrt(inv_det));
    // 
    double h[3];
  };



  // current parameter estimates
  double *cur_alphas; // mixing coefficents
  Mean  *cur_means;
  CoVar *cur_covars;
  bool curParmsAreRandom; // true when the cur_* values are random.
  
  
  CoVarHelp *cur_covars_help;
  CholCoVar *cur_chol; // used for sampling


  // The "next" parameter estimates, i.e., the ones
  // we compute using the current parameters.
  double *next_alphas; // mixing coefficents
  Mean  *next_means;
  CoVar *next_covars;

  void swapNextWithCur();
  void copyCurToNext();
  void copyNextToCur();

  void randomizeCurMixture(int l);
  void randomizeCurMixtures();
  void normalizeAlphas();
  void normalizeAlphasWithNoise();
  void eliminateMixture(int l);
    

  // temporary values used to compute
  // the new parameters from the old.

  // array of alpha scaled likelihoods for each component for current X
  double *likelihoods;

  // The overall log likelihood of all the data.
  // Computed during phaseII.
  double llikelihood;
  double prev_llikelihood;  // the previous one.

  // the number of elements added during each phase.
  int n_elements;

  // Note: preCompute() must be called prior to calling
  // these functions.
  // p_g(x,i), using the new parameters, just component i
  double pg(double x0,double x1,int l);
  void pg(double x0,double *x1,int l,int n,double alpha,
	  double *scratch,double *res);
  void pg(double x0,double *x1,int l,int n,double alpha,
	     double *scratch1,double *scratch2,double *resa);
  void pg(double *x0,double *x1,int l,int n,double alpha,
	     double *scratch,double *res,double *resa);
  void pg(double *x0,double *x1,int n,
	     double *scratch1,double *scratch2,double *res);
  // pg(x), using the new parameters.
  double pg(double x0,double x1);
  void pg(double x0,double *x1, int n,
	     double *scratch1,double *scratch2, double *res);

  // similar to above, but marginals.
  double pgx0(double x0);
  double pgx0(double x0,int l);
  double pgx1(double x0);
  double pgx1(double x0,int l);
  void pgx0(double *x0,int len,
	      double *scratch1,double *scratch2,
	      double *res);
  void pgx1(double *x1,int len,
	      double *scratch1,double *scratch2,
	      double *res);
  
  // helper functions for sampling.
  int sampleComponent();
  void sample(double *xp,double *yp,const int len);
  void prepareChol();


  // scratch arrays.
  static int scratch_len;
  static double *scratch1;
  static double *scratch2;  
  static double *scratch3;
  static double *scratch4;

  // number of re-randomizations of mixture components during
  // learning for the current number of mixture components
  int totalCurrentReRands;

  // A bitmask giving the state of the object.
  enum {
    bm_active = 0x01, // true if we have not yet converged.
    bm_dirty  = 0x02, // true if current parameters have not been saved

    bm_initState = 0x0
  };
  unsigned int bitmask;

public:

  // A static member to modify if an array
  // of these objects is required.
  static int GlobalNumComps;

  // If any of the variances drop below this value,
  // then we do a re-randomization.
  static double varianceFloor;

  // If the determinate of the covariance matrix drops below this value,
  // then we do a re-randomization.
  static double detFloor;

  // If any mixture coefficients goes below this value/num_components, we 
  // do a re-randomization
  static double mixtureCoeffVanishNumerator;

  // number of re-randomizations that occur before the
  // number of mixture components is reduced.
  static int reRandsPerMixCompRedux;


  // If true, only re-randomize the bad mixture component when
  // it goes awry. If false, all mixture components are re-randomized.
  static bool reRandomizeOnlyOneComp;

  // If True, then do *not* re-randomize all mixtues when a component
  // drop occurs. If false (default behavior), all mixture components
  // get re-randomized when a drop occurs. This option could
  // speed convergence when there really are too many mixtures. If
  // this option is true, then the work spent training the non-dropped
  // components will not be wasted.
  static bool noReRandomizeOnDrop;


  MixBiNormal(int ncomps = GlobalNumComps); // number of mixture components

  ~MixBiNormal();

  static void
  setScratchSize(const int n, const int nc);

  // "active" is used to say if this mixture has converged.
  void setActive() { bitmask |= bm_active; }
  void reSetActive() { bitmask &= ~bm_active; }
  bool active() { return (bitmask & bm_active); }

  // "dirty" is used to say if this mixture has been saved to disk.
  void setDirty() { bitmask |= bm_dirty; }
  void reSetDirty() { bitmask &= ~bm_dirty; }
  bool dirty() { return (bitmask & bm_dirty); }

  // Prepare the current parameters (do some pre-computation) for examination
  // with either the mi() functions or the p() functions. Also, check
  // that the current parameters are valid, and if they are not for
  // some reason, we re-randomize potentially dropping a mixture component.
  // Returns true if number of mixuture components was reduced.
  bool prepareCurrent();

  // Interface to the EM algorithm.
  void startEpoch();
  void addtoEpoch(double x0,double x1);
  void addtoEpoch(double *x0,double *x1,const int n);
  bool endEpoch();

  // Print the current parameters.
  void printCurParams(FILE *f);
  void printCurParamsBin(FILE *f); // binary version
  void seekOverCurParamsBin(FILE *f); // binary version
  void readCurParams(FILE *f);
  void readCurParamsBin(FILE *f); // binary version

  // return the log-likelihood percent difference between the current
  // and previous parameters.
  double llPercDiff();

  // Probability functions for the mixture and for the individual components.
  // These functions, unlike pg(), are appropriately normalized.
  double p(double x0,double x1) { return pg(x0,x1)/(2.0*M_PI); }
  double p(double x0,double x1,int i) { return pg(x0,x1,i)/(2.0*M_PI); }

  // return the MI between X and Y using a subsampled
  // bi-variate historgram with nbins*nbins bins over a range
  double mi(int nbins,double std);
  // this version also returns the entropies Hx, Hy, and Hxy
  double mi(int nbins,double std, double *margx,double*my,double *xy,
	    double *Hx, double *Hy, double *Hxy);

  // return the MI between X and Y by sampling from the resulting
  // distribution and computing sample averages of
  // the entropies, e.g., 
  // Hxy = E[ -log (p(x,y))] ~= (1/N)sum_i(-log(p(xi,yi)))
  double mi(int nsamps);
  // this version also returns the entropies Hx, Hy, and Hxy
  double mi(int nsamps,double *hx_buf,double *hy_buf,
	    double *Hx, double *Hy, double *Hxy);

};


#endif
