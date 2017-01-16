// $Header: /u/drspeech/repos/quicknet2/QN_cuvec.h,v 1.2 2011/05/13 02:34:02 davidj Exp $

#ifndef QN_cuvec_H_INCLUDED
#define QN_cuvec_H_INCLUDED

#ifdef QN_CUDA

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>

#include <cublas.h>
#include "QN_Logger.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

const int QN_CUDA_NTHREADS = 256;
const int QN_CUDA_MAXBLOCKS = 65535;

static const int NTHREADS = QN_CUDA_NTHREADS;
static const int MAXBLOCKS = QN_CUDA_MAXBLOCKS;

__global__ void qn_kern_mul_vfvf_vf(int n, const float* in_vec1,
				    const float* in_vec2, float* out_vec);
__global__ void qn_kern_sub_vfvf_vf(int n, const float* in_vec1,
				    const float* in_vec2, float* out_vec);
__global__ void qn_kern_sigmoid_vf_vf(int n, const float* in_vec, float* out_vec);
__global__ void qn_kern_tanh_vf_vf(int n, const float* in_vec, float* out_vec);
__global__ void qn_kern_dsigmoid_vf_vf(int n, const float* in_vec, float* out_vec);
__global__ void qn_kern_dtanh_vf_vf(int n, const float* in_vec, float* out_vec);
__global__ void qn_kern_copy_vf_mf(int mat_height, int vec_len,
				   const float*vec, float* mat);
__global__ void qn_kern_sumcol_mf_vf(int rows, int cols,
				     const float* in, float* res);
__global__ void qn_kern_multisoftmax_mf_mf(int rows, int cols,
					   const float* in_vec,
					   float* out_vec);

__global__ void qn_devnv_copy_vf_vf(size_t len, const float* from, float* to);
__global__ void qn_devnv_sub_vfvf_vf(size_t n, const float *in_vec1,
				     const float *in_vec2,
				     float *res_vec);
__global__ void qn_devnv_mul_vfvf_vf(size_t n, const float *in_vec1,
				     const float *in_vec2,
				     float *res_vec);
__global__ void qn_devnv_mulacc_vff_vf(size_t n, const float *in,
				       float scale, float *acc);
__global__ void qn_devnv_dsigmoid_vf_vf(size_t n, const float* in_vec,
				       float* out_vec);
__global__ void qn_devnv_sigmoid_vf_vf(int n, const float* in_vec,
				       float* out_vec);
__global__ void qn_devnv_softmax_vf_vf(int n, const float* in_vec,
				       float* out_vec);

__global__ void qn_devnv_copy_vf_mf(size_t mat_height, size_t vec_len,
				    const float* vec, float* mat);
__global__ void qn_devnv_sumcol_mf_vf(size_t rows, size_t cols,
				      const float* in, float* res);

__global__ void qn_devnv_mul_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
				     const float *A,const float *B,float *C);
__global__ void qn_devnv_mulntacc_mfmf_mf(size_t a_rows, size_t a_cols,
					  size_t b_rows, const float* a,
					  const float* b, float* res);
//__global__ void qn_devbl_mulntacc_mfmf_mf(size_t a_rows, size_t a_cols,
//					  size_t b_rows, const float* a,
//					  const float* b, float* res);
__global__ void qn_devnv_multnacc_fmfmf_mf(size_t Sk,size_t Sm,size_t Sn,
					   float scale,
					   const float *A,
					   const float *B,float *C);

////////////////////////
inline void
qn_dev_copy_vf_vf(size_t len, const float* from, float* to)
{
    cublasScopy(len, from, 1, to, 1);
}

inline void
qn_dev_sub_vfvf_vf(size_t n, const float *in_vec1,
		   const float *in_vec2, float *res_vec)
{
    int nblocks = (n + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_sub_vfvf_vf", "nblocks too large");
    qn_kern_sub_vfvf_vf<<<nblocks,NTHREADS>>>(n, in_vec1, in_vec2, res_vec);
}
inline void
qn_dev_mul_vfvf_vf(size_t n, const float *in_vec1,
		   const float *in_vec2,
		   float *res_vec)
{
    int nblocks = (n + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_mul_vfvf_vf", "nblocks too large");
    qn_kern_mul_vfvf_vf<<<nblocks,NTHREADS>>>(n, in_vec1, in_vec2, res_vec);
}

inline void
qn_dev_mulacc_vff_vf(size_t n, const float *in,
		     float scale, float *acc)
{
    cublasSaxpy(n, scale, in, 1, acc, 1);
}


inline void
qn_dev_dsigmoid_vf_vf(size_t n, const float* in_vec,
		      float* out_vec)
{
    int nblocks = (n + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_dsigmoid_vf_vf", "nblocks too large");
    qn_kern_dsigmoid_vf_vf<<<nblocks,NTHREADS>>>(n, in_vec, out_vec);
}

inline void
qn_dev_dtanh_vf_vf(size_t n, const float* in_vec,
		   float* out_vec)
{
    int nblocks = (n + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_dtanh_vf_vf", "nblocks too large");
    qn_kern_dtanh_vf_vf<<<nblocks,NTHREADS>>>(n, in_vec, out_vec);
}

inline void
qn_dev_sigmoid_vf_vf(int n, const float* in_vec,
		     float* out_vec)
{
    int nblocks = (n + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_sigmoid_vf_vf", "nblocks too large");
    qn_kern_sigmoid_vf_vf<<<nblocks,NTHREADS>>>(n, in_vec, out_vec);
}

inline void
qn_dev_tanh_vf_vf(int n, const float* in_vec,
		  float* out_vec)
{
    int nblocks = (n + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_tanh_vf_vf", "nblocks too large");
    qn_kern_tanh_vf_vf<<<nblocks,NTHREADS>>>(n, in_vec, out_vec);
}

inline void qn_dev_softmax_vf_vf(int n, const float* in_vec,
				 float* out_vec)
{
    qn_devnv_softmax_vf_vf<<<1,1>>>(n, in_vec, out_vec);
}

// Do multiple softmaxes at once
inline void qn_dev_multisoftmax_mf_mf(int rows, int cols,
				      const float* in_vecs,
				      float* out_vecs)
{
    int nblocks = (rows + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_multisoftmax_mf_mf", "nblocks too large");
    qn_kern_multisoftmax_mf_mf<<<nblocks, NTHREADS>>>(rows, cols, in_vecs, out_vecs);
}

inline void
qn_dev_copy_vf_mf(size_t mat_height, size_t vec_len,
		  const float* vec, float* mat)
{
    int nblocks = (vec_len + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_dev_copy_vf_mf", "nblocks too large");
    qn_kern_copy_vf_mf<<<nblocks, NTHREADS>>>(mat_height, vec_len, vec, mat);
}

inline void
qn_dev_sumcol_mf_vf(size_t rows, size_t cols,
		    const float* in, float* res)
{
    int nblocks = (cols + NTHREADS-1)/NTHREADS;
    if (nblocks>QN_CUDA_MAXBLOCKS)
	QN_ERROR("qn_sumcol_mf_vf", "nblocks too large");
    qn_kern_sumcol_mf_vf<<<nblocks, NTHREADS>>>(rows, cols, in, res);
}

inline void
qn_dev_mul_mfmf_mf(size_t Sm,size_t Sk,size_t Sn,
		   const float *A,const float *B,float *C)
{
    cublasSgemm('N', 'N',
		Sn, Sm, Sk, 1.0f, B, Sn,  A, Sk, 0.0f, C, Sn);

}

inline void
qn_dev_mulntacc_mfmf_mf(size_t m, size_t k,
			size_t n, const float* A,
			const float* B, float* C)
{
    cublasSgemm('T', 'N',
		n, m, k, 1.0f, (float*)B, k, (float*) A, k, 1.0f, C, n);
}

inline void
qn_dev_multnacc_fmfmf_mf(size_t Sk,size_t Sm,size_t Sn,
			 float scale, const float *A, const float *B,float *C)
{
    cublasSgemm('N', 'T',
	  Sn, Sm, Sk, scale, B, Sn, A, Sm, 1.0f, C, Sn);
}




////////////////////////
inline __device__ float
qn_devin_sigmoid_f_f(float x)
{
    return 1.0f/(1.0f + expf(-x));
}

inline __device__ void
qn_devin_maxmin_vf_ff(size_t n, const float *vec, float *maxp, float *minp)
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


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif // #ifdef QN_CUDA

#endif // #define QN_cuvec_H_INLCUDED

