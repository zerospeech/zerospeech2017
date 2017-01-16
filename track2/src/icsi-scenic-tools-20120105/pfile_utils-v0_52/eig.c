/*
 * $Header: /u/drspeech/repos/pfile_utils/eig.c,v 1.2 2012/01/05 21:47:22 dpwe Exp $
 *
 * Converted to "C" by
 *  Jeff Bilmes <bilmes@icsi.berkeley.edu>
 *
 * Eigenanalysis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <math.h>

#define SQR(a) ((a)*(a))
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

void tred2a(double *const a, /* nxn input matrix  */
	   const int n, 
	   double *const d, /* resulting diagonal elements */
	   double *const e) /* resulting off-diagonal elements */
{
  int l,k,j,i;
  double scale,hh,h,g,f;

  for (i=(n-1);i>0;i--) {
    l=i-1;
    h=scale=0.0;
    if (l > 0) {
      for (k=0;k<=l;k++)
	scale += fabs(a[i*n+k]);
      if (scale == 0.0)
	e[i]=a[i*n+l];
      else {
	double scale_inv = 1.0/scale;
	for (k=0;k<=l;k++) {
	  a[i*n+k] *= scale_inv;
	  h += a[i*n+k]*a[i*n+k];
	}
	f=a[i*n+l];
	g=(f >= 0.0 ? -sqrt(h) : sqrt(h));
	e[i]=scale*g;
	h -= f*g;
	a[i*n+l]=f-g;
	f=0.0;
	for (j=0;j<=l;j++) {
	  a[j*n+i]=a[i*n+j]/h;
	  g=0.0;
	  for (k=0;k<=j;k++)
	    g += a[j*n+k]*a[i*n+k];
	  for (;k<=l;k++)
	    g += a[k*n+j]*a[i*n+k];
	  e[j]=g/h;
	  f += e[j]*a[i*n+j];
	}
	hh=f/(h+h);
	for (j=0;j<=l;j++) {
	  f=a[i*n+j];
	  e[j]=g=e[j]-hh*f;
	  for (k=0;k<=j;k++)
	    a[j*n+k] -= (f*e[k]+g*a[i*n+k]);
	}
      }
    } else
      e[i]=a[i*n+l];
    d[i]=h;
  }
  d[0]=0.0;
  e[0]=0.0;
  /* Contents of this loop can be omitted if eigenvectors not
     wanted except for statement d[i]=a[i][i]; */
  for (i=0;i<n;i++) {
    l=i-1;
    if (d[i] != 0.0) {
      for (j=0;j<=l;j++) {
	g=0.0;
	for (k=0;k<=l;k++)
	  g += a[i*n+k]*a[k*n+j];
	for (k=0;k<=l;k++)
	  a[k*n+j] -= g*a[k*n+i];
      }
    }
    d[i]=a[i*n+i];
    a[i*n+i]=1.0;
    for (j=0;j<=l;j++) a[j*n+i]=a[i*n+j]=0.0;
  }
}



static 
double pythaga(double a, double b)
{
  double absa,absb,tmp;
  absa=fabs(a);
  absb=fabs(b);
  if (absa > absb) {
    tmp = absb/absa;
    return absa*sqrt(1.0+SQR(tmp));
  } else {
    tmp = absa/absb;
    return (absb == 0.0 ? 0.0 : absb*sqrt(1.0+SQR(tmp)));
  }
}



void tqlia(double *d,  /* diagonal elements */
	  double *e,   /* off diagonal elements */
	  int n, 
	  double *z)
{
  int m,l,iter,i,k;
  double s,r,p,g,f,dd,c,b;

  for (i=1;i<n;i++) e[i-1]=e[i];
  e[n-1]=0.0;
  for (l=0;l<n;l++) {
    iter=0;
    do {
      for (m=l;m<=(n-2);m++) {
	dd=fabs(d[m])+fabs(d[m+1]);
	if ((double)(fabs(e[m])+dd) == dd)
	  break;
      }
      if (m != l) {
	if (iter++ == 30) {
	  fprintf(stderr,"Error: Too many iterations in eigen evaluation.\n");
	  exit(-1);
	}
	g=(d[l+1]-d[l])/(2.0*e[l]);
	r=pythaga(g,1.0);
	g=d[m]-d[l]+e[l]/(g+SIGN(r,g));
	s=c=1.0;
	p=0.0;
	for (i=m-1;i>=l;i--) {
	  f=s*e[i];
	  b=c*e[i];
	  e[i+1]=(r=pythaga(f,g));
	  if (r == 0.0) {
	    d[i+1] -= p;
	    e[m]=0.0;
	    break;
	  }
	  s=f/r;
	  c=g/r;
	  g=d[i+1]-p;
	  r=(d[i]-g)*s+2.0*c*b;
	  d[i+1]=g+(p=s*r);
	  g=c*r-b;
	  for (k=0;k<n;k++) {
	    f=z[k*n+i+1];
	    z[k*n+i+1]=s*z[k*n+i]+c*f;
	    z[k*n+i]=c*z[k*n+i]-s*f;
	  }
	}
	if (r == 0.0 && i >= l) continue;
	d[l] -= p;
	e[l]=g;
	e[m]=0.0;
      }
    } while (m != l);
  }
}


void eigsrta(double *d, /* n-vector of eigenvalues */
	     double *v, /* nxn matrix of eigenvectors. */
	     int n)
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

void
eigenanalyasis(int n,
	       double *cov, /* nxn real symmetric covariance matrix */
	       double *vals,   /* n-vector, space for eigenvalues */
	       double *vecs)   /* nxn matrix, space for column eigenvectors */
{
  /* space for diagonal values */
  double *e;

  if ((e = (double*)malloc(n*sizeof(double))) == NULL) {
    fprintf(stderr,"eigenanalyasis: Can't allocate memory\n");
    exit(-1);
  }

  /* copy the cov to the vecs to not destroy it */
  memcpy(vecs,cov,sizeof(double)*n*n);

  tred2a(vecs,n,vals,e);
  tqlia(vals,e,n,vecs);
  eigsrta(vals,vecs,n);

  free(e);
}
	       
