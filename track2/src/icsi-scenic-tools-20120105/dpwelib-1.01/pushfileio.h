/*
 * pushfileio.c
 * 
 * Layer on top of fopen(3S) etc
 * to permit - pushing of arbitrary-sized blocks back onto input files
 *           - 'virtual' file repositioning (for multiple files on 
 *             on a single stream)
 *
 * necessitated to support Abbot soundfile streams for sndf(3).
 * 
 * 1997sep10 dpwe@icsi.berkeley.edu
 * $Header: /u/drspeech/repos/dpwelib/pushfileio.h,v 1.5 2011/03/09 01:35:03 davidj Exp $
 */

#ifndef _PUSHFILEIO_H
#define _PUSHFILEIO_H

/* constants */


/* prototypes */

int my_fread(void *buf, size_t size, size_t nitems, FILE *file);
    /* cover for fread uses the FINFO structure to do push back */
int my_fwrite(void *buf, size_t size, size_t nitems, FILE *file);
    /* just plain write */
int my_fpush(void *buf, size_t size, size_t nitems, FILE *file);
    /* push some data back onto a file so it will be read next;
       return the new pos of the file. */
void my_fsetpos(FILE *file, long pos);
    /* force the pos to be read as stated */
FILE *my_fopen(char *path, char *mode);
FILE *my_popen(const char *path, const char *mode);
void my_fclose(FILE *file);
int my_pclose(FILE *file);
int my_fseek(FILE *file, long pos, int mode);
    /* do fseek, return 0 if OK or -1 if can't seek */
long my_ftell(FILE *file);
int my_feof(FILE *Ffile);
    /* it's not EOF if we still have stuff in the pushbuf */
int my_fputs(char *s, FILE *file);
    /* write a regular string to the stream */
char *my_fgets(char *s, int n, FILE *stream);
    /* read a string from the stream */

long seek_by_read(FILE *file, long bytes);
    /* skip over bytes for files not supporting seek by repeated read */
long GetFileSize(FILE *file);
    /* return number of bytes in a file, or -1 if can't tell */

#ifdef REDEFINE_FOPEN
/* Use my_finfo calls for all ansi files */
#define fopen(a,b)	my_fopen(a,b)
#define popen(a,b)	my_popen(a,b)
#define fclose(a)	my_fclose(a)
#define pclose(a)	my_pclose(a)
#define fread(a,b,c,d)	my_fread(a,b,c,d)
#define fwrite(a,b,c,d)	my_fwrite(a,b,c,d)
#define fseek(a,b,c)	my_fseek(a,b,c)
#define ftell(a)	my_ftell(a)
#ifdef feof
#undef feof
#endif /* feof is a macro already */
#define feof(a)		my_feof(a)
#define fputs(a,b)	my_fputs(a,b)
#define fgets(a,b,c)	my_fgets(a,b,c)

/* the new fn all this provides - like fungetc but for a whole block */
#define fpush(a,b,c,d)	my_fpush(a,b,c,d)
/* also, a function to reset the notional file pointer */
#define fsetpos(a,b)	my_fsetpos(a,b)

/* tell source that using fpush/fsetpos is OK */
#define HAVE_FPUSH	1

#endif /* REDEFINE_FOPEN */



#endif /* !_PUSHFILEIO_H */
