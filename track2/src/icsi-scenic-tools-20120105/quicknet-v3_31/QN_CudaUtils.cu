const char* QN_CudaUtils_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_CudaUtils.cu,v 1.4 2011/05/21 00:10:35 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_CudaUtils.h"
#include "QN_fltvec.h"
#include "QN_intvec.h"
#include "QN_cuvec.h"

#include <cuda.h>
#include <cublas.h>


static int QN_cuda_inited = QN_FALSE;

size_t
QN_cuda_maxvec(void)
{
	return ((size_t) QN_CUDA_NTHREADS * (size_t) QN_CUDA_MAXBLOCKS) -1;
}

void
QN_cuda_init(void)
{

    if (QN_cuda_inited == QN_FALSE)
    {
	QN_cuda_inited = QN_TRUE;

	cudaError_t e;
	float* test;
	// A malloc test to trap lack of CUDA resources cleanly
	e = cudaMalloc((void **) &test, 4);
	if (e!=cudaSuccess)
	    QN_ERROR(NULL, cudaGetErrorString(e));
	e = cudaFree(test);
	if (e!=cudaSuccess)
	    QN_ERROR("QN_cuda_init", cudaGetErrorString(e));

	cublasStatus eb;
	eb = cublasInit();
	if (eb!=CUBLAS_STATUS_SUCCESS)
	    QN_ERROR("QN_cuda_init", "failed to initalize CUDA BLAS");

	// Make sure the current device is good enough
	int dev;
	e = cudaGetDevice(&dev);
	if (e!=cudaSuccess)
	    QN_ERROR("QN_cuda_init", "failed to get current device");

	struct cudaDeviceProp props;
	e = cudaGetDeviceProperties(&props, dev);
	if (e!=cudaSuccess)
	    QN_ERROR("QN_cuda_init", "failed to get device properties");
	if (props.major<1)
	    QN_ERROR("QN_cuda_init", "need minimum compute capability 1.x");
	if (props.major==1 && props.minor<3)
	    QN_WARN("QN_cuda_init", "QuickNet not tested on compute capability <1.3");
	if (QN_CUDA_NTHREADS > props.maxThreadsDim[0])
	    QN_ERROR("QN_cuda_init", "need GPU with maxThreadsDim[0]>=%i", QN_CUDA_NTHREADS);
	if (QN_CUDA_MAXBLOCKS > props.maxGridSize[0])
	    QN_ERROR("QN_cuda_init", "need GPU with maxGridSize[0]>=%i", QN_CUDA_MAXBLOCKS);
    }
}

void
QN_cuda_check(void)
{

    if (QN_cuda_inited == QN_TRUE)
    {
	cublasStatus eb;
	eb = cublasGetError();
	if (eb!=CUBLAS_STATUS_SUCCESS)
	    QN_ERROR("QN_cuda_check", "accumulated cublas error detected");
    }
}

void
QN_cuda_shutdown(void)
{

    if (QN_cuda_inited == QN_TRUE)
    {
	cublasStatus eb;
	eb = cublasGetError();
	if (eb!=CUBLAS_STATUS_SUCCESS)
	    QN_ERROR("QN_cuda_shutdown", "accumulated cublas error detected");
	eb = cublasShutdown();
	if (eb!=CUBLAS_STATUS_SUCCESS)
	    QN_ERROR("QN_cuda_shutdown", "failed to shutdown CUDA BLAS");
	QN_cuda_inited = QN_FALSE;
    }
}

static const char*
cuda_modet_to_txt(int mode)
{
    const char* res;

    switch(mode)
    {
    case cudaComputeModeDefault:
	res = "DEFAULT";
	break;
    case cudaComputeModeExclusive:
	res = "EXCLUSIVE";
	break;
    case cudaComputeModeProhibited:
	res = "PROHIBITED";
	break;
    default:
	res = "WEIRD";
	break;
    }
    return res;
}


const char*
QN_cuda_version(void)
{
    cudaError_t e;
    static char cuda_ver_info[1024];
    char* ptr = cuda_ver_info;
    int devcount = 0;

    e = cudaGetDeviceCount(&devcount);
    if (e!=cudaSuccess)
    {
	ptr += sprintf(ptr, "CUDA error: %s.\n", cudaGetErrorString(e));
    }

    if (devcount!=0)
    {
	int runver = 0;
	int drvver = 0;
	e = cudaDriverGetVersion(&drvver);
	if (e!=cudaSuccess)
	    QN_ERROR("QN_cuda_version", "failed to get CUDA driver version");
	e = cudaRuntimeGetVersion(&runver);
	if (e!=cudaSuccess)
	    QN_ERROR("QN_cuda_version", "failed to get CUDA runtime version");
	ptr += sprintf(ptr, "CUDA runtime version: %i\nCUDA driver version: %i.\n", runver, drvver);
    }
    ptr += sprintf(ptr, "CUDA available device count: %i.", devcount);

    int i;
    struct cudaDeviceProp props;

    for (i=0; i<devcount; i++)
    {
	e = cudaGetDeviceProperties(&props, i);
	if (e!=cudaSuccess)
	    QN_ERROR("QN_cuda_version", "failed to get device properties");
	ptr += sprintf(ptr,"\nCUDA device %i: %s %iMB @ %.2f GHz [%s].", i,
		       props.name,props.totalGlobalMem/(1024*1024),
		       ((double) props.clockRate)/1e6,
		       cuda_modet_to_txt(props.computeMode));
			
    }

    return cuda_ver_info;
}

//
int
QN_cuda_current_device(void)
{
    int dev;
    cudaError_t e;

    e = cudaGetDevice(&dev);
    if (e!=cudaSuccess)
	QN_ERROR("QN_cuda_current_device", "failed to get current device");
    return dev;
}

const char*
QN_cublas_error_string(int e)
{
    const char* s;

    switch(e)
    {
    case CUBLAS_STATUS_SUCCESS:
	s = "cublas ";
	break;
    case CUBLAS_STATUS_NOT_INITIALIZED:
	s = "cublas not initialized";
	break;
    case CUBLAS_STATUS_ALLOC_FAILED:
	s = "cublas alloc failed";
	break;
    case CUBLAS_STATUS_INVALID_VALUE:
	s = "cublas invalid value";
	break;
    case CUBLAS_STATUS_ARCH_MISMATCH:
	s = "cublas arch mismatch";
	break;
    case CUBLAS_STATUS_MAPPING_ERROR:
	s = "cublas mapping error";
	break;
    case CUBLAS_STATUS_EXECUTION_FAILED:
	s = "cublas execution failed";
	break;
    case CUBLAS_STATUS_INTERNAL_ERROR:
	s = "cublas internal error";
	break;
    default:
	s = "cublas unknown error";
	break;
    }

    return s;
}

