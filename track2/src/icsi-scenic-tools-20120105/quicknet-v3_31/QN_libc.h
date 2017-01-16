/* $Header: /u/drspeech/repos/quicknet2/QN_libc.h,v 1.19 2005/09/21 00:50:26 davidj Exp $
**
** This file contains some C library functions that may not exist on all
** systems
*/

#ifndef QN_libc_h_INCLUDED
#define QN_libc_h_INCLUDED

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>
#ifdef QN_HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef QN_HAVE_FSETPOS

typedef long fpos_t;

int fsetpos(FILE* stream, const fpos_t *pos);
int fgetpos(FILE* stream, fpos_t *pos);

#endif

#ifndef SEEK_SET
#define SEEK_SET (0)
#endif

#ifndef QN_HAVE_STRERROR

char* strerror(int errno);

#endif

#ifndef QN_HAVE_ERRNO_H
extern int errno;
#endif



#ifndef MAXPATHLEN
#define MAXPATHLEN (1024)
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN (1024)
#endif


#ifdef __cplusplus
}
#endif

#endif
