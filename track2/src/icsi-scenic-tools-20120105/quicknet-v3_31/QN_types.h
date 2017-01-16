// $Header: /u/drspeech/repos/quicknet2/QN_types.h,v 1.34 2010/10/29 02:20:38 davidj Exp $

#ifndef QN_types_h_INCLUDED
#define QN_types_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#ifdef QN_HAVE_FCNTL_H
#include <fcntl.h>
#endif

typedef signed char QNInt8;
typedef unsigned char QNUInt8;
typedef short QNInt16;
typedef unsigned short QNUInt16;
typedef int QNInt32;
typedef unsigned int QNUInt32;
typedef int QNBool;

enum 
{
    QN_FALSE = 0,
    QN_TRUE = 1
};

#define QN_INT8_MIN (-0x80)
#define QN_INT8_MAX (0x7f)
#define QN_UINT8_MAX (0xff)
#define QN_INT16_MIN (-0x8000)
#define QN_INT16_MAX (0x7fff)
#define QN_UINT16_MAX (0xffff)
#define QN_INT32_MIN (-0x7fffffff-1)
#define QN_INT32_MAX (0x7fffffff)

#define QN_E          2.7182818284590452354
#define QN_LOG2E      1.4426950408889634074
#define QN_LOG10E     0.43429448190325182765
#define QN_LN2        0.69314718055994530942
#define QN_LN10       2.30258509299404568402
#define QN_PI         3.14159265358979323846
#define QN_PI_2       1.57079632679489661923
#define QN_PI_4       0.78539816339744830962
#define QN_1_PI       0.31830988618379067154
#define QN_2_PI       0.63661977236758134308
#define QN_2_SQRTPI   1.12837916709551257390
#define QN_SQRT2      1.41421356237309504880
#define QN_SQRT1_2    0.70710678118654752440

// Use this to indicate in bad error info when returning a size_t

enum
{
#if (QN_SIZEOF_SIZE_T==4)
    QN_SIZET_BAD = 0xffffffffu, // FIXME - this should be configured.
    QN_SIZET_MAX = 0xfffffffeu,	// The maximum size of anything - infinity!
#elif (QN_SIZEOF_SIZE_T==8)
    QN_SIZET_BAD = 0xffffffffffffffffu, // FIXME - this should be configured.
    QN_SIZET_MAX = 0xfffffffffffffffeu,	// The maximum size of anything -
					// infinity! 
#else
#error sizeof(size_t) not 4 or 8
#endif
    QN_ALL = QN_SIZET_BAD	// If you want as many as possible of somat.
};

// The type used to uniquely identify a segment (typically, one sentence = one
// segment) within QuickNet.  Note that this does not necessarily correspond
// to the segment/sentence number in any specific file.

typedef long QN_SegID;
enum {
    QN_SEGID_UNKNOWN = 0,	// Returned when we do not know the segid.
    QN_SEGID_BAD = -1		// Used for passing back exceptions.
}; 

#define QN_UINT32_MAX 0xffffffffu;

enum QN_LayerSelector
{
    QN_LAYER1 = 0,
    QN_LAYER2 = 1,
    QN_LAYER3 = 2,
    QN_LAYER4 = 3,
    QN_LAYER5 = 4,
    QN_MLP2_INPUT = QN_LAYER1,
    QN_MLP2_OUTPUT = QN_LAYER2,
    QN_MLP3_INPUT = QN_LAYER1,
    QN_MLP3_HIDDEN = QN_LAYER2,
    QN_MLP3_OUTPUT = QN_LAYER3,
    QN_MLP4_INPUT = QN_LAYER1,
    QN_MLP4_HIDDEN1 = QN_LAYER2,
    QN_MLP4_HIDDEN2 = QN_LAYER3,
    QN_MLP4_OUTPUT = QN_LAYER4,
    QN_MLP5_INPUT = QN_LAYER1,
    QN_MLP5_HIDDEN1 = QN_LAYER2,
    QN_MLP5_HIDDEN2 = QN_LAYER3,
    QN_MLP5_HIDDEN3 = QN_LAYER4,
    QN_MLP5_OUTPUT = QN_LAYER5,
    QN_LAYER_UNKNOWN
};

// The maximum number of layers
enum
{
    QN_MLP_MIN_LAYERS = 2,
    QN_MLP_MAX_LAYERS = 5
};

// An indicator for different weight sections
enum QN_SectionSelector
{
    QN_LAYER12_WEIGHTS = 0,
    QN_LAYER2_BIAS,
    QN_LAYER23_WEIGHTS,
    QN_LAYER3_BIAS,
    QN_LAYER34_WEIGHTS,
    QN_LAYER4_BIAS,
    QN_LAYER45_WEIGHTS,
    QN_LAYER5_BIAS,
    QN_MLP2_INPUT2OUTPUT = QN_LAYER12_WEIGHTS,
    QN_MLP2_OUTPUTBIAS = QN_LAYER2_BIAS,
    QN_MLP3_INPUT2HIDDEN = QN_LAYER12_WEIGHTS,
    QN_MLP3_HIDDENBIAS = QN_LAYER2_BIAS,
    QN_MLP3_HIDDEN2OUTPUT = QN_LAYER23_WEIGHTS,
    QN_MLP3_OUTPUTBIAS = QN_LAYER3_BIAS,
    QN_MLP4_INPUTHIDDEN1 = QN_LAYER12_WEIGHTS,
    QN_MLP4_HIDDEN1BIAS = QN_LAYER2_BIAS,
    QN_MLP4_HIDDEN1HIDDEN2 = QN_LAYER23_WEIGHTS,
    QN_MLP4_HIDDEN2BIAS = QN_LAYER3_BIAS,
    QN_MLP4_HIDDEN2OUTPUT = QN_LAYER34_WEIGHTS,
    QN_MLP4_OUTPUTBIAS = QN_LAYER4_BIAS,
    QN_MLP5_INPUTHIDDEN1 = QN_LAYER12_WEIGHTS,
    QN_MLP5_HIDDEN1BIAS = QN_LAYER2_BIAS,
    QN_MLP5_HIDDEN1HIDDEN2 = QN_LAYER23_WEIGHTS,
    QN_MLP5_HIDDEN2BIAS = QN_LAYER3_BIAS,
    QN_MLP5_HIDDEN2HIDDEN3 = QN_LAYER34_WEIGHTS,
    QN_MLP5_HIDDEN3BIAS = QN_LAYER4_BIAS,
    QN_MLP5_HIDDEN3OUTPUT = QN_LAYER45_WEIGHTS,
    QN_MLP5_OUTPUTBIAS = QN_LAYER5_BIAS,
    QN_WEIGHTS_UNKNOWN,
    QN_WEIGHTS_NONE = QN_WEIGHTS_UNKNOWN
};

// SectionSelectors used to be called WeightSelectors
typedef QN_SectionSelector QN_WeightSelector;

enum QN_LayerType {
    QN_LAYER_LINEAR,
    QN_LAYER_SIGMOID,
    QN_LAYER_SIGMOID_XENTROPY,
    QN_LAYER_SOFTMAX,
    QN_LAYER_TANH
};


// The type of output layer for an MLP
enum QN_OutputLayerType {
    QN_OUTPUT_LINEAR = QN_LAYER_LINEAR,
    QN_OUTPUT_SIGMOID = QN_LAYER_SIGMOID,
    QN_OUTPUT_SIGMOID_XENTROPY = QN_LAYER_SIGMOID_XENTROPY,
    QN_OUTPUT_SOFTMAX = QN_LAYER_SOFTMAX,
    QN_OUTPUT_TANH = QN_LAYER_TANH
};

// The type of files used for feature/label/activation input/output

enum QN_StreamType {
    QN_STREAM_UNKNOWN,		// Not yet known
    QN_STREAM_PFILE,		// A PFile
    QN_STREAM_BERPFTR,		// An online equivalent of a PFile used in BERP
    QN_STREAM_RAPACT_ASCII,	// Rap-stle output activitions - ASCII
    QN_STREAM_RAPACT_HEX,	// Rap-stle output activitions - hex
    QN_STREAM_RAPACT_BIN,	// Rap-stle output activitions - binary
    QN_STREAM_LNA8,		// Cambridge 8-bit output activations
    QN_STREAM_LNA16,		// Cambridge 16-bit output activations
    QN_STREAM_PREFILE,		// Cambridge feature file format
    QN_STREAM_ONLFTR,		// Online feature file
    QN_STREAM_ASCII		// ASCII stream
};

// The types of files used for storing weights.

enum QN_WeightFileType {
    QN_WEIGHTFILE_UNKNOWN,	// Unknown weight file format.
    QN_WEIGHTFILE_RAP3,		// Traditional RAP-style 3 layer weight files.
    QN_WEIGHTFILE_MATLAB	// Matlab file.
};


// An indicator for the order of weight matrices of files
enum QN_WeightMaj
{
    QN_INPUTMAJOR,
    QN_OUTPUTMAJOR
};

// Modes for files
enum QN_FileMode
{
    QN_READ = O_RDONLY,
    QN_WRITE = O_RDWR
};

// The different types of arithmetic engines as bitmap masks
enum 
{
    QN_MATH_NV = 0,		// Naive - simple C routines
    QN_MATH_PP = 1,		// "portable performance" - cleverly
				// written C routines
    QN_MATH_BL = 2,		// Blas routines
    QN_MATH_FE = 4		// Fast exponent routines
};

// General error codes

enum
{
    QN_OK = 0,
    QN_BAD = -1,
    QN_EOF = -1
};

#ifdef QN_WORDS_BIGENDIAN
enum { QN_BIGENDIAN = 1, QN_LITTLEENDIAN=0 };
#else
enum { QN_BIGENDIAN = 0, QN_LITTLEENDIAN=1 };
#endif

extern const char* QN_progname;
extern FILE* QN_logfile;

#endif /* #ifndef QN_types_h_INCLUDED */
