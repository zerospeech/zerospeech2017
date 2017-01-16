const char* QN_seqgen_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_seqgen.cc,v 1.2 2006/01/05 01:04:17 davidj Exp $";

#include <QN_config.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "QN_seqgen.h"

////////////////////////////////////////////////////////////////

QN_SeqGen_Sequential::QN_SeqGen_Sequential(QNUInt32 a_max)
  : maximum(a_max),
    nextval(0)
{
}

QNUInt32
QN_SeqGen_Sequential::next()
{
    QNUInt32 ret;

    ret = nextval++;
    if (nextval>=maximum)
	nextval = 0;
    return ret;
}

////////////////////////////////////////////////////////////////

QN_SeqGen_Random48::QN_SeqGen_Random48(QNUInt32 a_max,QNUInt32 a_seed)
    : maximum(a_max)
{
    assert(maximum<0x7fffffff);	// lrand48 only works up to 2^31-1
    xsubi[0] = 0x330e;
    xsubi[1] = a_seed & 0xffff;
    xsubi[2] = (a_seed >> 16) & 0xffff;
    val = a_seed;
}

QNUInt32
QN_SeqGen_Random48::next()
{
    QNUInt32 retval;

    retval = val % maximum;
    val = (QNUInt32) nrand48(xsubi);
    return(retval);
}

////////////////////////////////////////////////////////////////

// A useful routine for working out the required xor constant
static QNUInt32 log2_ceil(QNUInt32);

// The algorithm used to generate the no-replace random sequence was stolen
// from BoB, and in turn comes from
// Graphics Gems I, "A Digital Dissolve Effect", pg. 224
// (and probably Knuth before that!)

// Its a classic linear feedback shift register algorithm.  It only works for
// powers of 2 sequence sizes - the value to xor depends on the size
// and is obtained from a table.  We check the value produced and discard
// those that are outside our required non-power-of-two range size.
// Note that linear feedback shift registers do not work with values of 0

// The table of xor values for given power-of-2 sequence sizes

enum {
    XOR_TABLE_SIZE = 32	// The size of the xor table
};

// There are the values below and other possible values available at
// http://homepage.mac.com/afj/taplist.html
// (google on "maximal lfsr" to find others)
static const QNUInt32 xor_table[XOR_TABLE_SIZE] = 
{
  0x1, 0x1, 0x3, 0x6, 0xc, 
  0x14, 0x30, 0x60, 0xb8, 0x110, 
  0x240, 0x500, 0xca0, 0x1b00, 0x3500, 
  0x6000, 0xb400, 0x12000, 0x20400, 0x72000, 
  0x90000, 0x140000, 0x300000, 0x420000, 0xd80000, 
  0x1200000, 0x3880000, 0x7200000, 0x9000000, 0x14000000, 
  0x32800000, 0x48000000
};

// This returns the log base 2 of a value, rounded up to the nearest integer.
// (taken from the SPERT fixed point library)
// It's basically a wrapped-up "count leading zero" routine

static inline QNUInt32
log2_ceil(QNUInt32 val)
{
    QNUInt32 mask;		/* Mask for current bits being checked */
    QNUInt32 clear;		/* Which top bits are set */
    int topbits;                /* Number of bits being checked */
    int count;                  /* Bits found clear so far */
    int j;                      /* Counter within one word */

    mask = 0xffff0000u;
    topbits = 16;
    count = 0;
    val -= 1;
    for (j=0; j<4; j++)
    {
        clear = val & mask;
        if (!clear)
        {
            count += topbits;
            val <<= topbits;
        }
        topbits >>= 1;
        mask <<= topbits;
    }
    clear = val & 0xc0000000;
    if (((int) clear) >=0)
        count++;
    if (clear==0)
        count++;
    return (32-count);
}

// Constructor

QN_SeqGen_RandomNoReplace::QN_SeqGen_RandomNoReplace(QNUInt32 a_max,
						     QNUInt32 a_seed)
  : max(a_max),
    xor_val(xor_table[log2_ceil(max+1)])
{
    assert(max>=1 && max<=0x7fffffff);
    nextval = (QNUInt32) (a_seed % max) + 1;
}

// Move on to the next value 

QNUInt32
QN_SeqGen_RandomNoReplace::next()
{
    QNUInt32 curval;	// The value we will return

    curval = nextval;
    do
    {
	if (nextval & 1)
	    nextval = (nextval>>1) ^ xor_val;
	else
	    nextval = nextval>>1;
    } while (nextval>max);

    return curval - 1;
}

////////////////////////////////////////////////////////////////
#ifdef NEVER

// Here is some test code for the log2_ceil function, in case anyone
// ever needs it in future...

static void
log2_ceil_check(void)
{
    assert(log2_ceil(1)==0);
    assert(log2_ceil(2)==1);
    assert(log2_ceil(3)==2);
    assert(log2_ceil(4)==2);
    assert(log2_ceil(5)==3);
    assert(log2_ceil(7)==3);
    assert(log2_ceil(8)==3);
    assert(log2_ceil(9)==4);
    assert(log2_ceil(15)==4);
    assert(log2_ceil(16)==4);
    assert(log2_ceil(17)==5);
    assert(log2_ceil(31)==5);
    assert(log2_ceil(32)==5);
    assert(log2_ceil(33)==6);
    assert(log2_ceil(63)==6);
    assert(log2_ceil(64)==6);
    assert(log2_ceil(65)==7);
    assert(log2_ceil(127)==7);
    assert(log2_ceil(128)==7);
    assert(log2_ceil(129)==8);
    assert(log2_ceil(255)==8);
    assert(log2_ceil(256)==8);
    assert(log2_ceil(257)==9);
    assert(log2_ceil(511)==9);
    assert(log2_ceil(512)==9);
    assert(log2_ceil(513)==10);
    assert(log2_ceil(65535)==16);
    assert(log2_ceil(65536)==16);
    assert(log2_ceil(65537)==17);
}

#endif
