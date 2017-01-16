/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      get_data(): offline input routine, accepting     *
 *                              binary shorts, ascii, MAT or esps files; *
 *                              calls get_ascdata, get_bindata,          *
 *                              get_matdata or esps library routines     *
 *                                                                       *
 *                      get_ascdata(): offline input routine, reads      *
 *                              ascii and allocates as it goes           *
 *                                                                       *
 *                      get_bindata(): offline input routine, reads      *
 *                              binary shorts and allocates as it goes   *
 *                                                                       *
 *                      get_online_bindata(): online input routine,      *
 *                              reads binary shorts into rest of         *
 *                              analysis                                 *
 *                                                                       *
 *                      open_out(): opens non-esps output file for       *
 *                               writing                                 *
 *                                                                       *
 *                      write_out(): writes output file; calls esps      *
 *                               library routines or print_vec()         *
 *                               or bin_vec()                            *
 *                                                                       *
 *                      print_vec(): offline output routine, writes      *
 *                              ascii                                    *
 *                                                                       *
 *                      binout_vec(): offline output routine, writes     *
 *                              binary floats                            *
 *                                                                       *
 *                      fvec_HPfilter(): IIR highpass filter on waveform *
 *                                                                       *
 *                      load_history(): load history                     *
 *                                                                       *
 *                      save_history(): save history                     *
 *                                                                       *
 *                      get_abbotdata(): read data in Abbot wav format   *
 *                                                                       *
 *                      abbot_write_eos(): write EOS to output for Abbot *
 *                                         I/O                           *
 *                                                                       *
 *                      abbot_read(): replacement for fread() that       *
 *                                     detects input EOS symbol          *
 *                                                                       *
 ************************************************************************/

/***********************************************************************

 (C) 1995 US West and International Computer Science Institute,
     All rights reserved
 U.S. Patent Numbers 5,450,522 and 5,537,647

 Embodiments of this technology are covered by U.S. Patent
 Nos. 5,450,522 and 5,537,647.  A limited license for internal
 research and development use only is hereby granted.  All other
 rights, including all commercial exploitation, are retained unless a
 license has been granted by either US West or International Computer
 Science Institute.

***********************************************************************/

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "rasta.h"

/* replacement for fread that looks for EOS in Abbot wav file input */

static int abbot_read(struct fhistory* hptr,
                      struct param* pptr,
                      short* buf,
                      int n);

/* output data in Abbot output format */
static void abbotout_vec(const struct param* pptr, struct fvec *fptr);

#ifdef HAVE_LIBESPS
#include <esps/esps.h>
#include <esps/fea.h>
#include <esps/feasd.h>

/* pre-defined brain dead definition for ESPS libs */
int debug_level = 4;

static double Start_time = 0.0;
/* Global to this file only */
#endif

#ifdef HAVE_LIBMAT
/* MATLAB */
#include <mat.h>
#endif

#ifdef HAVE_LIBSP
#include <sp/sphere.h>
#endif

/* Output buffer for Abbot (online features) output */

static struct fvec * abbot_outbuf = NULL;

/* Here we read in ascii numbers, binary shorts, 
   or esps format files where
   blocks are allocated as necessary. */
struct fvec *get_data(struct param * pptr)
{
#ifdef HAVE_LIBESPS
  /* ESPS input vars */
  struct header *inhead;
  struct feasd *sdrec = (struct feasd *)NULL;
  struct fvec *get_espsdata( FILE *, struct fvec *, 
                            struct header * );
  int readSampfreq;
#endif

#ifdef HAVE_LIBMAT
  /* MATLAB */
  struct fvec *get_matdata( MATFile *, struct fvec *);    
  MATFile *matfptr = (MATFile *)NULL;
#endif

#ifdef HAVE_LIBSP
  /* SPHERE input vars */
  SP_FILE* spInfile;
  SP_INTEGER channel_count;
  SP_INTEGER sample_rate;
  short* spBuffer;
  short* buf_endp;
  short* bp;
  float* fp;
#endif

  /* locally called functions */
  struct fvec *get_ascdata( FILE *, struct fvec *);
  struct fvec *get_bindata( FILE *, struct fvec *, struct param *);

  /* local variables */
  struct fvec *sptr;
  FILE *fptr = (FILE *)NULL;
  char *funcname;

  funcname = "get_data";

  /* Allocate space for structure */
  sptr = (struct fvec *) malloc(sizeof(struct fvec) );
  if(sptr == (struct fvec *)NULL)
    {
      fprintf(stderr,"Unable to allocate speech structure\n");
      exit(-1);
    }
  /* so that the test at the far end can tell that it didn't get set */
  sptr->values = NULL; 
        
  if(pptr->espsin == TRUE)
    {
#ifdef HAVE_LIBESPS
      eopen("rasta_plp", pptr->infname, "r", FT_FEA, FEA_SD,
            &inhead, &fptr );

      Start_time = (double) get_genhd_val( "start_time", inhead, 0.0);

      sptr->length = inhead->common.ndrec;

      readSampfreq = (int) get_genhd_val("record_freq", 
                                         inhead,
                                         (double)(pptr->sampfreq) );
      if (readSampfreq != pptr->sampfreq)
        {
          fprintf(stderr,
                  "%s does not have the expected sample rate.\n",
                  pptr->infname);
          exit(1);
        }

      if(sptr->length >= 1) 
        {
          sptr->values 
            = (float *)calloc( sptr->length, sizeof(float));
          sdrec = allo_feasd_recs( inhead, FLOAT, sptr->length, 
                                  (char *)sptr->values, NO);
        }
      else
        {
          /* Initial buffer allocation for float speech vector */
	  sptr->length = SPEECHBUFSIZE;
          sptr->values 
            = (float *) malloc(sptr->length * sizeof(float));
          if(sptr->values == (float *)NULL)
            {
              fprintf(stderr,
                      "Can't allocate first speech buffer (ESPS)\n");
              exit(-1);
            }
        }
      if(sdrec == ( struct feasd *)NULL)
        {
          fprintf(stderr,
                  "Can't do ESPS input allocation\n");
          exit(-1);
        }
#endif
    }
  else if(pptr->matin == TRUE)
    {
#ifdef HAVE_LIBMAT
      /* open MAT-file */
      matfptr = matOpen(pptr->infname, "r");
      if(matfptr == (MATFile *)NULL)
        {
          fprintf(stderr,"Error opening %s\n", pptr->infname);
          exit(-1);
        }
      /* Initial buffer allocation for float speech vector */
      sptr->values = (float *) malloc(SPEECHBUFSIZE * sizeof(float));
      if(sptr->values == (float *)NULL)
        {
          fprintf(stderr,"Can't allocate first speech buffer (MAT)\n");
          exit(-1);
        }
#endif
    }
  else if(pptr->spherein == TRUE)
    {
#ifdef HAVE_LIBSP
      /* open input file */
      if ((spInfile = sp_open(pptr->infname, "r")) == SPNULL)
        {
          fprintf(stderr, "Cannot open %s for input.\n",
                  pptr->infname);
          exit(-1);
        }

      /* want the data to come from the SPHERE file in 2-byte, native
         endian, PCM-encoded format */
      if (sp_set_data_mode(spInfile, "DF-RAW:SE-PCM:SBF-N") != 0)
        {
          sp_print_return_status(stderr);
          sp_close(spInfile);
          exit(1);
        }

      /* Ensure the input is single-channel */
      if (sp_h_get_field(spInfile,
                         "channel_count",
                         T_INTEGER,
                         (void **) &channel_count) != 0)
        {
          fprintf(stderr,
                  "Unable to read channel_count from %s\n",
                  pptr->infname);
          sp_close(spInfile);
          exit(1);
        }
      if (channel_count != 1)
        {
          fprintf(stderr,
                  "Can only process single-channel data\n");
          sp_close(spInfile);
          exit(1);
        }

      /* Check sampling rate of the file against the expected
         sampling rate */
      if (sp_h_get_field(spInfile,
                         "sample_rate",
                         T_INTEGER,
                         (void **) &sample_rate) != 0)
        {
          fprintf(stderr,
                  "Unable to read sample_rate from %s\n",
                  pptr->infname);
          sp_close(spInfile);
          exit(1);
        }
      if (pptr->sampfreq != sample_rate)
        {
          fprintf(stderr,
                  "%s does not have the expected sample rate.\n",
                  pptr->infname);
          sp_close(spInfile);
          exit(1);
        }

      /* get the file length (in points) and allocate the input
         buffers */
      if (sp_h_get_field(spInfile,
                         "sample_count",
                         T_INTEGER,
                         (void **) &(sptr->length)) != 0)
        {
          fprintf(stderr,
                  "Unable to read sample_count from %s\n",
                  pptr->infname);
          sp_close(spInfile);
          exit(1);
        }
      if ((sptr->values =
           (float*) malloc(sptr->length * sizeof(float))) == NULL)
        {
          fprintf(stderr, "Unable to allocate speech buffer (sphere).\n");
          exit(-1);
        }
      if ((spBuffer =
           (short*) malloc(sptr->length * sizeof(short))) == NULL)
        {
          fprintf(stderr, "Unable to allocate speech buffer.\n");
          exit(-1);
        }
      buf_endp = spBuffer + sptr->length;
#endif
    }
  else 
    {
      /* Initial buffer allocation for float speech vector */
      sptr->values = (float *) malloc(SPEECHBUFSIZE * sizeof(float));
      if(sptr->values == (float *)NULL)
        {
          fprintf(stderr,"Can't allocate first speech buffer (raw)\n");
          exit(-1);
        }
      if(strcmp(pptr->infname, "-") == 0)
        {
          fptr = stdin;
        }
      else
        {
          fptr = fopen(pptr->infname, "r");
          if (fptr == (FILE *)NULL)
            {
              fprintf(stderr,"Error opening %s\n", 
                      pptr->infname);
              exit(-1);
            }
        }
    }
  if(sptr->values == (float *)NULL)
    {
      fprintf(stderr,"Can't allocate first speech buffer (common)\n");
      exit(-1);
    }


  /* here we get data */

  if(pptr->espsin == TRUE)
    {
#ifdef HAVE_LIBESPS
      if(sptr->length < 1 )
        {
          /* read and allocate a chunk at a time */
          sptr = get_espsdata( fptr, sptr, 
                              inhead );
        }
      else
        {
          /* If the length was stored, get it all. */
          (void) get_feasd_recs( sdrec, 0, sptr->length, 
                                inhead, fptr);
        }
#endif
    }
  else if(pptr->matin == TRUE)
    {
#ifdef HAVE_LIBMAT
      sptr = get_matdata(matfptr, sptr);
      matClose(matfptr);    /* SK */
#endif
    }
  else if(pptr->spherein == TRUE)
    {
#ifdef HAVE_LIBSP
      if (sp_read_data(spBuffer, sptr->length, spInfile) !=
          sptr->length) 
        {
          fprintf(stderr, "Error reading %s.\n", pptr->infname);
          exit(-1);
        }
      bp = spBuffer;
      fp = sptr->values;
      while (bp != buf_endp) { *fp++ = (float) *bp++; }
      sp_close(spInfile);    /* SK */
      free(spBuffer);        /* SK */
#endif
    }
  else if(pptr->ascin == TRUE)
    {
      sptr = get_ascdata(fptr, sptr);
      fclose(fptr);    /* SK */
    }
  else
    {
      sptr = get_bindata(fptr, sptr, pptr);
      fclose(fptr);    /* SK */
    }

  return(sptr);
}

/* reads in ascii data into speech structure array, storing
   length information and allocating memory as necessary */
struct fvec *get_ascdata( FILE *fptr, struct fvec *sptr)
{
  int i, nread;
  char *funcname;
  int buflength = SPEECHBUFSIZE;

  funcname = "get_ascdata";

  sptr->values = (float *)realloc((char*)sptr->values, 
                                  buflength * sizeof(float) );
  if(sptr->values == (float *)NULL)
    {
      fprintf(stderr,
              "Unable to allocate %ld bytes for speech buffer\n",
              buflength * sizeof(float));
      exit(-1);
    }
  i = 0;
  while( (nread = fscanf(fptr, "%f", &(sptr->values[i])) )  == 1)
    {
      i++;                      /* On to the next sample */
      if(i >= buflength)
        {
          buflength += SPEECHBUFSIZE;
          sptr->values = (float *)realloc((char*)sptr->values, 
                                          buflength * sizeof(float) );
          if(sptr->values == (float *)NULL)
            {
              fprintf(stderr,"Can't allocate %ld byte buffer\n",
                      buflength * sizeof(float));
              exit(-1);
            }
        }
    }
  sptr->length = i;             /* Final value of index is the length */
  return ( sptr);
}

static void endian_swap_short(short *sbuf, int nshorts) {
    /* Swap the bytes in a buffer full of shorts if this machine 
       is not big-endian */
    int i, x;
    for (i=0; i<nshorts; ++i) {
	x = *sbuf;
	*sbuf++ = ((x & 0xFF) << 8) + ((x >> 8) & 0xFF);
    }
}

static void endian_swap_int(int *buf, int nints) {
    /* Swap the bytes in a buffer full of ints if this machine 
       is not big-endian */
    int i, x;
    assert(sizeof(int)==4);
    for (i=0; i<nints; ++i) {
	x = *buf;
	*buf++ = ((x & 0xFFL) << 24L) + ((x & 0xFF00L) << 8L) \
	           + ((x & 0xFF0000L) >> 8L) + ((x >> 24L) & 0xFF);
    }
}

static void endian_swap_long(long *buf, int nints) {
    /* Swap the bytes in a buffer full of longs if this machine 
       is not big-endian */
    int i;
    long x;
    assert(sizeof(long)==4);
    for (i=0; i<nints; ++i) {
	x = *buf;
	*buf++ = ((x & 0xFFL) << 24L) + ((x & 0xFF00L) << 8L) \
	           + ((x & 0xFF0000L) >> 8L) + ((x >> 24L) & 0xFF);
    }
}

static void endian_swap_float(float *buf, int nints) {
    /* Swap the bytes in a buffer full of floats if this machine 
       is not big-endian */
    assert(sizeof(float)==4);
    endian_swap_int((int*)buf, nints);
}

#ifdef WORDS_BIGENDIAN
#define endian_fix_short(a,b,swap)	if(swap) endian_swap_short(a,b)
#define endian_fix_int(a,b,swap)	if(swap) endian_swap_int(a,b)
#define endian_fix_long(a,b,swap)	if(swap) endian_swap_long(a,b)
#define endian_fix_float(a,b,swap)	if(swap) endian_swap_float(a,b)
#else /* !WORDS_BIGENDIAN */
#define endian_fix_short(a,b,swap)	if(!swap) endian_swap_short(a,b)
#define endian_fix_int(a,b,swap)	if(!swap) endian_swap_int(a,b)
#define endian_fix_long(a,b,swap)	if(!swap) endian_swap_long(a,b)
#define endian_fix_float(a,b,swap)	if(!swap) endian_swap_float(a,b)
#endif /* !WORDS_BIGENDIAN */

/* reads in binary short data into speech structure array, storing
   length information and allocating memory as necessary */
struct fvec *get_bindata(FILE *fptr, struct fvec *sptr, struct param *pptr)
{
  int i, nread, start;
  char *funcname;
  int totlength = 0;
  short buf[SPEECHBUFSIZE];
        
  funcname = "get_bindata";

  nread = fread((char*)buf, sizeof(short), SPEECHBUFSIZE, fptr);
  endian_fix_short(buf, nread, pptr->inswapbytes);
  while(nread == SPEECHBUFSIZE)
    {
      start = totlength;
      totlength += SPEECHBUFSIZE;
      sptr->values = (float *)realloc((char*)sptr->values, 
                                      totlength * sizeof(float) );
      if(sptr->values == (float *)NULL)
        {
          fprintf(stderr,"Can't allocate %ld byte buffer\n",
                  totlength * sizeof(float));
          exit(-1);
        }
      for(i=0; i<SPEECHBUFSIZE; i++)
        {
          sptr->values[start+i] = (float)buf[i];
        }
      /* re-initialize the loop */
      nread = fread((char*)buf, sizeof(short), SPEECHBUFSIZE, fptr);
      endian_fix_short(buf, nread, pptr->inswapbytes);
    }
  /* now read in last bunch that are less than one buffer full */
  start = totlength;
  totlength += nread;
  sptr->values = (float *)realloc((char*)sptr->values, 
                                  totlength * sizeof(float) );
  if(sptr->values == (float *)NULL)
    {
      fprintf(stderr,"Unable to realloc %ld bytes for speech buffer\n",
              totlength * sizeof(float));
      exit(-1);
    }
  for(i=0; i<nread; i++)
    {
          sptr->values[start+i] = (float)buf[i];
    }
  sptr->length = totlength;

  return (sptr );
}

#ifdef HAVE_LIBMAT
/* reads in MATLAB data into speech structure array, storing
   length information and allocating memory as necessary */
struct fvec *get_matdata( MATFile *matfptr, struct fvec *sptr)
{
  Matrix *matm;
  double *matr;
  int totlength, i;
  char *funcname;
        
  funcname = "get_matdata";

  /* load MATLAB-matrix (one row or one column: vector) */
  matm = matGetNextMatrix(matfptr);
  if(mxGetM(matm) == 1)
    totlength = mxGetN(matm);
  else if(mxGetN(matm) == 1)
    totlength = mxGetM(matm);
  else
    {
      fprintf(stderr,"MAT-file input error: more than one row or column\n");
      exit(-1);
    }
  /* allocation for float speech vector */
  sptr->values = (float *)realloc((char*)sptr->values, 
                                  totlength * sizeof(float) );
  if(sptr->values == (float *)NULL)
    {
      fprintf(stderr,"Unable to realloc %ld bytes for speech buffer\n",
              totlength * sizeof(float));
      exit(-1);
    }
  /* get speech data from MATLAB-matrix */
  matr = mxGetPr(matm);
  for(i = 0; i<totlength; i++)
    sptr->values[i] = (float)matr[i];
  sptr->length = totlength;

  return (sptr );
}
#endif

/* reads in binary short data into speech structure array, assuming
   data coming from stdin and only reading in enough to provide a full
   frame's worth of data for the analysis. This means a full frame's
   worth for the first frame, and one analysis step's worth on
   proceeding frames.  

   This routine is very similar to fill_frame(), which can be found in
   anal.c; it is here because it does data i/o (OK, just "i" )

   Speech samples are highpass filtered on waveform if option -F is
   used */

struct fvec* 
get_online_bindata(struct fhistory *hptr, struct param *pptr)
{
  static struct fvec *outbufptr = NULL;
  static struct fvec *inbufptr;
  static struct svec *sbufptr;
  static struct svec *padbufptr = NULL;
  static struct fvec *window;
  static int nsamples = 0;
  static int nframes = 0;
  static int target_nframes = 0;
  static int padptr = 0;
  int padlen;
  int i, overlap, nread;
  char *funcname;
        
  funcname = "get_online_bindata";
        
  if(outbufptr == (struct fvec *)NULL)
    /* first frame in analysis */
    {
      outbufptr = alloc_fvec(pptr->winpts);
      inbufptr = alloc_fvec(pptr->winpts); 
      sbufptr = alloc_svec(pptr->winpts);
                
      window = get_win(pptr, outbufptr->length);

      if (pptr->padInput == TRUE)
        {
          short* rp =
            sbufptr->values + ((pptr->winpts - pptr->steppts) >> 1);
          short* padp = rp;

          nread = fread((char*) rp,
                        sizeof(short),
                        pptr->winpts -
                        ((pptr->winpts - pptr->steppts) >> 1), 
                        stdin);
	  endian_fix_short(rp, nread, pptr->inswapbytes);

          if (nread !=
              (pptr->winpts - ((pptr->winpts - pptr->steppts) >> 1)))
            {
              if (pptr->debug == TRUE)
                {
                  fprintf(stderr,"Done with online input\n");
                }
              if(pptr->history == TRUE)
                {
                  save_history( hptr, pptr );
                }
              exit(0);
            }
          nsamples += nread;
          nframes++;

          /* do padding */
          while (padp != sbufptr->values)
            {
              *--padp = *++rp;
            }
        }
      else
        {
          nread = fread((char*)sbufptr->values,
                        sizeof(short), 
                        pptr->winpts, stdin);
	  endian_fix_short(sbufptr->values, nread, pptr->inswapbytes);

          if(nread != pptr->winpts)
            {
              if (pptr->debug == TRUE)
                {
                  fprintf(stderr,"Done with online input\n");
                }
              if(pptr->history == TRUE)
                {
                  save_history( hptr, pptr );
                }
              exit(0);
            }
        }

      for(i = 0; i< pptr->winpts; i++)
          inbufptr->values[i] = (float) sbufptr->values[i];
    }
  else
    {
      if (pptr->padInput == TRUE)
        {
          if (padbufptr == (struct svec*) NULL)
            /* still working from input */
            {
              nread = fread(sbufptr->values,
                            sizeof(short),
                            pptr->steppts, stdin);
	      endian_fix_short(sbufptr->values, nread, pptr->inswapbytes);
              nsamples += nread;

              if (nread != pptr->steppts)
                /* hit the end of input.  Check if we are done */
                {
                  target_nframes = nsamples / pptr->steppts;
                  if (nframes == target_nframes)
                    {
                      if (pptr->debug == TRUE)
                        {
                          fprintf(stderr,"Done with online input\n");
                        }
                      if(pptr->history == TRUE)
                        {
                          save_history( hptr, pptr );
                        }
                      exit(0);
                    }

                  /* set up padding buffer */
                  padlen = ((pptr->winpts - pptr->steppts + 1) >> 1) -
                    (nsamples % pptr->steppts);
                  padbufptr = alloc_svec(padlen);

                  if ((nread - 1) < padbufptr->length)
                    {
                      for (i = 0; i < nread - 1; i++)
                        {
                          padbufptr->values[i] =
                            sbufptr->values[nread - i - 2];
                        }
		      for (i = nread - 1; i < padbufptr->length; i++)
			{
			  padbufptr->values[i] = (short) inbufptr->values[inbufptr->length + nread - i - 2]; 
			}
                    }
                  else
                    {
                      for (i = 0; i < padbufptr->length; i++)
                        {
                          padbufptr->values[i] =
                            sbufptr->values[nread - i - 2];
                        }
                    }

                  /* pad sbuf */
                  svec_check(funcname,
                             padbufptr,
                             padptr + pptr->steppts - nread - 1);
                  
                  for (i = nread; i < pptr->steppts; i++)
                    {
                      sbufptr->values[i] =
                        padbufptr->values[padptr++];
                    }
                }
            }
          else
            /* padding the tail */
            {
              if (nframes == target_nframes)
                {
                  if (pptr->debug == TRUE)
                    {
                      fprintf(stderr,"Done with online input\n");
                    }
                  if(pptr->history == TRUE)
                    {
                      save_history( hptr, pptr );
                    }
                  exit(0);
                }
              svec_check(funcname,
                         padbufptr,
                         padptr + pptr->steppts - 1);
              for (i = 0; i < pptr->steppts; i++)
                {
                  sbufptr->values[i] = padbufptr->values[padptr++];
                }
            }
          nframes++;
        }
      else
        {
          nread = fread(sbufptr->values,
                        sizeof(short),
                        pptr->steppts, stdin);
	  endian_fix_short(sbufptr->values, nread, pptr->inswapbytes);

          if (nread != pptr->steppts)
            {
              if (pptr->debug == TRUE)
                {
                  fprintf(stderr,"Done with online input\n");
                }
              if(pptr->history == TRUE)
                {
                  save_history( hptr, pptr );
                }
              exit(0);
            }
        }
                
      /* Shift down input values */
      for (i = pptr->steppts; i < pptr->winpts; i++)
        inbufptr->values[i - pptr->steppts] = inbufptr->values[i];

      /* new values */
      overlap = pptr->winpts - pptr->steppts;
      for (i = overlap; i < pptr->winpts; i++)
        inbufptr->values[i] = (float) sbufptr->values[i - overlap];
    }

  if (pptr->HPfilter == TRUE)
    {
      fvec_HPfilter(hptr, pptr, inbufptr);
    }

  for (i = 0; i < outbufptr->length; i++)
    {
      outbufptr->values[i] =
        window->values[i] * inbufptr->values[i];
    }

  return (outbufptr);
}

/* reads in binary short data into speech structure array, assuming
   data coming from stdin and only reading in enough to provide a full
   frame's worth of data for the analysis. This means a full frame's
   worth for the first frame, and one analysis step's worth on
   proceeding frames.  

   Uses the Abbot wav format from CUED --- multiple utterances may
   come in, separated by an EOS marker: 0x8000.  Note that any values
   of 0x8000 in the input data must be mapped to 0x8001 before rasta
   processing for this to work.

   This routine is derived from get_online_bindata, with
   specialization for the Abbot wav format. */


struct fvec* 
get_abbotdata(struct fhistory *hptr, struct param *pptr)
{
  static struct fvec *outbufptr = NULL;
  static struct fvec *inbufptr;
  static struct svec *sbufptr;
  static struct svec *padbufptr = NULL;
  static struct fvec *window;
  static int nsamples = 0;
  static int nframes = 0;
  static int target_nframes = 0;
  static int padptr = 0;
  int padlen;
  int i, overlap, nread;
  char *funcname;
        
  funcname = "get_abbotdata";
        
  if(outbufptr == (struct fvec *)NULL)
    /* first frame in analysis */
    {
      outbufptr = alloc_fvec(pptr->winpts);
      inbufptr = alloc_fvec(pptr->winpts); 
      sbufptr = alloc_svec(pptr->winpts);
                
      window = get_win(pptr, outbufptr->length);

      if (pptr->padInput == TRUE)
        {
          short* rp =
            sbufptr->values + ((pptr->winpts - pptr->steppts) >> 1);
          short* padp = rp;

          nread = abbot_read(hptr,
                             pptr,
                             rp,
                             pptr->winpts -
                             ((pptr->winpts - pptr->steppts) >> 1));

          if (nread !=
              (pptr->winpts - ((pptr->winpts - pptr->steppts) >> 1)))
            {
              if (hptr->eof == TRUE)
                {
                  if (pptr->debug == TRUE)
                    {
                      fprintf(stderr,"Done with online input\n");
                    }
                  if(pptr->history == TRUE)
                    {
                      save_history( hptr, pptr );
                    }
                  exit(0);
                }
              else if (hptr->eos == TRUE)
                {
                  return (struct fvec*) NULL;
                }
              else
                /* this should never happen */
                {
                  fprintf(stderr,
                          "rasta (%s): bug in Abbot input.\n",
                          funcname);
                  exit(1);
                }
            }

          nsamples += nread;
          nframes++;

          /* do padding */
          while (padp != sbufptr->values)
            {
              *--padp = *++rp;
            }
        }
      else
        {
          nread = abbot_read(hptr,
                             pptr,
                             sbufptr->values,
                             pptr->winpts);

          if (nread != pptr->winpts)
            {
              if (hptr->eof == TRUE)
                {
                  if (pptr->debug == TRUE)
                    {
                      fprintf(stderr,"Done with online input\n");
                    }
                  if(pptr->history == TRUE)
                    {
                      save_history( hptr, pptr );
                    }
                  exit(0);
                }
              else if (hptr->eos)
                {
                  return (struct fvec*) NULL;
                }
              else
                /* this should never happen */
                {
                  fprintf(stderr,
                          "rasta (%s): bug in Abbot input.\n",
                          funcname);
                  exit(1);
                }
            }
        }

        for(i = 0; i< pptr->winpts; i++)
          inbufptr->values[i] = (float) sbufptr->values[i];
    }
  else
    {
      if (pptr->padInput == TRUE)
        {
          if (padbufptr == (struct svec*) NULL)
            /* still working from input */
            {
              nread = abbot_read(hptr,
                                 pptr,
                                 sbufptr->values,
                                 pptr->steppts); 
              nsamples += nread;

              if (nread != pptr->steppts)
                /* hit the end of utterance.  Check if we are done */
                {
                  target_nframes = nsamples / pptr->steppts;
                  if (nframes == target_nframes)
                    {
                      if (hptr->eof == TRUE)
                        {
                          abbot_write_eos(pptr);
                          if (pptr->debug == TRUE)
                            {
                              fprintf(stderr,"Done with online input\n");
                            }
                          if(pptr->history == TRUE)
                            {
                              save_history( hptr, pptr );
                            }
                          exit(0);
                        }
                      else if (hptr->eos)
                        /* clear state and return */
                        {
                          free_fvec(outbufptr);
                          outbufptr = NULL;
                          free_fvec(inbufptr);
                          free_svec(sbufptr);
                          free_fvec(window);
                          if (padbufptr != (struct svec*) NULL)
                            {
                              free_svec(padbufptr);
                              padbufptr = (struct svec*) NULL;
                            }
                          nsamples = 0;
                          nframes = 0;
                          target_nframes = 0;
                          padptr = 0;

                          if (pptr->HPfilter == TRUE)
                            {
                              fvec_HPfilter(hptr,
                                            pptr,
                                            (struct fvec*) NULL);
                            }
                          
                          return (struct fvec*) NULL;
                        }
                      else
                        /* this should never happen */
                        {
                          fprintf(stderr,
                                  "rasta (%s): bug in Abbot input.\n",
                                  funcname);
                          exit(1);
                        }
                    }

                  /* set up padding buffer */
                  padlen = ((pptr->winpts - pptr->steppts + 1) >> 1) -
                    (nsamples % pptr->steppts);
		  if (padbufptr != (struct svec*) NULL) {
		      free_svec(padbufptr);
		      padbufptr = (struct svec*) NULL;
		  }
                  padbufptr = alloc_svec(padlen);

                  if ((nread - 1) < padbufptr->length)
                    {
		      int nrlimit = ((nread-1)<0)?0:nread-1;
                      for (i = 0; i < nrlimit; i++)
                        {
                          padbufptr->values[i] =
                            sbufptr->values[nrlimit - i - 1];
                        }
		      for (i = nrlimit; i < padbufptr->length; i++)
			{
			  padbufptr->values[i] = (short) inbufptr->values[inbufptr->length + nrlimit - i - 1]; 
			}
                    }
                  else
                    {
                      for (i = 0; i < padbufptr->length; i++)
                        {
                          padbufptr->values[i] =
                            sbufptr->values[nread - i - 2];
                        }
                    }

                  /* pad sbuf */
                  svec_check(funcname,
                             padbufptr,
                             padptr + pptr->steppts - nread - 1);
                  
                  for (i = nread; i < pptr->steppts; i++)
                    {
                      sbufptr->values[i] =
                        padbufptr->values[padptr++];
                    }

                  hptr->eos = FALSE;
                }
            }
          else
            /* padding the tail */
            {
              if (nframes == target_nframes)
                {
                  if (hptr->eof == TRUE)
                    {
                      abbot_write_eos(pptr);
                      if (pptr->debug == TRUE)
                        {
                          fprintf(stderr,"Done with online input\n");
                        }
                      if(pptr->history == TRUE)
                        {
                          save_history( hptr, pptr );
                        }
                      exit(0);
                    }
                  else
                    /* clear state and return */
                    {

                      hptr->eos = TRUE;

                      free_fvec(outbufptr);
                      outbufptr = NULL;
                      free_fvec(inbufptr);
                      free_svec(sbufptr);
                      free_fvec(window);
                      if (padbufptr != (struct svec*) NULL)
                        {
                          free_svec(padbufptr);
                          padbufptr = (struct svec*) NULL;
                        }

                      nsamples = 0;
                      nframes = 0;
                      target_nframes = 0;
                      padptr = 0;

                      if (pptr->HPfilter == TRUE)
                        {
                          fvec_HPfilter(hptr,
                                        pptr,
                                        (struct fvec*) NULL);
                        }
                          
                      return (struct fvec*) NULL;
                    }
                }

              svec_check(funcname,
                         padbufptr,
                         padptr + pptr->steppts - 1);
              for (i = 0; i < pptr->steppts; i++)
                {
                  sbufptr->values[i] = padbufptr->values[padptr++];
                }
            }
          nframes++;
        }
      else
        {
          nread = abbot_read(hptr,
                             pptr,
                             sbufptr->values,
                             pptr->steppts);

          if (nread != pptr->steppts)
            {
              if (hptr->eof == TRUE)
                {
                  abbot_write_eos(pptr);
                  if (pptr->debug == TRUE)
                    {
                      fprintf(stderr,"Done with online input\n");
                    }
                  if(pptr->history == TRUE)
                    {
                      save_history( hptr, pptr );
                    }
                  exit(0);
                }
              else if (hptr->eos)
                /* clear state and return */
                {
                  free_fvec(outbufptr);
                  outbufptr = NULL;
                  free_fvec(inbufptr);
                  free_svec(sbufptr);
                  free_fvec(window);
                  if (padbufptr != (struct svec*) NULL)
                    {
                      free_svec(padbufptr);
                      padbufptr = (struct svec*) NULL;
                    }
                  nsamples = 0;
                  nframes = 0;
                  target_nframes = 0;
                  padptr = 0;

                  if (pptr->HPfilter == TRUE)
                    {
                      fvec_HPfilter(hptr,
                                    pptr,
                                    (struct fvec*) NULL);
                    }
                          
                  return (struct fvec*) NULL;
                }
              else
                /* this should never happen */
                {
                  fprintf(stderr,
                          "rasta (%s): bug in Abbot input.\n",
                          funcname);
                  exit(1);
                }
            }
        }

      /* Paranoia: shouldn't be able to get this far with
         hptr->eos == TRUE */
      if (hptr->eos == TRUE)
        {
          fprintf(stderr,
                  "rasta (%s): bug in Abbot input.\n",
                  funcname);
          exit(1);
        }

      /* Shift down input values */
      for (i = pptr->steppts; i < pptr->winpts; i++)
        inbufptr->values[i - pptr->steppts] = inbufptr->values[i];

      /* new values */
      overlap = pptr->winpts - pptr->steppts;
      for (i = overlap; i < pptr->winpts; i++)
	inbufptr->values[i] = (float) sbufptr->values[i - overlap];
    }

  if (pptr->HPfilter == TRUE)
    {
      fvec_HPfilter(hptr, pptr, inbufptr);
    }

  for (i = 0; i < outbufptr->length; i++)
    {
      outbufptr->values[i] =
        window->values[i] * inbufptr->values[i];
    }

  return (outbufptr);
}

/* read in Abbot wav input, looking for EOS */

static int
abbot_read(struct fhistory* hptr, 
           struct param* pptr,
           short* buf,
           int n)
{
  int nread;
  int t;
  unsigned short eos = 0x8000;
  unsigned short* p = (unsigned short*) buf;

  for (nread = 0; nread < n; nread++)
    {
      t = fread((char*) p, sizeof(short), 1, stdin);
      endian_fix_short((short *)p, t, pptr->inswapbytes);
      if (t == 0)
        {
          hptr->eof = TRUE;
          break;
        }
      if (*p == eos)
        {
          hptr->eos = TRUE;
          break;
        }
      p++;
    }
  return nread;
}

#ifdef HAVE_LIBESPS
/* reads in esps data into speech structure array, storing
        length information and allocating memory as necessary */
struct fvec *get_espsdata( FILE *fptr, struct fvec *sptr, 
        struct header *ihd )
{
        int i, nread;
        char *funcname;
        struct feasd *sdrec;
        float ftmp[SPEECHBUFSIZE]; 
        int start = 0;
        
        funcname = "get_espsdata";

        sptr->length = SPEECHBUFSIZE;
        sdrec = allo_feasd_recs( ihd, FLOAT, SPEECHBUFSIZE,
                (char *)ftmp, NO);

        /* length starts as what we have allocated space for */
        while( (nread = get_feasd_recs(sdrec, 0L, SPEECHBUFSIZE, 
                        ihd, fptr)) == SPEECHBUFSIZE)
        {

                for(i=0; i<SPEECHBUFSIZE; i++)
                {
                        sptr->values[start+i] = ftmp[i];
                }
                /* increment to next size we will need */
                sptr->length += SPEECHBUFSIZE;
                start += SPEECHBUFSIZE;

                sptr->values = (float *)realloc((char*)sptr->values, 
                          (unsigned)(sptr->length * sizeof(float)) );
                if(sptr->values == (float *)NULL)
                {
                        fprintf(stderr,"Can't allocate %ld byte buffer\n",
                                sptr->length * sizeof(float));
                        exit(-1);
                      }
                
              }
        
        /* now adjust length for last bunch that is less than one buffer full */
        
        sptr->length -= SPEECHBUFSIZE;
        sptr->length += nread;
        for(i=0; i<nread; i++)
          {
            sptr->values[start+i] = ftmp[i];
          }
        
        return (sptr );
      }
#endif

/* Opens output file unless writing ESPS file; in that case,
   file is opened when write_out is first called */
FILE *open_out(struct param *pptr)
{
  FILE *fptr;

  if(((pptr->espsout) == TRUE) || ((pptr->matout) == TRUE))
    {
      fptr = (FILE *)NULL;
    }
  else if(strcmp(pptr->outfname, "-") == 0)
    {
      fptr = stdout;
    }
  else
    {
      fptr = fopen(pptr->outfname, "w");
      if (fptr == (FILE *)NULL)
        {
          fprintf(stderr,"Error opening %s\n", pptr->outfname);
          exit(-1);
        }
      pptr->BOF = TRUE;  /* SK Set Beginning Of File (BOF) flag
			    to inform write_out that it has to output
			    a file header in HTK output mode.*/
    }
  return (fptr);
}

/* General calling routine to write out a vector */
void write_out( struct param *pptr, FILE *outfp, struct fvec *outvec )
{
#ifdef HAVE_LIBESPS
  /* ESPS variables */
  static struct header *outhead = NULL;
  static struct fea_data *outrec;
  static float *opdata;
  static double rec_freq;

  /* Ignore outfp if an ESPS file; file is opened an
     written to within this routine only. */
  static FILE *esps_fp;
#endif

#ifdef HAVE_LIBMAT
  /* MATLAB variables */
  static MATFile *matfptr;
  static double *matr;
  static double *matr_start = (double *)NULL;
  static double *matr_end;
#endif

  /* BEGIN SK */
  typedef struct {              /* HTK File Header */
    long nSamples;
    long sampPeriod;
    short sampSize;
    short sampKind;
  } HTKhdr;

  HTKhdr hdr_out;
  static int frame_counter;
  /* END SK */

  int i;
  char *funcname;
  funcname = "write_out";

  if((pptr->espsout)   == TRUE)
    {
#ifdef HAVE_LIBESPS
      if(outhead == (struct header *)NULL)
        {
          rec_freq = 1.0 / (pptr->stepsize * .001);
          eopen("rasta_plp", pptr->outfname, "w", NONE, NONE,
                (struct header **)NULL, &esps_fp);              
          outhead = new_header(FT_FEA);
          add_fea_fld( "rasta_plp", (long)outvec->length, 
                      1, NULL, FLOAT, NULL, outhead);
          *add_genhd_d("start_time", NULL,1,outhead) = Start_time;
          *add_genhd_d("record_freq", NULL,1,outhead) = rec_freq;
          write_header( outhead, esps_fp);
          outrec = allo_fea_rec( outhead );
          opdata = (float *)get_fea_ptr( outrec, "rasta_plp", 
                                        outhead);
                          
          /* ESPS seems to handle its own error
             checking (pretty much) */
        }
      for(i=0; i<outvec->length; i++)
        {
          opdata[i] = outvec->values[i];
        }
      put_fea_rec(outrec, outhead, esps_fp);
#endif
    }
  else if((pptr->matout) == TRUE )
    {
#ifdef HAVE_LIBMAT
      if(matr_start == (double *)NULL)
        {
          matr = (double *) malloc((pptr->nframes * outvec->length) * sizeof(double));
          if(matr == (double *)NULL)
            {
              fprintf(stderr,"Can't allocate output buffer\n");
              exit(-1);
            }
          matr_start = matr;
          matr_end   = matr + (pptr->nframes * outvec->length);
        }
      for(i=0; i<outvec->length; i++, matr++)
        {
          *matr = (double)outvec->values[i];
        }       
      if( matr == matr_end )
        {
          /* open MAT-file */
          matfptr = matOpen(pptr->outfname,"w");
          if(matfptr == (MATFile *)NULL)
            {
              fprintf(stderr,"Error opening %s\n", pptr->outfname);
              exit(-1);
            }
          /* save output as MAT-file */
          if(matPutFull(matfptr, "rasta_out", outvec->length, pptr->nframes, matr_start, NULL) != 0)
            {
              fprintf(stderr, "Error saving %s\n", pptr->outfname);
              exit(-1);
            }                               
          matClose(matfptr);
          mxFree;
	  matr_start = NULL; /* SK */
        }
#endif
    }
  else if ((pptr->ascout == TRUE))
    {
	/* 1998feb17: used to automatically invoke this with
           (pptr->crbout == TRUE) || (pptr->comcrbout == TRUE); 
	   not any more! */
      print_vec(pptr, outfp, outvec, outvec->length);
    }
  else if (pptr->abbotIO == TRUE)
    {
      abbotout_vec(pptr, outvec);
    }
  /****** BEGIN SK *****/
  else if (pptr->htkout == TRUE)
    {
      if ( pptr->BOF == TRUE ) /* If beginning of output file...*/
	{
	  /* make and write header */
	  hdr_out.nSamples = (long)pptr->nframes;  /* nr of output frames */
	  hdr_out.sampPeriod = (long)pptr->stepsize*10000; /* in 100ns HTK units */
	  hdr_out.sampSize = (short)(pptr->nout*4); /* nr of bytes/frame */
	  hdr_out.sampKind = 9; /* USER feature type */

	  endian_fix_long(&hdr_out.nSamples, 2, pptr->outswapbytes);
	  endian_fix_short(&hdr_out.sampSize, 2, pptr->outswapbytes);
	  if ( !fwrite( &hdr_out , sizeof(HTKhdr), 1 , outfp) )
	    {
	      fprintf(stderr, "rasta (%s): error writing HTK header.\n",
		      funcname);
	      exit(1);
	    }
	  pptr->BOF = FALSE;

	  /* undo swapping */
	  endian_fix_long(&hdr_out.nSamples, 2, pptr->outswapbytes);
	  endian_fix_short(&hdr_out.sampSize, 2, pptr->outswapbytes);

	}

      binout_vec( pptr, outfp, outvec );

    }
  /****** END SK ******/
  else
    {
      binout_vec( pptr, outfp, outvec );
    }
}

/* Print out ascii for float vector with specified width (n columns) */
void print_vec(const struct param *pptr, FILE *fp, struct fvec *fptr, int width)
{
  int i;
  char *funcname = "print_vec";

  for(i=0; i<fptr->length; i++) {
      fprintf(fp, "%g ", fptr->values[i]);
      if((i+1)%width == 0) {
	  fprintf(fp, "\n");
      }
  }
}

/* Swap the bytes in a floating point value */
float
swapbytes_float(float f)
{
    union {
        float f;
        char b[sizeof(float)];
    } in, out;
    int i;

    in.f = f;
    for (i=0; i<sizeof(float); i++)
    {
        out.b[i] = in.b[sizeof(float)-i-1];
    }
    return out.f;
}

/* Send float vector values to output stream */
void binout_vec( const struct param* pptr,
                 FILE *fp,
                 struct fvec *fptr )
{
  char *funcname;
  int count;
  float* buf;
  float* buf_endp;
  float* sp = fptr->values;
  float* dp;

  funcname = "binout_vec";

  endian_fix_float(fptr->values, fptr->length, pptr->outswapbytes);
  count = fwrite((char*) fptr->values,
		 sizeof(float),
		 fptr->length,
		 fp);

  if (count != fptr->length)
    {
      fprintf(stderr, "rasta (%s): error writing output.\n",
              funcname);
      exit(1);
    }
  /* restore any swapping */
  endian_fix_float(fptr->values, fptr->length, pptr->outswapbytes);
}

/* Send float vector values to output stream in Abbot format */
static void
abbotout_vec(const struct param* pptr,
             struct fvec *fptr)
{
  char *funcname;
  int count;
  const unsigned char eos = 0x00;
  float* buf_endp;
  float* sp = fptr->values;
  float* dp;

  funcname = "abbotout_vec";

  if (abbot_outbuf == NULL)
    /* first frame in utterance.  Allocate the output buffer. */
    {
      if ((abbot_outbuf = alloc_fvec(fptr->length)) == NULL)
        {
          fprintf(stderr, "rasta (%s): out of heap space.\n",
                  funcname);
          exit(1);
        }
    }
  else
    /* write out last frame */
    {
      if (fwrite((char*) &eos, 1, 1, stdout) != 1)
        {
          fprintf(stderr, "rasta (%s): error writing output.\n",
                  funcname);
          exit(1);
        }

      endian_fix_float(abbot_outbuf->values, abbot_outbuf->length, pptr->outswapbytes);
      count = fwrite((char*) abbot_outbuf->values,
                     sizeof(float),
                     abbot_outbuf->length,
                     stdout);
      endian_fix_float(abbot_outbuf->values, abbot_outbuf->length, pptr->outswapbytes);

      if (count != abbot_outbuf->length)
        {
          fprintf(stderr, "rasta (%s): error writing output.\n",
                  funcname);
          exit(1);
        }
    }

  /* fill buffer with next frame */
  if(abbot_outbuf->length != fptr->length)
      {
          fprintf(stderr, "rasta (%s): vector changed size unexpectedly.\n",
                  funcname);
          exit(1);
      }

  buf_endp = abbot_outbuf->values + fptr->length;
      for (dp = abbot_outbuf->values; dp != buf_endp; dp++, sp++)
        {
          *dp = *sp;
        }
}

/* Write an EOS record to the output for Abbot I/O */

void
abbot_write_eos(struct param* pptr)
{
  int count;
  const unsigned char eos = 0x80;
  char* funcname = "abbot_write_eos";

  if (abbot_outbuf == NULL) {
      fprintf(stderr, "rasta (%s): attempt to write EOS on empty seg\n",
	      funcname);
      exit(1);
  }

  if (fwrite((char*) &eos, 1, 1, stdout) != 1)
    {
      fprintf(stderr, "rasta (%s): error writing output.\n",
              funcname);
      exit(1);
    }

  endian_fix_float(abbot_outbuf->values, abbot_outbuf->length, pptr->outswapbytes);
  count = fwrite((char*) abbot_outbuf->values,
                 sizeof(float),
                 abbot_outbuf->length,
                 stdout);
  endian_fix_float(abbot_outbuf->values, abbot_outbuf->length, pptr->outswapbytes);

  if (count != abbot_outbuf->length)
    {
      fprintf(stderr, "rasta (%s): error writing output.\n",
              funcname);
      exit(1);
    }

  free_fvec(abbot_outbuf);
  abbot_outbuf = NULL;
  if (fflush(stdout))
    {
      fprintf(stderr, "rasta (%s): error flushing output.\n",
              funcname);
      exit(1);
    }
}

/* digital IIR highpass filter on waveform to remove DC offset
   H(z) = (0.993076-0.993076*pow(z,-1))/(1-0.986152*pow(z,-1)) 
   offline and online version, inplace calculating */
void
fvec_HPfilter(struct fhistory* hptr,
              struct param *pptr,
              struct fvec *fptr)
{
  static double coefB,coefA;
  double d,c,p2;
  static int first_call = 1;
  static float old;
  static int start;
  float tmp;
  int i;
        
  if (hptr->eos == TRUE)
    {
      first_call = 1;
    }
  else
    {
      if (first_call)
        {
          /* computing filter coefficients */
          d = pow(10.,-3./10.);     /* 3dB passband attenuation at frequency f_p */
          c = cos(2.*M_PI* pptr->hpf_fc /(double)pptr->sampfreq); /* frequency f_p about 45 Hz */
          p2 = (1.-c+2.*d*c)/(1.-c-2.*d);
          coefA = floor((-p2-sqrt(p2*p2-1.))*1000000.) / 1000000.; /* to conform to */
          coefB = floor((1.+coefA)*500000.) / 1000000.; /* older version */

          start = 1;
          old = fptr->values[0];
        }
        
      for(i=start; i<fptr->length; i++)
        {
          tmp = fptr->values[i];
          fptr->values[i] =  coefB * (fptr->values[i] - old) + coefA * fptr->values[i-1];
          old = tmp;
        }

      if(first_call)
        {
          start = fptr->length - pptr->steppts;
          if(start < 1)
            {
              fprintf(stderr,"step size >= window size -> online highpass filter not available");
              exit(-1);
            }
          first_call = 0;
        }
    }
}

/* load history from file */
void load_history(struct fhistory *hptr, const struct param *pptr)
{
  int head[3];
  int i;
  FILE *fptr;
        

  /* open file */
  fptr = fopen(pptr->hist_fname,"r");
  if(fptr == (FILE *)NULL)
    {
      fprintf(stderr,"Warning: Cannot open %s, using normal initialization\n", pptr->hist_fname);
      return;
    }

  /* read header */
  if(fread(head,sizeof(int),3,fptr) != 3)
    {
      fprintf(stderr,"Warning: Cannot read history file header, using normal initialization\n");
      return;
    }
  endian_fix_int(head, 3, 0);

  if((head[0] != pptr->nfilts) || (head[1] != FIR_COEF_NUM) || (head[2] != IIR_COEF_NUM))
    {
       fprintf(stderr,"Warning: History file is incompatible (header %d %d %d is not %d %d %d), using normal initialization\n", head[0], head[1], head[2], pptr->nfilts, FIR_COEF_NUM, IIR_COEF_NUM);
      return;
    }

  /* read stored noise estimation level */
  if(fread(hptr->noiseOLD, sizeof(float), head[0], fptr) != head[0])
    {
      fprintf(stderr,"Warning: History file failure, using normal initialization\n");
      return;
    }
  endian_fix_float(hptr->noiseOLD, head[0], 0);
  /* read stored RASTA filter input buffer */
  for(i=0; i<head[0]; i++)
    {
      if(fread(hptr->filtIN[i], sizeof(float), head[1], fptr) != head[1])
        {
          fprintf(stderr,"Warning: History file failure, using normal initialization\n");
          return;
        }
      endian_fix_float(hptr->filtIN[i], head[1], 0);
    }
  /* read stored RASTA filter input buffer */
  for(i=0; i<head[0]; i++)
    {
      if(fread(hptr->filtOUT[i], sizeof(float), head[2], fptr) != head[2])
        {
          fprintf(stderr,"Warning: History file failure, using normal initialization\n");
          return;
        }
      endian_fix_float(hptr->filtOUT[i], head[2], 0);
    }
        
  /* use history values instead of normal initialization */
  hptr->normInit = FALSE;
        
  fclose(fptr);
}


/* save history to file */
void save_history(struct fhistory *hptr, const struct param *pptr)
{
  int head[3];
  int i;
  FILE *fptr;
        

  /* open file */
  fptr = fopen(pptr->hist_fname,"w");
  if(fptr == (FILE *)NULL)
    {
      fprintf(stderr,
              "Warning: cannot open %s for writing.\n",
              pptr->hist_fname);
      return;
    }

  /* write header */
  head[0] = pptr->nfilts;
  head[1] = FIR_COEF_NUM;
  head[2] = IIR_COEF_NUM;
  endian_fix_int(head, 3, 0);
  fwrite(head, sizeof(int), 3, fptr);
  endian_fix_int(head, 3, 0);
        
  /* write noise level estimation */
  endian_fix_float(hptr->noiseOLD, head[0], 0);
  fwrite(hptr->noiseOLD, sizeof(float), head[0], fptr);
  endian_fix_float(hptr->noiseOLD, head[0], 0);

  /* write RASTA filter input buffer */
  for(i=0; i<head[0]; i++) {
    endian_fix_float(hptr->filtIN[i], head[1], 0);
    fwrite(hptr->filtIN[i], sizeof(float), head[1], fptr);
    endian_fix_float(hptr->filtIN[i], head[1], 0);
  }

  /* write RASTA filter output buffer */
  for(i=0; i<head[0]; i++) {
    endian_fix_float(hptr->filtOUT[i], head[2], 0);
    fwrite(hptr->filtOUT[i], sizeof(float), head[2], fptr);
    endian_fix_float(hptr->filtOUT[i], head[2], 0);
  }
  fclose(fptr);
}
