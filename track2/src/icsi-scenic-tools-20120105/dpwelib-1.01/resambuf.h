/***************************************************************\
*	resambuf.h  (after resmp.c)				*
* Buffer that uses resample.c to incrementally resample blocks	*
* that takes input a bit at a time - for resmp.c		*
* dpwe 23feb91							*
* 1997feb18 dpwe@icsi.berkeley.edu  Included in SPRACHworks	*
* $Header: /u/drspeech/repos/dpwelib/resambuf.h,v 1.3 2011/03/09 01:35:03 davidj Exp $
\***************************************************************/

#include "resample.h"

#define DESCLEN 16

typedef struct
    {
    SAMPL	*buf;
    long	bufSize;
    long	firstInvalid;
    long	firstUntouched;
    long	ifIndex;
    RsmpDescType *rsmpDescPtr;
    char	desc[DESCLEN];	/* user space for label for object */
    }	RsmpBufType;

typedef struct
    {
    RsmpBufType	*rsmpBufPtr;	/* target resampling buffer */
    long	sentSize;	/* first element of buf not yet chained */
    long	chainSize;	/* block size for chaining function */
    long	(*chainFn)();	/* func to call with new data */
    char 	*chainArg;	/* argument for chained function */
    long	emptSize;	/* block size for emptying function */
    long	(*emptFn)();	/* func to call to empty the buffer */
    int		emptArg;	/* arg passed to emptying function */
    char	desc[DESCLEN];	/* user space for label for object */
    }	TrigRSBufType;

/* the empty and chaining functions are called whenever enough pts are held as
   long r = (*fn)(long arg, SAMPL *buf, long size, int flushFlag);
   where r is returned as the number of points actually consumed,
   and size is guaranteed to be a multiple of the relevent block size */

TrigRSBufType 
*NewTrigRSBuf PARG((long chainSize, long emptSize, long actLen, long fill, 
	      long (*chainFn)(),char *chainArg,long (*emptFn)(), int emptArg,
	      RsmpDescType *rdp, char *desc));
/* Instance a new resampling buffer that passes new pts to a chained func,
   and automatically empties itself with an empty func. */

void FreeTrigRSBuf PARG((TrigRSBufType *trBp));

long FeedTrigRSBuf PARG((TrigRSBufType *trBp, SAMPL *sptr, long siz, 
			 int flush));
    /* resample new data into buffer, pass to chain fn, empty with emptFn */

void DropFromTrigRSBuf PARG((TrigRSBufType *trBp, long size));
    /* remove first <size> pts from buffer, update my ptrs */

long CopyAsEmptFun PARG((SAMPL *dst, SAMPL *buf, long siz, int  flag));
    /* a buffer-emptying function for unfancy buffer ops */

/** RsmpBufTyp **/
/* Note to dpwe:
   Should maybe adjust function of 'flushing' to only return points within
   the ifIndex, i.e. current actual time passed */

long RBSize PARG((RsmpBufType *rsBp, int flush));
    /* return data points in buffer; <flush> set gives a higher answer */

long RBValidSize PARG((RsmpBufType *rsBp));
    /* return number of strictly valid data points in buffer */

long RBTotalSize PARG((RsmpBufType *rsBp));
    /* return total number of 'touched' data points in buffer */

SAMPL *RBBufPtr PARG((RsmpBufType *rsBp, long offset, long size));
    /* Pointer to a linear block. <size> allows circular buffers to rotate */

RsmpBufType *NewResampBuf PARG((long size, long fullness, RsmpDescType *rdp,
			     char *desc));
    /* allocate a new buffer, optionally part-full */

void FreeResampBuf PARG((RsmpBufType *rsBp));

long AddToResampBuf PARG((RsmpBufType *rsBp, SAMPL *sptr, long siz, 
			  int flush));
    /* resample & overlay new data into the buffer */

void DropFromResampBuf PARG((RsmpBufType *rsBp, long num));
    /* remove first <num> pts from the buffer, clean, fix ptrs */







