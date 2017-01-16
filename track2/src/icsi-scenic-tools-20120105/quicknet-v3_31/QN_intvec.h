// $Header: /u/drspeech/repos/quicknet2/QN_intvec.h,v 1.6 2006/03/09 02:06:09 davidj Exp $

// Vector utility routines for QuickNet

#ifndef QN_intvec_h_INCLUDED
#define QN_intvec_h_INCLUDED

#include "QN_types.h"
#include <stddef.h>

static inline size_t
qn_min_zz_z(size_t a, size_t b)
{
    if (a<b)
	return a;
    else
	return b;
}

static inline size_t
qn_max_zz_z(size_t a, size_t b)
{
    if (a>b)
	return a;
    else
	return b;
}


inline void
qn_copy_vi8_vi8(size_t len, const QNInt8* from, QNInt8* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = *from++;
}

inline void
qn_copy_vi16_vi16(size_t len, const QNInt16* from, QNInt16* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = *from++;
}

inline void
qn_copy_vi32_vi32(size_t len, const QNInt32* from, QNInt32* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = *from++;
}

inline void
qn_copy_i8_vi8(size_t len, QNInt8 from, QNInt8* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = from;
}

inline void
qn_copy_i16_vi16(size_t len, QNInt16 from, QNInt16* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = from;
}


inline void
qn_copy_i32_vi32(size_t len, QNInt32 from, QNInt32* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = from;
}

// Copying size_t types.

inline void
qn_copy_vz_vz(size_t len, const size_t* from, size_t* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = *from++;
}

inline void
qn_copy_z_vz(size_t len, size_t from, size_t* to)
{
    size_t i;

    for (i=len; i!=0; i--)
	*to++ = from;
}



inline QNInt16
qn_swapb_i16_i16(QNInt16 val)
{
    QNUInt16 uval;
    QNUInt16 res;
    QNInt16 b0, b1;

    uval = (QNUInt16) val;
    b0 = uval >> 8;
    b1 = uval << 8;

    res = b0 | b1;

    return (QNInt16) res;
}

inline QNInt32
qn_swapb_i32_i32(QNInt32 val)
{
    QNUInt32 uval;
    QNUInt32 res;
    QNInt32 b0, b1, b2, b3;

    uval = (QNUInt32) val;
    b0 = uval >> 24;
    b1 = (uval >> 8) & 0x0000ff00;
    b2 = (uval << 8) & 0x00ff0000;
    b3 = uval << 24;

    res = b0 | b1 | b2 | b3;

    return (QNInt32) res;
}

inline void
qn_swapb_vi16_vi16(size_t len, const QNInt16* from, QNInt16* to)
{
    size_t i;

    for (i=0; i<len; i++)
	*to++ = qn_swapb_i16_i16(*from++);
}

inline void
qn_swapb_vi32_vi32(size_t len, const QNInt32* from, QNInt32* to)
{
    size_t i;

    for (i=0; i<len; i++)
	*to++ = qn_swapb_i32_i32(*from++);
}

#ifdef QN_WORDS_BIGENDIAN
#define qn_htob_i32_i32(val) (val)
#define qn_htob_vi32_vi32(len, in, out) qn_copy_vi32_vi32(len, in, out)
#define qn_htol_i32_i32(val) qn_swapb_i32_i32(val)
#define qn_htol_vi32_vi32(len, in, out) qn_swapb_vi32_vi32(len, in, out)
#define qn_btoh_i32_i32(val) (val)
#define qn_btoh_vi32_vi32(len, in, out) qn_copy_vi32_vi32(len, in, out)
#define qn_ltoh_i32_i32(val) qn_swapb_i32_i32(val)
#define qn_ltoh_vi32_vi32(len, in, out) qn_swapb_vi32_vi32(len, in, out)

#define qn_htob_i16_i16(val) (val)
#define qn_htob_vi16_vi16(len, in, out) qn_copy_vi16_vi16(len, in, out)
#define qn_htol_i16_i16(val) qn_swapb_i16_i16(val)
#define qn_htol_vi16_vi16(len, in, out) qn_swapb_vi16_vi16(len, in, out)
#define qn_btoh_i16_i16(val) (val)
#define qn_btoh_vi16_vi16(len, in, out) qn_copy_vi16_vi16(len, in, out)
#define qn_ltoh_i16_i16(val) qn_swapb_i16_i16(val)
#define qn_ltoh_vi16_vi16(len, in, out) qn_swapb_vi16_vi16(len, in, out)

#else

#define qn_htob_i32_i32(val) qn_swapb_i32_i32(val)
#define qn_htob_vi32_vi32(len, in, out) qn_swapb_vi32_vi32(len, in, out)
#define qn_htol_i32_i32(val) (val)
#define qn_htol_vi32_vi32(len, in, out) qn_copy_vi32_vi32(len, in, out)
#define qn_btoh_i32_i32(val) qn_swapb_i32_i32(val)
#define qn_btoh_vi32_vi32(len, in, out) qn_swapb_vi32_vi32(len, in, out)
#define qn_ltoh_i32_i32(val) (val)
#define qn_ltoh_vi32_vi32(len, in, out) qn_copy_vi32_vi32(len, in, out)

#define qn_htob_i16_i16(val) qn_swapb_i16_i16(val)
#define qn_htob_vi16_vi16(len, in, out) qn_swapb_vi16_vi16(len, in, out)
#define qn_htol_i16_i16(val) (val)
#define qn_htol_vi16_vi16(len, in, out) qn_copy_vi16_vi16(len, in, out)
#define qn_btoh_i16_i16(val) qn_swapb_i16_i16(val)
#define qn_btoh_vi16_vi16(len, in, out) qn_swapb_vi16_vi16(len, in, out)
#define qn_ltoh_i16_i16(val) (val)
#define qn_ltoh_vi16_vi16(len, in, out) qn_copy_vi16_vi16(len, in, out)

#endif /* #ifdef QN_WORDS_BIGENDIAN */

inline void
qn_conv_vi8_vi32(size_t len, const QNInt8* from, QNInt32* to)
{
    size_t i;

    for (i=0; i<len; i++)
	*to++ = *from++;
}

// Convert a vector of 32 bit integers to single precision floating point,
// rounding if necessary.

inline void
qn_conv_vi32_vf(size_t len, const QNInt32* from, float* to)
{
    size_t i;

    for (i=0; i<len; i++)
	*to++ = (float) *from++;
}


#endif /* #ifndef QN_intvec_h_INCLUDED */
