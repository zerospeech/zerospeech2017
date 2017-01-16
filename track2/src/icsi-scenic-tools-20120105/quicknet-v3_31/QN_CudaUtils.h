// $Header: /u/drspeech/repos/quicknet2/QN_CudaUtils.h,v 1.4 2011/05/21 00:10:35 davidj Exp $

#ifndef QN_CudaUtils_H_INCLUDED
#define QN_CudaUtils_H_INCLUDED

#ifdef QN_CUDA

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include "QN_MLP.h"
#include "QN_MLP_BaseFl.h"
#include "QN_Logger.h"

// Various CUDA utility routines

// **** IMPORTANT - no CUDA stuff in this header ****

// Initialize the CUDA set for QuickNet
// Can be called multiple times
void QN_cuda_init(void);

// The maximum vector that the QN_cuvec library can handle
size_t QN_cuda_maxvec(void);

// Check for any CUDA errors
void QN_cuda_check(void);

// Wind down CUDA usage
void QN_cuda_shutdown(void);

// Return a string holding information about the available CUDA hardware
const char* QN_cuda_version(void);

// Return the current CUDA device - only works after a context has been
// created by e.g. by using cudaMalloc
int QN_cuda_current_device(void);

// Returen the error string for a cublas status value
const char* QN_cublas_error_string(int e);

#endif // #ifdef QN_CUDA

#endif // #define QN_CudaUtils_H_INLCUDED

