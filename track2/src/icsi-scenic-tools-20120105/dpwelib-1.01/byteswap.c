/***************************************************************\
*	byteswap.c		  				*
*   swap bytes e.g. for files between different machines 	*
*   dpwe 11nov91 after sndcvt 20feb91				*
\***************************************************************/

#include <math.h>	/* for fabs() prototype */
#include <ctype.h>	/* for NumericQ() */
#include "dpwelib.h"
#include "byteswap.h"

int MatchByteMode(INT32 red, INT32 target)
{	/* return the bytemode to apply to 'red' to make 'target', or -1 */
    int mode = -1;	/* error return */

    if(lshuffle(red, BM_INORDER) == target)	mode = BM_INORDER;
    else if(lshuffle(red, BM_BYTEREV) == target)	mode = BM_BYTEREV;
    else if(lshuffle(red, BM_WORDREV) == target)	mode = BM_WORDREV;
    else if(lshuffle(red, BM_BYWDREV) == target)	mode = BM_BYWDREV;
    return mode;
}

int HostByteMode()	/* relative to 68000-style (MSB byte first) */
    {
    /* work it out */
    char tchrs[4];
    INT32 l, m = 0x01020304;
    int mode;

    tchrs[0] = '\1';	tchrs[1] = '\2';
    tchrs[2] = '\3';	tchrs[3] = '\4';
    l = *(INT32 *)tchrs;
    if( (mode = MatchByteMode(l, m)) < 0)  {
	FPRINTF((stderr, "byteswap: unrecognised longword order %lx\n", (unsigned long)l));
	abort();
    }
    return mode;
    }

char *BytemodeName(bytemode)
    int bytemode;
    {
    char *boStr;

    switch(bytemode)
	{
    case BM_INORDER:       boStr = "in order";		break;
    case BM_BYTEREV:       boStr = "byte reversed";	break;
    case BM_WORDREV:	   boStr = "word reversed";	break;
    case BM_BYWDREV:       boStr = "byte & word reversed";	break;
    default:		   boStr = "? byte order";
	}
    return(boStr);
    }

/*** from dec2mac.c ***/
short wshuffle(w,mode)
    int   w;
    int   mode;
    {
    short lb, hb;   /* actually bytes */

    lb = 0xFF & w;
    hb = 0xFF & (w>>8);
    if(mode==BM_BYTEREV || mode == BM_BYWDREV)
	return( (lb<<8) + hb );
    else
	return( (hb<<8) + lb );
    }

INT32 lshuffle(l,mode)
    INT32 l;
    int  mode;
    {
    short lw, hw;

    hw = wshuffle( (short) (l>>16L) , mode );
    lw = wshuffle( (short) l , mode );
    if(mode == BM_INORDER || mode == BM_BYTEREV)
	return( ((0xFFFFL&(INT32)hw)<<16L) + (0xFFFFL&(INT32)lw ) );
    else
	return( ((0xFFFFL&(INT32)lw)<<16L) + (0xFFFFL&(INT32)hw ) );
    }

INT32 FloatAsLong(f, mode)
    FLOATARG f;
    int   mode;
    {
    INT32 l, *pl;

    pl = (INT32 *)&f;
/*    if(mode == WORDREV || mode == BYWDREV)  /* detect DEC float formats */
/*	l = ToUFloat(f);
/*    else
 */
	l = *pl;
    return(lshuffle(l, mode));
    }

float FloatFromLong(l, mode, fixup)
    INT32  l;
    int   mode;
    INT32  fixup;	/* poke and hope to fix DEC float formats */
    {
    INT32  *pl;
    float f, *pf;

    pf = &f;
    pl = (INT32 *)pf;
    *pl = lshuffle(l,mode);
    *pl += (INT32)fixup;
    return(f);
/*    if(mode == WORDREV || mode == BYWDREV)  /* detect DEC float formats */
/*	l = ToUFloat(f);	*/
    }

void ConvertBufferBytes(buf, bytes, wordsz, mode)
    char	*buf;
    long 	bytes;
    int 	wordsz;		/* 1, 2 or 4 : maybe should be FLOAT etc */
    int 	mode;
    {
    long 	i;
    short	*sp;
    INT32	*lp;
    /* no changes needed if buffer is chars, or no swapping implied, or 
       word swapping for width-2 data */

/*    FPRINTF((stderr, "buf %lx bytes %ld wsz %d mode %d\n",
	    buf, bytes, wordsz, mode));	*/
    if(wordsz < 2 
       || mode == BM_INORDER
       || (mode == BM_WORDREV && wordsz < 4) )	return;
    if(wordsz == 4 && mode == BM_BYTEREV)  {
	wordsz = 2;	/* swapping bytes in a word same for long/short */
    }
    if(wordsz == 2)  {		/* only actual operation on words is swab */
	sp = (short *)buf;
	i = bytes/wordsz;
	while(i > 0)  {
	    *sp = WSWAPB(*sp);
	    ++sp;	--i;
	}
    } else {				/* presume wordsz == 4 */
	lp = (INT32 *)buf;
	i = bytes/wordsz;
	if(mode == BM_BYWDREV) {
	    while(i > 0)  {
		*lp = LSWAPB(*lp);
		++lp; --i;
	    }
	} else {			/* mode == WORDREV */
	    while(i > 0)  {
		*lp = LSWAPW(*lp);
		++lp; --i;
	    }
	}	
    }
}

#ifdef MAIN

main()	/* for test */
    {
    printf("hostmode is %s\n", BytemodeName(HostByteMode()));
    }
	   

#endif /* MAIN */
