// CmdLineStuff.H
//
// Definitions for the command-line argument structures
// for the feacalc project (enhanced rasta user interface).
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/CmdLineStuff.H,v 1.13 2007/08/30 20:30:51 janin Exp $

#ifndef _CMDLINESTUFF_H
#define _CMDLINESTUFF_H

// Get the type definitions for the clarg interface lib we're using
extern "C" {
#include <dpwelib/cle.h>
}
// The basic structure into which our command-line options will go
typedef struct _cl_vals {
    int rasta;		/* rasta processing flag */
    int plp;		/* plp processing flag */
    int cmpwt;		/* should we cube-root-compress & weight spectrum? */
    int domain;		/* output domain flag */
    int slimspect;	/* flag to strip the 'junk channels' from spectra */
    int cepsorder;	/* order of cepstra, if used */
    int nFilts;         /* number of peripheral filters (2007-08-29) */
//    char *listpath;	/* file with list of input ids */
    int uselists;	/* treat input files as lists of utids */
    int firstutt;	/* how many specified files to skip */
    int nutts;		/* maximum number of files to process */
    char *filecmd;	/* command to map input ids to filename paths */
    char *wavdirectory; /* instead of filecmd, start in this directory */
    char *wavextension; /* instead of filecmd, append this extension */
    char *outpath;	/* filename for output */
    int	outfilefmt;	/* flag for type of output file */
    int ipformat;	/* input file format ID */
    char *ipformatname;	/* input file format string */
    float jah;		/* constant J value for CJAH rasta */
    float fcupper;	/* frequency for placing upper zero in rasta filt */
    float fclower;	/* 3dB freq for d.c. pole/zero pair in rasta filt */
    float sampleRate;	/* sampling rate expected of input */
    float nyqRate;	/* maybe treat data as bandlimited to this frq. */
    float windowTime;	/* how long to analyze at once (in msec) */
    float stepTime;	/* increment in time between analysis windows (ms) */
    float zpadTime;	/* add this much exact silence at each end (ms) */
    float rangeRate;	/* sampling rate for range data */
    float rngStartOffset;	/* add to range start indices before scaling */
    float rngEndOffset;	/* add to range end indices before scaling */
    int doPad;		/* should we add half a window of silence at ends? */
    /*    int	doHpf; */		/* use rasta's DC-removal filter */
    int doDither;	/* add 1 bit of noise to protect from artif. silence */
    int doMel;		/* use HTK mel-scaled rather than bark-scaled fbank */
    int filtershape;	/* which filtershape; 0=trapez, 1=triang */
    int deltaOrder;	/* 0=plain, 1=plain+deltas, 2=plain+deltas+doubdelt */
    int deltaWindow;	/* number of time frames for delta calc */
    int strutmode;	/* flag for strutmode hack (IGNORED at present) */
    int hpf_fc_int;	/* 1000 * cutoff frq for highpass filt in Hz*/
    char *histpath;	/* name of the history filename for jrasta */
    int	readhist;	/* should we use jah history file for initializing? */
    char *mapfile;	/* filename for ascii jah mapping params */
    char *vtnfile;	/* file of per-utterance VTN warp parameters */
    int cmdHistory;	/* should we write this command line to a file? */
    char *cmdHistName;  /* file name to append with current command */
    int verbose;	/* 0=quiet, 1=normal reporting, 2=verbose */
    int help;		/* if set, just return usage msg */
  // dynamic values (can get updated each utt)
    float vtnWarp;      /* VT normalization warp */
} CL_VALS;

// Define tables for the command-line options
enum {
    RASTA_NONE = 0,
    RASTA_LOG,
    RASTA_JAH, 
    RASTA_CJAH
};
enum {
    PLP_NONE = 0, 
    PLP_DEFAULT = 1
};

enum {
    DOMAIN_LIN = 0,
    DOMAIN_LOG, 
    DOMAIN_CEPSTRA
};

enum {
    OUTFF_PFILE = 0,
    OUTFF_ONLINE,
    OUTFF_ONLINE_SWAPPED,
    OUTFF_RAW,
    OUTFF_RAW_SWAPPED,
    OUTFF_ASCII,
    OUTFF_HTK,
    OUTFF_HTK_SWAPPED,
    OUTFF_SRI
};

extern CLE_ENTRY clargs[];

/* single exported function */
CL_VALS *HandleCmdLine(int* pargc, char** pargv[]);
    /* Handle setting the arguments as well as recording the command line 
       in a history file if requested. */

#endif /* _CMDLINESTUFF_H */
