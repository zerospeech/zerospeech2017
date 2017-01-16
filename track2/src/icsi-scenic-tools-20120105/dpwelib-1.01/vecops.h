/*******************************************************************\
*   vecops.h							    *
*   nuseful vectorised operations				    *
*   dpwe 08sep91 after db.dsp.c 30sep90				    *
\*******************************************************************/

#ifndef _VECOPS_H_
#define _VECOPS_H_

#include "dpwelib.h"	/* for PARG() definition at least */

typedef struct {
    float mag, pha;
} cpxpolar;

typedef struct {
    float re, im;
} cpxrect;

#define VFLOAT	float
    /* type of all the vectors - could be float or double */

void VCopyBak PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VCopyFwd PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VCopy PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VZero PARG((VFLOAT *dst, int dinc, long pts));
long VCopyZero PARG((float *src, long sinc, long slen, 
	       			 float *dst, long dinc, long dlen));
void VRvrs PARG((float *buf, long skip, long len));
void VVAdd PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VVSub PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VSAdd PARG((VFLOAT *dst, int dinc, FLOATARG val, long pts));
void VSVAdd PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, FLOATARG val, 
		  long pts));
void VVMul PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VSMul PARG((VFLOAT *dst, int dinc, FLOATARG val, long pts));
void VLog  PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, 
		 FLOATARG ref, long pts));
void VExp  PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, 
		 FLOATARG scale, long pts));
void VSq   PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void VSqrt PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
void Vlin2DB PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, 
		 FLOATARG ref, long pts));
void VDB2lin PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, 
		 FLOATARG scale, long pts));
void VFODiff PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
    /* first order difference along vector - last pt of o/p is set to zero */
void VMinMax PARG((VFLOAT *src, int sinc, long pts, 
		   double *pmin, double *pmax));
double VMax  PARG((VFLOAT *src, int sinc, long pts));
double VMag  PARG((VFLOAT *src, int sinc, long pts));
double VSum  PARG((VFLOAT *src, int sinc, long pts));
double VAvg  PARG((VFLOAT *src, int sinc, long pts));
double VSumSq PARG((VFLOAT *src, int sinc, long pts));
double VAvSq PARG((VFLOAT *src, int sinc, long pts));
double VVMulSum PARG((VFLOAT *src1, int sinc1, VFLOAT *src2, int sinc2, long pts));

void HalfRotate PARG((VFLOAT *buf, long len));
    /* rotate to remove linear phase */
void PadInMiddle PARG((VFLOAT *buf, long origLen, long newLen));
    /* insert zeros */

void VSFloor PARG((VFLOAT *dst, int dinc, FLOATARG val, long pts));
    /* ensure no value below <val> */
void VVFloor PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
    /* no point in <dst> is smaller than corresponding pt in <src> */
void VSCeil  PARG((VFLOAT *dst, int dinc, FLOATARG val, long pts));
    /* ensure no value above <val> */
void VVCeil PARG((VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts));
    /* no point in <dst> is larger than corresponding pt in <src> */

void VSetHanning PARG((VFLOAT *data, int dinc, double scale, long len));
    /* fill <data> with a <len> point hanning window scaled to have 
       a maximum value of <scale>.  The window will always have a 
       single maximum value; if <len> is even, only the first point 
       will be zero; if <len> is odd, the first and last points will 
       be zero */

/* polar complex vectors */
void CVCopy PARG((cpxpolar *src, int sinc, cpxpolar *dst, int dinc, long pts));
void CVVAdd PARG((cpxpolar *src, int sinc, cpxpolar *dst, int dinc, long pts));
void CVVSub PARG((cpxpolar *src, int sinc, cpxpolar *dst, int dinc, long pts));
void CVSAdd PARG((cpxpolar *dst, int dinc, cpxpolar *val, long pts));
void mskPhs PARG((double *p));
void CVVMul PARG((cpxpolar *src, int sinc, cpxpolar *dst, int dinc, long pts));
void CVSMul PARG((cpxpolar *dst, int dinc, cpxpolar *val, long pts));

/* rectangular complex vectors */
void RCVVMul PARG((cpxrect *src, int sinc, cpxrect *dst, int dinc, long pts));
void RCV2CV PARG((cpxrect *src, int sinc, cpxpolar *dst, int dinc, long pts));
void RCVmag PARG((cpxrect *src, int sinc, cpxpolar *dst, int dinc, long pts));
	/* Like RCV2CV but don't calculate phase, just set to zero */
void CV2RCV PARG((cpxpolar *src, int sinc, cpxrect *dst, int dinc, long pts));

#endif /* !_VECOPS_H_ */
