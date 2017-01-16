/****************************************************************************
*                                                                           *
*    ROUTINES IN THIS FILE:                                                 *
*                                                                           *
*        comp_noisepower() : computes the noise power of a speech signal    *
*        creat_filt() : find the coefficients of the filter for high        *
*                       energy speech detection                             *
*        det() :      for detecting high energy speech                      * 
*                     
****************************************************************************/

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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rasta.h"

/* comp_noisepower
  
   This routine computes the noise power of a speech signal. In the 
   first 100ms, it is assumed that the first imcoming speech samples
   of a recording are related to the noise only. Thus the average
   signal energy is used as the first estimation of the noise
   power. After 200ms, the noise level in a certain subband is
   estimated by a statistical analysis of a segment of the magnitude
   spectral envelope ("Estimation of Noise Spectrum and its
   Application to SNR-Estimation and Speech Enhancement" by H. Guenter
   Hirsch).  Given a spectral envelope and the corresponding
   distribution density function in a certain subband, the most
   frequently occurring spectral magnitude value is taken as an
   estimation for the noise level inside this band. These noise levels
   for different subbands are squared and then the average of these
   squared values gives the noise power estimate. The noise power is
   computed using this histogram method every 100ms. In Hirsch's
   paper, FFT subbands were used for the analysis. In this routine, we
   have chosen to use critical subbands for the analysis. Some
   modifications are required. Sometimes the most frequently occurring
   value has an unrealistically  high value. A simple detector is used
   to exclude some of the unrealistically high values due to speech
   from the histograms. */

/* local function prototypes */
void create_filt(float,  int , float , struct fvec *);
int det(struct fmat *, int ,  struct fvec *, struct fvec *);   


struct fvec*
comp_noisepower(struct fhistory *hptr,
                const struct param *pptr,
                struct fvec *aspectrum)
{
  char *funcname;
  static int i_call = 0;                     /*  Frame number */

  static struct fvec *noiseptr = NULL;       /* the noise power */

  static struct fvec *max = NULL;            /* Most frequently
                                                occurring value in a
                                                subband */

  static struct svec *new_max = NULL;        /*  Flag = 1 if current
                                                 spectral magnitude in
                                                 a subband is a new
                                                 maximum */

  static struct svec *new_max_index = NULL;  /* index of the current
                                                maximum */

  static struct fvec *noise_level_lt = NULL; /* estimated noise level
                                                in a subband */ 

  static struct fvec *waste_trajec = NULL;   /* oldest value in the
                                                histogram are 
                                                thrown away  */ 

  static struct fmat *all_trajec = NULL;     /* matrix containing all
                                                trajectories */ 

  static struct fmat *histo_values = NULL;   /* matrix containing the
                                                entries for the
                                                histograms for each
                                                band */

  static struct svec *histo_index = NULL;    /* index for the entries
                                                of the histograms */

  static int stat_lt[MAXFILTS][HISTO_RESOLUTION];  /* the histograms,
                                                      the first index
                                                      is the subband
                                                      number, the
                                                      second index is
                                                      the histogram
                                                      bin number */

  static struct fvec *filter = NULL;         /* filter coefficients
                                                for high energy speech
                                                detector  */ 

  static struct fmat *buffer = NULL;         /* buffer for the high
                                                energy speech
                                                detector */
  int i,j,k;
  int t;
  static int m1;
  int lastfilt;
  float average, sum = 0.0;
  static int forget_frame = 1;     /* forget first frame (building-up
                                      of highpass filter) */ 

  static float powerOLD;           /* noise power of history file */

  static float noisepower;         /* the estimated noise power */

  static int histo_length;         /* number of entries in each
                                      histogram */

  static int histo_step_framesize; /* The histograms are re-evaluated
                                      every histo_step_framesize */  
  int mmax_lp;     
  int mindex = 0;
  int length;    
  float mmax;     
  int mmax_index;  
  float average1;  
	
  static FILE *outdebug;

  funcname = "comp_noisepower";

  if (hptr->eos == TRUE)
    {
      i_call = 0;
      forget_frame = 1;
      free(max);
      max = (struct fvec*) NULL;
      free(new_max);
      new_max = (struct svec*) NULL;
      free(new_max_index);
      new_max_index = (struct svec*) NULL;
      free(noise_level_lt);
      noise_level_lt = (struct fvec*) NULL;
      free(waste_trajec);
      waste_trajec = (struct fvec*) NULL;
      free(all_trajec);
      all_trajec = (struct fmat*) NULL;
      free(histo_values);
      histo_values = (struct fmat*) NULL;
      free(histo_index);
      histo_index = (struct svec*) NULL;
      free(filter);
      filter = (struct fvec*) NULL;
      free(buffer);
      buffer = (struct fmat*) NULL;
      return (struct fvec*) NULL;
    }
  else
    {
      if (noiseptr == (struct fvec *)NULL)
        {
          noiseptr = alloc_fvec(1);
        }
	
      if(forget_frame == 1)
        {
          forget_frame = 0;
          if(pptr->HPfilter == 1) /* forget first frame */
            {
              noiseptr->values[0] = 1/(C_CONSTANT * 1.0e-7);
              return(noiseptr);
            }
        } 
	
      i_call++;
      lastfilt = pptr->nfilts - pptr->first_good; 
	
      if ((i_call == 1) && (pptr->debug==TRUE))
        {
          if ((outdebug = fopen("rasta.debug","wt")) == NULL)
            {
              fprintf(stderr,"Cannot open output file rasta.debug\n");
              exit(-1);
            }
        }
  
      /* Initialization before the first frame */
      if (i_call  == 1)
        {
          histo_length = HISTO_TIME/(pptr->stepsize);  
          histo_step_framesize = HISTO_STEPTIME/(pptr->stepsize);  
          max = alloc_fvec(pptr->nfilts);
          new_max = alloc_svec(pptr->nfilts);
          new_max_index = alloc_svec(pptr->nfilts);
          noise_level_lt = alloc_fvec(pptr->nfilts);
          waste_trajec = alloc_fvec(pptr->nfilts);
          all_trajec = alloc_fmat(pptr->nfilts, histo_length);
          histo_values =  alloc_fmat(pptr->nfilts, histo_length);
          histo_index = alloc_svec(pptr->nfilts);
          filter = alloc_fvec(FILT_LENGTH);
          buffer = alloc_fmat(pptr->nfilts, FILT_LENGTH);

          /* Find coefficients of the filter for the high energy speech
             detector */

          create_filt(OVER_EST_FACTOR,
                      FILT_LENGTH,
                      TIME_CONSTANT,
                      filter);

          for (i = 0; i < pptr->nfilts; i++)
            {
              max->values[i] = 0;
              new_max->values[i] = RESET;
              new_max_index->values[i] = 0;
              noise_level_lt->values[i] = 0;
              histo_index->values[i] = -1;
              for (j=0; j<histo_length; j++)
                {
                  all_trajec->values[i][j] = 0;
                  histo_values->values[i][j] = 0;
                }
            }
		
          if(hptr->normInit == FALSE)
            {
              for(i=pptr->first_good, powerOLD = 0.0; i<lastfilt; i++)
                powerOLD += (hptr->noiseOLD[i] * hptr->noiseOLD[i]);
              powerOLD /= (float)(lastfilt - (pptr->first_good));
            }
        }
	
      t = ((i_call - 1) % histo_length);  
	
  /* copy the spectral magnitudes in the matrix all_trajec */

      if((i_call <= histo_step_framesize) && (hptr->normInit == FALSE))
        {
          for (i=pptr->first_good, sum=0.0; i<lastfilt; i++)
            sum += aspectrum->values[i];
          sum /= (float)(lastfilt - (pptr->first_good));
          if(sum <= powerOLD)
            {
              for (i=pptr->first_good; i<lastfilt; i++)
                all_trajec->values[i][t] = (float)sqrt((double)aspectrum->values[i]);
            }
          else
            {
              for (i=pptr->first_good; i<lastfilt; i++)
                all_trajec->values[i][t] =	hptr->noiseOLD[i];
            }
        }
      else
        {
          for (i=pptr->first_good; i<lastfilt; i++)
            all_trajec->values[i][t] = (float)sqrt((double)aspectrum->values[i]);
        }
	
      /* A simple detector is used in each subband. When the value
     returned by the subroutine det is "1" for a certain subband, the
     spectral magnitude in that subband is not entered into the
     histogram. Otherwise the spectral magnitude is entered into the
     matrix histo_values and histo_index is incremented.  For high
     energy speech detection, there is a FIR filter for each
     subband. The filter for each subband is identical.  The buffers
     of the filters for high energy speech detection are initialized
     with spectral magnitude values. The first spectral magnitude
     value of each subband is assigned to the
     "histo_step_framesize"-th element (f-th) of the corresponding
     buffer. The 2rd to f-th values are assigned to the (f-1)-th to
     1st elements of the buffer. The rest of the elements are
     initialized with the average spectral magnitude values */
	
      if (i_call <= histo_step_framesize)
        {
          for(i = pptr->first_good; i<lastfilt; i++)
            {
              buffer->values[i][histo_step_framesize - i_call] =
                all_trajec->values[i][t]; 
              histo_values->values[i][t] = all_trajec->values[i][t];
              histo_index->values[i] ++;
            }
        }
	
      if (i_call == histo_step_framesize)
        {
          for (i= pptr->first_good; i<lastfilt; i++)
            {
              mmax = 0;
              mmax_index = 0;
              for (j= 0; j< histo_step_framesize; j++)
                {
                  if (buffer->values[i][j] > mmax)
                    {
                      mmax = buffer->values[i][j];
                      mmax_index = j;
                    }
                }
              average1 = 0.;
              for (j = 0; j < histo_step_framesize; j++)
                {
                  if (j != mmax_index)
                    average1 += buffer->values[i][j];
                }
              average1 /= (float)(histo_step_framesize - 1);
              buffer->values[i][mmax_index] = average1;
              for(j = histo_step_framesize; j< FILT_LENGTH; j++)
                buffer->values[i][j] = average1;
            }
        }  /* end of if (i_call == histo_step_framesize) */
	
      if (i_call > histo_step_framesize)
        {
          for(i = pptr->first_good; i<lastfilt; i++)
            {
			
              if(det(buffer, i, filter, aspectrum) == 0)
                {
                  /* spectral magnitude value is entered into the
                     histogram.  The oldest value in the histogram is
                     stored so that it can be excluded from the histogram
                     later */

                  histo_index->values[i]++;
                  m1 = histo_index->values[i]  % histo_length;
                  waste_trajec->values[i] = histo_values->values[i][m1];
                  histo_values->values[i][m1] = all_trajec->values[i][t];
                }
              else
                /* spectral magnitude value is not entered into the
                   histogram keep the oldest value in the histogram */
                waste_trajec->values[i] = -100;
            }
        }
	
      /* computation of the noise power for the first 100 ms */
      if ((i_call == 1) || (i_call == histo_step_framesize))
        {
          /* use parseval theorem in the beginning */
          for(i=pptr->first_good, sum = 0.0; i<lastfilt; i++)
            {
              for(j= 0, average = 0.0; j < i_call; j++)
                {
                  average += all_trajec->values[i][j];
                }
              average /= i_call;
              sum += (average * average);
            }
          sum /= ( lastfilt-(pptr->first_good));
          noiseptr->values[0] = sum; 
        }

      /* The x-axis of the histogram is scaled by the maximum spectral
     magnitude value. For e.g. given spectral magnitude value x and
     maximum value of y, the i th bin of the histogram is incremented
     by one where i = x/y * HISTO_RESOLUTION
	
     See if there is a new maximum in the trajectory */

      for (i = pptr->first_good; i< lastfilt; i++)
        {
          m1 = histo_index->values[i] % histo_length;
          /* if new_max_index->values[i] == m1, the maximum value is the
             oldest value of the histogram which should be excluded from
             the histogram. Thus a new maximum is found */

          if ((new_max_index->values[i] == (short) m1) &&
              (i_call != 1) &&
              (waste_trajec->values[i] != -100))
            {
              new_max->values[i] = SET;
              for (j= 0, max->values[i] = 0; j < histo_length; j++)
                {
                  if (histo_values->values[i][j] > max->values[i])
                    {
                      new_max_index->values[i] = (short)j;
                      max->values[i] = histo_values->values[i][j];
                    } 
                }
            }
          else
            {
              if( (waste_trajec->values[i] != -100) &&
                  (histo_values->values[i][m1] > max->values[i]))
                {
                  new_max->values[i] = SET;
                  max->values[i] = histo_values->values[i][m1]; 
                  new_max_index->values[i] = (short)m1;
                }
            }
        }

      /* After 200ms, the histogram method is used for noise estimation */

      if (i_call >= (2* histo_step_framesize))
        {
          for ( i= pptr->first_good; i< lastfilt; i++)
            {
              m1 = (histo_index->values[i]) % histo_length;
              /* decide the length of the histogram */
              if (histo_index->values[i] >= histo_length)
                {
                  length = histo_length;
                }
              else
                {
                  length = histo_index->values[i]+1; 
                }
              if (new_max->values[i] == SET)
                {
                  /* completely new computation of the histogram */
                  for (k=0; k < HISTO_RESOLUTION; k++) stat_lt[i][k] = 0;
                  for (k=0; k < length; k++)
                    {
                      stat_lt[i][(int)(((histo_values->values[i][k])/(max->values[i]))*(HISTO_RESOLUTION-EPSILON))]++;
                    }
                  new_max->values[i] = RESET;
                }
              else if (waste_trajec->values[i] != -100) 
                {
                  stat_lt[i][(int)(((histo_values->values[i][m1])/(max->values[i]))*(HISTO_RESOLUTION-EPSILON))]++;
                  if (histo_index->values[i] >=  histo_length)
                    {
                      /* the oldest value of the histogram is thrown away */
                      stat_lt[i][(int)(((waste_trajec->values[i])/(max->values[i]))*(HISTO_RESOLUTION-EPSILON))]--;
                    }
                }
            
              if ((i_call%histo_step_framesize) == 0) 
                /* Find the most frequently occurring value from the
                   histogram every histo_step_framesize frames */

                {
                  /* compute the actual average signal energy in this
                     band.  The possible estimated noise level is limited
                     to the average spectral value. This is related to the
                     fact that no noise energy can occur which is higher
                     than the total amount of energy inside a band. */

                  average = 0.0;
                  if (i_call > histo_length)
                    length = histo_length;
                  else
                    length = i_call;
                  for(j=0; j<length; j++)
                    {
                      average += all_trajec->values[i][j];
                    }
                  average /= length;
                  mmax_lp = 0;
                  /* Find the most frequently occuring value from the
                     histogram */
                  for (k=0; k <  HISTO_RESOLUTION; k++)
                    {
                      if ( stat_lt[i][k] > mmax_lp)
                        {
                          mmax_lp = stat_lt[i][k];
                          mindex = k;
                        }
                    }   
                
                  noise_level_lt->values[i] =
                    (mindex + 0.5) / HISTO_RESOLUTION * (max->values[i]);
                  if (noise_level_lt->values[i] > average)
                    noise_level_lt->values[i] = average;
                  if (i_call == 2*histo_step_framesize)
                    hptr->noiseOLD[i] = noise_level_lt->values[i];
                  /* The noise level is smoothed by the preceding noise
                     level */
                  noise_level_lt->values[i] =
                    0.5*hptr->noiseOLD[i] + 0.5*noise_level_lt->values[i];
                  hptr->noiseOLD[i] = noise_level_lt->values[i];
                }
            }
		
          /* Noise levels for different subbands are squared and then the
             average of these squared values gives the noise power
             estimate. The noise power is computed every 100ms */ 

          if (i_call%histo_step_framesize == 0)
            {
              for(i = pptr->first_good, noisepower = 0.0; i<lastfilt; i++)
                {
                  noisepower += pow(noise_level_lt->values[i],2.0);
                }
              noisepower /=  (lastfilt - (pptr->first_good)); 
            }
          noiseptr->values[0] = noisepower; 
        }
	
      if ((i_call == 1 || i_call%histo_step_framesize == 0) &&
          (pptr->debug==TRUE))
        {
          fprintf(outdebug,
                  "%d %f %e\n",
                  i_call,
                  noiseptr->values[0],
                  1/(C_CONSTANT*(noiseptr->values[0])));
        }
      return(noiseptr);
    }
}

/* create_filt is a subroutine for finding the coefficients of the
   filter for high energy speech detection */

void
create_filt(float over_est_factor,
            int time,
            float time_constant,
            struct fvec *filter)
{
  int i;
  float sum = 0;
	
	
  filter->values[0] = 1;
  for(i = 1; i< time; i++)
    {
      filter->values[i] =
        (float) (-pow((double)time_constant, (double)i));
      sum += fabs((double) filter->values[i]);
    }
  sum /= over_est_factor;
  for (i = 1; i< time; i++)
    filter->values[i] /= sum;
}


/* det is a subroutine for detecting high energy speech */
int
det(struct fmat *buffer,
    int Crband,
    struct fvec *filter,
    struct fvec *aspectrum)
{
  int j;
  float output;
  int flag;
	
  output =
    filter->values[0] *
    (float)sqrt((double)aspectrum->values[Crband]);

  for (j= 1; j<FILT_LENGTH; j++)
    output += filter->values[j]* buffer->values[Crband][j-1];

  /* When output >= 0 , high energy speech  is detected */

  for (j= FILT_LENGTH - 1; j>0; j--)
    buffer->values[Crband][j] = buffer->values[Crband][j-1];

  if (output < 0)
    {
      buffer->values[Crband][0] =
        (float)sqrt((double)aspectrum->values[Crband]);
      flag = 0;
    }
  else
    {
      buffer->values[Crband][0] = 1.05 * buffer->values[Crband][1];
      flag = 1;
    }
  return(flag);
}
