// $Header: /u/drspeech/repos/quicknet2/QN_args.h,v 1.5 2006/02/08 03:18:41
// davi

#ifndef QN_args2_h_INCLUDED
#define QN_args2_h_INCLUDED

#include "QN_types.h"

// QN_ArgType denotes the type of the variable which corresponds to the
// argument.
enum QN_ArgType
{
    QN_ARG2_NOMOREARGS,
    QN_ARG2_DESC,
    QN_ARG2_INT,
    QN_ARG2_LONG,
    QN_ARG2_STR,
    QN_ARG2_BOOL,
    QN_ARG2_INVBOOL,
    QN_ARG2_FLOAT,
    QN_ARG2_DOUBLE,
    QN_ARG2_TOKEN,
    QN_ARG2_LIST_INT,
    QN_ARG2_LIST_FLOAT,
    QN_ARG2_RANGE,
    QN_ARG2_TIME,
    QN_ARG2_SPECIAL,
    QN_ARG2_NUMARGTYPES
};

/* define a special function for handling exceptions to general structure */
typedef int (*QN_Args_SpecialFunc)(char *arg, void *where, char *flag, void *data);
    /* set an int value based on *arg (which followed *flag) using 
       private data at *data; write appropriate value into *where; 
       return 1 if arg used, 0 if value set without using arg, or 
       -1 (or other value) if *arg was not a valid token. */



typedef struct {
    char *flag;			// The name of the argument.
    char *usage;    		// the usage message for this entry 
    QN_ArgType  type;		// QN_ARG2_INT etc.
    char *deflt;                // Text version of default value 
    void *where;                // address of variable to write to 
    QN_Args_SpecialFunc specialfn;  // fn to call for special types
    void *specData;             // Extra data if necessary (eg token table)
} QN_Arg2Entry;


typedef struct {    /* table maps strings to int values */
    char *string;
    int  code;
} QN_Arg2Token;

/* e.g.: */
/*
QN_Arg_Token algoTable[] = {
    { "mpeg1", AWCMP_MPEG_LAYER_I },
    { "mpeg2", AWCMP_MPEG_LAYER_II },
    { "mr"   , AWCMP_MULTIRATE },
    { "mr2"  , AWCMP_MULTIRATE_II },
    { NULL, 0 }
};
 */
 /* final null entry is terminator */

/* actual command line definition might look like: */
/*
QN_Arg2Entry clargs[] = {
    { "-a", QN_Arg_Token,  "algorithm type",
	  DFLT_ALGO, &algoType, NULL, algoTable },
    { "-fixbr", QN_ARG2_LONG, "fixed bitrate",
	  DFLT_FXBR, &fixedBR, NULL, NULL },
    { "-psy", QN_ARG2_INT, "psychoacoustics model",
	  DFLT_PSYM, &psyModel, NULL, NULL },
    { "-Alpha", QN_ARG2_FLOAT, "psycho model 1 alpha",
	  DFLT_ALPH, &psyAlpha, NULL, NULL },
    { "-nokbd", QN_ARG2_BOOL, "ignore the keyboard interrupt",
	  NULL, &verbose, NULL, NULL },
    { "", QN_ARG2_STR, "input sound file name",
	  NULL, &inpath, NULL, NULL },
    { "", QN_ARG2_STR, "output compressed file name",
	  NULL, &outpath, NULL, NULL },
    { "", QN_ARG2_NOMOREARGS, NULL,	
	  NULL, NULL, NULL, NULL }
};
 */
/* again, null entry terminates list */

/* A QN_Args_SpecialFunc: */
int cleTokenSetFn(char *arg, void *where, char *tok, void *data);
    /* set an int value from a QN_Arg_Token table, return 1 if arg used, 0 if 
       no arg used, -1 if error (unrecog) */

void cleUsage(QN_Arg2Entry *entries, char *programName);
    /* print a usage message to stderr, based on the QN_Arg2Entry table */

int QN_arg2_setdefaults(QN_Arg2Entry *entries);
    /* go through whole entries table setting up the defaults first */

int cleHandleArgs(QN_Arg2Entry *entries, int argc, char *argv[]);
    /* parse a set of command line arguments according to a structure */

int cleExtractArgs(QN_Arg2Entry *entries, int* pargc, char *argv[], 
		   int noExtraArgs);
   /* parse a set of command line arguments according to a structure.
      If noExtraArgs is not set, allow for a variable number of arguments 
      that we don't parse; return modified argc and argv pointing to the 
      residue. Return value is number of actual error objected to. */


#endif
