//
// Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
//

#include "util.h"
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

int same_vectors(int *vec1, int n1, int *vec2, int n2)
{
  if(n1 != n2) return 0;
  for(int i=0;i<n1;i++)
    if(vec1[i] != vec2[i]) return 0;
  return 1;
}

void make_cumhist(int *cumhist, int *hist, int n)
{
  cumhist[0]=0;
  for(int i=0; i<n; i++)
    cumhist[i+1] = cumhist[i] + hist[i];
}

char *mmap_file(char *fn, int *len)
{
  char *result;
  struct stat stat_buf;
  int fd = open(fn, O_RDONLY);

  if(fstat(fd, &stat_buf) == EOF) {
    fprintf(stderr, "mmapfile: can't stat %s\n", fn);
    fatal("mmapfile failed");
  }
  *len = stat_buf.st_size;
  result = mmap(NULL, *len, PROT_READ, MAP_PRIVATE, fd, 0);
  if(result == (char *) EOF) fatal("mmapfile failed");
  close(fd);
  return result;
}

int lenchars_file(char *fn)
{
  FILE *fd = fopen(fn, "r");
  if(!fd) {
    fprintf(stderr, "lenchars_file: fn = %s\n", fn);
    fatal("open failed");
  }
  if(fseek(fd, 0, SEEK_END) == EOF) fatal("seek failed");
  int len = ftell(fd);
  fclose(fd);
  return len;
}

char *readchars_file(char *fn, long offset, int *nread)
{
   FILE *fd = fopen(fn, "r");
  if(!fd) {
    fprintf(stderr, "readchars_file: fn = %s\n", fn);
    fatal("open failed");
  }
   if(fseek(fd, offset, SEEK_SET) == EOF) fatal("seek failed");
   
   char *result = MALLOC(*nread);
   int nbytesread = fread(result, 1, *nread, fd);
   //fprintf(stderr,"nread = %d\n",nbytesread);
   
   if(nbytesread != *nread)
      fatal("fread failed");
   
   fclose(fd);
   return result;
}

float *readfeats_file(char *fn, int D, int *fA, int *fB, int *N) 
{
   int nbytes = lenchars_file(fn);
   int nfloats = nbytes/sizeof(float);

   if ( nfloats % D != 0 )
      fatal("ERROR: Feature dimension inconsistent with file\n");

   int nframes = nfloats/D;

   //fprintf(stderr,"nframes = %d\n",nframes);

   if ( *fA == -1 ) *fA = 0;
   if ( *fA >= nframes ) {
      fatal("ERROR: fA is larger than total number of frames\n");
   }

   if ( *fB == -1 ) *fB = nframes-1;
   if ( *fB < *fA ) {
      fatal("ERROR: fB is less than fA\n");
   }
      
   *N = *fB - *fA + 1;
   //fprintf(stderr,"nreq = %d\n",*N);
   int nbytesread = (*N)*D*sizeof(float); 

   return (float*) readchars_file(fn, (*fA)*D*sizeof(float), &nbytesread);
}

int readdim_file(char *fn)
{
  FILE *fd = fopen(fn, "r");

  if(!fd) {
    fprintf(stderr, "readdim_file: fn = %s\n", fn);
    fatal("open failed");
  }

  if ( fd == NULL )
    fprintf(stderr,"File does not exist: %s\n", fn);
   
  int D = 0;
  fread((void *)&D, sizeof(int), 1, fd);

  fclose(fd);
  return D;
}

char *readchars_file2(char *fn, long offset, int *nread)
{
   FILE *fd = fopen(fn, "r");
  if(!fd) {
    fprintf(stderr, "readchars_file2: fn = %s\n", fn);
    fatal("open failed");
  }

   if(fseek(fd, offset+sizeof(float), SEEK_SET) == EOF) fatal("seek failed");
   
   char *result = MALLOC(*nread);
   int nbytesread = fread(result, 1, *nread, fd);
   //fprintf(stderr,"nread = %d\n",nbytesread);
   
   if(nbytesread != *nread)
      fatal("fread failed");
   
   fclose(fd);
   return result;
}

float *readfeats_file2(char *fn, int fA, int fB, int *N, int *D) 
{
   int nbytes = lenchars_file(fn)-sizeof(int);
   int nfloats = nbytes/sizeof(float);

   *D = readdim_file(fn);

   if ( nfloats % *D != 0 )
      fatal("ERROR: Feature dimension inconsistent with file\n");

   int nframes = nfloats/(*D);

   if ( fA == -1 ) fA = 0;
   if ( fA >= nframes ) {
      fatal("ERROR: fA is larger than total number of frames\n");
   }

   if ( fB == -1 ) fB = nframes-1;
   if ( fB < fA ) {
      fatal("ERROR: fB is less than fA\n");
   }
      
   *N = fB - fA + 1;
   //fprintf(stderr,"nreq = %d\n",*N);
   int nbytesread = (*N)*(*D)*sizeof(float); 

   return (float*) readchars_file2(fn, fA*(*D)*sizeof(float), &nbytesread);
}

int *readtokens_file(char *fn, int fA, int fB, int *N) 
{
   int nbytes = lenchars_file(fn);
   int nframes = nbytes/sizeof(int);

   if ( fA == -1 ) fA = 0;
   if ( fA >= nframes ) {
      fatal("ERROR: fA is larger than total number of frames\n");
   }

   if ( fB == -1 ) fB = nframes-1;
   if ( fB < fA ) {
      fatal("ERROR: fB is less than fA\n");
   }
      
   *N = fB - fA + 1;
   //fprintf(stderr,"nreq = %d\n",*N);
   int nbytesread = (*N)*sizeof(int); 

   return (int*) readchars_file(fn, fA*sizeof(int), &nbytesread);
}

static int malloc_count = 0;

void *MALLOC(size_t sz)
{
   void *ptr = malloc(sz);

   if(NULL == ptr) fatal("malloc failed\n");
   else malloc_count++;

   return ptr;
}

void FREE(void *ptr)
{
   if (NULL == ptr) fatal("Attempt to free NULL pointer\n");
   else free(ptr);

   malloc_count--;
   return;
}

int get_malloc_count()
{
   return malloc_count;
}

long primep(long base)
{
  long end = sqrt((double)base);
  for (long n = 2; n<end; n++)
    if (base % n == 0) return 0;
  return 1;
}
	
long closest_prime(long base)
{
  for(;;base++)
    if(primep(base))
      return base;
}

int min( int a, int b)
{
   return (a < b) ? a : b;
}

int max(int a, int b)
{
   return (a > b) ? a : b;
}

void fatal(char * msg)
{
   fprintf(stderr, "%s\n", msg);
   exit(2);
}


void threshold_vector(float *vec, int n, double T)
{
  float *end = vec + n;
  for( ; vec < end; vec++)
    *vec = *vec > T;
}

static double lasttime = -1;

void tic( void )
{
   struct timeval time;
   gettimeofday(&time, NULL);
   lasttime = time.tv_sec + ((double)time.tv_usec)/1000000.0;
}

float toc( void )
{
   struct timeval time;
   gettimeofday(&time, NULL);
   double newtime = time.tv_sec + ((double)time.tv_usec)/1000000.0;

   if ( lasttime == -1 ) 
      lasttime = newtime;

   return (float)(newtime - lasttime);
}

int file_line_count( char * file )
{
   int ch = 0;
   int linecnt = 0;
   FILE *fptr = fopen(file, "r");
  if(!fptr) {
    fprintf(stderr, "fn = %s\n", file);
    fatal("file_line_count: open failed");
  }

   while( (ch = fgetc(fptr)) != EOF ) 
      if ( ch == '\n' ) linecnt++;
   fclose(fptr);
   return linecnt;
}
