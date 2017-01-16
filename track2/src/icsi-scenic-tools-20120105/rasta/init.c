/*************************************************************************
 *                                                                       *
 *               ROUTINES IN THIS FILE:                                  *
 *                                                                       *
 *                      get_comline(): command line reading;             *
 *                              sets params                              *
 *                                                                       *
 *                      usage(): shows command line arguments            *
 *                                                                       *
 *                      init_param(): compute some params                *
 *                                                                       *
 *                      check_args(): check if params are reasonable     *
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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rasta.h"

/*
 * read and interpret command line
 */
void
get_comline( struct param * pptr, int argc,char** argv)
{
  int c;
  extern int optind;
  extern char *optarg;
  char *funcname;

  funcname = "get_comline";

  /* First, initialize run parameters to default values 
     (see rasta.h for definitions of upper-case constants) */

  pptr->winsize  = TYP_WIN_SIZE; 
  pptr->stepsize = TYP_STEP_SIZE; 
  pptr->padInput = FALSE;
  pptr->sampfreq = PHONE_SAMP_FREQ;
  pptr->nyqhz = 0;	/* flag to fill in later (dpwe 1998jun27) */
  /*  pptr->polepos = POLE_DEFAULT; */
  pptr->polepos = POLE_NOTSET;	/* mark that it's not been set */
  pptr->fclower = LOWER_CUTOFF_NOTSET; /* nor this one */
  pptr->fcupper = UPPER_CUTOFF_FRQ;
  pptr->order = TYP_MODEL_ORDER;
  pptr->lift = TYP_ENHANCE;
  pptr->winco = HAMMING_COEF;
  pptr->rfrac =  ONE;
  pptr->jah = JAH_DEFAULT;
  pptr->trapezoidal = TRUE;
  pptr->gainflag = TRUE;
  pptr->lrasta = FALSE;
  pptr->jrasta = FALSE;
  pptr->cJah = FALSE;
  pptr->mapcoef_fname = NULL;
  pptr->crbout = FALSE;
  pptr->comcrbout = FALSE;
  pptr->infname = NULL;
  pptr->outfname = NULL;
  /* Default IO files are stdin/stdout; NULL is converted to "-" 
     in rasta.c */
  /* END SK */
  pptr->num_fname = NULL;       /* file with RASTA polynomial numer\n") */
  pptr->denom_fname = NULL;     /* file with RASTA polynomial denom\n") */
  pptr->ascin = FALSE;
  pptr->ascout = FALSE;
  pptr->debug = FALSE;
  pptr->smallmask = FALSE;
  pptr->espsin = FALSE;
  pptr->espsout = FALSE;
  pptr->matin = FALSE;
  pptr->matout = FALSE;
  pptr->htkout = FALSE;              /* <== SK */
  pptr->BOF = TRUE;                  /* <== SK */
  pptr->IOlist = FALSE;              /* <== SK */
  pptr->list_fname = NULL;           /* <== SK */
  pptr->spherein = FALSE;
  pptr->abbotIO = FALSE;
  /* deltas stuff */
  pptr->deltawindow = 9;
  pptr->deltaorder = 0;

  /* binary I/O is, by definition, assumed to be in big-endian.  
     We used to fake it for little-endian machines by fiddling 
     the defaults for these values.  Now we automatically swap 
     after every fread/ before each fread in io.c */
  pptr->inswapbytes = FALSE;
  pptr->outswapbytes = FALSE;

  pptr->nfilts = NOT_SET;
  pptr->nout = NOT_SET;
  pptr->online = FALSE;         /* If set, do frame-by-frame analysis
                                   rather than reading in whole file first */
  pptr->HPfilter = FALSE;
  pptr->hpf_fc = HPF_FC;
  pptr->history = FALSE;
  pptr->hist_fname = "history.out";

  pptr->useMel = FALSE;
  pptr->strut_mode = FALSE;	/* 2001-03-16 dpwe@ee.columbia.edu */
  pptr->f_warp = 1.0;		/* 2001-03-23 dpwe@ee.columbia.edu */

  /* Note to maintainers:  the remaining available letters for
     command-line options are:

     *none* 					*/

  while((c = getopt(argc, argv, 
                    "OFaAeEbBXZzkTUyJChH:f:RPLdMp:i:o:I:w:W:s:S:l:m:n:c:vgr:j:t:D:N:q:Q:Y:u:x:KV:G:"))
        != -1)
    {
      switch( c )
        {
        case 'q':               /* degree of delta calculation */
          pptr->deltaorder = atoi(optarg);
          break;

        case 'Q':               /* delta calc time window size */
          pptr->deltawindow = atoi(optarg);
          break;

        case 'O':               /* flag for online processing */
          pptr->online = TRUE;
          break;

        case 'd':               /* flag for debug output */
          pptr->debug = TRUE;
          break;

        case 'F':               /* highpass filter on waveform */
          pptr->HPfilter = TRUE;
          break;

        case 'M':               /* flag for equiv of small noise addition */
          pptr->smallmask = TRUE;
          break;

        case 'i':               /* input file */
          pptr->infname = optarg;
          break;

        case 'a':               /* input as ascii */
          pptr->ascin = TRUE;
          break;

        case 'o':               /* output file */
          pptr->outfname = optarg;
          break;

        case 'A':               /* output as ascii */
          pptr->ascout = TRUE;
          break;

        case 'I':               /* list of files for input/output SK */
          pptr->IOlist = TRUE;
          pptr->list_fname = optarg;
          break;

        case 'e':               /* input as ESPS */
          pptr->espsin = TRUE;
          break;

        case 'E':               /* output as ESPS */
          pptr->espsout = TRUE;
          break;

        case 'b':               /* input as MAT */
          pptr->matin = TRUE;
          break;

        case 'B':               /* output as MAT */
          pptr->matout = TRUE;
          break;

        case 'X':               /* output as HTK */
          pptr->htkout = TRUE;
          break;

        case 'z':               /* input is SPHERE */
          pptr->spherein = TRUE;
          break;
          
        case 'Z':               /* use Mel-scaled triangular front-end */
          pptr->useMel = TRUE;
          break;
          
        case 'k':               /* read and write Abbot wav format */
          pptr->abbotIO = TRUE;
          break;

        case 'T':               /* for little-endian input */
          pptr->inswapbytes = !pptr->inswapbytes;
          break;

        case 'U':               /* for little-endian output */
          pptr->outswapbytes = !pptr->outswapbytes;
          break;

        case 'y':               /* for padded input */
          pptr->padInput = TRUE;
          break;

        case 'J':               /* Jah rasta */
          pptr->jrasta = TRUE;
          break;

        case 'C':               /* Constant Jah */
          pptr->cJah = TRUE;
          break;
  
        case 'f':               /* Jah Rasta mapping coefficients input text file */
          pptr->mapcoef_fname = optarg;
          break;

        case 'R':               /* output critical band values instead of cep. coefs */
          pptr->crbout = TRUE;
          break;

        case 'P':               /* output compressed critical band values instead */
          pptr->comcrbout = TRUE;
          break;

        case 'L':               /* log rasta */
          pptr->lrasta = TRUE;
          break;

        case 'w':               /* analysis window in msecs */
          pptr->winsize = atof(optarg);
          break;

        case 'W':               /* windowing constant other than HAMMING_COEF
                                   for w[n] = W - (1-W)*cos(angle[n]) */
          pptr->winco = atof(optarg);
          break;

        case 's':               /* step size in msecs */
          pptr->stepsize = atof(optarg);
          break;

        case 'S':               /* sampling frequency */
          pptr->sampfreq = atoi(optarg);
          break;

        case 'Y':               /* effective bandwidth (nyquist freq) */
          pptr->nyqhz = atof(optarg);
          break;

        case 'p':               /* pole position */
          pptr->polepos = atof(optarg);
          break;

        case 'u':               /* RASTA upper cutoff frq */
          pptr->fcupper = atof(optarg);
          break;

        case 'x':               /* RASTA lower cutoff frq */
          pptr->fclower = atof(optarg);
          break;

        case 't':               /* filter time constant file */
          fprintf(stderr,"-t time constant file not implemented\n");
          break;

        case 'l':               /* peak enhancement factor */
          pptr->lift = atof(optarg);
          break;

        case 'm':               /* model order */
          pptr->order = atoi(optarg);
          break;

        case 'n':               /* number of output parameters */
          pptr->nout = atoi(optarg);
          break;

        case 'c':               /* number of critical-band-like filters */
          pptr->nfilts = atoi(optarg);
          break;

        case 'v':               /* use triangular auditory filters */
          pptr->trapezoidal = FALSE;
          break;
          
        case 'g':               /* don't include gain */
          pptr->gainflag = FALSE;
          break;
                        
        case 'r':               /* if r=1.0 => full RASTA, if r=0.0 full PLP */
          pptr->rfrac = atof(optarg);
          break;

        case 'j':               /* set depending on the noise level, 
                                   default 1e-6  */
          pptr->jah = atof(optarg);
          break;

        case 'D':               /* Denominator file */
          fprintf(stderr,"RASTA denom file not yet implemented\n");
          break;

        case 'N':               /* Numerator file */
          fprintf(stderr,"RASTA numer file not yet implemented\n");
          break;

        case 'h':               /* use stored history for initialization */
          pptr->history = TRUE;
          break;          

        case 'H':               /* history filename */
          pptr->hist_fname = optarg;
          break;

        case 'K':               /* STRUT-compatible mode */
          pptr->strut_mode = TRUE;
          break;

        case 'V':               /* freq warp factor */
          pptr->f_warp = atof(optarg);
          break;

        case 'G':               /* HPF cutoff */
          pptr->hpf_fc = atof(optarg);
          break;

	case '?':		/* error from optarg */
        default:
          usage(argv[0]);
          break;
        }
    }
}


void
usage(char *fname)
{
  char *funcname;
  funcname = "usage";

  fprintf(stderr,"rasta version " RASTA_VERSION "\n");
  fprintf(stderr,"usage: %s [options]\n", fname );
  fprintf(stderr,"Where at least one option is required, and\n");
  fprintf(stderr,"where options can be any of the following:\n");
  fprintf(stderr,"\t -O for online processing [FALSE]\n");
  fprintf(stderr,"\t -d for debug output [FALSE]\n");
  fprintf(stderr,"\t -M adds a small constant to the power spectrum [FALSE]\n");
  fprintf(stderr,"\t\tequivalent to one bit of random noise on the input.\n");
  fprintf(stderr,"\t\tThis helps to avoid the numerical problems that occur\n");
  fprintf(stderr,"\t\twhen there are long stretches of zeros in the data\n");
  fprintf(stderr,"\t -F highpass filter on waveform (in case of DC offset problems) [FALSE]\n");
  fprintf(stderr,"\t -G <freq> Cutoff frequency of -F's HPF [%f]\n", HPF_FC);
  fprintf(stderr,"\t -i input file name [stdin]\n");
  fprintf(stderr,"\t -o output file name [stdout]\n");
  fprintf(stderr,"\t -I file of input and output filename pairs\n");  /* SK */
  fprintf(stderr,"\t -a for input file ascii rather than shorts [FALSE]\n");
  fprintf(stderr,"\t\tNote that default input is binary shorts\n");
  fprintf(stderr,"\t -A for output file ascii rather than floats [FALSE]\n");
  fprintf(stderr,"\t\tNote that default output is binary floats\n");
  fprintf(stderr,"\t -e for input files ESPS format[FALSE]\n");
  fprintf(stderr,"\t\tNote that default input is binary shorts\n");
  fprintf(stderr,"\t -E for output files ESPS format[FALSE]\n");
  fprintf(stderr,"\t\tNote that default output is binary floats\n");
  fprintf(stderr,"\t -b for input files MAT format[FALSE]\n");
  fprintf(stderr,"\t\tNote that default input is binary shorts\n");
  fprintf(stderr,"\t -B for output files MAT format[FALSE]\n");
  fprintf(stderr,"\t\tNote that default output is binary floats\n");
  fprintf(stderr,"\t -X for output files HTK format[FALSE]\n");      /* SK */
  fprintf(stderr,"\t\tNote that default output is binary floats\n"); /* SK */
  fprintf(stderr,"\t -z for input files SPHERE format[FALSE]\n");
  fprintf(stderr,"\t\tNote that default input is binary shorts\n");
  fprintf(stderr,"\t -k for Abbot I/O [FALSE]\n");
  fprintf(stderr,"\t\tNote that the default I/O format is binary shorts\n");
  fprintf(stderr,"\t -T for little-endian input. [FALSE]\n");
  fprintf(stderr,"\t\tThis option only applies to raw binary input.\n");
  fprintf(stderr,"\t -U for little-endian output. [FALSE]\n");
  fprintf(stderr,"\t\tThis option only applies to raw binary output.\n");
  fprintf(stderr,"\t -y for padded input. [FALSE]\n");
  fprintf(stderr,"\t\tIf there are M points per step and L input points,\n");
  fprintf(stderr,"\t\tproduce L/M frames, where the ((n + 0.5) * M)-th point\n");
  fprintf(stderr,"\t\tis centered in the n-th frame.\n");
  fprintf(stderr,"\t\tThere must be at least one window of data to produce output.\n");
  fprintf(stderr,"\t -J for JAH rasta [FALSE]\n");
  fprintf(stderr,"\t -C for constant JAH.  This option should [FALSE]\n");
  fprintf(stderr,"\t\tgenerally be used during recognizer training.\n");
  fprintf(stderr,"\t\tDefault is adapting JAH according to noisepower\n"); 
  fprintf(stderr,"\t\tOnly use option -C when -J is used\n"); 
  fprintf(stderr,"\t -f for JAH Rasta mapping coefficients input text file\n");
  fprintf(stderr,"\t -L for log rasta [FALSE]\n");
  fprintf(stderr,"\t -R for getting critical band values as output\n");
  fprintf(stderr,"\t\t instead of cepstral coefficients [FALSE]\n");
  fprintf(stderr,"\t\tNote: This is useful for users who would like to find\n");
  fprintf(stderr,"\t\t      their own spectral mapping coefficients.\n"); 
  fprintf(stderr,"\t\tNote: This overrules the -P option.\n");
  fprintf(stderr,"\t -P for getting cube root compressed and equalized\n");
  fprintf(stderr,"\t\t critical band values as output\n");
  fprintf(stderr,"\t\t instead of cepstral coefficients [FALSE]\n");
  fprintf(stderr,"\t -r for partially rasta, partially plp\n");
  fprintf(stderr,"\t    e.g. -r 1.0 => no mixing, -r 0.8 => 80%% rasta 20%% PLP  [1.0]\n");  
  fprintf(stderr,"\t -w analysis window size (in milliseconds) [%d]\n",
          TYP_WIN_SIZE);
  fprintf(stderr,"\t -W windowing constant [%f]\n",
          HAMMING_COEF);
  fprintf(stderr,"\t -s window step size (in milliseconds) [%d]\n",
          TYP_STEP_SIZE);
  fprintf(stderr,"\t -S Sampling frequency (in Hertz) [%d]\n",
          PHONE_SAMP_FREQ );
   fprintf(stderr,"\t -Y Effective maximum frequency (Nyquist rate) in Hz [sf/2]\n");
  fprintf(stderr,"\t -l liftering exponent [%f]\n", TYP_ENHANCE);
  fprintf(stderr,"\t -u upper cutoff zero freq [%f]\n",UPPER_CUTOFF_FRQ);
  fprintf(stderr,"\t -x lower cutoff 3dB freq (NOT with -p) [%f]\n",LOWER_CUTOFF_FRQ);
  fprintf(stderr,"\t -p pole position (NOT with -x) [%f]\n",POLE_DEFAULT);
  fprintf(stderr,"\t -m model order [%d]\n",TYP_MODEL_ORDER);
  fprintf(stderr,"\t -n number of output parameters \n");
  fprintf(stderr,"\t\twhere default is model order plus 1 (log gain) \n");
  fprintf(stderr,"\t\tand log gain is given first\n");
  fprintf(stderr,"\t -c num of crit band filters order \n");
  fprintf(stderr,"\t\twhere default depends on sampling freq, but "); 
  fprintf(stderr,"is 17 for 8000 Hz\n");
  fprintf(stderr,"\t -v (if you want triangular filters)[trapezoidal]\n");
  fprintf(stderr,"\t -Z for triangular, Mel-axis filters [bark, trapezoidal]\n");
  fprintf(stderr,"\t -g (if you don't want gain computed)[compute gain]\n");
  fprintf(stderr,"\t -N numerator rastafilt file \n");
  fprintf(stderr,"\t\t (currently unimplemented)\n");
  fprintf(stderr,"\t -D denominator rastafilt file \n");
  fprintf(stderr,"\t\t (currently unimplemented)\n");
  fprintf(stderr,"\t -j Constant J [%e] depends on the noise level,\n",
          JAH_DEFAULT);
  fprintf(stderr,"\t\t( smaller for more noisy speech)\n");
  fprintf(stderr,"\t\tNote: Only use option -j when -C is used\n");
  fprintf(stderr,"\t -h use stored noise level estimation and RASTA filter\n");
  fprintf(stderr,"\t\thistory for initialization (if history file is\n");
  fprintf(stderr,"\t\tavailable, otherwise use normal initialization)\n");
  fprintf(stderr,"\t -H history filename [history.out]\n"); 
  fprintf(stderr,"\t -q delta-calculation order [0]\n");
  fprintf(stderr,"\t -Q delta-calculation window size (frames) [9]\n");
  fprintf(stderr,"\t -K STRUT compatibility: adds true energy as ftr 0, double-delta windows are Q+2, strange profile\n");
  fprintf(stderr,"\t -V factor   vocal tract length warping; proportional to VT length\n");
  fprintf(stderr,"\n");

  exit( 0 );
}

/* Initialize parameters that can be computed from other parameter,
   as opposed to being initialized explicitly by the user.
   */
void init_param(struct fvec *sptr, struct param *pptr) 
{
  int overlap, usable_length;
  float tmp;
  float step_barks;
  char *funcname;
  int log2length, fftlength;

  funcname = "init_param";

  pptr->winpts = (int)((double)pptr->sampfreq * (double)pptr->winsize
                       / 1000.);

  pptr->steppts = (int)((double)pptr->sampfreq * (double)pptr->stepsize
                        / 1000.);

  overlap = pptr->winpts - pptr->steppts;

  log2length = ceil(log((double)(pptr->winpts))/log(2.0));
  fftlength = two_to_the((double)log2length);
  /* Allow space for pspec from bin 0 through bin N/2 */
  pptr->pspeclen = ( (fftlength / 2) + 1);

  if ((pptr->online == TRUE) || (pptr->abbotIO == TRUE))
    {
      pptr->nframes = 1;        /* Always looking at one frame,
                                   in infinite loop. */
    }
  else if (pptr->padInput == FALSE)
    {
      usable_length = sptr->length - overlap;
      pptr->nframes = (double)usable_length / (double)pptr->steppts;
    }
  else
    {
      pptr->nframes = sptr->length / pptr->steppts;
    }

  /* default nyquist rate if not specified */
  if (pptr->nyqhz <= 0) {
    pptr->nyqhz = pptr->sampfreq / 2.0;
  }

  /* Nyquist rate in bark specifies audspec range and default num filters */
  /* Here is some magical stuff to get the Nyquist frequency in barks */
  pptr->nyqbar = HZ2BARK(pptr->nyqhz);

  /* compute number of filters for at most 1 Bark spacing;
     This includes the constant 1 since we have a filter at d.c and
     a filter at the Nyquist (used only for dft purposes) */

  if(pptr->nfilts == NOT_SET)
    {
      pptr->nfilts = ceil(pptr->nyqbar) + 1;
    }
  if((pptr->nfilts < MINFILTS) || (pptr->nfilts > MAXFILTS))
    {
      fprintf(stderr,"Nfilts value of %d is not between MINFILTS (%d) and MAXFILTS (%d)\n",
              pptr->nfilts, MINFILTS, MAXFILTS);
      exit(-1);
    }

  /* compute filter step in Barks */
  step_barks = pptr->nyqbar / (float)(pptr->nfilts - 1);
  /* for a given step, must ignore the first and last few filters */
  pptr->first_good = (int)(1.0 / step_barks + 0.5);


  if(pptr->nout == NOT_SET)
    {
      pptr->nout = pptr->order + 1;
    }
  if((pptr->nout < MIN_NFEATS) || (pptr->nout > MAX_NFEATS))
    {
      fprintf(stderr,"Feature vector length of %d not OK\n",
              pptr->nout);
      exit(-1);
    }
}



/* Check numerical parameters to see if in a reasonable range, and the logical
   sense of combinations of flags. For the numerical comparisons,
   see the constant definitions in rasta.h . */
void
check_args( struct param *pptr )
{
        
  char *funcname;
  funcname = "check_args";

#ifndef HAVE_LIBESPS
  if(pptr->espsin == TRUE || pptr->espsout == TRUE)
    {
      fprintf(stderr,"Compiled without ESPS library (no ESPS licence) -> no ESPS file I/O available");
      fprintf(stderr,"\n");
      exit(-1);
    }
#endif
#ifndef HAVE_LIBMAT
  if(pptr->matin == TRUE || pptr->matout == TRUE)
    {
      fprintf(stderr,"Compiled without matlab library (no MATLAB licence) -> no MAT file I/O available");
      fprintf(stderr,"\n");
      exit(-1);
    }
#endif
#ifndef HAVE_LIBSP
  if(pptr->spherein == TRUE)
    {
      fprintf(stderr,"Compiled without SPHERE (or ESPS) library -> no SPHERE file input available");
      fprintf(stderr,"\n");
      exit(-1);
    }
#endif
  if((pptr->winsize < MIN_WINSIZE ) || (pptr->winsize > MAX_WINSIZE ))
    {
      fprintf(stderr,"Window size of %f msec not OK\n",
              pptr->winsize);
      exit(-1);
    }
  if((pptr->stepsize < MIN_STEPSIZE )||(pptr->stepsize > MAX_STEPSIZE ))
    {
      fprintf(stderr,"Step size of %f msec not OK\n",
              pptr->stepsize);
      exit(-1);
    }
  if (pptr->winsize - pptr->stepsize <= (1000.0 / pptr->sampfreq))
    {
      fprintf(stderr, "Step size (%.1fms) must be less than window size (%.1fms).\n", pptr->stepsize, pptr->winsize);
      exit(1);
    }
  if((pptr->sampfreq < MIN_SAMPFREQ ) || (pptr->sampfreq > MAX_SAMPFREQ ))
    {
      fprintf(stderr,"Sampling frequency of %d not OK\n",
              pptr->sampfreq);
      exit(-1);
    }
  /* Logic for polepos: can't define both fclower and polepos; 
     if neither is set, use the default for fclower, and let the polepos 
     be set later (in rasta_filt.c:get_integ()) */
  if (pptr->polepos == POLE_NOTSET) {
    if (pptr->fclower == LOWER_CUTOFF_NOTSET) {
      pptr->fclower = LOWER_CUTOFF_FRQ;
    }
    if (pptr->fclower <= MIN_LOWER_CUTOFF || pptr->fclower >MAX_LOWER_CUTOFF) {
      fprintf(stderr, "Lower cutoff of %f Hz is not between %f and %f\n", 
	      pptr->fclower, MIN_LOWER_CUTOFF, MAX_LOWER_CUTOFF);
      exit(-1);
    }
  } else {
    /* polepos was set in command line */
    if (pptr->fclower != LOWER_CUTOFF_NOTSET) {
      fprintf(stderr, "Cannot define both polepos (%f) and lower cutoff (%f)\n", pptr->polepos, pptr->fclower);
      exit(-1);
    }
    if((pptr->polepos < MIN_POLEPOS ) || (pptr->polepos >= MAX_POLEPOS )) {
      fprintf(stderr,"Pole position of %f not OK\n",
              pptr->polepos);
      exit(-1);
    }
  }
  if (pptr->fcupper <= MIN_UPPER_CUTOFF || pptr->fcupper >MAX_UPPER_CUTOFF) {
    fprintf(stderr, "Upper cutoff of %f Hz is not between %f and %f\n", 
	    pptr->fcupper, MIN_UPPER_CUTOFF, MAX_UPPER_CUTOFF);
    exit(-1);
  }
  if((pptr->order < MIN_ORDER ) || (pptr->order > MAX_ORDER ))
    {
      fprintf(stderr,"LPC model order of %d not OK\n",
              pptr->order);
      exit(-1);
    }
  if((pptr->lift < MIN_LIFT ) || (pptr->lift > MAX_LIFT ))
    {
      fprintf(stderr,"Cepstral exponent of %f not OK\n",
              pptr->lift);
      exit(-1);
    }
  if((pptr->winco < MIN_WINCO ) || (pptr->winco > MAX_WINCO ))
    {
      fprintf(stderr,"Window coefficient of %f not OK\n",
              pptr->winco);
      exit(-1);
    }
  if((pptr->rfrac < MIN_RFRAC ) || (pptr->rfrac > MAX_RFRAC ))
    {
      fprintf(stderr,"Rasta fraction of %f not OK\n",
              pptr->rfrac);
      exit(-1);
    }
  if((pptr->jah < MIN_JAH ) || (pptr->jah > MAX_JAH ))
    {
      fprintf(stderr,"Jah value of %e not OK\n",
              pptr->jah);
      exit(-1);
    }
  if((pptr->lrasta ==FALSE) && (pptr->jrasta == FALSE))
    {
      if(pptr->rfrac != 1.0)
        {
          fprintf(stderr,"Can't mix if no rasta flag\n");
          exit(-1);
        }
    }
  if((pptr->lrasta == TRUE) && (pptr->jrasta == TRUE))
    {
      fprintf(stderr,"Can't do log rasta and jah rasta at the same time\n");
      exit(-1);
    }
  if(pptr->online == TRUE)
    {
      if(pptr->espsin==TRUE)
        {
          fprintf(stderr,"can't run on-line on esps input\n");
          exit(-1);
        }
      if(pptr->matin==TRUE)
        {
          fprintf(stderr,"can't run on-line on MAT input\n");
          exit(-1);
        }
      if(pptr->spherein==TRUE)
        {
          fprintf(stderr,"can't run on-line on SPHERE input\n");
          exit(-1);
        }
      if(pptr->ascin==TRUE)
        {
          fprintf(stderr,"can't run on-line on ascii input\n");
          exit(-1);
        }
      if(pptr->infname != NULL && strcmp(pptr->infname, "-") != 0)
        {
          fprintf(stderr,"on-line mode uses stdin only\n");
          exit(-1);
        }
      /* BEGIN SK */
      if(pptr->IOlist == TRUE)
        {
          fprintf(stderr,"can't run on-line with in/out files in a list\n");
          exit(-1);
        }
      /* END SK */
      if (pptr->abbotIO == TRUE)
        {
          fprintf(stderr,
                  "Warning: Abbot I/O is, by definition, online.\n");
        }
    }

  /* BEGIN SK */
  if( (pptr->IOlist == TRUE && pptr->infname != NULL) ||
      (pptr->IOlist == TRUE && pptr->outfname != NULL) )
    {
      fprintf(stderr,"can't use file list and explicit in/out filenames simultaneously\n");
      exit(-1);
    }
  if( (pptr->IOlist == TRUE && pptr->espsout == TRUE) )
    {
      fprintf(stderr,"use of file list unimplemented in the case of esps output\n");
      exit(-1);
    }
  /* END SK */

  if((pptr->espsin == TRUE && pptr->matin == TRUE) ||
     (pptr->espsin == TRUE && pptr->ascin == TRUE) ||
     (pptr->espsin == TRUE && pptr->spherein == TRUE) ||
     (pptr->espsin == TRUE && pptr->abbotIO == TRUE) ||
     (pptr->ascin == TRUE && pptr->matin == TRUE) ||
     (pptr->ascin == TRUE && pptr->spherein == TRUE) ||
     (pptr->ascin == TRUE && pptr->abbotIO == TRUE) ||
     (pptr->matin == TRUE && pptr->spherein == TRUE) ||
     (pptr->matin == TRUE && pptr->abbotIO == TRUE))
    {
      fprintf(stderr,"can't read different input formats simultaneously\n");
      exit(-1);
    }
  if ((pptr->spherein == TRUE) && (pptr->abbotIO == TRUE)) {
      fprintf(stderr, "warning: sphere input overrides abbot input\n");
  }

  if((pptr->espsout == TRUE && pptr->matout == TRUE) ||
     (pptr->espsout == TRUE && pptr->ascout == TRUE) ||
     (pptr->espsout == TRUE && pptr->abbotIO == TRUE) ||
     (pptr->ascout == TRUE && pptr->matout == TRUE) ||
     (pptr->ascout == TRUE && pptr->abbotIO == TRUE) ||
     (pptr->matout == TRUE && pptr->abbotIO == TRUE) ||
     (pptr->htkout == TRUE && pptr->matout == TRUE) ||   /* SK */
     (pptr->htkout == TRUE && pptr->ascout == TRUE) ||   /* SK */
     (pptr->htkout == TRUE && pptr->abbotIO == TRUE) ||  /* SK */
     (pptr->htkout == TRUE && pptr->espsout == TRUE)     /* SK */
     )
    {
      fprintf(stderr,"can't write different output formats simultaneously\n");
      exit(-1);
    }
  if((pptr->inswapbytes == TRUE) && (sizeof(short) != 2) &&
     (sizeof(short) != 4))
    {
      fprintf(stderr,"Shorts are %ld bytes.\n", sizeof(short));
      fprintf(stderr,"Byte-swapping function in rasta.h will\n");
      fprintf(stderr,"not work!\n");
      exit(-1);
    }
  if(pptr->deltaorder != 0) {
      if( !(pptr->deltaorder == 1 || pptr->deltaorder == 2) ) {
	  fprintf(stderr, "delta order can only be 0, 1 or 2 (not %d)\n", 
		  pptr->deltaorder);
	  exit(-1);
      }
      if( pptr->deltawindow < 3 || pptr->deltawindow > 31) {
	  fprintf(stderr, "delta window must be in range 3..31 (not %d)\n", 
		  pptr->deltawindow);
	  exit(-1);
      }
      if( (pptr->deltawindow & 1) == 0 ) {
	  fprintf(stderr, "delta window must be odd-length (not %d)\n", 
		  pptr->deltawindow);
	  exit(-1);
      }
  }
}

