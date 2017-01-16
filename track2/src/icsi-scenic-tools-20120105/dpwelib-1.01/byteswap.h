/***************************************************************\
*	byteswap.h		  				*
*   swap bytes e.g. for files between different machines 	*
*   dpwe 11nov91 after sndcvt 20feb91				*
\***************************************************************/

#ifndef _BYTESWAP_H
#define _BYTESWAP_H
/* byte order types - machmode values */
#define BM_INORDER 0
#define BM_BYTEREV 1
#define BM_WORDREV 2
#define BM_BYWDREV 3
/* which bit to test if just worried about words */
#define BM_WORDMASK 1

#ifndef INT32_DEFINED
#define INT32	int
#define INT32_DEFINED
#endif /* !INT32_DEFINED */

#ifndef INT16_DEFINED
#define INT16	short
#define INT16_DEFINED
#endif /* !INT16_DEFINED */

/* nice macros */
/* swap bytes in a word */
#define WSWAPB(w)  (((0xFF&w)<<8)|((0xFF00&w)>>8))
/* reverse the bytes in a long (BYWDREV) */
#define LSWAPB(l)  ((((0xFF000000&l)>>24)&0xFF)|((0xFF0000&l)>>8)\
                                 |((0xFF00&l)<<8)|((0xFF&l)<<24))
/* just swap the words in a long (WORDREV) */
#define LSWAPW(l)  ((((0xFFFF0000&l)>>16)&0xFFFF)|((0xFFFF&l)<<16))

/* static function prototype */
int MatchByteMode PARG((INT32 red, INT32 target));
    /* return the bytemode to apply to 'red' to make 'target', or -1 */
int   HostByteMode PARG((void)); /* relative to 68000-style (MSB byte first) */
char *BytemodeName PARG((int bytemode));
short wshuffle PARG((int w, int mode));
INT32  lshuffle PARG((INT32 l, int mode));
INT32  FloatAsLong PARG((FLOATARG f, int mode));
float FloatFromLong PARG((INT32 l, int mode, INT32 fixup));
void  ConvertBufferBytes PARG((char *buf, long bytes, int wordsz, int mode));

#endif /* _BYTESWAP_H */

