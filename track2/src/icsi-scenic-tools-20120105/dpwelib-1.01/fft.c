/***********************************************************************\
*	fft.c								*
*   Fast Fourier Transform C library - 					*
*   Based on RECrandell's. With all declarations			*
*   dpwe 1992apr30   just imported billg's revision wholesale		*
*   dpwe 22jan90							*
*   08apr90 With experimental FFT2torl - converse of FFT2real		*
\***********************************************************************/

/*  Routines include:
    FFT2raw : Radix-2 FFT, in-place, with yet-scrambled result.
    FFT2    : Radix-2 FFT, in-place and in-order.
    FFTreal : Radix-2 FFT, with real data assumed,
	      in-place and in-order (this routine is the fastest of the lot).
    FFTarb  : Arbitrary-radix FFT, in-order but not in-place.
    FFT2dimensional : Image FFT, for real data, easily modified for other
	      purposes.
	
    To call an FFT, one must first assign the complex exponential factors:
    A call such as
	e = AssignBasis(NULL, size)
    will set up the my_complex *e to be the array of cos, sin pairs corresponding
    to total number of complex data = size.   This call allocates the
    (cos, sin) array memory for you.  If you already have such memory
    allocated, pass the allocated pointer instead of NULL.
 */

/* for csound, #include <stdio.h>
               #include <prototypes.h>
   instead of  <dpwelib.h>
   AND replace the two malloc()s with mmalloc()s
   dpwe 04feb92
 */

#include <dpwelib.h>
#include <genutils.h>	/* for Mymalloc() */
#include <math.h>
#include <fft.h>

/* 1994mar16 : used doubles for temp vars, not floats - faster for FPU? */
/*
#define DOUBLE double
typedef struct {
    DOUBLE re;
    DOUBLE im;
} DCOMPLEX;
#define CCOPY(d,s) d.re = s.re; d.im = s.im
*/
#define DOUBLE float	/* faster on SGI?? */
#define DCOMPLEX my_complex
#define CCOPY(d,s) d=s
/* for copying my_complex to DCOMPLEX */

LNODE	*lroot = NULL;	/* root of look up table list */

int IsPowerOfTwo(n) 	/* Query whether n is a pure power of 2 */
long n;
{
    while(n>1L) {
		if(n%2L) break;
		n >>= 1L;
	}
    return(n==1L);
}

my_complex *FindTable(n)	/* search our list of existing LUT's */
long    n;		/* set up globals expn and lutSize too */
{
    LNODE *plnode;

    plnode = lroot;
    while (plnode != NULL)  {
		if (plnode->size == n) 
	    	return plnode->table;
		plnode = plnode->next;
	}
    return NULL;
}

my_complex *AssignBasis(ex, n)
my_complex *ex;
long n;
{
    register long i;
    register DOUBLE a;
    LNODE    *plnode;
    my_complex  *expn;

    if(expn=FindTable(n))	
		return(expn);
    if(ex != NULL) 
		expn = ex;
    else
		expn = (my_complex *)Mymalloc(n,sizeof(my_complex),"FFT:AssignBasis");
    for(i=0; i<n; i++)  {
		a = 2 * PI * i / n;
		expn[i].re = cos(a);
		expn[i].im = -sin(a);
	}
    plnode = lroot;
    lroot = (LNODE *)Mymalloc(1L,sizeof(LNODE),"FFT:AsBa");
    lroot->next = plnode;
    lroot->size = n;
    lroot->table = expn;
    return expn;
}

void reverseDig(x, n, skip)
my_complex *x;
long n, skip;
{
	register long i, j, k, jj, ii;
	my_complex tmp;
	
	for (i = 0, j = 0; i < n - 1; i++) {
		if (i < j) {
			jj = j * skip;
			ii = i * skip;
			tmp = x[jj];
			x[jj] = x[ii];
			x[ii] = tmp;
		}
		k = n/2;
		while (k <= j) {
			j -= k;
			k >>= 1;
		}
		j += k;
	}
}

int PureReal(x, n)	/* Query whether the data is pure real. */
my_complex *x;
long n;
{
    register long m;

    for(m=0;m<n;m++) {
		if(x[m].im!=0.0) return(0);
    }
    return(1);
}

/*
 * Performs FFT on data assumed complex conjugate to give purely
 * real result.
 */
void FFT2torl(x, n, skip, scale, ex)
my_complex *x;
long n, skip;
FLOATARG scale;		 /* (frigged for ugens7.c) */
my_complex *ex;		 /* pass in lookup table */
{
register long half = n >> 1, m, mm;

	if (!(half >> 1)) return;
	Reals(x,n,skip,-1,ex);
	ConjScale(x, half + 1, 2.0 * scale);
	FFT2raw(x, half, 2L, skip, ex);
	reverseDig(x, half, skip);
	for (mm = half - 1; mm >= 0; mm--) {
		m = mm * skip;
		x[m << 1].re = x[m].re;
		x[(m << 1) + skip].re = -x[m].im;		/* need to conjugate result for true ifft */
		x[m << 1].im = x[(m << 1) + skip].im = 0.0;
	}
}

/*
 * Perform real FFT, data arrange as {re, 0, re, 0, re, 0...};
 * leaving full complex result in x.
 */
void FFT2real(x, n, skip, ex)
my_complex *x;
long n, skip;
my_complex *ex;	/* lookup table */
{
register long half = n >> 1, m, mm;

	if (!(half >> 1)) return;
	for (mm = 0; mm < half; mm++) {
		m = mm * skip;
		x[m].re = x[m << 1].re;
		x[m].im = x[(m << 1) + skip].re;
	}
	FFT2raw(x, half, 2L, skip, ex);
	reverseDig(x, half, skip);
	Reals(x, n, skip, 1L, ex);
}

/*
 * sign is 1 for FFT2real, -1 for torl.
 */
void Reals(x, n, skip, sign, ex)
my_complex *x;
long n, skip;
int sign;
my_complex *ex;	/* lookup table passed in */
{
register long half = n >> 1, quarter = half >> 1, m, mm;
register DOUBLE tmp;
DCOMPLEX a, b, s, t;

	half *= skip;
	n *= skip;
	/* We will look at the midpoint, x[n/2].  After a half-size fft, 
	   it has not been written, so it a *forward* Reals, when sign == 1, 
	   we need to copy to first value (pure real) there.  In the *reverse*
	   (sign == -1), it has already been set up */
	if (sign == 1) x[half]= x[0];	/* only for Yc to Xr */
	for (mm = 0; mm <= quarter; mm++) {
		m = mm * skip;
		s.re = (1.0 + ex[mm].im) / 2;
		s.im = -sign * ex[mm].re / 2;
		t.re = 1.0 - s.re;
		t.im = -s.im;
		CCOPY(a,x[m]);
		CCOPY(b,x[half - m]);
		b.im = -b.im;

		tmp = a.re;
		a.re = (a.re*s.re - a.im*s.im);
		a.im = (tmp*s.im + a.im*s.re);

		tmp = b.re;
		b.re = (b.re*t.re - b.im*t.im);
		b.im = (tmp*t.im + b.im*t.re);
	
		b.re += a.re;
		b.im += a.im;
	
		CCOPY(a,x[m]);
		a.im = -a.im;
		CCOPY(x[m],b);
		if (m) {
			 b.im = -b.im;
			 CCOPY(x[n - m],b);
		}
		CCOPY(b,x[half - m]);
	
		tmp = a.re;
		a.re = (a.re * t.re + a.im * t.im);
		a.im = (-tmp * t.im + a.im * t.re);
	
		tmp = b.re;
		b.re = (b.re * s.re + b.im * s.im);
		b.im = (-tmp * s.im + b.im * s.re);
	
		b.re += a.re;
		b.im += a.im;
	
		CCOPY(x[half - m],b);
		if (m) {
			 b.im = -b.im;
			 CCOPY(x[half + m],b);
		}
	}
}

void PackedReals(x, n, skip, sign, ex)
my_complex *x;
long n, skip;
int sign;
my_complex *ex;
{	/* Same as Reals(), but does not write conj. symm top half
       i.e. will only read x[0]..x[n/2?-1] and write x[0]..x[n/2]  */
register long half = n >> 1, quarter = half >> 1, m, mm;
register DOUBLE tmp;
DCOMPLEX a, b, s, t;

	half *= skip;
	n *= skip;
	/* We will look at the midpoint, x[n/2].  After a half-size fft, 
	   it has not been written, so it a *forward* Reals, when sign == 1, 
	   we need to copy to first value (pure real) there.  In the *reverse*
	   (sign == -1), it has already been set up */
	if (sign == 1) x[half]= x[0];	/* only for Yc to Xr */
	for (mm = 0; mm <= quarter; mm++) {
		m = mm * skip;
		s.re = (1.0 + ex[mm].im) / 2;
		s.im = -sign * ex[mm].re / 2;
		t.re = 1.0 - s.re;
		t.im = -s.im;
		CCOPY(a,x[m]);
		CCOPY(b,x[half - m]);
		b.im = -b.im;

		tmp = a.re;
		a.re = (a.re*s.re - a.im*s.im);
		a.im = (tmp*s.im + a.im*s.re);

		tmp = b.re;
		b.re = (b.re*t.re - b.im*t.im);
		b.im = (tmp*t.im + b.im*t.re);
	
		b.re += a.re;
		b.im += a.im;
	
		CCOPY(a,x[m]);
		a.im = -a.im;
		CCOPY(x[m],b);
		CCOPY(b,x[half - m]);
	
		tmp = a.re;
		a.re = (a.re * t.re + a.im * t.im);
		a.im = (-tmp * t.im + a.im * t.re);
	
		tmp = b.re;
		b.re = (b.re * s.re + b.im * s.im);
		b.im = (-tmp * s.im + b.im * s.re);
	
		b.re += a.re;
		b.im += a.im;
	
		CCOPY(x[half - m],b);
	}
}

/*
 * Perform 2D FFT on image of width w, height h (both powers of 2).
 * IMAGE IS ASSUMED PURE-REAL.	If not, the FFT2real call should be just FFT2.
 */
void FFT2dimensional(x, w, h, ex)
my_complex *x;
long w, h;
my_complex *ex;
{
register long i;
	for (i = 0; i < h; i++) FFT2real(x + i * w, w, 1L, ex);
	for (i = 0; i < w; i++) FFT2(x + i, h, w, ex);
}

/*
 * Conjugate and scale complex data (e.g. prior to IFFT by FFT)
 */
void ConjScale(x, n, scale)
my_complex *x;
long n;
FLOATARG scale;
{
DOUBLE pscale = scale;
DOUBLE miscale = -scale;

	while (n-- > 0) {
		x->re *= pscale;
		x->im *= miscale;
		x++;
	}
}

/*
 * Perform FFT for n = a power of 2.
 * The relevant data are the complex numbers x[0], x[skip], x[2*skip], ...
 */
void FFT2(x, n, skip, ex)
my_complex *x;
long n, skip;
my_complex *ex;
{
	FFT2raw(x, n, 1L, skip, ex);
	reverseDig(x, n, skip);
}

/*
 * Data is x,
 * data size is n,
 * dilate means: library global expn is the (cos, -j sin) array, EXCEPT for
 * effective data size n/dilate,
 * skip is the offset of each successive data term, as in "FFT2" above.
 */
void FFT2raw(x, n, dilate, skip, ex)
my_complex *x;
long n, dilate, skip;
my_complex *ex;	/* lookup table */
{
register long j, m = 1, p, q, i, k, n2 = n, n1;
register DOUBLE c, s, rtmp, itmp;

	while (m < n) {
		n1 = n2;
		n2 >>= 1;
		for (j = 0, q = 0; j < n2; j++) {
			c = ex[q].re;
			s = ex[q].im;
			q += m*dilate;
			for (k = j; k < n; k += n1) {
				p = (k + n2) * skip;
				i = k * skip;
				rtmp = x[i].re - x[p].re;
				x[i].re += x[p].re;
				itmp = x[i].im - x[p].im;
				x[i].im += x[p].im;
				x[p].re = (c * rtmp - s * itmp);
				x[p].im = (c * itmp + s * rtmp);
			 }
		}
		m <<= 1;
	}
}

/*
 * Perform direct Discrete Fourier Transform.
 */
void DFT(data, result, n, ex)
my_complex *data, *result;
long n;
my_complex *ex;
{
long j, k, m;
DOUBLE arg, s, c;

	for (j = 0; j < n; j++) {
		result[j].re = 0;
		result[j].im = 0;
		for (k = 0; k < n; k++) {
			 m = (j * k) % n;
			 c = ex[m].re;
			 s = ex[m].im;
			 result[j].re += (data[k].re * c - data[k].im * s);
			 result[j].im += (data[k].re * s + data[k].im * c);
		}
	}
}

/*
 * Simplified routines.
 */

void fft_packed(float *x, long n)
{   /* x is taken as real data
	   returned spectrum is {re,im} for n/2+1 bins I.E. <n+2> locations - 
	   Beware! */
    my_complex *ex = AssignBasis(NULL,n);
    long nOn2 = n/2;

	FFT2raw((my_complex *)x, nOn2, 2, 1, ex);
	reverseDig((my_complex *)x, nOn2, 1);
	PackedReals((my_complex *)x, n, 1, 1, ex);
}

void ifft_packed(float *x, long n)
{	/* takes n+2 floats interpreted as (n/2+1) {re,im} fourier coeffs
	   transforms back to n reals in the array */
    my_complex *ex = AssignBasis(NULL,n);
    long nOn2 = n/2;
    float *ptr;
    long i;

	PackedReals((my_complex *)x, n, 1, -1, ex);
	ConjScale((my_complex *)x, nOn2, 2.0/n);  	
	FFT2raw((my_complex *)x, nOn2, 2, 1, ex);
	reverseDig((my_complex *)x, nOn2, 1);
	ptr = x+1;
	for(i=0; i<nOn2; ++i)  {
		*ptr *= -1.0;
		ptr += 2;
	}
/*	VSMul(x+1, 2, -1.0, nOn2);			/* 'conjugate' packed result */
	x[n] = 0.0;	x[n+1] = 0.0;			/* clear middle value */
}

void fft_real(x, n)
my_complex *x;
long n;
{
my_complex *ex;
	ex = AssignBasis(NULL,n);
	FFT2real(x, n, 1L, ex);
}

void ifft_real(x, n)
my_complex *x;
long n;
{
my_complex *ex;
	ex = AssignBasis(NULL,n);
	FFT2torl(x, n, 1L, 1.0 / n, ex);
}

void fft_skip(x, n, skip)
my_complex *x;
long n;
long skip;
{
my_complex *ex;
	ex = AssignBasis(NULL,n);
	FFT2(x, n, skip, ex);
}

void fft(x, n)
my_complex *x;
long n;
{
	fft_skip(x, n, 1L);
}

void ifft_skip(x, n, skip)
my_complex *x;
long n;
long skip;
{
my_complex *ex;
my_complex tmp;
DOUBLE c, s;
long i;
long j, k;	/* for optimized skip indexing */
	ex = AssignBasis(NULL,n);
	/* reverse direction of x */
	for (i = 0; i < n/2; i++) {
		j = (n - 1 - i) * skip;
		k = i * skip;
		tmp = x[j];
		x[j] = x[k];
		x[k] = tmp;
	}
	FFT2(x, n, skip, ex);
	/* now do final twiddle */
	for (i = 0; i < n; i++) {
		k = i * skip;
		c = ex[i].re;
		s = ex[i].im;
		tmp.re = (x[k].re * c - x[k].im * s) / n;
		tmp.im = (x[k].re * s + x[k].im * c) / n;
		x[k] = tmp;
	}
}

void ifft(x, n)
my_complex *x;
long n;
{
	ifft_skip(x, n, 1L);
}

void fft_2d(x, n1, n2)
my_complex *x;
long n1, n2;	/* n1 is x or #cols, n2 is y or #rows */
{
long i;
fprintf(stderr, "fft_2d: n1=%ld, n2=%ld\n",n1,n2);
	/* cols */
	fprintf(stderr, "%ld cols:",n1);	fflush(stderr);
	for (i = 0; i < n1; i++) {
		fft_skip(x + i, n2, n1);
		if((i%20)==0) { fprintf(stderr, " %ld",i); fflush(stderr); }
	}
	fprintf(stderr, "\n%ld rows:",n2);	fflush(stderr);
	/* rows */
	for (i = 0; i < n2; i++) {
		fft(x + i * n1, n1);
		if((i%20)==0) { fprintf(stderr, " %ld",i); fflush(stderr); }
	}
	fprintf(stderr, "\n");
}

void ifft_2d(x, n1, n2)
my_complex *x;
long n1, n2;	/* n1 is x or #cols, n2 is y or #rows */
{
long i;
fprintf(stderr, "ifft_2d: n1=%ld, n2=%ld\n",n1,n2);
	/* cols */
	fprintf(stderr, "%ld cols:",n1);	fflush(stderr);
	for (i = 0; i < n1; i++) {
		ifft_skip(x + i, n2, n1);
		if((i%20)==0) { fprintf(stderr, " %ld",i); fflush(stderr); }
	}
	/* rows */
	fprintf(stderr, "\n%ld rows:",n2);	fflush(stderr);
	for (i = 0; i < n2; i++) {
		ifft(x + i * n1, n1);
		if((i%20)==0) { fprintf(stderr, " %ld",i); fflush(stderr); }
	}
	fprintf(stderr, "\n");
}

#ifdef MAIN

/* prototypes required! */
void ShowCpx(my_complex x[], long siz, char *s);
static void TestArith(void);
static void TestReals(void);
static void TestPacked(void);

void ShowCpx(x,siz,s)
    my_complex x[];
    long siz;
    char *s;
    {
    long i;

    printf("%s \n",s);
    for(i=0; i<siz; ++i)
	printf("%5.1f",x[i].re);
    printf("\n");
    for(i=0; i<siz; ++i)
	printf("%5.1f",x[i].im);
    printf("\n");
    }

static void TestArith(void)
/* Lets just see what happens if you do these things ..*/
    {
    float   a,b,c,d;

    a = -10000;
    b = (1/2);
    c = (a*b);
    printf("Result .. and what it should have been\n");
    printf("%f %f\n",c,a/2);
    }

#define tDLEN 16

#include <time.h>

/* frozen noise pattern to test .. */
float pat[] = { 3.00, 1.00, -3.00, 6.00, 4.00, 7.00, -9.00, -2.00, 
				2.00, 4.00, 5.00, -1.00, -8.00, 1.00, 9.00, -7.00 };

static void TestPacked(void)
{	/* test out our 'packed' functions */
	float data[2*tDLEN];
    int i, len;

    len = tDLEN;
    printf("Testing 'packed'\n");
    srand((int)time(NULL));
    for(i = 0; i<tDLEN; i++)  {
		data[i] = pat[i];    data[tDLEN+i] = 0.0;
	}
    ShowCpx((my_complex *)data, tDLEN, "Start data");
	fft_packed(data, tDLEN);
    ShowCpx((my_complex *)data, tDLEN, "Transform");
    ifft_packed(data, tDLEN);
    ShowCpx((my_complex *)data, tDLEN, "Final result");
}

static void TestReals(void)
{	/* test out our 'reals' function */
    my_complex data[tDLEN], datb[tDLEN];
    my_complex *e;
    int i,len;

    len = tDLEN /2;
    printf("Hello dan.\n");
    srand((int)time(NULL));
    e = AssignBasis(NULL,tDLEN);
    for(i = 0; i<tDLEN; i++)  {
	    datb[i].re = data[i].re = pat[i];
	    /* -7+(15&rand());   /* (i-1)? 0 : 1;	/* i+1; */
	    datb[i].im = data[i].im = 0*(tDLEN - i);
	}
    ShowCpx(data, tDLEN, "Start data");
/*    FFT2real(data, tDLEN, 1L, e);	/* */
/*    FFT2(data, tDLEN, 1L, e); 	/* */
	fft_real(data, tDLEN);
    ShowCpx(data, tDLEN, "Transform");
/*    data[tDLEN/2].re = 0; data[tDLEN/2].im = 0;
    for(i=tDLEN/2+1; i<tDLEN; ++i)  {
	    data[i].re = 0.0; data[i].im = 0.0;
	}
 */
	/* conjugate 2nd half of data should make no difference to FFT2torl */
/*    FFT2torl(data, tDLEN, 1L, 1.0/(float)tDLEN,e); /* */
/*	  ConjScale(data, tDLEN, 1.0/tDLEN);
      FFT2(data, tDLEN, 1L, e); /* */
    ifft_real(data, tDLEN);
    ShowCpx(data, tDLEN, "Final result");
}

void main(void)
    {
    TestPacked();
    }

#endif

