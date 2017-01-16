// $Header: /u/drspeech/repos/quicknet2/QN_fltvec.h,v 1.25 2011/03/15 22:43:47 davidj Exp $

// Vector floating point utility routines for QuickNet

#ifndef QN_fltvec_h_INCLUDED
#define QN_fltvec_h_INCLUDED

#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <QN_config.h>
#ifdef QN_HAVE_SUNMATH_H
#include <sunmath.h>
#endif
#ifdef QN_HAVE_SUNPERF_H
#include <sunperf.h>
#endif
#ifdef QN_HAVE_ACML_H
#include <acml.h>
#endif
#ifdef QN_HAVE_ESSL_H
#include <essl.h>
#endif
#ifdef QN_HAVE_MKL_H
#include "mkl.h"
#endif
#ifdef QN_HAVE_CBLAS_H
extern "C" {
#include "cblas.h"
}
#endif
#ifdef QN_HAVE_VECLIB_CBLAS_H
// MacOS CBLAS
#include <vecLib/cblas.h>
#endif
#ifdef QN_HAVE_MASSV_H
#include <massv.h>
#endif

#include "QN_types.h"
#include "QN_intvec.h"

// Some BLAS use char* for the transpose values, some use char's
#ifdef QN_HAVE_ESSL_H
#define QN_BLAS_TRANSPOSE "T"
#define QN_BLAS_NOTRANSPOSE "N"
#else
#define QN_BLAS_TRANSPOSE 'T'
#define QN_BLAS_NOTRANSPOSE 'N'
#endif

//// This global variable decides what type of optimization to use.

extern QNUInt32 qn_math;

static inline float
qn_max_ff_f(float a, float b)
{
    if (a>=b)
	return a;
    else
	return b;
}

static inline float
qn_min_ff_f(float a, float b)
{
    if (a<=b)
	return a;
    else
	return b;
}

static inline double
qn_max_dd_d(double a, double b)
{
    if (a>=b)
	return a;
    else
	return b;
}

static inline double
qn_min_dd_d(double a, double b)
{
    if (a<=b)
	return a;
    else
	return b;
}

//// These are in QN_fltvec.cc ////
// Convert a big-endian double vector into native format
void qn_btoh_vd_vd(size_t len, const double *from, double *to);
// Convert a native double vector big endiant
void qn_htob_vd_vd(size_t len, const double *from, double *to);
// Convert a little-endian double vector into native format
void qn_ltoh_vd_vd(size_t len, const double *from, double *to);
// Convert a native double vector little endian format
void qn_htol_vd_vd(size_t len, const double *from, double *to);
void qn_nv_sigmoid_vf_vf(size_t, const float* in_vec, float* out_vec);
void qn_fe_sigmoid_vf_vf(size_t, const float* in_vec, float* out_vec);
void qn_ms_sigmoid_vf_vf(size_t, const float* in_vec, float* out_vec);
void qn_mk_sigmoid_vf_vf(size_t, const float* in_vec, float* out_vec);
void qn_nv_tanh_vf_vf(size_t, const float* in_vec, float* out_vec);
void qn_fe_tanh_vf_vf(size_t, const float* in_vec, float* out_vec);
void qn_nv_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec);
void qn_fe_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec);
void qn_ms_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec);
void qn_mk_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec);
void qn_htopre8_vf_vi8(size_t len, const float* from, QNInt8* to);
void qn_pre8toh_vi8_vf(size_t len, const QNInt8* bits, float* res);
void qn_htolna8_vf_vi8(size_t len, const float* from, QNInt8* to);
void qn_lna8toh_vi8_vf(size_t len, const QNInt8* bits, float* res);
size_t qn_imax_vf_u(size_t n, const float* v);
float* qn_pre8table_vf(void);
float* qn_lna8table_vf(void);
void qn_urand48_ff_vf(size_t n, float min, float max, float *vec);
int qn_findexp_f_i(float f);
// Sum the columns of a matrix into a vector.
void qn_sumcol_mf_vf(size_t rows, size_t cols, const float* in, float* res);

// Copy a dense matrix into a strided matrix.
void qn_copy_mf_smf(size_t height, size_t in_width, size_t stride,
		    const float* from, float* to);
// Copy a strided matrix into a dense matrix.
void qn_copy_smf_mf(size_t height, size_t width, size_t stride,
		    const float* from, float* to);
// Transopos a matrix.  Note that the height and width refer to the first 
// matrix - they're the other way around in the second.
void qn_trans_mf_mf(size_t in_height, size_t in_width, const float *in,
		    float *res);

// Routines for reading and writing vectors from streams, doing
// type and endianness conversion if necessary.
// "B"=bigendian, "L"=littleendian, "N"=nativeendian, "O"=other-endian

// The size of the buffer in bytes
enum { QN_FREAD_DEF_BUFSIZE = 16384 };

inline size_t
qn_fread_Nf_vf(size_t count, FILE* file, float* f)
{
    return(fread((void*) f, sizeof(float), count, file));
}
inline size_t
qn_fread_Nd_vd(size_t count, FILE* file, double* d)
{
    return(fread((void*) d, sizeof(double), count, file));
}

size_t qn_fread_Of_vf(size_t count, FILE* file, float* f);
size_t qn_fread_Od_vd(size_t count, FILE* file, double* d);
size_t qn_fread_Od_vf(size_t count, FILE* file, float* f);
size_t qn_fread_Of_vd(size_t count, FILE* file, double* d);
size_t qn_fread_Nf_vd(size_t count, FILE* file, double* d);
size_t qn_fread_Nd_vf(size_t count, FILE* file, float* f);

inline size_t
qn_fread_Bf_vf(size_t count, FILE* file, float* f)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Nf_vf(count, file, f);
#else
    return qn_fread_Of_vf(count, file, f);
#endif
}

inline size_t
qn_fread_Bf_vd(size_t count, FILE* file, double* d)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Nf_vd(count, file, d);
#else
    return qn_fread_Of_vd(count, file, d);
#endif
}

inline size_t
qn_fread_Bd_vf(size_t count, FILE* file, float* f)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Nd_vf(count, file, f);
#else
    return qn_fread_Od_vf(count, file, f);
#endif
}

inline size_t
qn_fread_Bd_vd(size_t count, FILE* file, double* d)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Nd_vd(count, file, d);
#else
    return qn_fread_Od_vd(count, file, d);
#endif
}

inline size_t
qn_fread_Lf_vf(size_t count, FILE* file, float* f)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Of_vf(count, file, f);
#else
    return qn_fread_Nf_vf(count, file, f);
#endif
}

inline size_t
qn_fread_Lf_vd(size_t count, FILE* file, double* d)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Of_vd(count, file, d);
#else
    return qn_fread_Nf_vd(count, file, d);
#endif
}

inline size_t
qn_fread_Ld_vf(size_t count, FILE* file, float* f)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Od_vf(count, file, f);
#else
    return qn_fread_Nd_vf(count, file, f);
#endif
}

inline size_t
qn_fread_Ld_vd(size_t count, FILE* file, double* d)
{
#ifdef QN_WORDS_BIGENDIAN
    return qn_fread_Od_vd(count, file, d);
#else
    return qn_fread_Nd_vd(count, file, d);
#endif
}


// Stuff we need below
inline void
qn_copy_f_vf(size_t len, float from, float* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = from;
}
    
// Convolve two vectors of floats
void qn_nv_convol_vfvf_vf(size_t h_len, size_t x_len, const float *h,
			  const float *x, float *y);
void qn_pp_convol_vfvf_vf(size_t h_len, size_t x_len, const float *h,
			  const float *x, float *y);
inline void
qn_convol_vfvf_vf(size_t h_len, size_t x_len, const float *h,
		       const float *x, float *y)
{
// PP version doesn't work - davidj - 2007/02/18
//  if (qn_math & QN_MATH_PP)
//	qn_pp_convol_vfvf_vf(h_len, x_len, h, x, y);
//  else
    qn_nv_convol_vfvf_vf(h_len, x_len, h, x, y);
}

// Matrix-vector multiply (forward pass)
void qn_nv_mul_mfvf_vf(const size_t rows, const size_t cols,
			const float* mat, const float* vec, float* res);
void qn_pp_mul_mfvf_vf(const size_t rows, const size_t cols,
		       const float* mat, const float* vec, float* res);
inline void qn_mul_mfvf_vf(const size_t rows, const size_t cols,
			   const float* mat, const float* vec, float* res)
{
    if (qn_math & QN_MATH_PP)
	qn_pp_mul_mfvf_vf(rows, cols, mat, vec, res);
    else
	qn_nv_mul_mfvf_vf(rows, cols, mat, vec, res);
}

// Scaled vector outer product (weight update)
void qn_nv_mulacc_fvfvf_mf(const size_t rows, const size_t cols,
			const float scale, const float* in_vec,
			const float* in_vec_t, float *mat);
void qn_pp_mulacc_fvfvf_mf(const size_t rows, const size_t cols,
			const float scale, const float* in_vec,
			const float* in_vec_t, float *mat);
inline void
qn_mulacc_fvfvf_mf(const size_t rows, const size_t cols,
		   const float scale, const float* in_vec,
		   const float* in_vec_t, float *mat)
{
    if (qn_math & QN_MATH_PP)
	qn_pp_mulacc_fvfvf_mf(rows, cols, scale, in_vec, in_vec_t, mat);
    else
	qn_nv_mulacc_fvfvf_mf(rows, cols, scale, in_vec, in_vec_t, mat);
}

// Matrix-vector multiply - back propagation
void qn_nv_mul_vfmf_vf(const size_t rows, const size_t cols,
		    const float* vec, const float* mat, float* res);
void qn_pp_mul_vfmf_vf(const size_t rows, const size_t cols,
		    const float* vec, const float* mat, float* res);
inline void
qn_mul_vfmf_vf(const size_t rows, const size_t cols,
	       const float* vec, const float* mat, float* res)
{
    if (qn_math & QN_MATH_PP)
	qn_pp_mul_vfmf_vf(rows, cols, vec, mat, res);
    else
	qn_nv_mul_vfmf_vf(rows, cols, vec, mat, res);
}

// SAXYP - used by mulacc_fvfvf_mf
void qn_nv_muladd_vffvf_vf(const size_t n,
			   const float* xvec, const float s, const float *yvec,
			   float* res);
void qn_pp_muladd_vffvf_vf(const size_t n,
			   const float* xvec, const float s, const float *yvec,
			   float* res);
inline void
qn_muladd_vffvf_vf(const size_t n,
		   const float* xvec, const float s, const float *yvec,
		   float* res)
{
    if (qn_math & QN_MATH_PP)
	qn_pp_muladd_vffvf_vf(n, xvec, s, yvec, res);
    else
	qn_nv_muladd_vffvf_vf(n, xvec, s, yvec, res);
}

// Dot product - used by mul_mfvf_vf
float qn_nv_mulsum_vfvf_f(const size_t n, const float* avec,
			  const float* bvec);
float qn_pp_mulsum_vfvf_f(const size_t n, const float* avec,
			  const float* bvec);
inline float
qn_mulsum_vfvf_f(const size_t n, const float* avec, const float* bvec)
{
    if (qn_math & QN_MATH_PP)
	return qn_pp_mulsum_vfvf_f(n, avec, bvec);
    else
	return qn_nv_mulsum_vfvf_f(n, avec, bvec);
}

//// These are in QN_fltvec_bmul1.cc
// Forward pass
// The internal strided routine
void qn_pp_mulntacc_mfmf_mf_sss(const int M, const int K, const int N,
				const float *const A, const float *const B,
				float *const C,
				const int Astride, const int Bstride,
				const int Cstride);
void qn_nv_mulntacc_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_rows,
			    const float* a, const float* b, float* res);
inline void
qn_pp_mulntacc_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_rows,
		       const float* a, const float* b, float* res)
{
    qn_pp_mulntacc_mfmf_mf_sss(a_rows, a_cols, b_rows,
			       a, b, res, a_cols, a_cols, b_rows);
}

#ifdef QN_HAVE_LIBBLAS
inline void
qn_bl_mulntacc_mfmf_mf(const int m, const int k, const int n,
		       const float* A, const float* B, float* C)
{
#ifdef QN_HAVE_LIBCBLAS
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
		m, n, k, 1.0f, A, k, B, k, 1.0f, C, n);
#else
    sgemm(QN_BLAS_TRANSPOSE, QN_BLAS_NOTRANSPOSE,
	  n, m, k, 1.0f, (float*)B, k, (float*) A, k, 1.0f, C, n);
#endif
}
#endif

inline void
qn_mulntacc_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_rows,
		    const float* a, const float* b, float* res)
{
#ifdef QN_HAVE_LIBBLAS
    if (qn_math & QN_MATH_BL)
	qn_bl_mulntacc_mfmf_mf(a_rows, a_cols, b_rows, a, b, res);
    else
#endif
    if (qn_math & QN_MATH_PP)
	// Go straight to the internal strided version for speed.
	qn_pp_mulntacc_mfmf_mf(a_rows, a_cols, b_rows,
				   a, b, res);
    else
	qn_nv_mulntacc_mfmf_mf(a_rows, a_cols, b_rows, a, b, res);
}

//// These are in QN_fltvec_bmul2.cc
// Weight update
// Internal strided version
void qn_pp_multnacc_fmfmf_mf_sss(const int M, const int K, const int N, const
 				 float *const A, const float *const B,
				 float *const C,
				 const int Astride, const int Bstride,
				 const int Cstride, const float alpha);
void qn_nv_multnacc_fmfmf_mf(size_t Sk,size_t Sm,size_t Sn, float scale,
			     const float *A,const float *B,float *C);

inline void
qn_pp_multnacc_fmfmf_mf(size_t a_rows, size_t a_cols, size_t b_cols,
			float scale, const float* a, const float* b,
			float* res)
{
    qn_pp_multnacc_fmfmf_mf_sss(a_cols, a_rows, b_cols,
				a, b, res, a_cols, b_cols, b_cols, scale);
}

#ifdef QN_HAVE_LIBBLAS
inline void
qn_bl_multnacc_fmfmf_mf(const int k, const int m, const int n,
			float alpha, const float* A, const float* B, float* C) 
{
#ifdef QN_HAVE_LIBCBLAS
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans,
		m, n, k, alpha,	A, m, B, n, 1.0f, C, n);
#else
    sgemm(QN_BLAS_NOTRANSPOSE, QN_BLAS_TRANSPOSE,
	  n, m, k, alpha, (float*) B, n, (float*)A, m, 1.0f, C, n);
#endif
}
#endif



inline void
qn_multnacc_fmfmf_mf(size_t a_rows, size_t a_cols, size_t b_cols,
		     float scale, const float* a, const float* b,
		     float* res)
{
#ifdef QN_HAVE_LIBBLAS
    if (qn_math & QN_MATH_BL)
	qn_bl_multnacc_fmfmf_mf(a_rows, a_cols, b_cols, scale, a, b, res);
    else
#endif
    if (qn_math & QN_MATH_PP)
	qn_pp_multnacc_fmfmf_mf(a_rows, a_cols, b_cols, scale, a, b, res);
    else
	qn_nv_multnacc_fmfmf_mf(a_rows, a_cols, b_cols, scale, a, b, res);
}

//// These are in QN_fltvec_bmul3.cc
// Back propagation
// THe optimized internal routine
void qn_pp_mulacc_mfmf_mf_sss(const int M, const int K, const int N, const
			      float 
			 *const A, const float *const B, float *const C, const
			 int Astride, const int Bstride, const int Cstride);

void qn_nv_mulacc_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
			  const float *A,const float *B,float *C);
inline void
qn_pp_mulacc_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_cols,
		   const float* a, const float* b, float* res)
{
    qn_pp_mulacc_mfmf_mf_sss(a_rows, a_cols, b_cols,
			     a, b, res, a_cols, b_cols, b_cols);
}
#ifdef QN_HAVE_LIBBLAS
inline void
qn_bl_mulacc_mfmf_mf(const int 	m, const int k, const int n, 
		     const float* A, const float* B, float* C) 
{
#ifdef QN_HAVE_LIBCBLAS
    cblas_sgemm(CblasRowMajor,CblasNoTrans, CblasNoTrans,
		m, n, k, 1.0f, A, k, B, n, 1.0f, C, n);
#else
    sgemm(QN_BLAS_NOTRANSPOSE, QN_BLAS_NOTRANSPOSE,
	  n, m, k, 1.0f, (float*) B, n, (float*)A, k, 1.0f, C, n);
#endif
}
#endif

inline void
qn_mulacc_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_cols,
		  const float* a, const float* b, float* res)
{
#ifdef QN_HAVE_LIBBLAS
    if (qn_math & QN_MATH_BL)
	qn_bl_mulacc_mfmf_mf(a_rows, a_cols, b_cols, a, b, res)	;
    else
#endif
    if (qn_math & QN_MATH_PP)
	qn_pp_mulacc_mfmf_mf(a_rows, a_cols, b_cols, a, b, res)	;
    else
	qn_nv_mulacc_mfmf_mf(a_rows, a_cols, b_cols, a, b, res)	;
}

void qn_nv_mul_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
		       const float *A,const float *B,float *C);
inline void
qn_pp_mul_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_cols,
		  const float* a, const float* b, float* res)
{
    // A hack because we do not have a non-accumulating version
    qn_copy_f_vf(a_rows * b_cols, 0.0f, res);
    qn_pp_mulacc_mfmf_mf_sss(a_rows, a_cols, b_cols,
			     a, b, res, a_cols, b_cols, b_cols);
}

#ifdef QN_HAVE_LIBBLAS
inline void
qn_bl_mul_mfmf_mf(const int m, const int k, const int n, 
		  const float* A, const float* B, float* C) 
{
#ifdef QN_HAVE_LIBCBLAS
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
		m, n, k, 1.0, A, k, B, n, 0.0f, C, n);
#else
    sgemm(QN_BLAS_NOTRANSPOSE, QN_BLAS_NOTRANSPOSE,
	  n, m, k, 1.0, (float*) B, n, (float*) A, k, 0.0f, C, n);
#endif
}
#endif

inline void
qn_mul_mfmf_mf(size_t a_rows, size_t a_cols, size_t b_cols,
	       const float* a, const float* b, float* res)
{
#ifdef QN_HAVE_LIBBLAS
    if (qn_math & QN_MATH_BL)
	qn_bl_mul_mfmf_mf(a_rows, a_cols, b_cols, a, b, res);
    else
#endif
    if (qn_math & QN_MATH_PP)
	qn_pp_mul_mfmf_mf(a_rows, a_cols, b_cols, a, b, res);
    else
	qn_nv_mul_mfmf_mf(a_rows, a_cols, b_cols, a, b, res);
}


//// *** Inline routines ***

// Return true if a float is not a number
inline QNBool
qn_isnan_f_b(float f)
{
    /* With IEEE FP, a NaN does not equal anything */
    return ((f==f) ? QN_FALSE : QN_TRUE);
}

inline float
qn_nan_f()
{
#ifdef QN_WORDS_BIGENDIAN
#  define _qn_nan_bytes		{ 0x7f, 0xc0, 0, 0 }
#else
#  define _qn_nan_bytes		{ 0, 0, 0xc0, 0x7f }
#endif

    static union { unsigned char c[4]; float f; } _qn_nan_union =
	{ _qn_nan_bytes };
    return _qn_nan_union.f;
}


// Here we have macros for really hacky but quick exponential function.
// This was taken from Chris Oei's version and tidied up a little.
// I don't know anything about the origin, but it appears to be relying
// on the fact that storing an int in the exponent of a double gives
// you an exponential in the higher bits with the lower bits being
// used to add a straight line fit.
// Note that, due to the use of the QN_WORDS_BIGENDIAN macro, only works on
// machines where the words-in-a-double ordering  is the same as the
// bytes-in-a-word ordering.  There are exceptions, e.g. the Vax!
// It also relies on the static qn_d2i being initialized to zero.
// ARLO: This is from the paper "A Fast, Compact Approximation of the
//       Exponential Function" by Schraudolph (1999).  It is only valid
//       for the range -700 to 700, so I added this check.  It can be
//       about 4x faster for integer inputs, but the conversion of float
//       to int just about cancels out the speedup.

//For integers, set QN_EXP_A = 1512775
#define QN_EXP_A (1048576/M_LN2)
#define QN_EXP_C 60801
#ifdef QN_WORDS_BIGENDIAN
#define QN_EXPQ(y) (qn_d2i.n.j = (int) (QN_EXP_A*(y)) + (1072693248 - QN_EXP_C), (y > -700.0f && y < 700.0f) ? qn_d2i.d : exp(y) )
#else 
#define QN_EXPQ(y) (qn_d2i.n.i = (int) (QN_EXP_A*(y)) + (1072693248 - QN_EXP_C), (y > -700.0f && y < 700.0f) ? qn_d2i.d : exp(y) )
#endif
#define QN_EXPQ_WORKSPACE union { double d; struct { int j,i;} n; } qn_d2i; qn_d2i.d = 0.0;

inline float
qn_fe_sigmoid_f_f(float x)
{
    QN_EXPQ_WORKSPACE;
    return 1.0f/(1.0f + QN_EXPQ(-x));
}

inline float
qn_nv_sigmoid_f_f(float x)
{
#ifdef QN_HAVE_EXPF
    return 1.0f/(1.0f + expf(-x));
#else
    return 1.0f/(1.0f + exp(-x));
#endif
}

inline float
qn_sigmoid_f_f(float x)
{
    float r;

    if (qn_math & QN_MATH_FE)
	r = qn_fe_sigmoid_f_f(x);
    else
	r = qn_nv_sigmoid_f_f(x);
    return r;
}


inline void
qn_sigmoid_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
#ifdef QN_HAVE_LIBMASS
    if (qn_math & QN_MATH_BL)
        qn_ms_sigmoid_vf_vf(n, in_vec, out_vec);
    else
#endif
#ifdef NEVER /* QN_HAVE_LIBMKL */
    if (qn_math & QN_MATH_BL)
        qn_mk_sigmoid_vf_vf(n, in_vec, out_vec);
    else
#endif
    if (qn_math & QN_MATH_FE)
	qn_fe_sigmoid_vf_vf(n, in_vec, out_vec);
    else
	qn_nv_sigmoid_vf_vf(n, in_vec, out_vec);
}

inline float
qn_fe_tanh_f_f(float x)
{
    QN_EXPQ_WORKSPACE;
    float t;

    t = -2.0 * x;
    return 2.0f/(1.0f + QN_EXPQ(t)) - 1.0f;
}

inline float
qn_nv_tanh_f_f(float x)
{
#ifdef QN_HAVE_TANHF
    return tanhf(x);
#else
    return tanh(x);
#endif
}

inline float
qn_tanh_f_f(float x)
{
    float r;

    if (qn_math & QN_MATH_FE)
	r = qn_fe_tanh_f_f(x);
    else
	r = qn_nv_tanh_f_f(x);
    return r;
}

inline void
qn_tanh_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    if (qn_math & QN_MATH_FE)
	qn_fe_tanh_vf_vf(n, in_vec, out_vec);
    else
	qn_nv_tanh_vf_vf(n, in_vec, out_vec);
}

inline void
qn_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
#ifdef QN_HAVE_LIBMASSXX
    if (1)
        qn_ms_softmax_vf_vf(n, in_vec, out_vec);
    else
#endif
    if (qn_math & QN_MATH_FE)
	qn_fe_softmax_vf_vf(n, in_vec, out_vec);
    else
	qn_nv_softmax_vf_vf(n, in_vec, out_vec);
}

inline float
qn_ierf_f_f(float x)
{

     float last = 1.0f;		/* Current approximate ierf val */
     float curr = 0.0f;		/* Previous approximate ierf val */
     float erfap;		/* Erf() of current approximate ierf */
     float slope;		/* Slope at current point on ierf curve */
     const float DELTA = (1.0 / (1 << 10)); /* Delta use to calc. slope */
     const float LIMIT = (1.0 / (1 << 20)); /* Stop when error this small */
     
     while(fabs(last - curr) > LIMIT) {
	  erfap = erf(curr);
	  slope = (erf(curr + DELTA) - erf(curr)) / DELTA;
	  
	  last = curr;
	  curr += (x - erfap) / slope;
     }
     return(curr);
}

inline float
qn_erf_f_f(float x)
{
    return erf(x);
}

//************************************************/
//** Cambridge "pre-file" conversion functions ***/
//************************************************/
//
// The "pre" format is used by the Cambridge speech group to store
// speech features in a file efficiently.  The format uses bytes to encode
// a floating point feature value.
//

inline QNInt8
qn_htopre8_f_i8(float val)
{
    QNInt32 res;

    /* QN_SQRT1_2 = sqrt(1/2) */
    res = (QNInt32) floor( (erf(val * QN_SQRT1_2)+1.0) * 128.0 );
    if (res>QN_UINT8_MAX)
	res = QN_UINT8_MAX;
    assert(res>=0);
    return ((QNInt8) res);
}

inline float
qn_pre8toh_i8_f(QNInt8 bits)
{
    float ret;			/* Return value */
    float* table;		/* Pointer to the start of the lookup table */
    
    table = qn_pre8table_vf();	/* Point to the table for converting pre8
				   bytes to floating point */
    ret = table[(QNUInt8) bits]; /* Perform the lookup */

    return ret;
}

//************************************************/
//** Cambridge "LNA file" conversion functions ***/
//************************************************/
//
// The "LNA" format is used by the Cambridge and ICSI speech groups to store
// probabilities efficiently.
// To convert a probability to an 8 bit LNA value, the following formula
// is used:
//
//       lna8 = trunc(-24 * log(x + VERY_SMALL))
//

#define QN_LNA8_VERY_SMALL (1e-37)
#define QN_LNA8_SCALE (24.0)

inline QNInt8
qn_htolna8_f_i8(float val)
{
    QNInt32 res;

    res = (int) floor( - QN_LNA8_SCALE * log( val + QN_LNA8_VERY_SMALL ) ) ;
    if (res>QN_UINT8_MAX)
	res = QN_UINT8_MAX;
    if (res<0)
	res = 0;
    return ((QNInt8) res);
}

inline float
qn_lna8toh_i8_f(QNInt8 lna)
{
    float ret;			/* Return value */
    float* table;		/* Pointer to the start of the lookup table */
    
    table = qn_lna8table_vf();	/* Point to the table for converting lna8
				   bytes to floating point */
    ret = table[(QNUInt8) lna]; /* Perform the lookup */

    return ret;
}

///////////////////////////
// Copies and endianness conversion.

inline void
qn_nv_copy_vf_vf(size_t len, const float* from, float* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = *from++;
}

inline void
qn_copy_vf_vf(size_t len, const float* from, float* to)
{
    qn_nv_copy_vf_vf(len, from, to);
}


inline void
qn_copy_vd_vd(size_t len, const double* from, double* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = *from++;
}

// btoh is for "bigendian to host" format conversion.  Note that on big
// endian systems this is a no-op.
inline void
qn_btoh_vf_vf(size_t len, const float* from, float* to)
{
    qn_btoh_vi32_vi32(len, (const QNInt32*) from, (QNInt32*) to);
}

inline void
qn_htob_vf_vf(size_t len, const float* from, float* to)
{
    qn_htob_vi32_vi32(len, (const QNInt32*) from, (QNInt32*) to);
}


// ltoh is for "littleendian to host" format conversion.  Note that on little
// endian systems this is a no-op.
inline void
qn_ltoh_vf_vf(size_t len, const float* from, float* to)
{
    qn_ltoh_vi32_vi32(len, (const QNInt32*) from, (QNInt32*) to);
}

inline void
qn_htol_vf_vf(size_t len, const float* from, float* to)
{
    qn_htol_vi32_vi32(len, (const QNInt32*) from, (QNInt32*) to);
}

void qn_swapb_vd_vd(size_t len, const double *from, double* to);

inline void
qn_swapb_vf_vf(size_t len, const float *from, float* to)
{
    qn_swapb_vi32_vi32(len, (QNInt32*) from, (QNInt32*) to);
}

inline void
qn_btoh_vd_vd(size_t len, const double *from, double *to)
{
#ifdef QN_WORDS_BIGENDIAN
    qn_copy_vd_vd(len, from, to);
#else 
    qn_swapb_vd_vd(len, from, to);
#endif
}

inline void
qn_htol_vd_vd(size_t len, const double *from, double *to)
{
#ifndef QN_WORDS_BIGENDIAN
    qn_copy_vd_vd(len, from, to);
#else 
    qn_swapb_vd_vd(len, from, to);
#endif
}


inline void
qn_ltoh_vd_vd(size_t len, const double *from, double *to)
{
#ifndef QN_WORDS_BIGENDIAN
    qn_copy_vd_vd(len, from, to);
#else 
    qn_swapb_vd_vd(len, from, to);
#endif
}

inline void
qn_htob_vd_vd(size_t len, const double *from, double *to)
{
#ifdef QN_WORDS_BIGENDIAN
    qn_copy_vd_vd(len, from, to);
#else 
    qn_swapb_vd_vd(len, from, to);
#endif
}

inline void
qn_copy_d_vd(size_t len, double from, double* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = from;
}

void qn_copy_svf_vf(size_t len, size_t stride, const float* in, float* out);

void qn_nv_copy_vf_mf(size_t mat_height, size_t vec_len, const float* vec,
		      float* mat);

void qn_pp_copy_vf_mf(size_t mat_height, size_t vec_len, const float* vec,
		      float* mat);

// Fill each row of a matrix with the same vector.
inline void
qn_copy_vf_mf(size_t mat_height, size_t vec_len, const float* vec, float* mat)
{
    qn_nv_copy_vf_mf(mat_height, vec_len, vec, mat);
}

inline void
qn_conv_vf_vd(size_t len, const float* a, double* res)
{
    size_t i;

    for (i=len; i!=0; i--)
	*res++ = (double) *a++;
}

// Convert a vector of doubles to a vector of floats using rounding.

inline void
qn_conv_vd_vf(size_t len, const double* a, float* res)
{
    size_t i;

    for (i=len; i!=0; i--)
	*res++ = (float) *a++;
}

inline void
qn_acc_vf_vf(size_t n, const float *in_vec, float *res_vec)
{
    size_t i;

    for (i=n; i!=0; i--)
        (*res_vec++) += (*in_vec++);
}

inline void
qn_acc_vd_vd(size_t n, const double *in_vec, double *res_vec)
{
    size_t i;
    for (i=0; i<n; i++)
        res_vec[i] += in_vec[i];
}

inline void
qn_neg_vf_vf(size_t len, const float* from, float* to)
{
    size_t i;

    for (i=len; i!=0; i--)
    {
	*to++ = - (*from++);
    }
}

inline void
qn_sqrt_vf_vf(size_t len, const float* a, float* res)
{
    size_t i;

    for (i=0; i<len; i++)
    {
#ifdef QN_HAVE_SQRTF
	*res++ = sqrtf(*a++);
#else
	*res++ = sqrt(*a++);
#endif
    }
}

inline void
qn_sqr_vf_vf(size_t len, const float* a, float* res)
{
    size_t i;

    for (i=0; i<len; i++)
    {
	*res++ = (*a) * (*a);
	a++;
    }
}

inline void
qn_sqr_vd_vd(size_t len, const double* a, double* res)
{
    size_t i;

    for (i=0; i<len; i++)
    {
	*res++ = (*a) * (*a);
	a++;
    }
}

inline void
qn_recip_vf_vf(size_t len, const float* a, float* res)
{
    size_t i;

    for (i=0; i<len; i++)
    {
	*res++ = 1.0f/(*a++);
    }
}

inline void
qn_add_vfvf_vf(size_t n,
	       const float *in_vec1, const float *in_vec2,
	       float *res_vec)
{
    size_t i;
    for (i=0; i<n; i++)
        res_vec[i] = in_vec1[i] + in_vec2[i];
}

inline void
qn_sub_vff_vf
(size_t len, const float* a, float b, float* res)
{
    size_t i;

    for (i=0; i<len; i++)
	*res++ = (*a++) - b;
}

inline void
qn_sub_vfvf_vf(size_t n,
	       const float *in_vec1, const float *in_vec2,
	       float *res_vec)
{
    size_t i;
    for (i=n; i!=0; i--)
        (*res_vec++) = *(in_vec1++) - (*in_vec2++);
}

inline void
qn_sub_vdvd_vd(size_t n,
	       const double *in_vec1, const double *in_vec2,
	       double *res_vec)
{
    size_t i;
    for (i=0; i<n; i++)
        res_vec[i] = in_vec1[i] - in_vec2[i];
}

inline void
qn_mul_vff_vf(size_t len, const float* a, float b, float* res)
{
    size_t i;

    for (i=0; i<len; i++)
	*res++ = (*a++) * b;
}

inline void
qn_mul_vdd_vd(size_t len, const double* a, double b, double* res)
{
    size_t i;

    for (i=0; i<len; i++)
	*res++ = (*a++) * b;
}

inline void
qn_div_fvf_vf(size_t len, float a, const float* b, float* res)
{
    size_t i;

    for (i=0; i<len; i++)
	*res++ = a / (*b++);
}

inline void
qn_mul_vfvf_vf(size_t n,
	       const float *in_vec1, const float *in_vec2,
	       float *res_vec)
{
    size_t i;
    for (i=n; i!=0; i--)
        (*res_vec++) = (*in_vec1++) * (*in_vec2++);
}

inline void
qn_div_vfvf_vf(size_t n,
	       const float *in_vec1, const float *in_vec2,
	       float *res_vec)
{
    size_t i;
    for (i=0; i<n; i++)
        res_vec[i] = in_vec1[i] / in_vec2[i];
}

#if defined(QN_HAVE_LIBBLAS)
inline void
qn_bl_mulacc_vff_vf(size_t n, const float *in, float scale, float *acc)
{
#if defined(QN_HAVE_LIBCBLAS)
    cblas_saxpy(n, scale, in, 1, acc, 1);
#else
    saxpy(n, scale, (float*) in, 1, acc, 1);
#endif
}
#endif

inline void
qn_nv_mulacc_vff_vf(size_t n, const float *in, float scale, float *acc)
{
    size_t i;

    for (i=n; i!=0; i--)
        (*acc++) += scale * (*in++);
}

inline void
qn_mulacc_vff_vf(size_t n, const float *in, float scale, float *acc)
{
#if defined(QN_HAVE_LIBBLAS)
      if (qn_math & QN_MATH_BL)
  	qn_bl_mulacc_vff_vf(n, in, scale, acc);
      else
#endif
	qn_nv_mulacc_vff_vf(n, in, scale, acc);
}

inline void
qn_dsigmoid_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    size_t i;

    for (i=n; i!=0; i--)
    {
        const float y = *in_vec++;

	*out_vec++ = (1.0f - y) * y;
    }
}

inline void
qn_dtanh_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    size_t i;

    for (i=n; i!=0; i--)
    {
        const float y = *in_vec++;

	*out_vec++ = (1.0f - y) * (1.0f + y);
    }
}


inline void
qn_maxmin_vf_ff(size_t n, const float *vec, float *maxp, float *minp)
{
    if (n>0)
    {
        float max = *vec;
        float min = *vec++;
        size_t i;

        for (i=n-1; i!=0; i--)
        {
            const float elem = *vec++;
            if (elem > max)
                max = elem;
            else if (elem < min)
                min = elem;
        }

        *maxp = max;
        *minp = min;
    }
}

#endif /* #ifndef QN_fltvec_h_INCLUDED */

