/* rasta.h */

/* #include <values.h> */
#include <math.h>

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

#define two_to_the(N) (int)(pow(2.0,(N))+0.5)

#ifndef TRUE
/* TRUE and FALSE not yet defined */
#define TRUE  ( 1 == 1 )
#define FALSE ( 1 == 0 )
#endif /* TRUE */

/* This byte-swapping routine works only if shorts are 2 or 4 bytes! */

#define MASK 0x0ff

#define SWAP_BYTES(A) ( (short) ((sizeof(short) == 2) ? \
                                 (((A & MASK) << 8) | ((A >> 8) & MASK)) : \
                                 (((A & MASK) << 24) | ((A & (MASK << 8)) << 8) | \
                                  ((A & (MASK << 16)) >> 8) | ((A >> 24) & MASK))) )

/* 	Constant values 	*/

#define SPEECHBUFSIZE 512
	/* This is only used to buffer input samples, and
	is noncritical (in other words, doesn't need to be changed
	when you change sample rate, window size, etc.) */

#define COMPRESS_EXP .33
		/* Cube root compression of spectrum is used for
	intensity - loudness conversion */

#define LOG_BASE 10. 
		/* Used for dB-related stuff */

#ifndef MAXFLOAT
#define MAXFLOAT 1e37
#endif /* !MAXFLOAT */

#define LOG_MAXFLOAT ((float)log((double) MAXFLOAT ))

#define TINY 1.0e-45
  
#define MAXHIST 25
  
#define TYP_WIN_SIZE 20

#define TYP_STEP_SIZE 10

#define PHONE_SAMP_FREQ 8000

#define TYP_SAMP_FREQ 16000

#define TYP_ENHANCE 0.6

#define TYP_MODEL_ORDER 8

#define NOTHING_FLOAT 0.0

#define NOTHING 0

#define ONE	1.0

#define NOT_SET 0

#define JAH_DEFAULT 1.0e-6

#define POLE_DEFAULT .94

#define POLE_NOTSET  -999

#define HAMMING_COEF .54

#define HANNING_COEF .50

#define RECT_COEF	1.0

#define FIR_COEF_NUM 5

#define IIR_COEF_NUM 1

#define UPPER_CUTOFF_FRQ 29.0215311628

#define MIN_UPPER_CUTOFF 2.0
#define MAX_UPPER_CUTOFF 100.0

#define LOWER_CUTOFF_FRQ 0.955503546532
#define LOWER_CUTOFF_NOTSET -999

#define MIN_LOWER_CUTOFF 0.0
#define MAX_LOWER_CUTOFF 10.0

/* Limits for parameter checking  */

#define MINFILTS 1

#define MAXFILTS 100

#define MIN_NFEATS 1 

#define MAX_NFEATS 100 

/* window and step sizes in milliseconds */
#define MIN_WINSIZE 1.0

#define MAX_WINSIZE 1000.

#define MIN_STEPSIZE 1.0

#define MAX_STEPSIZE 1000.

#define MIN_SAMPFREQ 1000

#define MAX_SAMPFREQ 50000

#define MIN_POLEPOS 0.0

#define MAX_POLEPOS 1.0

#define MIN_ORDER 1

#define MAX_ORDER 100

#define MIN_LIFT .01

#define MAX_LIFT 100.

#define MIN_WINCO HANNING_COEF

#define MAX_WINCO RECT_COEF

#define MIN_RFRAC 0.0

#define MAX_RFRAC 1.0

#define MIN_JAH 1.0e-16

#define MAX_JAH 1.0e16

/*-------------------------------------------------------------*/
#define HISTO_TIME 1000

#define HISTO_STEPTIME 100

#define MAX_HISTO_WINDOWLENGTH 200

#define RESET 0

#define SET 1 
  
#define HISTO_RESOLUTION  30 

#define EPSILON  0.01

#define FILT_LENGTH 30

#define TIME_CONSTANT 0.95

#define OVER_EST_FACTOR 2.0

#define C_CONSTANT 3.0

#define HPF_FC (44.7598)

/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
#define MAXNJAH  32   

#define MAXMAPBAND 256

#define MAXMAPCOEF 256

/*-------------------------------------------------------------*/

/*     	Structures		*/


struct fvec
{
  float *values;
  int length;
};

struct fmat
{
  float **values;
  int nrows;
  int ncols;
};

struct svec
{
  short *values;
  int length;
};


struct range
{
  int start;
  int end;
};

struct param
{
  float winsize;       /* analysis window size in msec               */
  int winpts;          /* analysis window size in points             */
  float stepsize;      /* analysis step size in msec                 */
  int steppts;         /* analysis step size in points               */
  int padInput;        /* if true, pad input so the (n * steppts)-th
                          sample is centered in the n-th frame       */
  int sampfreq;        /* sampling frequency in Hertz                */
  int nframes;         /* number of analysis frames in sample        */
  float nyqhz;         /* highest ip frq to be used (i.e. nyquist rate) */
  float nyqbar;	       /* Nyquist frequency  in barks                */
  int nfilts;	       /* number of critical band filters used       */
  int first_good;      /* number of critical band filters to ignore 
                          at start and end (computed)                */
  int trapezoidal;     /* set true if the auditory filters are
                          trapezoidal                                */
  float winco;         /* window coefficient	                     */
  float fcupper;       /* freq of zero in rasta filter	             */
  float fclower;       /* freq of -3dB pt near d.c. in rasta filter  */
  float polepos;       /* rasta integrator pole mag (redundant with fclower) */
  int order;           /* LPC model order                            */
  int nout;            /* length of final feature vector             */
  int gainflag;        /* flag that says to use gain                 */
  float lift;          /* cepstral lifter exponent                   */
  int lrasta;          /* set true if log rasta used                 */
  int jrasta;          /* set true if jah rasta used                 */
  int cJah;            /* set true if constant Jah used              */
  char *mapcoef_fname; /* Jah Rasta mapping coefficients input text 
                          file name                                  */
  int crbout;          /* set true if critical band values after
                          bandpass filtering instead of cepstral
                          coefficients are desired as outputs        */
  int comcrbout;       /* set true if critical band values after
                          cube root compression and equalization
                          are desired as outputs.                    */
  float rfrac;         /* fraction of rasta mixed with plp           */
  float jah;           /* Jah constant                               */
  char *infname;       /* Input file name, where "-" means stdin     */
  char *num_fname;     /* RASTA Numerator polynomial file name       */
  char *denom_fname;   /* RASTA Denominator polynomial file name     */
  char *outfname;      /* Output file name, where "-" means stdout   */
  int ascin;	       /* if true, read ascii in                     */
  int ascout;	       /* if true, write ascii out                   */
  int espsin;	       /* if true, read esps                         */
  int espsout;	       /* if true, write esps                        */
  int matin;	       /* if true, read MAT                          */
  int matout;	       /* if true, write MAT                         */
  int htkout;          /* if true, write HTK              <= SK      */
  int BOF;             /* flag to indicate beginning of file (and
			  need to write a header before the frames)  <= SK */
  int IOlist;          /* if true, in/out files are listed in a file <= SK */
  char *list_fname;    /* name of IO files list           <= SK      */
  int spherein;        /* read SPHERE input                          */
  int abbotIO;         /* read and write CUED's Abbot wav format     */
  int inswapbytes;     /* swap bytes of input                        */
  int outswapbytes;    /* swap bytes of output                       */
  int debug;           /* enable debug info 	                     */
  int smallmask;       /* add small constant to power spectrum       */
  int online;          /* online, not batch file processing          */
  int HPfilter;        /* highpass filter on waveform is used        */
  int history;         /* use stored noise level estimation and
                          RASTA filter history for initialization    */
  char *hist_fname;    /* history filename                           */
    /* incorporated delta additions */
    int deltawindow;	/* number of frames to use in delta calc     */
    int deltaorder;	/* 0=no deltas, 1=just deltas, 2=d+dd	     */
    /* mel filtering */
    int useMel;
    /* special flag for compatibility with common STRUT usage */
    int strut_mode;
    /* VT-len match warping (-V) */
    float f_warp;
    /* track how many FFT points we are using */
    int pspeclen;
    /* frequency of input HPF cutoff */
    float hpf_fc;
};

struct map_param
{
  float reg_coef[MAXNJAH][MAXMAPBAND][MAXMAPCOEF];  /* mapping coefficients */
  float jah_set[MAXNJAH];                           /* set of quantized Jahs  */
  int   n_sets;                                     /* number of quantized Jahs */
  int   n_bands;                                    /* number of critical bands */
  int   n_coefs;                                    /* number of mapping coefficients */
  float boundaries[MAXNJAH];                        /* decision boundaries */
}; 

struct fhistory{
  float noiseOLD[MAXFILTS];          /* last noise level estimation            */
  float filtIN[MAXFILTS][MAXHIST];   /* RASTA filter input buffer              */
  float filtOUT[MAXFILTS][MAXHIST];  /* RASTA filter output buffer             */
  int normInit;  /* if true, use normal initialization
                    (RASTA filter, noise estimation) */
  int eos;       /* for Abbot I/O.  If true, we've hit the end of an
                    utterance and need to reset the state in the
                    rasta filter, highpass filter, and noise
                    estimation */
  int eof;       /* for Abbot I/O.  Indicates we have hit the end of
                    input */
};

#define HZ2BARK(a) (6. * log(((double)(a)/600.0)+sqrt((a)*(a)/360000.0+1.0)))

/* Function prototypes for librasta.a */


/* Function prototypes for Rasta */

/***********************************************************************

 rasta/functions.h

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

/* Basic analysis routines */
struct fvec *get_win(struct param *,int);

struct fvec *powspec(const struct param *, struct fvec *, float *);

int set_freq_axis(const struct param *);

struct fvec *audspec(const struct param *, struct fvec *);

struct fvec *comp_noisepower(struct fhistory *, const struct
                             param *, struct fvec *);

void comp_Jah(struct param *pptr, struct map_param *mptr,
              int *mapset);  

struct fmat *cr_map_source_data(const struct param *pptr,
                                const struct map_param *mptr,
                                const struct fvec *ras_nl_aspectrum);

void do_mapping(const struct param *pptr,
                const struct map_param *mptr,
                const struct fmat *map_s_ptr, 
                int mapset, struct fvec *ras_nl_aspectrum);

struct fvec *nl_audspec(const struct param *, struct fvec *);

struct fvec *rasta_filt(struct fhistory *, const struct param *,
                        struct fvec *); 

struct fvec *get_delta(int);

struct fvec *get_rasta_fir( const struct param *pptr );

struct fvec *get_integ(const struct param *);

struct fvec *inverse_nonlin(const struct param *, struct fvec *);

struct fvec *post_audspec(const struct param *, struct fvec *);

struct fvec *lpccep(const struct param *, struct fvec *, int nout);

void band_to_auto( const struct param *, struct fvec *, struct fvec *); 

void auto_to_lpc(const struct param *pptr, struct fvec *, 
                 struct fvec *, float *);

void lpc_to_cep(const struct param *pptr, struct fvec *, 
                struct fvec *, int nout);

int fft_pow(float *, float *, long, long);

/* Matrix - vector arithmetic */
void fmat_x_fvec(const struct fmat *, const struct fvec *, struct fvec *);

void norm_fvec(struct fvec *, float);

/* Allocation and checking */
struct fvec *alloc_fvec(int);

struct svec *alloc_svec(int);

struct fmat *alloc_fmat(int, int);

void free_fvec(struct fvec* fv);

void free_svec(struct svec* fv);

void free_fmat(struct fmat* fm);

void fvec_check(char *, const struct fvec *, int);

void fvec_copy(char *, const struct fvec *, struct fvec *);

/* I/O */
struct fvec *get_bindata(FILE *, struct fvec *, struct param *);

struct fvec *get_ascdata(FILE *, struct fvec *);

struct fvec* get_online_bindata(struct fhistory*, struct param*);

struct fvec* get_abbotdata(struct fhistory*, struct param*);

void abbot_write_eos(struct param*);

void print_vec(const struct param *pptr, FILE *, struct fvec *, int);

float swapbytes_float(float f);

void binout_vec(const struct param* pptr, FILE *, struct fvec *);

FILE *open_out(struct param *);

void write_out(struct param *, FILE *, struct fvec *);

void fvec_HPfilter(struct fhistory*, struct param*, struct fvec*);

void load_history(struct fhistory *, const struct param *);

void save_history(struct fhistory *, const struct param *);

/* debugging aids */
void show_args(struct param *);

void show_param(struct fvec *, struct param *);

void show_vec(const struct param *pptr, struct fvec *);

void get_comline(struct param *, int, char **);

void check_args(struct param *);

void usage(char *);

/* function prototypes for local calls */

struct fvec* rastaplp(struct fhistory*, struct param*, struct fvec*);

/* Initialize parameters that can be computed from other parameter,
   as opposed to being initialized explicitly by the user.
   */
void init_param(struct fvec *sptr, struct param *pptr);

/*
 * deltas_c.h
 *
 * Calculate deltas on the fly
 * for sequential output from rasta to pfile
 * 1997jun18 dpwe@icsi.berkeley.edu
 * $Header: /u/drspeech/src/rasta/RCS/rasta.h,v 1.24 2006/03/10 23:57:19 davidj Exp $
 */

/* #include "rasta.h"	/* definition of struct fvec etc */

typedef struct _DeltaCalc {
    /* Class that acts as a 'filter' to append deltas to features 
       it receives */
    struct fmat *state;	/* history of featues, for calculating deltas */
    struct fvec *dfilt;	/* FIR filter for deltas */
    struct fvec *ddfilt; /* FIR filter for double-deltas */
    int rownum;		/* how many rows we've been given */
    int minrows;	/* we need this many rows before we emit any */
    int degree;		/* 0 = pass-thru, 1 = deltas, 2 = deltas+ddeltas */
    int nfeas;		/* feature count */
    int firstpad;	/* how many copies of the first frame to use */
    int middle;		/* which row is the middle? */
    int dfoffset;	/* apply dfilt starting at this row of state */
    int unpadding;	/* count how many post-end frames we've returned */

    int nOutputFtrs;	/* output columns */
    struct fvec *retvec;	/* structure for returning result */
} DeltaCalc;

DeltaCalc* alloc_DeltaCalc(int feas, int winlen, int dgree, int strut_mode);
    /* Initialize the object that will "filter" feature vectors 
       to return them augmented by their deltas. 
       <feas> is the size of the vectors we will be passed; <winlen> 
       is the window length over which deltas are to be calculated, 
       and <dgree> is 1 for plain deltas, or 2 for deltas and 
       double deltas. 
       strut_mode is a flag for modified double-delta envelope, for 
       compatibility with STRUT's rasta. */

void free_DeltaCalc(DeltaCalc *dc);
    /* free the DeltaCalc object */

void DeltaCalc_reset(DeltaCalc *dc);
    /* return to initial state */

struct fvec *DeltaCalc_appendDeltas(DeltaCalc *dc, const struct fvec *infeas);
    /* Take a row of features, and append it to our state.  When 
       we have enough state, return a nonempty floatRef with 
       fully-padded feature rows for output. 
       When caller reaches end of input, they repeatedly call DeltaCalc 
       with a zero-len <infeas> to allow it to flush its state; DeltaCalc
       will eventually give a zero-len response, meaning all done. */

/*
 * invcep.h
 *
 * Convert from cepstral coefficients back to a spectral representation.
 * For back-converting recognized frames to a spectral approximation.
 *
 * 1997jul29 dpwe@icsi.berkeley.edu
 * $Header: /u/drspeech/src/rasta/RCS/rasta.h,v 1.24 2006/03/10 23:57:19 davidj Exp $
 */

int _cep2spec(float *incep, int ceplen, float *outspec, int specpts, 
	      double lift);
    /* Invert a cepstrum into a (linear) spectrum (without fvecs!) */
struct fvec *cep2spec(struct fvec *incep, int nfpts, double lift);
    /* Invert a cepstrum into a (linear) spectrum */
struct fvec *tolog(struct fvec *vec);
    /* take the log of an fvec */
struct fvec *toexp(struct fvec *vec);
    /* inverse of tolog() */
struct fvec *spec2cep(struct fvec *spec, int ncpts, double lift);
    /* Construct a cepstrum directly from a spectrum */
struct fvec *inv_postaud(const struct param *pptr, struct fvec *input);
    /* invert the post-audspec compression and weighting */
int _inv_postaud(float *input, float *output, int len, double srate);
    /* invert the post-audspec compression and weighting, no fvecs */

void lpc_to_spec(struct fvec* lpcptr, struct fvec* specptr );
    /* convert lpc coefficients directly to a spectrum */

struct fvec *spec2lpc(const struct param *pptr, struct fvec *in);
    /* Find an LPC fit to a spectrum */
struct fvec *lpc2cep(const struct param *pptr, struct fvec *lpcptr);
    /* Convert the LPC params to cepstra */
struct fvec *lpc2spec(const struct param *pptr, struct fvec *lpcptr);
    /* Convert the LPC params to a spectrum */

/* Covers to allow otcl access to inner rasta fns */
float my_auto_to_lpc(const struct param *pptr, struct fvec *autoptr, 
		     struct fvec *lpcptr);
/* do Levinson/Durbin recursion to get LPC coefs from autocorrelation vec */
