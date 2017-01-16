// -*- C++ -*-
// 
// Author: C. Wooters
// Date: 09/06/01
//
#ifndef __LINALG__
#define __LINALG__

using namespace std;

class linalg {
 public:
  // some useful constants
  static const double tiny = 1.0e-20;
  static const double epsilon = 0.0000001;

  // the static methods
  static void ludcmp(icsiarray<double>&, const int, icsiarray<int>&, float*);

  static void lubksb(const icsiarray<double>&, const int, 
		     const icsiarray<int>&, icsiarray<double>&);

  static void linearSolve(const icsiarray<double>& a, const double eigVal, 
		    const int n, icsiarray<double>& y, 
		    const icsiarray<double>& x);

  static void getEigenVals(icsiarray<double>& a, const int n, 
			   icsiarray<double>& wr);

  static void getEigenVec(const icsiarray<double>& a, double& eigVal, 
			  icsiarray<double>& eigVec, const int n);

  static void eigenanalysis(const icsiarray<double>& a, const int n,
			    icsiarray<double>& e_vals, 
			    icsiarray<double>& e_vecs);

  static void eigsrta(icsiarray<double>& d,icsiarray<double>& v,const int n);

  static void balance(icsiarray<double>& a, const int n);

  static void elmhes(icsiarray<double>& a, const int n);

  static void hqr(icsiarray<double>& a, const int n, 
		  icsiarray<double>& wr, icsiarray<double>& wi);

  static void invAxB(icsiarray<double>&, icsiarray<double>&,
		     const icsiarray<double>&, const int);

  static void matinv(icsiarray<double>&, icsiarray<double>&, const int);

  static void outerProd(icsiarray<double>&, const icsiarray<double>&,
			const icsiarray<double>&);

  static double innerProd(const icsiarray<double>& x, 
			  const icsiarray<double>& y);

  static double vecnorm(const icsiarray<double>&);

  static void normalize(icsiarray<double>&);

  static void random_unit_vector(icsiarray<double>&);

  static double euclidian_dist(const icsiarray<double>& x, 
			       const icsiarray<double>& y);

  static inline int imax(const int a, const int b) {return (a > b) ? a : b;}
  static inline double sign(const double a, const double b) {
    return (b>=0.0) ? fabs(a) : -fabs(a);
  }

};

#endif
