/***************************************************************\
*	genutils.h						*
*	random useful routines for the library			*
*	20feb91 dpwe						*
\***************************************************************/

#ifndef HAVE_STRDUP
char *strdup PARG((const char *));
#endif  /* HAVE_STRDUP */

long fSize PARG((FILE *f));
void SubstExtn PARG((char *sce, char *newExt, char *dst));   
    /* must include "." */
int NumericQ PARG((char *s));		
    /* does string look like a numeric argument */
char *Mymalloc PARG((long num, long siz, char *title));
void Myfree PARG((char *ptr, char *title));
long EnclPwrOf2 PARG((long siz));
