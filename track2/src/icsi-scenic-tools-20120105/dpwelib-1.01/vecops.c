/*******************************************************************\
*   vecops.c							    *
*   nuseful vectorised operations				    *
*   dpwe 08sep91 after db.dsp.c 30sep90				    *
\*******************************************************************/

#include <dpwelib.h>
#include <vecops.h>
#include <math.h>
#include <assert.h>

void VCopyBak(src, sinc, dst, dinc, pts)
    VFLOAT	*src;
    int 	sinc;
    VFLOAT	*dst;
    int 	dinc;
    long	pts;
{
    while(pts--)  {
	*dst = *src;
	dst += dinc; src += sinc;
    }
}

void VCopyFwd(src, sinc, dst, dinc, pts)
    VFLOAT	*src;
    int 	sinc;
    VFLOAT	*dst;
    int 	dinc;
    long	pts;
{	/* copy forward: start at end */
	dst += pts * dinc;
	src += pts * sinc;
    while(pts--)  {
	dst -= dinc; src -= sinc;
	*dst = *src;
    }
}

void VCopy(src, sinc, dst, dinc, pts)
    VFLOAT	*src;
    int 	sinc;
    VFLOAT	*dst;
    int 	dinc;
    long	pts;
{	/* choose direction (semi)intelligently (can't solve sinc != dinc) */
	if( (dst > src) && (dst - src < pts * sinc))    /* dst within src */
		VCopyFwd(src, sinc, dst, dinc, pts);
	else
		VCopyBak(src, sinc, dst, dinc, pts);
}

void VZero(dst, dinc, pts)
    VFLOAT	*dst;
    int 	dinc;
    long	pts;
{
    while(pts--)  {
	*dst = 0;
	dst += dinc;
    }
}

long VCopyZero(float *src, long sinc, long slen, 
	       float *dst, long dinc, long dlen)
{	/* Copy src into dst upto dlen.  If slen is not long enough, 
	   zero the rest */
    long  len = MIN(slen, dlen);

    VCopy(src, sinc, dst, dinc, len);
    if(len < dlen)
    	VZero(dst + dinc*len, dinc, dlen - len);
    return len;		/* return how much taken from src */
}

void VRvrs(float *buf, long skip, long len)
{   /* reverse order in <buf>.	If <len> is odd, middle member not touched */
    long  i = len/2;
    float *l = buf;
    float *u = buf+skip*(len-1);
    double f;

    while(i--)	{
		f = *u;
		*u = *l;
		*l = f;
		u -= skip;
		l += skip;
    }
}

void VVAdd(src, sinc, dst, dinc, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    long pts;
    {
    while(pts--)
	{
	*dst += *src;
	dst += dinc; src += sinc;
	}
    }

void VVSub(src, sinc, dst, dinc, len)	/* *dst -= *src */
    VFLOAT	*src;
    int 	sinc;
    VFLOAT	*dst;
    int 	dinc;
    long	len;
    {
    while(len--)
	{
	*dst -= *src;
	src += sinc; 	dst += dinc;
	}
    }

void VSAdd(dst, dinc, val, pts)
    VFLOAT *dst;
    int    dinc;
    FLOATARG val;
    long pts;
    {
    while(pts--)
	{
	*dst += val;
	dst += dinc;
	}
    }

void VSVAdd(src, sinc, dst, dinc, val, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    FLOATARG val;
    long pts;
    {	/* add contents of src SCALED BY val to dst */
    while(pts--)
	{
	*dst += val * *src;
	dst += dinc; src += sinc;
	}
    }

void VVMul(src, sinc, dst, dinc, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    long pts;
    {
    while(pts--)
	{
	*dst *= *src;
	dst += dinc; src += sinc;
	}
    }

void VSMul(dst, dinc, val, pts)
    VFLOAT *dst;
    int    dinc;
    FLOATARG val;
    long pts;
    {
    while(pts--)
	{
	*dst *= val;
	dst += dinc;
	}
    }

void VLog(src, sinc, dst, dinc, ref, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    FLOATARG ref;
    long pts;
    {
    double logoneonref = log(1.0/ref);

    while(pts--)
	{
	*dst = log(*src) + logoneonref;
	dst += dinc; src += sinc;
	}
    }

void VExp(src, sinc, dst, dinc, scale, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    FLOATARG scale;
    long pts;
{
    double scaleoffs = log(scale);

    while(pts--)  {
	*dst = exp(scaleoffs + *src);
	dst += dinc; src += sinc;
    }
}

void VSq(src, sinc, dst, dinc, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    long pts;
{
    double d;
    while(pts--)  {
	d = *src;
	*dst = d * d;
	dst += dinc; src += sinc;
    }
}

void VSqrt(src, sinc, dst, dinc, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    long pts;
{
    double d;
    while(pts--)  {
	d = *src;
	*dst = sqrt(d);
	dst += dinc; src += sinc;
    }
}

void Vlin2DB(src, sinc, dst, dinc, ref, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    FLOATARG ref;
    long pts;
    {
    double logoneonref = log(1.0/ref);
    double dbofln = 8.68588963806504;  /* 20/ln(10) */

    while(pts--)
	{
	*dst = dbofln * (log(*src) + logoneonref);
	dst += dinc; src += sinc;
	}
    }

void VDB2lin(src, sinc, dst, dinc, scale, pts)
    VFLOAT *src;
    int    sinc;
    VFLOAT *dst;
    int    dinc;
    FLOATARG scale;
    long pts;
{
    double scaleoffs = log(scale);
    double lnofdb = 1.0/8.68588963806504; /* ln(10)/20 */

    while(pts--)  {
	*dst = exp(scaleoffs + lnofdb * *src);
	dst += dinc; src += sinc;
    }
}

void VFODiff(src, sinc, dst, dinc, pts)
    VFLOAT *src;
    int sinc;
    VFLOAT *dst;
    int dinc;
    long pts;
    {	/* first order difference along vector */
    /* last point of output is set to zero */
    if(pts > 0)
	while(--pts)
	    {
	    *dst = *(src+sinc) - *src;
	    src += sinc;
	    dst += dinc;
	    }
    *dst = 0;
    }

double VMax(src, sinc, pts)
    VFLOAT *src;
    int    sinc;
    long   pts;
    {
    double f, max = *src;
    
    while(pts--)
	{
	if( (f = *src) > max)
	    max = f;
	src += sinc;
	}
    return max;
    }

double VMag(src, sinc, pts)
    VFLOAT *src;
    int    sinc;
    long   pts;
    {	/* sqrt(SUM(ai*ai)) */
    double f, sum = 0.0;
    
    while(pts--)
	{
	f = *src;
	sum += f * f;
	src += sinc;
	}
    return sqrt(sum);
    }


double VSum(src, sinc, pts)
    VFLOAT *src;
    int    sinc;
    long   pts;
    { /* return the sum of data[0] + data[step] + .. + data[step*(len-1)] */
    double f, sum = 0.0;
    
    while(pts--)
	{
	sum += *src;    
	src += sinc;
	}
    return sum;
    }

double VAvg(src, sinc, pts)
    VFLOAT *src;
    int    sinc;
    long   pts;
    {
    double a = 0.0;
    long len = pts;

    assert(len != 0);
    while(pts--)
	{
	a += *src;
	src += sinc;
	}
    return a / (double)len;
    }

double VAvSq(src, sinc, pts)
    VFLOAT *src;
    int    sinc;
    long   pts;
    {
    double a = 0.0, b;
    long len = pts;
    
    while(pts--)
	{
        b = *src;
	a += b * b;
	src += sinc;
	}
    return a / (double)len;
    }

double VSumSq(src, sinc, pts)
    VFLOAT *src;
    int    sinc;
    long   pts;
    {
    double a = 0.0, b;
    long len = pts;
    
    while(pts--)
	{
        b = *src;
	a += b * b;
	src += sinc;
	}
    return a;
    }

double VVMulSum(VFLOAT *src1, int sinc1, VFLOAT *src2, int sinc2, long pts)
{   /* return the inner product of two vectors (1998nov19) */
    double s = 0;

    while(pts--) {
	s += *src1 * *src2;
	src1 += sinc1;
	src2 += sinc2;
    }
    return s;
}

void VMinMax(src, sinc, pts, pmin, pmax)
    VFLOAT *src;
    int    sinc;
    long   pts;
    double *pmin;
    double *pmax;	/* returns here */
    {
    double f, max = *src, min = *src;
    
    while(pts--)
	{
	if( (f = *src) > max)
	    max = f;
	if( f  < min)
	    min = f;
	src += sinc;
	}
    *pmax = max;	*pmin = min;
    }

void HalfRotate(buf, len)
    VFLOAT *buf;
    long len;  /* rotate to remove linear phase */
    {
    long i,j, hlen;
    double	t;
    double	last;

    if(len&1)	last = buf[len-1];	/* odd length is slightly diff */
    hlen = len /2L;			/* same result for len=2n or 2n+1 */
    j = len - 1;			/* j = i + hlen + (len&1)?1:0; */
    for(i = hlen-1; i >= 0; --i)
	{
	t = buf[hlen+i]; buf[j--] = buf[i]; buf[i] = t;
	}
    if(len&1)	buf[hlen] = last;
    }

void PadInMiddle(buf, origLen, newLen)
    VFLOAT *buf;
    long origLen;
    long newLen;	/* insert zeros */
    {
    long 	i, hlen = (origLen+1)/2;	/* for odd len, move (n-1)/2 */
    long 	diff = newLen - origLen;

    for(i = origLen - 1; i >= hlen; --i)
	buf[i+diff] = buf[i];
    for(i = hlen; i<hlen+diff; ++i)
	buf[i] = 0;
    }

void VSFloor(VFLOAT *dst, int dinc, FLOATARG val, long pts)
{   /* ensure no value below <val> */
    while(pts--)  {
	if(*dst < val)	*dst = val;
	dst += dinc;
    }
}

void VVFloor(VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts)
{   /* no point in <dst> is smaller than corresponding pt in <src> */
    while(pts--)  {
	if(*dst < *src)	*dst = *src;
	dst += dinc;
	src += sinc;
    }
}

void VSCeil(VFLOAT *dst, int dinc, FLOATARG val, long pts)
{   /* ensure no value above <val> */
    while(pts--)  {
	if(*dst > val)	*dst = val;
	dst += dinc;
    }
}

void VVCeil(VFLOAT *src, int sinc, VFLOAT *dst, int dinc, long pts)
{   /* no point in <dst> is larger than corresponding pt in <src> */
    while(pts--)  {
	if(*dst > *src)	*dst = *src;
	dst += dinc;
	src += sinc;
    }
}

void VSetHanning(VFLOAT *data, int dinc, double scale, long len)
{   /* fill <data> with a <len> point hanning window scaled to have 
       a maximum value of <scale>.  The window will always have a 
       single maximum value; if <len> is even, only the first point 
       will be zero; if <len> is odd, the first and last points will 
       be zero */
    int i;
    double hannLen = (double)(len + (((len%2)==0)?1:0));
    double hannFact = 2.0*PI/hannLen;
    double a = scale * 0.5; /* .54 for hamm */
    double b = scale - a; 
    VFLOAT *d = data;

    for(i = 0; i < len; ++i)  {
	*d = a - b * cos(hannFact*(double)i);
	d += dinc;
    }
}

/***************** complex vector fns *****************/

void CVCopy(src, sinc, dst, dinc, pts)
    cpxpolar	*src;
    int 	sinc;
    cpxpolar	*dst;
    int 	dinc;
    long	pts;
{
    while(pts--)  {
	dst->mag = src->mag;
	dst->pha = src->pha;
	dst += dinc; src += sinc;
    }
}

void CVVAdd(src, sinc, dst, dinc, pts)
    cpxpolar	*src;
    int 	sinc;
    cpxpolar	*dst;
    int 	dinc;
    long	pts;
{
    double	re, im;

    while(pts--)  {
	re = src->mag*cos(src->pha);
	im = src->mag*sin(src->pha);
	re += dst->mag*cos(dst->pha);
	im += dst->mag*sin(dst->pha);
	dst->mag = hypot(im, re);
	if(dst->mag != 0.0)
	    dst->pha = atan2(im, re);
	else
	    dst->pha = 0.0;
	dst += dinc; src += sinc;
    }
}

void CVVSub(src, sinc, dst, dinc, pts)
    cpxpolar	*src;
    int 	sinc;
    cpxpolar	*dst;
    int 	dinc;
    long	pts;
{
    double	re, im;

    while(pts--)  {
	re = dst->mag*cos(dst->pha);
	im = dst->mag*sin(dst->pha);
	re -= src->mag*cos(src->pha);
	im -= src->mag*sin(src->pha);
	dst->mag = hypot(im, re);
	if(dst->mag != 0.0)
	    dst->pha = atan2(im, re);
	else
	    dst->pha = 0.0;
	dst += dinc; src += sinc;
    }
}

void CVSAdd(dst, dinc, val, pts)
    cpxpolar	*dst;
    int 	dinc;
    cpxpolar	*val;	/* single val, passed by ref coz' struct */
    long	pts;
{
    double	re, im;
    double	vre, vim;

    vre = val->mag*cos(val->pha);
    vim = val->mag*sin(val->pha);
    while(pts--)  {
	re = vre + dst->mag*cos(dst->pha);
	im = vim + dst->mag*sin(dst->pha);
	dst->mag = hypot(im, re);
	if(dst->mag != 0.0)
	    dst->pha = atan2(im, re);
	else
	    dst->pha = 0.0;
	dst += dinc;
    }
}

#define MMmaskPhs(p,q,r,s) /* p is pha, q is as int, r is PI, s is 1/PI */ \
    q = (int)(s*p);	\
    p -= r*(double)((int)((q+((q>=0)?(q&1):-(q&1) )))); \

void mskPhs(double *p)
{
    static int q;
    static double r = PI;
    static double s = 1.0/PI;

    MMmaskPhs((*p),q,r,s);
}

void CVVMul(src, sinc, dst, dinc, pts)
    cpxpolar	*src;
    int 	sinc;
    cpxpolar	*dst;
    int 	dinc;
    long	pts;
{
    int 	q;
    double 	r = PI;
    double 	s = 1.0/PI;

    while(pts--)  {
	dst->mag = dst->mag*src->mag;
	dst->pha = dst->pha+src->pha;
	MMmaskPhs(dst->pha, q, r, s);
	dst += dinc; src += sinc;
    }
}

void CVSMul(dst, dinc, val, pts)
    cpxpolar	*dst;
    int 	dinc;
    cpxpolar	*val;	/* single val, passed by ref coz' struct */
    long	pts;
{
    int 	q;
    double 	r = PI;
    double 	s = 1.0/PI;
    double	mag, pha;

    mag = val->mag;	pha = val->pha;
    while(pts--)  {
	dst->mag = dst->mag*mag;
	dst->pha = dst->pha+pha;
	MMmaskPhs(dst->pha, q, r, s);
	dst += dinc;
    }
}

/***** rectangular complex vectors ******/

void RCVVMul(src, sinc, dst, dinc, pts)
    cpxrect	*src;
    int 	sinc;
    cpxrect	*dst;
    int 	dinc;
    long	pts;
{	/* tricky process of multiplying rectangular complex vectors */
    double	sre, sim, dre, dim;

    while(pts--)  {
    	sre = src->re;	dre = dst->re;	/* save both in case src == dst */
    	sim = src->im;	dim = dst->im;
    	dst->re = sre*dre - sim*dim;
    	dst->im = sre*dim + dre*sim;
    	src += sinc;	dst += dinc;
    }
}

void RCV2CV(src, sinc, dst, dinc, pts)
    cpxrect	*src;
    int 	sinc;
    cpxpolar *dst;
    int 	dinc;
    long	pts;
{	/* yes, Rect2Polar has a new name.. */
    double	re, im;

    while(pts--)  {
		re = src->re;	im = src->im;
		dst->mag = hypot(re, im);
		dst->pha = atan2(im, re);
    	src += sinc;	dst += dinc;
    }
}

void RCVmag(src, sinc, dst, dinc, pts)
    cpxrect	*src;
    int 	sinc;
    cpxpolar *dst;
    int 	dinc;
    long	pts;
{	/* Like RCV2CV but don't calculate phase, just set to zero 
	   (for speed) */
    double	re, im;

    while(pts--)  {
		re = src->re;	im = src->im;
		dst->mag = hypot(re, im);
		dst->pha = 0;
    	src += sinc;	dst += dinc;
    }
}

void CV2RCV(src, sinc, dst, dinc, pts)
    cpxpolar *src;
    int 	sinc;
    cpxrect	*dst;
    int 	dinc;
    long	pts;
{	/* same treatment for Polar2Rect */
    double	mag, pha;

    while(pts--)  {
		mag = src->mag;	pha = src->pha;
		dst->re = mag*cos(pha);
		dst->im = mag*sin(pha);
    	src += sinc;	dst += dinc;
    }
}

