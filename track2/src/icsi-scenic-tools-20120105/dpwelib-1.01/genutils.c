/***************************************************************\
*	genutils.c						*
*	random useful routines for the library			*
*	20feb91 dpwe						*
\***************************************************************/

#include "dpwelib.h"
#include <ctype.h>

#include "genutils.h"

#ifndef HAVE_STRDUP 	/* hard to find this nowadays */
char *strdup(const char *s)
{	/* think c doesn't have this?? */
    char *t = NULL;
    
    if(s)  {
	t = malloc(strlen(s)+1);
	strcpy(t, s);
	}
    return t;
}
#endif /* !HAVE_STRDUP */

/* DON'T use this with sndf files - use pushfileio.c:GetFileSize instead */

long fSize(FILE *f)
{	/* returns minus one for unseekable streams */
    long	l,p;

    l = -1;
    if(f != NULL)
	{
	p = ftell(f);
	if(p >= 0)
	    {
	    fseek(f, 0L, SEEK_END);
	    l = ftell(f);
	    fseek(f, p, SEEK_SET);
	    }
	}
    return l;
}

void SubstExtn(
    char	*sce,
    char	*newExt,	/* extension in form ".ext" ("." important) */
    char	*dst)
{
    char	*s;

    strcpy(dst,sce);
    s = strrchr(dst, *newExt);		/* point to last '.' (or..) */
    if(s == NULL)
	s = dst + strlen(dst);		/* point to the final '\0' */
    strcpy(s,newExt);			/* dst == "<root of sce>.<newExt>" */
}

int NumericQ(char *s)
{   /* does string look like a numeric argument */
    char c;

    while( (c = *s++)!='\0' && isspace((int)c))	/* strip leading ws */
	;
    if( c == '+' || c == '-' )
	c = *s++;		/* perhaps skip leading + or - */
    return isdigit((int)c);
}

char *Mymalloc(
    long	num,
    long	siz,	/* number and size of items to allocate */
    char	*title)	/* for error message */
{   /* malloc cover */
    long 	nbytes;
    char 	*ptr;

    nbytes = num*siz;
    ptr = (char *)malloc(nbytes);
    if(ptr == NULL)
	{
	FPRINTF((stderr, "Mymalloc fail (from %s): wanted %ld of %ld = %ld\n",
		title, num, siz, nbytes));
	ABORT;
	}
    memset(ptr, 0, nbytes);	/* clear it all */
#ifdef TRACEALLOC
    FPRINTF((stderr, "Mymalloc: '%s' gets %ld of %ld at %lx\n", 
	    title, num, siz, ptr));
#endif /* TRACEALLOC */
    return ptr;
}

void Myfree(
    char *ptr,
    char *s)
{   /* free cover */
#ifdef TRACEALLOC
	FPRINTF((stderr, "Myfree: '%s' returns %lx\n", s, ptr));
#endif /* TRACEALLOC */
    free(ptr);
}

#define BITSINLONG	32
long EnclPwrOf2(long x)
{	/* find 2^n >= x */
    long 	n = 0;
    long 	m = 1;

    if(x<0)	x = -x;
    while(m<x && n<BITSINLONG)
	{	m <<= 1;	++n;	}
    return m;
}

