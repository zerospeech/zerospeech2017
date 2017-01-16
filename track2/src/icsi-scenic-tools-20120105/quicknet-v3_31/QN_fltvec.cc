const char* QN_fltvec_rcsid = "$Header: /u/drspeech/repos/quicknet2/QN_fltvec.cc,v 1.22 2011/03/15 22:43:47 davidj Exp $";

// Floating point vector utility routines for QuickNet

#include <assert.h>
#include <string.h>
#include "QN_fltvec.h"

#if !QN_HAVE_DECL_DRAND48
extern "C" {
double drand48(void);
}
#endif

QNUInt32 qn_math = QN_MATH_NV;

void
qn_swapb_vd_vd(size_t len, const double *from, double* to)
{
    size_t i;

    for (i=len; i!=0; i--)
    {
	QNInt32 *p1 = (QNInt32 *)to++;
	QNInt32 *p3 = (QNInt32 *)from++;
	QNInt32 *p2 = p1 + 1;
	QNInt32 *p4 = p3 + 1;
	*p1 = qn_btoh_i32_i32(*p4);
	*p2 = qn_btoh_i32_i32(*p3);
    }
}


void
qn_urand48_ff_vf(size_t n, float min, float max, float *vec)
{
    const float range = max - min;

    size_t i;
    for (i=0; i<n; i++)
    {
        *vec++ = ((float) drand48() * range) + min;
    }
}

int
qn_findexp_f_i(float f)
{
    const double RECIP_LOG_2 = 1.442695040888963;

    return (int) ceil(log(fabs(f))*RECIP_LOG_2);
}

void
qn_nv_muladd_vffvf_vf(const size_t n,
	  const float* xvec, const float s, const float *yvec,
	  float* res)
{
    size_t i;
    for (i=0; i<n; i++)
        res[i] = s * xvec[i] + yvec[i];
}

void
qn_fe_sigmoid_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    size_t i;

    for (i=n; i!=0; i--)
    {
	*out_vec++ = qn_fe_sigmoid_f_f(*in_vec++);
    }
}

// Vector sigmoid using IBM vector routines
#ifdef QN_HAVE_LIBMASS
void
qn_ms_sigmoid_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    
    size_t i;
    int count = n;
    float* outp;

    outp = out_vec;
    for (i=n; i!=0; i--)
    {
	*outp++ = -(*in_vec++);
    }
    vsexp(out_vec, out_vec, &count);
    outp = out_vec;
    for (i=n; i!=0; i--)
    {
        (*outp++) += 1.0f;
    }
    vsrec(out_vec, out_vec, &count);
}
#endif

// Vector sigmoid using Intel Math Kernel Library routines
#ifdef QN_HAVE_LIBMKL
void
qn_mk_sigmoid_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    
    size_t i;
    int count = n;
    float* outp;

    outp = out_vec;
    for (i=n; i!=0; i--)
    {
	*outp++ = -(*in_vec++);
    }
    vsExp(count, out_vec, out_vec);
    outp = out_vec;
    for (i=n; i!=0; i--)
    {
        (*outp++) += 1.0f;
    }
    vsInv(count, out_vec, out_vec);
}
#endif

void
qn_nv_sigmoid_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    size_t i;

    for (i=0; i<n; i++)
    {
	*out_vec++ = qn_nv_sigmoid_f_f(*in_vec++);
    }
}

void
qn_nv_tanh_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    size_t i;

    for (i=n; i!=0; i--)
    {
	*out_vec++ = qn_nv_tanh_f_f(*in_vec++);
    }
}

void
qn_fe_tanh_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    size_t i;

    for (i=n; i!=0; i--)
    {
	*out_vec++ = qn_fe_tanh_f_f(*in_vec++);
    }
}

void
qn_nv_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    float max;
    float min;
    float sumexp = 0.0f;	/* Sum of exponents */
    float scale;		/* 1/sum of exponents */
    size_t i;

    qn_maxmin_vf_ff(n, in_vec, &max, &min);	/* Find constant bias. */
    for (i=0; i<n; i++)
    {
	float f;		/* Input value. */
	float e;		/* Exponent of current value. */

	f = in_vec[i];

#ifdef QN_HAVE_EXPF
	e = expf(f - max);
#else
	e = exp(f - max);
#endif
	out_vec[i] = e;
	sumexp += e;
    }
    scale = 1.0f/sumexp;
    for (i=0; i<n; i++)
    {
	out_vec[i] = out_vec[i] * scale;
    }

}
void
qn_fe_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    float max;
    float min;
    float sumexp = 0.0f;	/* Sum of exponents */
    float scale;		/* 1/sum of exponents */
    size_t i;
    QN_EXPQ_WORKSPACE;
    float* out_vec2 = out_vec;

    qn_maxmin_vf_ff(n, in_vec, &max, &min);	/* Find constant bias. */
    for (i=n; i!=0; i--)
    {
	float f;		/* Input value. */
	float e;		/* Exponent of current value. */

	f = *in_vec++;
	e = QN_EXPQ(f - max);
	*out_vec++ = e;
	sumexp += e;
    }
    scale = 1.0f/sumexp;
    for (i=0; i<n; i++)
    {
	*out_vec2 = (*out_vec2) * scale;
	out_vec2++;
    }
}

#ifdef QN_HAVE_LIBMASS
void
qn_ms_softmax_vf_vf(size_t n, const float* in_vec, float* out_vec)
{
    float max;
    float min;
    float sumexp = 0.0f;	/* Sum of exponents */
    float scale;		/* 1/sum of exponents */
    size_t i;
    float* outp;
    int count = n;

    qn_maxmin_vf_ff(n, in_vec, &max, &min);	/* Find constant bias. */
    outp = out_vec;
    for (i=n; i!=0; i--)
    {
	*outp++ = *in_vec++ - max;
    }
    vsexp(out_vec, out_vec, &count);
    outp = out_vec;
    for (i=n; i!=0; i--)
    {
      sumexp += (*outp++);
    }
    scale = 1.0f/sumexp;
    outp = out_vec;
    for (i=n; i!=0; i--)
    {
	*outp = (*outp) * scale;
	outp++;
    }
}
#endif

void
qn_htopre8_vf_vi8(size_t len, const float* from, QNInt8* to)
{
    size_t i;

    for (i=0; i<len; i++)
    {
	*to++ = qn_htopre8_f_i8(*from++);
    }
}

void
qn_pre8toh_vi8_vf(size_t len, const QNInt8* bits, float* res)
{
    float* table;		/* Pointer to the start of the lookup table */
    size_t i;			/* Local counter */
    
    table = qn_pre8table_vf();	/* Point to the table for converting pre8
				   bytes to floating point */
    for (i=0; i<len; i++)
    {
        *res++ = table[(QNUInt8) *bits++];
    }
}

void
qn_lna8toh_vi8_vf(size_t len, const QNInt8* bits, float* res)
{
    float* table;		/* Pointer to the start of the lookup table */
    size_t i;			/* Local counter */
    
    table = qn_lna8table_vf();	/* Point to the table for converting lna8
				   bytes to floating point */
    for (i=0; i<len; i++)
    {
        *res++ = table[(QNUInt8) *bits++];
    }
}

void
qn_htolna8_vf_vi8(size_t len, const float* from, QNInt8* to)
{
    size_t i;

    for (i=0; i<len; i++)
    {
	*to++ = qn_htolna8_f_i8(*from++);
    }
}

size_t
qn_imax_vf_u(size_t n, const float* v)
{
    float best_val = *v++;
    size_t index = 0;
    int nan = (int) qn_isnan_f_b(best_val); /* 1 if we have seen a nan */
    size_t i;

    assert(n>0);
    for (i=1; i<n; i++)
    {
        const float val = *v++;
	nan |= qn_isnan_f_b(val);
        if (val>best_val)
        {
            best_val = val;
            index = i;
        }
    }
    if (nan)
	return(QN_SIZET_BAD);
    else
	return index;
}

float*
qn_pre8table_vf(void)
{
    enum { TABLEN = 256 };
    static float table[TABLEN];
    static int init = 0;

    /* If the tables not build, then build it */
    if (!init)
    {
	size_t i;

        init = 1;
	/* Fill in the values in the table */
	for (i = 0; i<TABLEN; i++)
            table[i] = QN_SQRT2 * qn_ierf_f_f(2.0 * (i + 0.5) / TABLEN - 1.0);
    }
    return &table[0];
}


float*
qn_lna8table_vf(void)
{
    enum { TABLEN = 256 };
    static float table[TABLEN];
    static int init = 0;

    /* If the table is not built, then build it */
    if (!init)
    {
	size_t i;

        init = 1;
	/* Fill in the values in the table */
	for (i = 0; i<TABLEN; i++)
            table[i] = exp(-((double) i + 0.5) / QN_LNA8_SCALE);
    }
    return &table[0];
}

void
qn_copy_svf_vf(size_t len, size_t stride, const float* in, float* out)
{
    const float* from = in;
    float* to = out;

    while(len--)
    {
	*to = *from;
	to++;
	from += stride;
    }
}


void
qn_nv_copy_vf_mf(size_t mat_height, size_t vec_len, const float* vec, float* mat)
{
    size_t i, j;

    const float* vec_ptr;
    float* mat_ptr = mat;

    for (i=mat_height; i!=0; i--)
    {
	vec_ptr = vec;
	for (j=vec_len; j!=0; j--)
	{
	    (*mat_ptr++) = (*vec_ptr++);
	}
    }
}

void
qn_pp_copy_vf_mf(size_t mat_height, size_t vec_len, const float* vec, float* mat)
{
    size_t i;

    const float* vec_ptr;
    float* mat_ptr = mat;

    for (i=mat_height; i!=0; i--)
    {
	vec_ptr = vec;
	memcpy((void*) mat_ptr, (void*) vec, vec_len);
	mat_ptr += vec_len;
    }
}

float
qn_nv_mulsum_vfvf_f(const size_t n, const float* avec, const float* bvec)
{
    register float sum = 0.0f;
    size_t i;

    for (i=0; i<n; i++)
        sum += avec[i] * bvec[i];

    return sum;
}

// Sum the columns of a matrix into a vector.
// This is a hand-optimized version that should be faster than more
// straightforward code.

void
qn_sumcol_mf_vf(size_t rows, size_t cols, const float* in, float* res)
{
    const float *const res_end_b8p = res + (cols & ~7);
    const float *const res_end_p = res + cols;
    float* res_p;
    const float* in_p = in;
    size_t i;

    /* Initialize the result */
    res_p = res;
    while(res_p != res_end_b8p)
    {
	res_p[0] = in_p[0];
	res_p[1] = in_p[1];
	res_p[2] = in_p[2];
	res_p[3] = in_p[3];
	res_p[4] = in_p[4];
	res_p[5] = in_p[5];
	res_p[6] = in_p[6];
	res_p[7] = in_p[7];
	res_p += 8;
	in_p += 8;
    }
    while (res_p != res_end_p)
    {
	(*res_p++) = (*in_p++);
    }
    /* The main loop */
    for (i=1; i!=rows; i++)
    {
	res_p = res;
	while(res_p != res_end_b8p)
	{
	    res_p[0] += in_p[0];
	    res_p[1] += in_p[1];
	    res_p[2] += in_p[2];
	    res_p[3] += in_p[3];
	    res_p[4] += in_p[4];
	    res_p[5] += in_p[5];
	    res_p[6] += in_p[6];
	    res_p[7] += in_p[7];
	    res_p += 8;
	    in_p += 8;
	}
	while (res_p != res_end_p)
	{
	    (*res_p++) += (*in_p++);
	}
    }
}

void
qn_copy_mf_smf(size_t height, size_t in_width, size_t stride,
	       const float* from, float* to)
{
    size_t i;

    for (i=0; i<height; i++)
    {
	qn_copy_vf_vf(in_width, from, to);
	from += in_width;
	to += stride;
    }
}

void
qn_copy_smf_mf(size_t height, size_t width, size_t stride,
		const float* from, float* to)
{
    size_t i;

    for (i=0; i<height; i++)
    {
	qn_copy_vf_vf(width, from, to);
	from += stride;
	to += width;
    }
}

void
qn_trans_mf_mf(size_t in_height, size_t in_width, const float *in, float *res)
{
    size_t i, j;
    const float *from;		/* Get elements from here */
    float *to;			/* Put elements here */
    float *tocol;		/* Current column in result */

    from = in;
    to = res;
    tocol = res;
    for (i=0; i<in_height; i++)
    {
	to = tocol;
	for (j=0; j<in_width; j++)
	{
	    *to = *from;
	    from++;
	    to += in_height;
	}
	tocol++;
    }
}

// Some naive routines that have blas or pp versions elsewhere

void
qn_nv_mul_mfvf_vf(const size_t rows, const size_t cols,
		  const float *in_mat, const float *in_vec, float *out_vec)
{

    size_t i;
    size_t j;

    for (i=0; i<rows; i++)
    {
        float sum = 0.0;
        const float *inp = in_vec;
        for (j=0; j<cols; j++)
        {
            sum += (*in_mat++) * (*inp++);
        }
        *out_vec++ = sum;
    }
}

void
qn_nv_mul_vfmf_vf(const size_t rows, const size_t cols,
		  const float *in_vec, const float *in_mat, float *out_vec)
{
    size_t i;
    size_t j;

    for (i=0; i<cols; i++)
    {
        float sum = 0.0;
        const float *vp = in_vec;
        const float *mp = in_mat + i;
        for (j=0; j<rows; j++)
        {
            sum += (*mp) * (*vp++);
            mp += cols;
        }
        *out_vec++ = sum;
    }
}

void
qn_nv_mulacc_fvfvf_mf(const size_t rows, const size_t cols,
		      const float scale,
		      const float *in_vec, const float *in_vec_t,
		      float *res_mat)
{
    size_t i;
    size_t j;

    for (i=0; i<rows; i++)
    {
        const float row_val = in_vec[i] * scale;

        const float *vtp = in_vec_t;

        for (j=0; j<cols; j++)
        {
            *res_mat++ += vtp[j] * row_val;
        }
    }
}


void
qn_nv_mulacc_fmfmf_mf(size_t Sm,size_t Sk,size_t Sn, float scale,
		   const float *A,const float *B,float *C)
{
    size_t i,j,k;
    float acc;

    for (i=0;i<Sm;i++)
    {
	for (j=0;j<Sn;j++)
	{
	    acc = 0.0f;
	    for (k=0;k<Sk;k++)
		acc += A[i*Sk+k]*B[k*Sn+j];
	    C[i*Sn+j] += acc * scale;
	}
    }
}

void
qn_nv_mulntacc_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
		    const float *A,const float *B,float *C)
{
  size_t i,j,k;
  for (i=0;i<Sm;i++)
    for (j=0;j<Sn;j++)
      for (k=0;k<Sk;k++)
        C[i*Sn+j] += A[i*Sk+k]*B[j*Sk+k];
}

void
qn_nv_multnacc_fmfmf_mf(size_t Sk,size_t Sm,size_t Sn, float scale,
			const float *A,const float *B,float *C)
{
    size_t i,j,k;
    float acc;

    for (i=0;i<Sm;i++)
    {
	for (j=0;j<Sn;j++)
	{
	    acc = 0.0f;
	    for (k=0;k<Sk;k++)
		acc += A[k*Sm+i]*B[k*Sn+j];
	    C[i*Sn+j] += acc * scale;
	}
    }
}

void
qn_nv_mulacc_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
		     const float *A,const float *B,float *C)
{
  size_t i,j,k;
  for (i=0;i<Sm;i++)
    for (j=0;j<Sn;j++)
      for (k=0;k<Sk;k++)
        C[i*Sn+j] += A[i*Sk+k]*B[k*Sn+j];
}

void
qn_nv_mul_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
		  const float *A,const float *B,float *C)
{
  size_t i,j,k;
  for (i=0;i<Sm;i++)
  {
      for (j=0;j<Sn;j++)
      {
	  C[i*Sn+j] = 0.0f;
	  for (k=0;k<Sk;k++)
	      C[i*Sn+j] += A[i*Sk+k]*B[k*Sn+j];
      }
  }
}

void
qn_nv_convol_vfvf_vf(size_t h_len, size_t x_len, const float *h,
		     const float *x, float *y)
{
  size_t i;
  const float *x_startp = x + h_len - 1;
  const float *x_endp = x - 1;

  for( i = 0; i != x_len - h_len + 1; i++ ) {
    float y_acc = 0.0;
    const float *hp = h;
    const float *xp;

    for( xp = x_startp; xp != x_endp; xp-- ) {
      y_acc += *xp * *hp;
      hp++;
    }
    *y++ = y_acc;
    x_startp++;
    x_endp++;
  }
}


size_t
qn_fread_Of_vf(size_t count, FILE* file, float* f)
{
    enum { bufsize = QN_FREAD_DEF_BUFSIZE/sizeof(*f) };
    float buf[bufsize];
    size_t rc = 0;
    size_t chunk = 0;
    size_t res = 0;

    while (count > 0 && rc == chunk)
    {
	chunk = qn_min_zz_z(bufsize, count);
	rc = fread(buf, sizeof(*f), chunk, file);
	qn_swapb_vf_vf(rc, buf, f);
	count -= rc;
	res += rc;
	f += rc;
    }
    return res;
}

size_t
qn_fread_Od_vd(size_t count, FILE* file, double* d)
{
    enum { bufsize = QN_FREAD_DEF_BUFSIZE/sizeof(*d) };
    double buf[bufsize];
    size_t rc = 0;
    size_t chunk = 0;
    size_t res = 0;

    while (count > 0 && rc == chunk)
    {
	chunk = qn_min_zz_z(bufsize, count);
	rc = fread(buf, sizeof(*d), chunk, file);
	qn_swapb_vd_vd(rc, buf, d);
	count -= rc;
	res += rc;
	d += rc;
    }
    return res;
}

size_t
qn_fread_Od_vf(size_t count, FILE* file, float* f)
{
    enum { bufsize = QN_FREAD_DEF_BUFSIZE/sizeof(double) };
    double buf[bufsize];
    double buf2[bufsize];
    size_t rc = 0;
    size_t chunk = 0;
    size_t res = 0;

    while (count > 0 && rc == chunk)
    {
	chunk = qn_min_zz_z(bufsize, count);
	rc = fread(buf, sizeof(double), chunk, file);
	qn_swapb_vd_vd(rc, buf, buf2);
	qn_conv_vd_vf(rc, buf2, f);
	count -= rc;
	res += rc;
	f += rc;
    }
    return res;
}

size_t
qn_fread_Of_vd(size_t count, FILE* file, double* d)
{
    enum { bufsize = QN_FREAD_DEF_BUFSIZE/sizeof(float) };
    float buf[bufsize];
    float buf2[bufsize];
    size_t rc = 0;
    size_t chunk = 0;
    size_t res = 0;

    while (count > 0 && rc == chunk)
    {
	chunk = qn_min_zz_z(bufsize, count);
	rc = fread(buf, sizeof(float), chunk, file);
	qn_swapb_vf_vf(rc, buf, buf2);
	qn_conv_vf_vd(rc, buf2, d);
	count -= rc;
	res += rc;
	d += rc;
    }
    return res;
}

size_t
qn_fread_Nf_vd(size_t count, FILE* file, double* d)
{
    enum { bufsize = QN_FREAD_DEF_BUFSIZE/sizeof(float) };
    float buf[bufsize];
    size_t rc = 0;
    size_t chunk = 0;
    size_t res = 0;

    while (count > 0 && rc == chunk)
    {
	chunk = qn_min_zz_z(bufsize, count);
	rc = fread(buf, sizeof(float), chunk, file);
	qn_conv_vf_vd(rc, buf, d);
	count -= rc;
	res += rc;
	d += rc;
    }
    return res;
}

size_t
qn_fread_Nd_vf(size_t count, FILE* file, float* f)
{
    enum { bufsize = QN_FREAD_DEF_BUFSIZE/sizeof(float) };
    double buf[bufsize];
    size_t rc = 0;
    size_t chunk = 0;
    size_t res = 0;

    while (count > 0 && rc == chunk)
    {
	chunk = qn_min_zz_z(bufsize, count);
	rc = fread(buf, sizeof(double), chunk, file);
	qn_conv_vd_vf(rc, buf, f);
	count -= rc;
	res += rc;
	f += rc;
    }
    return res;
}
