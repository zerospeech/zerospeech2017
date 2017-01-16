// -*- C++ -*-
// 
// Author: C. Wooters
// Date: 09/06/01
//

#include <cmath>
#include <cstdlib>
#include <time.h>
#include <error.h>
#include <limits.h>

#include <float.h>	// 2002-03-17 dpwe@ee.columbia.edu for DBL_MIN

#include "icsiarray.h"
#include "linalg.h"

using namespace std;

/**
 ** Performs LU decomposition on a matrix of doubles. The matrix
 ** is represented as a icsiarray.
 **
 ** This algorithm is taken from Numerical Recipes in C, Section 2.3 
 ** page 46.
 **
 ** The description in NR is:
 **
 ** "Given a matrix a[1..n][1..n], this routine replaces it by the LU
 ** decomposition of a rowwise permutation of itself. a and n are input.
 ** a is output, arranged as in equation (2.3.14) above; indx[1..n] is
 ** an output vector that records the row permutation effected by the partial
 ** pivoting; d is output as +-1 depending on whether the number of row
 ** interchanges was even or odd, respectively. This routine is used in
 ** combination with lubksb() to solve linear equations or invert a matrix."
 */
void 
linalg::ludcmp(icsiarray<double>& a, const int n, 
	       icsiarray<int>& indx, float* d) {
    int i,imax,j,k;
    double big,dum,sum,temp;
    icsiarray<double> vv(n);

    *d = 1.0;
    for(i=0;i<n;i++) {
	big=0.0;
	int row = i*n;
	for(j=0;j<n;j++)
	    if((temp=fabs(a[row+j])) > big) big=temp;
	if(big==0.0) error("ludcmp(): Singular matrix");
	vv[i] = 1.0/big;
    }

    for(j=0;j<n;j++) {
	for(i=0;i<j;i++) {
	    int idx = i*n+j;
	    sum=a[idx];
	    for(k=0;k<i;k++) sum -= a[i*n+k] * a[k*n+j];
	    a[idx]=sum;
	}
	big=0.0;
	for(i=j;i<n;i++) {
	    int idx = i*n+j;
	    sum = a[idx];
	    for(k=0;k<j;k++)
		sum -= a[i*n+k] * a[k*n+j];
	    a[idx] = sum;
	    if((dum=vv[i]*fabs(sum)) >= big) {
		big=dum;
		imax=i;
	    }
	}
	if(j != imax) {
	    int idx1 = imax*n;
	    int idx2 = j*n;      
	    for(k=0;k<n;k++) {
		dum=a[idx1+k];
		a[idx1+k]=a[idx2+k];
		a[idx2+k]=dum;
	    }
	    *d = -(*d);
	    vv[imax] = vv[j];
	}
	indx[j]=imax;
	int idx = j*n+j;
	if(a[idx] == 0.0) a[idx] = tiny;
	if(j!=n){
	    dum=1.0/(a[idx]);
	    for(i=j+1;i<n;i++) a[i*n+j] *= dum;
	}
    }
}

/**
 ** Performs backsubstitution on an LU decomposed matrix of doubles. 
 **
 ** This algorithm is taken from Numerical Recipes in C, Section 2.3 
 ** page 47.
 **
 ** The description in NR is:
 **
 ** "Solves the set of n linear equations A*X=B. Here a[1..n][1..n]
 ** is input, not as a the matrix A but rather as its LU decomposition,
 ** determined by the routine ludcmp(). b[1..n] is input as the right-hand
 ** side vector B, and returns with the solution vector X. a, n, and indx
 ** are not modified by this routine and can be left in place for 
 ** successive calls with different right-hand sides b. This routine takes
 ** into account the possibility that b will begin with many zero elements,
 ** so it is efficient for use in matrix inversion."
 */
void 
linalg::lubksb(const icsiarray<double>& a, const int n, 
	       const icsiarray<int>& indx, icsiarray<double>& b){
    int i,ii=0,ip,j;
    double sum;

    for(i=1;i<=n;i++) {
	ip = indx[i-1];
	sum=b[ip];
	b[ip]=b[i-1];
	if(ii)
	    for(j=ii;j<=i-1;j++) sum -= a[(i-1)*n+(j-1)] * b[j-1];
	else if (sum) ii=i;
	b[i-1]=sum;
    }
    for(i=n-1;i>=0;i--){
	sum=b[i];
	for(j=i+1;j<n;j++) sum -= a[i*n+j] * b[j];
	b[i]=sum/a[i*n+i];
    }
}

/** 
 ** Performs inv(A)*B on the matrices of doubles A and B. This *should*
 ** give the same results as matlab's matrix left division operator '\'.
 ** Assumes that A, B and the result are all n x n matrices.
 **
 ** WARNING: the contents of A will be destroyed (munged).
 **
 ** This algorithm is based on Numerical Recipes in C, section 2.3
 ** page 48. Actually, on page 49 they state:
 **
 ** "Incidentally, if you ever have the need to compute inv(A)*B from
 ** matrices A and B, you should LU decompose A and then backsubstitute
 ** with the columns of B instead of with the unit vectors that would
 ** give A's inverse. This saves a whole matrix multiplication, and is
 ** also more accurate."
 */
void 
linalg::invAxB(icsiarray<double>& res, 
	       icsiarray<double>& a, 
	       const icsiarray<double>& b, const int n) {
    int i,j;
    float d;
    icsiarray<int> indx(n);
    icsiarray<double> col(n);	// store columns of B

    if(res.size() != (n*n))
	error("linalg::invAxB(): res matrix not n x n");

    if(a.size() != (n*n))
	error("linalg::invAxB(): A matrix not n x n");

    if(b.size() != (n*n))
	error("linalg::invAxB(): B matrix not n x n");

    ludcmp(a,n,indx,&d);		// perform LU decomposition of A
				// result goes back into A

    for(j=0;j<n;j++){
	for(i=0;i<n;i++) col[i] = b[i*n+j];	// get column values
	lubksb(a,n,indx,col);	// perform backsubstitution for this col
	for(i=0;i<n;i++) res[i*n+j] = col[i];
    }
}

/** 
 ** Computes the inverse of a matrix A of doubles.
 ** Assumes that A and the result (res) are n x n matrices.
 **
 ** This algorithm is based on Numerical Recipes in C, section 2.3
 ** page 48.
 **
 */
void 
linalg::matinv(icsiarray<double>& res, icsiarray<double>& a, const int n) {
    int i,j;
    float d;
    icsiarray<int> indx(n);
    icsiarray<double> col(n);	// store columns of B

    if(res.size() != (n*n))
	error("linalg::matinv(): res matrix not n x n");

    if(a.size() != (n*n))
	error("linalg::matinv(): A matrix not n x n");

    ludcmp(a,n,indx,&d);		// perform LU decomposition of A
				// result goes back into A

    for(j=0;j<n;j++){
	for(i=0;i<n;i++) col[i] = 0.0;
	col[j] = 1.0;
	lubksb(a,n,indx,col);
	for(i=0;i<n;i++) res[i*n+j] = col[i];
    }
}

/**
 ** Compute the outer product of two vectors (icsiarrays)
 */
void 
linalg::outerProd(icsiarray<double>& res, 
		  const icsiarray<double>& va1, 
		  const icsiarray<double>& va2) {

    size_t va1size = va1.size();
    size_t va2size = va2.size();
  
    if(va1size != va2size)
	error("linalg::outerProd(): va1size != va2size");

    if(res.size() != (va1size * va2size))
	error("linalg::outerProd(): res.size() != (va1size * va2size)");

    for(int i=0;i<va1size;i++){
	for(int j=0;j<va2size;j++){
	    size_t idx = i*va1size+j;
	    res[idx] = va1[i]*va2[j];
	}
    }
}

/**
 ** This routine uses the ludcmp and lubksb routines to find the eigenvector 
 ** corresponding to a given eigen value using the inverse iteration method.
 ** This routine is meant to be called iteratively in order to get closer 
 ** to the true eigenvector. (See numerical recipes, the art of scientific 
 ** computing, second edition, pages 493 - 495.) 
 **
 ** (This code was adapted from Eigen.java obtained from ISIP at Mississippi 
 ** State University.)
 */
void
linalg::linearSolve(const icsiarray<double>& a, const double eigVal, 
		    const int n, icsiarray<double>& y, 
		    const icsiarray<double>& x){
    float d;
    icsiarray<int> indx(n);
    icsiarray<double> tmpa(a);
        
    for(int i = 0; i < n; i++ )
	tmpa[i*n+i] -= eigVal;

    ludcmp(tmpa,n,indx,&d);
    y.set(x);
    lubksb(tmpa,n,indx,y);
}

/**
 ** This will find the eigen vector for the corresponding eigen value of
 ** the matrix a. If a better eigen value is found, the original eigen value
 ** will be overwritten with this value.
 **
 ** (This code was adapted from Eigen.java obtained from ISIP at Mississippi 
 ** State University.)
 */
void
linalg::getEigenVec(const icsiarray<double>& a, double& eigVal, 
		    icsiarray<double>& eigVec, const int n) {

    // some constants
    const int max_iter = 8;
    const int max_initial_iter = 10;
    const int max_l_update = 5;
    const double min_initial_growth = 1000.0;
    const double min_decrease = 0.01;

    // temp vectors
    icsiarray<double> xi(n);
    icsiarray<double> xi1(n);
    icsiarray<double> y(n);
    icsiarray<double> y_init_max(n);
        
    int nrInitialIter = 0;        
    double len_y = 0.0;
    double maxGF = DBL_MIN;	// min value for a double (from limits.h)
    double lambdaI = eigVal;
    do {
	nrInitialIter++;
	random_unit_vector(xi);
	linearSolve( a, lambdaI, n, y, xi );
	len_y = vecnorm(y);
	if( len_y > maxGF ) {
	    maxGF = len_y;
	    y_init_max.set(y);
	}
    } while( len_y < min_initial_growth && nrInitialIter < max_initial_iter );

    y.set(y_init_max);
    xi1.set_quot(y, len_y);

    int nrIter,nrLambdaUpdates,betterEigVal,done = 0;
    do {
	nrIter++;
	double di1 = euclidian_dist(xi1, xi);
	double di = 0.0;

	if( di1 < epsilon || nrIter >= max_iter || 
	    nrLambdaUpdates >= max_l_update )
	    done = 1;
	else {
	    double decrease = fabs(di1 - di);
	    if( decrease < min_decrease ) {
		nrLambdaUpdates++;
		double sum_nom = 0.0; 
		double sum_denom = 0.0;
		for(int i = 0; i < n; i++ ) {
		    sum_nom += xi[i] * xi[i];
		    sum_denom += xi[i] * y[i];
		}
		double deltaL = sum_nom / fabs(sum_denom);
		double lambdaI1 = lambdaI + deltaL;
		betterEigVal = 1;
		lambdaI = lambdaI1;
		if( deltaL == 0.0 ) {
		    done = 1;
		} else {
		    nrIter = 0;
		}
	    }
	    if( done == 0 ) {
		xi.set(xi1);
		di = di1;
		linearSolve( a, lambdaI, n, y, xi );
		len_y = vecnorm(y);
		xi1.set_quot(y, len_y);
	    }
	}
    } while( done == 0 );
   
    eigVec.set(xi1);
    if( betterEigVal ) {
	eigVal = lambdaI;
    }
}

/**
 ** Perform eigen analysis on the given matrix. The eigenvalues will
 ** be returned in the vector 'e_vals' and the eigenvectors will be 
 ** returned in columns of the matrix 'e_vecs'. 
 **
 ** The input matrix 'a' is not assumed to be symmetric.
 */
void
linalg::eigenanalysis(const icsiarray<double>& a, const int n,
		      icsiarray<double>& e_vals, icsiarray<double>& e_vecs) {

    icsiarray<double> tmpa(a);	// make a copy so we don't munge the original
    // get the eigen values
    getEigenVals(tmpa,n,e_vals);

  // for each eigen value, get the corresponding eigenvector
    icsiarray<double> tmpvec(n);	// temp storage
    for(int j=0;j<n;j++){
	getEigenVec(a,e_vals[j],tmpvec,n);
	// Copy the eigen vector into the output matrix.
	// Each eigen vector occupies a column in the output matrix.
	for(int k=0;k<n;k++)
	    e_vecs[k*n+j] = tmpvec[k];
    }
    eigsrta(e_vals,e_vecs,n);
}

/**
 ** This routine sorts the eigen vectors and eigen values according to the
 ** eigen values. 
 **
 ** This code was taken from Jeff Bilmes' eig.c program.
 */
void 
linalg::eigsrta(icsiarray<double>& d, /* n-vector of eigenvalues */
		icsiarray<double>& v, /* nxn matrix of eigenvectors. */
		const int n)
{
    int k,j,i;
    double p;

    for (i=0;i<(n-1);i++) {
	p=d[k=i];
	for (j=i+1;j<n;j++)
	    if (d[j] >= p)
		p=d[k=j];
	if (k != i) {
	    /* k'th had max, so swap k'th with i'th */
	    d[k]=d[i];
	    d[i]=p;
	    for (j=0;j<n;j++) {
		p=v[j*n+i];
		v[j*n+i]=v[j*n+k];
		v[j*n+k]=p;
	    }
	}
    }
}

/**
 ** Description from Numerical Recipes:
 **
 ** Given a matrix a[1..n][1..n], this routine replaces it by a balanced 
 ** matrix with identical eigenvalues. A symmetric matrix is already balanced
 ** and is unaffected by this procedure. The parameter RADIX should be 
 ** the machine's floating-point radix.
 ** (see Numerical Recipes, pages 482 - 484.)
 */
void
linalg::balance(icsiarray<double>& a, const int n) {
    const double radix = 2.0;
    const double sqrdx = radix*radix;
    double s,r,g,f,c = 0.0;
    int j;
    int last=0;
    while (last == 0) {
	last=1;
	for (int i=1;i<=n;i++) {
	    r=c=0.0;
	    for (j=1;j<=n;j++)
		if (j != i) {
		    c += fabs(a[(j-1)*n+(i-1)]);
		    r += fabs(a[(i-1)*n+(j-1)]);
		}
	    if ((c != 0.0) && (r != 0.0)) {
		g=r/radix;
		f=1.0;
		s=c+r;
		while (c<g) {
		    f *= radix;
		    c *= sqrdx;
		}
		g=r*radix;
		while (c>g) {
		    f /= radix;
		    c /= sqrdx;
		}
		if ((c+r)/f < 0.95*s) {
		    last=0;
		    g=1.0/f;
		    for (j=1;j<=n;j++) a[(i-1)*n+(j-1)] *= g;
		    for (j=1;j<=n;j++) a[(j-1)*n+(i-1)] *= f;
		}
	    }
	}
    }
}

/**
 ** Description from Numerical Recipes:
 **
 ** Reduction to Hessenberg form by the elimination method. The real,
 ** nonsymetric matrix a[1..n][1..n] is replaced by an upper Hessenberg
 ** matrix with identical eigenvalues. Recommended, but not required,
 ** is that this routine be preceeded by balanc(e). On output, the
 ** Hessenberg matrix is in elements a[i][j] with i<=j+1. Elements
 ** with i>j+1 are to be thought of as zero, but are returned with random 
 ** values.
 ** 
 ** See Numerical Recipes, pages 484 - 486.
 */  
void 
linalg::elmhes(icsiarray<double>& a, const int n) {
    int m,j,i = 0;
    double y,x,tmp = 0.0;
        
    for (m=2;m<n;m++) {
	x=0.0;
	i=m;
	for (j=m;j<=n;j++) {
	    if (fabs(a[(j-1)*n+(m-2)]) > fabs(x)) {
		x=a[(j-1)*n+(m-2)];
		i=j;
	    }
	}
	if (i != m) {
	    for (j=m-1;j<=n;j++) {
		int idxo =(i-1)*n+(j-1);
		int idxn =(m-1)*n+(j-1);
		tmp=a[idxo]; 
		a[idxo]=a[idxn]; 
		a[idxn]=tmp;}
	    for (j=1;j<=n;j++) {
		int idxo = (j-1)*n+(i-1);
		int idxn = (j-1)*n+(m-1);
		tmp=a[idxo]; 
		a[idxo]=a[idxn]; 
		a[idxn]=tmp;
	    }
	}
	if (x != 0) {
	    for (i=m+1;i<=n;i++) {
		y=a[(i-1)*n+(m-2)];
		if (y != 0) {
		    y /= x;
		    a[(i-1)*n+(m-2)]=y;
		    for (j=m;j<=n;j++)
			a[(i-1)*n+(j-1)] -= y*a[(m-1)*n+(j-1)];
		    for (j=1;j<=n;j++)
			a[(j-1)*n+(m-1)] += y*a[(j-1)*n+(i-1)];
		}
	    }
	}
    }
}

/**
 ** Description from Numerical Recipes:
 **
 ** Finds all eignvalues of an upper Hessenberg matrix a[1..n][1..n]. 
 ** On input a can be exactly as output from elmhes Sec.11.5; on output
 ** it is destroyed. The real and imaginary parts of the eigenvalues
 ** are returned in wr[1..n] and wi[1..n], respectively.
 ** 
 ** See Numerical Recipes, pages 491 - 493.
 */
void 
linalg::hqr(icsiarray<double>& a, const int n, 
	    icsiarray<double>& wr, icsiarray<double>& wi) {
        
    int nn,m,l,k,j,its,i,mmin = 0;
    double z,y,x,w,v,u,t,s,r,q,p = 0.0;

    double anorm = 0.0;
    for(i=1;i<=n;i++)
	for(j=imax(i-1,1);j<=n;j++) {
	    int idx = (i-1)*n+(j-1);
	    anorm += fabs(a[idx]);
	}

    nn=n;
    t=0.0;
    while (nn >= 1) {
	its=0;
	do {
	    for (l=nn;l>=2;l--) {
		s=fabs(a[(l-2)*n+(l-2)])+fabs(a[(l-1)*n+(l-1)]);
		if (s == 0.0) s=anorm;
		if ((fabs(a[(l-1)*n+(l-2)]) + s) == s) break;
	    }
	    x=a[(nn-1)*n+(nn-1)];
	    if (l == nn) {
		wr[nn-1]=x+t;
		wi[nn-1]=0.0; nn--;
	    } else {
		y=a[(nn-2)*n+(nn-2)];
		w=a[(nn-1)*n+(nn-2)]*a[(nn-2)*n+(nn-1)];
		if (l == (nn-1)) {
		    p=0.5*(y-x);
		    q=p*p+w;
		    z=sqrt(fabs(q));
		    x += t;
		    if (q >= 0.0) {
			z=p+sign(z,p);
			wr[nn-2]=wr[nn-1]=x+z;
			if (z != 0.0) wr[nn-1]=x-w/z;
			wi[nn-2]=wi[nn-1]=0.0;
		    } else {
			wr[nn-2]=wr[nn-1]=x+p;
			wi[nn-2]= -(wi[nn-1]=z);
		    }
		    nn -= 2;
		} else {
		    if (its == 30) error("Too many iterations in hqr."); 
		    if (its == 10 || its == 20) {
			t += x;
			for (i=1;i<=nn;i++) a[(i-1)*n+(i-1)] -= x;
			s=fabs(a[(nn-1)*n+(nn-2)])+fabs(a[(nn-2)*n+(nn-3)]);
			y=x=0.75*s;
			w = -0.4375*s*s;
		    }
		    ++its;
		    for (m=(nn-2);m>=l;m--) {
			z=a[(m-1)*n+(m-1)];
			r=x-z;
			s=y-z;
			p=(r*s-w)/a[m*n+(m-1)]+a[(m-1)*n+m];
			q=a[m*n+m]-z-r-s;
			r=a[(m+1)*n+m];
			s=fabs(p)+fabs(q)+fabs(r);
			p /= s;
			q /= s;
			r /= s;
			if (m == l) break;
			u=fabs(a[(m-1)*n+(m-2)])*(fabs(q)+fabs(r));
			v=fabs(p)*(fabs(a[(m-2)*n+(m-2)])+fabs(z)+fabs(a[m*n+m]));
			if ((u+v) == v) break;
		    }
		    for (i=m+2;i<=nn;i++) {
			a[(i-1)*n+(i-3)]=0.0;
			if  (i != (m+2)) a[(i-1)*n+(i-4)]=0.0;
		    }
		    for (k=m;k<=nn-1;k++) {
			if (k != m) {
			    p=a[(k-1)*n+(k-2)];
			    q=a[k*n+(k-2)];
			    r=0.0;
			    if (k != (nn-1)) r=a[(k+1)*n+(k-2)];
			    x=fabs(p)+fabs(q)+fabs(r);
			    if (x != 0) {
				p /= x;
				q /= x;
				r /= x;
			    }
			}
			if ((s=sign(sqrt(p*p+q*q+r*r),p)) != 0.0) {
			    if (k == m) {
				if (l != m)
				    a[(k-1)*n+(k-2)] = -a[(k-1)*n+(k-2)];
			    } else
				a[(k-1)*n+(k-2)] = -s*x;
			    p += s;
			    x=p/s;
			    y=q/s;
			    z=r/s;
			    q /= p;
			    r /= p;
			    for (j=k;j<=nn;j++) {
				p=a[(k-1)*n+(j-1)]+q*a[k*n+(j-1)];
				if (k != (nn-1)) {
				    p += r*a[(k+1)*n+(j-1)];
				    a[(k+1)*n+(j-1)] -= p*z;
				}
				a[k*n+(j-1)] -= p*y;
				a[(k-1)*n+(j-1)] -= p*x;
			    }
			    mmin = nn<k+3 ? nn : k+3;
			    for (i=l;i<=mmin;i++) {
				p=x*a[(i-1)*n+(k-1)]+y*a[(i-1)*n+k];
				if (k != (nn-1)) {
				    p += z*a[(i-1)*n+(k+1)];
				    a[(i-1)*n+(k+1)] -= p*r;
				}
				a[(i-1)*n+k] -= p*q;
				a[(i-1)*n+(k-1)] -= p;
			    }
			}
		    }
		}
	    }
	} while (l < nn-1);
    }
}

/**
 ** Compute the eigen values for a matrix. Warning: the input matrix
 ** will be destroyed by this operation!! The eigen values will be 
 ** returned in the icsiarray 'wr'.
 **
 ** (For more info on this algorithm, see Numerical Recipes in C, Chapter 11.)
 **
 */
void
linalg::getEigenVals(icsiarray<double>& a, const int n, icsiarray<double>& wr) {
    icsiarray<double> wi(n);	// imaginary part of eigenvalues
    balance(a,n);			// balance the matrix
    elmhes(a,n);			// convert to Hessenberg
    hqr(a,n,wr,wi);		// find the eigen values
}

/**
 ** Computes the inner product of two vectors (icsiarrays) of doubles.
 */
double
linalg::innerProd(const icsiarray<double>& x, const icsiarray<double>& y){

    if(x.size() != y.size())
	error("linalg::innerProd: x and y of different sizes");

    //icsiarray<double> p = x * y;
    // too strong for gcc-2.95
    double r = 0.0;
    for (int i = 0; i < x.size(); i++)
	r += x.get(i) * y.get(i);
    //icsiarray<double> p;
    //p.add_prod(x, y);
    //return p.sum();
    return r;
}

/**
 ** Computes the norm (length) of the given vector (icsiarray).
 */
double
linalg::vecnorm(const icsiarray<double>& a) {
    return sqrt(innerProd(a,a));
}

/**
 ** Normalize a vector (icsiarray) to have unit length.
 */
void
linalg::normalize(icsiarray<double>& a) {
    double len = vecnorm(a);
    if(len != 0.0)
	a.div(len);
}

/**
 ** Fills the given vector with random numbers. The resulting vector
 ** is normalized to have unit length.
 */
void
linalg::random_unit_vector(icsiarray<double>& a){
    // SHRT_MAX is defined in limits.h
    const double max = static_cast<double>(SHRT_MAX);
    const double half_max = static_cast<double>(SHRT_MAX/2);
    static int seeded = 0;	// has rand num generator been seeded?
    if(!seeded){
	//      srand48(time(0));
	srand48(1234L);
	seeded = 1;
    }
    for(int i=0;i<a.size();i++)
	a[i] = (drand48() * max - half_max);
    normalize(a);
}

/**
 ** Compute the Euclidean distance between two vectors (icsiarrays).
 */
double
linalg::euclidian_dist(const icsiarray<double>& x, 
		       const icsiarray<double>& y){
    // compute the difference between x and y, and square it
    //icsiarray<double> sqdiff = pow((x - y),2.0);
    // too strong for gcc-2.95
    //icsiarray<double> sqdiff = x;
    //sqdiff -= y;
    //sqdiff = pow(sqdiff,2.0);
    double sum = 0.0;
    for (int i = 0; i < x.size(); i++)
	sum += (x.get(i) - y.get(i)) * (x.get(i) - y.get(i));
    return sqrt(sum);
}
