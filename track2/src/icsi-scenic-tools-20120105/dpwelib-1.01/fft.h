/***********************************************************************\
*	fft.h								*
*   Fast Fourier Transform C library - header file			*
*   Based on RECrandell's						*
*   dpwe 1992apr30	incorporated all billg's modifications		*
*   dpwe 22jan90							*
*   08apr90 With experimental FFT2torl - converse of FFT2real		*
\***********************************************************************/

/*  
    To call an FFT, one must first assign the complex exponential factors:
    A call such as
	e = AssignBasis(NULL, size)
    will set up the my_complex *e to be the array of cos, sin pairs corresponding
    to total number of complex data = size.   This call allocates the
    (cos, sin) array memory for you.  If you already have such memory
    allocated, pass the allocated pointer instead of NULL.
 */

typedef struct {
    float re, im;
    } my_complex;

typedef struct lnode
    {
    struct lnode *next;
    long		 size;
    my_complex	 *table;
    } LNODE;

int PureReal PARG((my_complex *, long));
int IsPowerOfTwo PARG((long));
my_complex *FindTable PARG((long));	/* search our list of existing LUT's */
my_complex *AssignBasis PARG((my_complex *, long));
void reverseDig PARG((my_complex *, long, long));
void FFT2dimensional PARG((my_complex *, long, long, my_complex *));
void FFT2torl PARG((my_complex *, long, long, FLOATARG, my_complex *));
void ConjScale PARG((my_complex *, long, FLOATARG));
void FFT2real PARG((my_complex *, long, long, my_complex *));
void Reals PARG((my_complex *, long, long, int, my_complex *));
void PackedReals PARG((my_complex *, long, long, int, my_complex *));
void FFT2 PARG((my_complex *, long, long, my_complex *));
void FFT2raw PARG((my_complex *, long, long, long, my_complex *));
void FFTarb PARG((my_complex *, my_complex *, long, my_complex *));
void DFT PARG((my_complex *, my_complex *, long, my_complex *));
/* simplified routines per billg */
void fft_packed PARG((float *, long));
void ifft_packed PARG((float *, long));
void fft_real PARG((my_complex *, long));
void ifft_real PARG((my_complex *, long));
void fft PARG((my_complex *, long));
void ifft PARG((my_complex *, long));
void fft_skip PARG((my_complex *, long, long));
void ifft_skip PARG((my_complex *, long, long));
void fft_2d PARG((my_complex *, long, long));
void ifft_2d PARG((my_complex *, long, long));
