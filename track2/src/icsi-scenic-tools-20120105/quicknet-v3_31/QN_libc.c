#ifndef NO_RCSID
const char* QN_libc_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_libc.c,v 1.11 2005/09/21 00:50:10 davidj Exp $";
#endif
/* This file contains some C library functions that may not exist on all
** systems
*/

#include <QN_config.h>
#include <stdio.h>
#include <errno.h>
#include "QN_libc.h"


#ifndef QN_HAVE_FSETPOS

#ifndef QN_HAVE_DECL_FSEEK
int fseek(FILE* stream, long offset, int);
#endif

int
fsetpos(FILE* stream, const fpos_t *pos)
{
    int ec;

    ec = fseek(stream, *pos, 0);
    return(ec);
}

int
fgetpos(FILE* stream, fpos_t *pos)
{
    *pos = ftell(stream);
    return 0;
}

#endif

#ifndef QN_HAVE_STRERROR

#ifndef QN_HAVE_DECL_SYS_NERR
extern int sys_nerr;
#endif
#ifndef QN_HAVE_DECL_SYS_ERRLIST
extern char* sys_errlist[];
#endif

char*
strerror(int errnum)
{
    char* unknown = "unknown error";

    if (errnum<sys_nerr)
	return(sys_errlist[errnum]);
    else
	return(unknown);
}

#endif

#ifndef QN_HAVE_STRSTR
#error oh dear, there is no strstr() function on your system or in this file 
#endif

#ifndef QN_HAVE_VPRINTF
#error oh dear, there is no vprintf() function on your system or in this file 
#endif


