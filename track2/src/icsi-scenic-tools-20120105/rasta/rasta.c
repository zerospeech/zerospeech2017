/************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      main(): main calling routine                     *
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

/********************************************************************
	Scope of Algorithm


The actual processing is split up into a few pieces:

        1) power spectral analysis
        2) auditory spectrum computation
        3) compression (possibly adaptive)
        4) temporal filtering of each channel trajectory
        5) expansion (also possibly adaptive)
        6) postprocessing (e.g., preemphasis, loudness compression)
        7) autoregressive all-pole modeling (cepstral coefficients)

and i/o can either be ascii, binary (shorts on the input and floats
on the output), or standard ESPS files.

Since the lowest frequency and highest frequency bands extend
into forbidden territory (negative or greater-than-Nyquist
frequencies), they are ignored for most of the analysis. This
is done by computing a variable called first_good (which for
1 bark spacing is 1) and generally ignoring the first and last
``first_good'' channels. Just prior to the all-pole modeling,
the values from the good channels are copied over to the questionable
ones. (Note that ``first_good'' is available to most routines
via a pointer to a general parameter structure that has such
useful things as the number of filters, the analysis stepsize
in msec, the sampling rate, etc. . See rasta.h for
a definition of this structure).


This program (Rasta 2.0) implements the May 1994 version of RASTA-PLP
analysis. It incorporates several primary pieces:

1) PLP Analysis - as described in Hermansky's 1990 JASA paper,
the basic form of spectral analysis is Perceptual Linear Prediction,
or PLP. This computes the cepstral parameters of an all-pole model
of an auditory spectrum, which is a power spectrum that
has been frequency warped to the bark scale, smoothed by
an asymmetric critical-band trapezoid function,
down-sampled into approximately 1 Bark intervals,
cube root compressed for an intensity-loudness transformation, 
and weighted by a fixed equal loudness curve. 

2) RASTA filtering - as described in several Hermansky and Morgan
papers, the basic idea here is to bandpass filter the trajectories
of the spectral parameters. In the case of RASTA-PLP, this filtering
is done on a nonlinear transformation of an auditory-like spectrum, 
prior to the autoregressive modeling in PLP.

3) J-processing - For RASTA, the bandpass filtering is done in the 
log domain. An alternative is to use the J-family
of log-like curves

	y = log(1 + Jx)

where J is a constant that can appears to be optimally set when
it is inversely proportional to the noise power, (currently typically
1/3 of the inverse noise), x is the
critical band value, and y is the non-linearly transformed critical 
band value.
Rather than do the true inverse, which would be

	x = (exp(y) - 1)/J

and could get negative values, we use

	x' = (exp(y))/J

This prevents negative values, and in doing so effectively adds noise
floor by adding 1/J to the true inverse. 

One way of doing J-processing is to pick one constant J value and enter
this value at the command line. This J value should be dependent on the
SNR of the speech. We also may want to estimate the noise level for 
adaptive settings of the J-parameter during the utterance. 
Both methods of picking J should be handled with care. For the first
case, see the README file for a discussion of the perils of using even
a default J if it is too far from what you really need;
if the application situation is relatively fixed, you are better off making
an off-line noise measurement to get a good J value; in any event some
experimentation will soon show the proper constants involved for a problem.
For the second case, noise level is estimated for adaptive settings of the
J-parameter during the utterance. This should be done with care as well, 
as the use of a time-varying J brings in a new complication that you must
consider in the training and recognition, since changing J's over a time
series introduces a new source of variability in the analysis that must
be accounted for. The different J values, as required by differing noise
conditions, generates different spectral shapes and dynamics of the spectra.
This means that the training system must contend with a new source of 
variability due to the change in processing strategy that is adaptively
determined from the data. One approach to handle this variability is by
doing spectral mapping. In the current version, Spectral mapping is 
performed whenever J-Rasta processing is used with adaptive Js.

Spectral mapping - transform the spectrum processed with a J-value  
                   corresponding to noisy speech to a spectrum processed
                   with a J value for clean speech. In other words, we
                   find a mapping between log(1+xJ) and log(1+xJref) 
                   where Jref is the reference J, i.e. J value for clean speech.
                   For this approach, we have used a multiple regression 
                   within each critical band. In principle, this solution
                   reduces the variability due to the choice of J, and so
                   minimizes the effect on the training process.
       

How this works is:

1) Training of the recognizer:
   -- Train the recognizer with clean speech processed with J = 1.0e-6, a 
      suitable J value for clean speech.

2) Finding the relationship between spectrum corresponding to different Jah 
   values to the spectrum corresponding to J = 1.0e-6
   -- For each of the Jahs in the set {3.16e-7, 1.0e-7, 3.16e-8, 1.0e-8, 
      3.16e-9, 1.0e-9}, find a mapping relationship of the corresponding 
      bandpass filtered spectrum to the spectrum corresponding to J =
      1.0e-6. In other words, find a set of mapping coefficients for each
      Jah to 1.0e-6. The mapping method will be discussed later.

3) Extracting the speech features for the testing speech data
   -- obtain the critical band values as in PLP
   -- estimate the noise energy and thus the Jah value. Call this Jah value 
      J(orig).
   -- Pick a Jah from the set {3.16e-7, 1.0e-7, 3.16e-8, 1.0e-8,3.16e-9, 1.0e-9}
      that is closest to J(orig) and call this J(quant).
   -- perform the non-linear transformation of the spectrum using 
      log (1+J(quant)* X).
   -- bandpass filter the transformed critical band values.
   -- use the set of mapping coefficients for J(quant) to do the spectral 
      mapping or spectral transformation.
   -- preemphasize via the equal-loudness curve and then perform amplitude
      compression using the intensity-loudness power law as in PLP
   -- take the inverse of the non-linear function. 
   -- compute the cepstral parameters for the AR model.   
    
    
 
How regression coefficients are computed in our experiment:

    In order to map critical band values(after bandpass filtering) processed
    with different J values to those processed with J = 1.0e-6, J-Rasta 
    filtered critical band outputs from 10 speakers(excluded from the training
    and testing sets) are used to train 
    multiple regression models.  
    For example, for mapping from J= J(quant) to J = 1.0e-6, the regression
    equation can be written as:

    Yi = B2i* X2 + B3i* X3 + ... + B16i * X16 + B17i       (**)

where Yi = i th bandpass filtered critical band processed with J=1.0e-6
           i = 2, .. 16
      X2, X3, ... X16    2rd to 16th bandpass filtered critical band values
                         processed with J = J(quant), where
                         J(quant) is in the set 
                         {3.16e-7, 1.0e-7, 3.16e-8, 1.0e-8,3.16e-9, 1.0e-9} 
      B2i, B3i ... B17i     are the 16  mapping coefficients 

      
    For equation (**), we have made the assumption that the sampling frequency
    is 8kHz and the number of critical bands is seventeen. The first and 
    the last bands extend into forbidden territory -- negative or greater
    than Nyquist frequencies. Thus the the two bands are ignored for most
    of the analysis. Their values are made equal to the adjacent band's just
    before the autoregressive all-pole modeling. This is why we only make
    Yi dependent on bandpass filtered critical bands X2,X3,... X16, altogether
    fifteen critical bands. 

    The default mapping coefficients sets is stored in map_weights.dat. 
    This is suitable for s.f. 8kHz, 17 critical bands. For users who have 
    a different setup, they may want to find their own mapping coefficients
    set. This could be done by using the command options -R and -f. Command 
    -R allows you to get bandpass filtered critical band values as output
    instead of cepstral coefficients. 
    These outputs could be used as regression data. A simple multiple
    regression routine can be used to generate the mapping coefficients
    from these regression data. These mapping coefficients can be stored
    in a file. Command -f allows you to use this file to replace the
    default file map_weights.dat. The format of this file is:

     beginning of file 
    <Total number of Jahs, for the example shown above, it is [7] > 
    <# of critical bands, for the setup for 8kHz, this is [15]>
    <# of mapping coefficients/band, for the setup for 8kHz, this is [16]>
    
    <The J for clean speech, [1.0e-6]>
   
    <mapping coefficients for Y2, [B22, B32, B42,...]>
    <mapping coefficients for Y3, [B23, B33, B43 ...]>
        |
        |
        |
        V
    <mapping coefficients for Y16>
 
    
    <The second largest Jah besides 1e-6, [3.16e-7]>
        
        |
        |      mapping coefficients
        |
        |
        V 
  
    <The third largest Jah besides 1e-6, [1.0e-7]>
       
        |
        |      mapping coefficients
        |
        |
        V 
  
     .
     .
     .
     .    
     end of file



*********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rasta.h"

/******************************************************/

static void reset_state(struct param *runparam, struct fhistory *history) {
    /* reset state in history structure, rasta filter, and
       noise estimation. */

    history->normInit = TRUE;
    if (runparam->history == TRUE) {
	load_history(history, runparam);
    }

    history->eos = TRUE;	/* causes rasta_filt to reset */
    rasta_filt(history, runparam, (struct fvec*) NULL);

    if ((runparam->jrasta == TRUE ) &&
	(runparam->cJah == FALSE)) { 
	comp_noisepower(history, runparam, (struct fvec*) NULL);
    }

    history->eos = FALSE;
}


int
main(int argc,char** argv)
{
  /* function prototypes for local calls */
  void init_param(struct fvec*, struct param*);
  struct fvec* get_data(struct param*);
  struct fvec* rastaplp(struct fhistory*,
                        struct param*,
                        struct fvec*);
  struct fvec* fill_frame(struct fvec*,
                          struct fhistory*,
                          struct param*,
                          int);

  /*  	Variables and data structures	*/
  struct param runparam;
  struct fhistory history;
  struct fvec *speech, *cepstrum, *frame, *plusdeltas;
  int nframe;
  char *funcname;
  FILE *outfp;
  FILE *lstfp;           /* SK */
  char lstbuffin[512];   /* SK */
  char lstbuffout[512];  /* SK */
  DeltaCalc *dc;

  if(argc == 1) { 
      /* print out usage if no arguments used */
      usage(argv[0]);
      exit(0);
  }

  funcname = "main";

  get_comline(&runparam, argc,argv); /* preferably list arguments in 
					command line so there is a record */

  check_args( &runparam ); /* Exits if params out of range */

  /* If the debugging is on, show the command-line argurments */

  if (runparam.debug == TRUE) {
      show_args( &runparam );
  }

  if (runparam.IOlist == TRUE) {  /* Using filelist */
      /* Open filelist */
      if ( ! ( lstfp = fopen( runparam.list_fname , "r" )) ) {
          fprintf(stderr,"Unable to open file list.\n");
          exit(-1);
      }
  }

  if (runparam.infname == NULL) {
      runparam.infname = "-";
  }
  if (runparam.outfname == NULL) {
      runparam.outfname = "-";
  }

  history.eof = FALSE;

  /* initialize delta calculation */
  dc = alloc_DeltaCalc(0 /* runparam.nout */, runparam.deltawindow, 
		       runparam.deltaorder, runparam.strut_mode);
  /* You can't actually tell the width of the output features unless 
     you examine the cbrout and comcbrout flags.  Passing 0 tells the 
     DeltaCalc code to defer deciding the feature width until the 
     first frame is seen, and take it from that! dpwe 1997aug13 */
  
  /* preset the 'speech' structure to something we'll know not to use */
  speech = (struct fvec *)NULL;

  do {    /* Filelist loop (executed just once if !IOlist) */
      
      if (runparam.IOlist == TRUE) {  /* Using filelist */
          if ( fscanf(lstfp,"%s %s",lstbuffin,lstbuffout) != 2 ) {
	      break;
	  } else { 
	      runparam.infname = lstbuffin;
	      runparam.outfname = lstbuffout;
	  }
      }

      if ( runparam.online == FALSE && runparam.abbotIO == FALSE) {
	  if (speech) { free_fvec(speech); }
	  speech = get_data(&runparam); /* allocate for and read data */
      }

      outfp = open_out( &runparam ); /* Open output file */

      init_param(speech, &runparam); /* Compute necessary parameters 
					for analysis */

      reset_state(&runparam, &history);

      /* main analysis loop */

      for (nframe = 0; nframe < runparam.nframes; ) {

	  if (runparam.online == TRUE) {
	      frame = get_online_bindata( &history, &runparam );
	  } else if (runparam.abbotIO == TRUE) {
	      frame = get_abbotdata(&history, &runparam);
	  } else {
	      frame = fill_frame( speech, &history, &runparam, nframe);
	      /* only count frames if not online - else rp.nframes == 1 */
	      nframe++;
	  }

          if (history.eos) {
	      /* terminate the loop by warping to the end condition */
	      nframe = runparam.nframes;
	  } else {
	      cepstrum = rastaplp( &history, &runparam, frame );

	      plusdeltas = DeltaCalc_appendDeltas(dc, cepstrum);
	      
	      if(plusdeltas->length) {
		  struct fvec tmpout;
		  if (runparam.strut_mode) {
		      /* implement the strut-style Cep+dE+dCep+ddE */
		      tmpout.values = plusdeltas->values + 1;
		      tmpout.length = (runparam.deltaorder)*cepstrum->length;
		  } else {
		      tmpout.values = plusdeltas->values;
		      tmpout.length = plusdeltas->length;
		  }
		  write_out( &runparam, outfp, &tmpout );
	      }
	  }
      }

      /* flush deltas state */
      plusdeltas = DeltaCalc_appendDeltas(dc, NULL);
      while(plusdeltas->length) {
	  struct fvec tmpout;
	  if (runparam.strut_mode) {
	      /* implement the strut-style Cep+dE+dCep+ddE */
	      tmpout.values = plusdeltas->values + 1;
	      tmpout.length = (runparam.deltaorder)*cepstrum->length;
	      /* luckily, cepstrum->length is still available... */
	  } else {
	      tmpout.values = plusdeltas->values;
	      tmpout.length = plusdeltas->length;
	  }
	  write_out( &runparam, outfp, &tmpout );
	  plusdeltas = DeltaCalc_appendDeltas(dc, NULL);
      }
      DeltaCalc_reset(dc);

      if (runparam.abbotIO == TRUE) {
	  abbot_write_eos(&runparam);
      }

      if (outfp != stdout) {
	  fclose(outfp);
      }

/*      fprintf(stderr, "rasta: bottom of loop: iolist=%d abbotio=%d online=%d eof=%d\n", runparam.IOlist,  runparam.abbotIO, runparam.online, history.eof);
*/
  } while ( (runparam.IOlist == TRUE && !feof(lstfp)) \
	   || (runparam.abbotIO == TRUE && !history.eof) \
           || (runparam.online == TRUE && !history.eof));

  /* fprintf(stderr, "rasta: exiting\n"); */
  /* clean up */
  free_DeltaCalc(dc);

  if (speech) {
      free(speech->values);
      free(speech);
  }

  if(runparam.history == TRUE) {
      save_history(&history, &runparam);
  }

  return(0);
}
