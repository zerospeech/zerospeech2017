/*  ULAW.C	                                                    */
/*  Conversion tables for ulaw encode/decode of 16-bit linear audio */

#include "dpwelib.h"
#include "ulaw.h"

char exp_lut[128] = {  0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,
			5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
			6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
			6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7   };

short ulaw_decode[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
     -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
     -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
     -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
     -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
     -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
     -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
      -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
      -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
      -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
      -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
      -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
       -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
     32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
     23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
     15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
     11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
      7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
      5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
      3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
      2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
      1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
      1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
       876,    844,    812,    780,    748,    716,    684,    652,
       620,    588,    556,    524,    492,    460,    428,    396,
       372,    356,    340,    324,    308,    292,    276,    260,
       244,    228,    212,    196,    180,    164,    148,    132,
       120,    112,    104,     96,     88,     80,     72,     64,
	56,     48,     40,     32,     24,     16,      8,      0 };

#define MUCLIP  32635
#define BIAS    0x84
#define MUZERO  0x02
#define ZEROTRAP

void Short2Ulaw(
    short	*sh_ptr,	/* 16 bit linear input */
    unsigned char *u_ptr,	/* ulaw byte output buffer */
    int 	sinc,		/* input steps */
    int 	uinc,		/* step size for above */
    long	len)		/* how many ulaw bytes to generate */
{   /* was static void ulawtran() ... ulaw-encode spout vals & put in outbuf */
    register long   longsmp,n;
    register short  *sp;
    register unsigned char *up;
    register int    sign;
    int sample, exponent, mantissa, ulawbyte;
    extern  char    exp_lut[];               /* mulaw encoding table */

/*    if(debug) printf("lin2mu: bytes %ld, from %lx to %lx\n",
		     ulaw_Blen, snd_ptr, u_ptr);	/* */
    n  = len;
    sp = sh_ptr;	up = u_ptr;
    /* work forwards to allow in-place conversion */
    while(n--)  
	{
	if ((longsmp = *sp) < 0) {		/* if sample negative	*/
	    sign = 0x80;
	    longsmp = - longsmp;		/*  make abs, save sign	*/
	    }
	else sign = 0;
	if (longsmp > MUCLIP)  		/* out of range? */
	    longsmp = MUCLIP;       	/*   clip	 */
	sample = longsmp + BIAS;
	exponent = exp_lut[( sample >> 8 ) & 0x7F];
	mantissa = ( sample >> (exponent+3) ) & 0x0F;
	ulawbyte = ~ (sign | (exponent << 4) | mantissa );
#ifdef ZEROTRAP
	if (ulawbyte == 0) ulawbyte = MUZERO;    /* optional CCITT trap */
#endif	
	*up = ulawbyte;
	sp += sinc;
	up += uinc;
       	}
    }	

void Ulaw2Short(
    unsigned char *u_ptr,	/* ulaw byte output buffer */
    short	*sh_ptr,	/* 16 bit linear output */
    int 	uinc,		/* step size for above */
    int 	sinc,		/* input steps */
    long	len)		/* how many ulaw bytes to generate */
{
    register long   n;
    register short  *sp;
    register unsigned char *up;
    register int u;

#ifdef DEBUG	/* check data on both sides */
    for(n=0; n<10; ++n)
	fprintf(stderr, "%3d ", *(u_ptr+n));
    fprintf(stderr, "\n");
#endif /* DEBUG */

    n = len;
    up = u_ptr + n * (long)uinc;
    sp = sh_ptr + n * (long)sinc;
    while(n--)			/* work backwards to allow in-place */
	{
	up -= uinc;
	sp -= sinc;
	u = *up;
#ifdef ZEROTRAP
	if (u == MUZERO) u = 0;    /* optional CCITT trap */
#endif	
	*sp = ulaw_decode[u];
	}

#ifdef DEBUG	/* check data on both sides */
    for(n=0; n<10; ++n)
	fprintf(stderr, "%7d ", *(sh_ptr+n));
    fprintf(stderr, "\n");
#endif /* DEBUG */
}

/* from
http://www.frontierd.com/data/artbuilder_examples/linear2alaw.htm
 */

/**********************************************************************/
/**                                                                  **/
/** function : linear2alaw                                           **/
/** purpose  : conversion of linear PCM to alaw                      **/
/** input    : linear, 13 bit 2's complement                         **/
/** output   : alaw, 8 bit alaw = PSSSQQQQ                           **/
/**              P    = polarity bit (= sign of linear)              **/
/**              SSS  = segment number                               **/
/**                   = position of MSB of linear                    **/
/**              QQQQ = quantization number                          **/
/**                   = 4 LSB's of ( linear >> (SSS+1) )             **/
/**                                                                  **/
/** alaw encoding/decoding table                                     **/
/** ----------------------------                                     **/
/**  ______________________________________________________________  **/
/** | abs ( linear ) |  Segment   |   Quantiz   | alaw  | Decoder  | **/
/** |                | number SSS | number QQQQ | value |  ampl    | **/
/** |________________|____________|_____________|_______|__________| **/
/** |  0 - 2         |            |   0000      |   0   |   1      | **/
/** |  2 - 4         |            |   0001      |   1   |   3      | **/
/** |    .           |    000     |     .       |   .   |   .      | **/
/** | 30 - 32        |            |   1111      |  15   |  31      | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 32 - 34        |            |   0000      |  16   |  33      | **/
/** |    .           |    001     |     .       |   .   |   .      | **/
/** | 62 - 64        |            |   1111      |  31   |  63      | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 64 - 68        |            |   0000      |  32   |  66      | **/
/** |    .           |            |     .       |   .   |   .      | **/
/** |    .           |    010     |     .       |   .   |   .      | **/
/** |    .           |            |     .       |   .   |   .      | **/
/** | 124 - 128      |            |   1111      |  47   | 126      | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 128 - 136      |            |   0000      |  48   | 132      | **/
/** |     .          |            |     .       |   .   |   .      | **/
/** |     .          |    011     |     .       |   .   |   .      | **/
/** |     .          |            |     .       |   .   |   .      | **/
/** | 248 - 256      |            |   1111      |  63   |  252     | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 256 - 272      |            |   0000      |  64   |  264     | **/
/** |     .          |            |     .       |   .   |   .      | **/
/** |     .          |    100     |     .       |   .   |   .      | **/
/** |     .          |            |     .       |   .   |   .      | **/
/** | 496 - 512      |            |   1111      |  79   |  504     | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 512 - 544      |            |   0000      |  80   |  528     | **/
/** |     .          |            |     .       |   .   |   .      | **/
/** |     .          |    101     |     .       |   .   |   .      | **/
/** |     .          |            |     .       |   .   |   .      | **/
/** | 992 - 1024     |            |   1111      |  95   | 1008     | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 1024 - 1088    |            |   0000      |  96   | 1056     | **/
/** |      .         |            |     .       |   .   |   .      | **/
/** |      .         |    110     |     .       |   .   |   .      | **/
/** |      .         |            |     .       |   .   |   .      | **/
/** | 1984 - 2048    |            |   1111      |  111  | 2016     | **/
/** |________________|____________|_____________|_______|__________| **/
/** | 2048 - 2176    |            |   0000      |  112  | 2112     | **/
/** |      .         |            |     .       |   .   |   .      | **/
/** |      .         |    111     |     .       |   .   |   .      | **/
/** |      .         |            |     .       |   .   |   .      | **/
/** | 3968 - 4096    |            |   1111      |  127  | 4032     | **/
/** |________________|____________|_____________|_______|__________| **/
/** **/
/**********************************************************************/

/* BUT code from opt/softsound/abbot-1.4.2/src/Slib/Sulawalaw.c */

short alaw_decode[256] = 
{ -5504, -5248, -6016, -5760, -4480, -4224,
  -4992, -4736, -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
  -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368, -3776, -3648,
  -4032, -3904, -3264, -3136, -3520, -3392, -22016, -20992, -24064,
  -23040, -17920, -16896, -19968, -18944, -30208, -29184, -32256,
  -31232, -26112, -25088, -28160, -27136, -11008, -10496, -12032,
  -11520, -8960, -8448, -9984, -9472, -15104, -14592, -16128, -15616,
  -13056, -12544, -14080, -13568, -344, -328, -376, -360, -280, -264,
  -312, -296, -472, -456, -504, -488, -408, -392, -440, -424, -88, -72,
  -120, -104, -24, -8, -56, -40, -216, -200, -248, -232, -152, -136,
  -184, -168, -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
  -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696, -688, -656,
  -752, -720, -560, -528, -624, -592, -944, -912, -1008, -976, -816,
  -784, -880, -848, 5504, 5248, 6016, 5760, 4480, 4224, 4992, 4736,
  7552, 7296, 8064, 7808, 6528, 6272, 7040, 6784, 2752, 2624, 3008,
  2880, 2240, 2112, 2496, 2368, 3776, 3648, 4032, 3904, 3264, 3136,
  3520, 3392, 22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
  30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136, 11008, 10496,
  12032, 11520, 8960, 8448, 9984, 9472, 15104, 14592, 16128, 15616,
  13056, 12544, 14080, 13568, 344, 328, 376, 360, 280, 264, 312, 296,
  472, 456, 504, 488, 408, 392, 440, 424, 88, 72, 120, 104, 24, 8, 56,
  40, 216, 200, 248, 232, 152, 136, 184, 168, 1376, 1312, 1504, 1440,
  1120, 1056, 1248, 1184, 1888, 1824, 2016, 1952, 1632, 1568, 1760,
  1696, 688, 656, 752, 720, 560, 528, 624, 592, 944, 912, 1008, 976,
  816, 784, 880, 848};

void alaw2lin(unsigned char *alawp, short *linp, int ainc, int linc, long npts)
{
    unsigned char alaw;
    long i;

    for (i = 0; i < npts; ++i) {
	alaw = *alawp;
	*linp = alaw_decode[alaw];
	
	alawp += ainc;
	linp += linc;
    }
}

/* this is derived from the Sun code - it is a bit simpler and has int input */
#define QUANT_MASK      (0xf)           /* Quantization field mask. */
#define NSEGS           (8)             /* Number of A-law segments. */
#define SEG_SHIFT       (4)             /* Left shift for segment number. */

void lin2alaw(short *linp, unsigned char *alawp, int linc, int ainc, long npts)
{
    int linear, seg; 
    unsigned char aval, mask;
    unsigned char alaw; 
    static short seg_aend[NSEGS] 
	= {0x1f,0x3f,0x7f,0xff,0x1ff,0x3ff,0x7ff,0xfff};
    long i;

    for (i = 0; i < npts; ++i) {
	linear = (*linp) >> 3;

	if(linear >= 0) {
	    mask = 0xd5;                /* sign (7th) bit = 1 */
	} else {
	    mask = 0x55;                /* sign bit = 0 */
	    linear = -linear - 1;
	}

	/* Convert the scaled magnitude to segment number. */
	for(seg = 0; seg < NSEGS && linear > seg_aend[seg]; seg++);
  
	/* Combine the sign, segment, and quantization bits. */
	if(seg >= NSEGS)              /* out of range, return maximum value. */
	    aval = (unsigned char) (0x7F ^ mask);
	else {
	    aval = (unsigned char) seg << SEG_SHIFT;
	    if (seg < 2)
		aval |= (linear >> 1) & QUANT_MASK;
	    else
		aval |= (linear >> seg) & QUANT_MASK;
	    aval = (aval ^ mask);
	}

	/* alaw */
	*alawp = aval;

	alawp += ainc;
	linp += linc;
    }
}
