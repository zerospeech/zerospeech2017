/***************************************************************\
*   cle.h							*
*   Header for Table-driven command-line argument parsing	*
*   orignally implemented for yympeg				*
*   dpwe 1994dec12						*
\***************************************************************/

#include "dpwelib.h"

/* Useful macro for quoting literals (turning them into literal strings) */
#define _QUOTE(ARG)	#ARG
#define QUOTE(ARG)	_QUOTE(ARG)
/* thus
        #define VAL 1
	char *valstr = QUOTE(VAL);
  results in
        char *valstr = "1";
 */

/* define a special function for handling exceptions to general structure */
typedef int (*cleSpecialSetFn)(const char *arg, void *where, const char *flag, void *data);
    /* set an int value based on *arg (which followed *flag) using 
       private data at *data; write appropriate value into *where; 
       return 1 if arg used, 0 if value set without using arg, or 
       -1 (or other value) if *arg was not a valid token. */

typedef struct {    /* a command line entry */
    const char *flag;     /* e.g. "-n", or "" for any-match (must be at bottom) */
    int  type;	    /* CLE_T_INT, CLE_T_FLOAT, CLE_T_STRING, ... */
    const char *usage;    /* the usage message for this entry */
    const char *deflt;    /* text version of default value */
    void *where;    /* address of variable to write to */
    cleSpecialSetFn specialfn;/* fn to call for special types */
    void *specData; /* extra data e.g. token table for _T_TOKEN */
} CLE_ENTRY;

enum {	/* for CLE_ENTRY->type */
    CLE_T_END, CLE_T_INT, CLE_T_LONG, CLE_T_BOOL, CLE_T_FLOAT, CLE_T_STRING,
    CLE_T_TOKEN, CLE_T_SPECIAL, CLE_T_INVBOOL, CLE_T_USAGE, CLE_T_TIME
};

typedef struct {    /* table maps strings to int values */
    const char *string;
    int  code;
} CLE_TOKEN;

/* e.g.: */
/*
CLE_TOKEN algoTable[] = {
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
CLE_ENTRY clargs[] = {
    { "-a", CLE_T_TOKEN,  "algorithm type",
	  DFLT_ALGO, &algoType, NULL, algoTable },
    { "-fixbr", CLE_T_LONG, "fixed bitrate",
	  DFLT_FXBR, &fixedBR, NULL, NULL },
    { "-psy", CLE_T_INT, "psychoacoustics model",
	  DFLT_PSYM, &psyModel, NULL, NULL },
    { "-Alpha", CLE_T_FLOAT, "psycho model 1 alpha",
	  DFLT_ALPH, &psyAlpha, NULL, NULL },
    { "-nokbd", CLE_T_BOOL, "ignore the keyboard interrupt",
	  NULL, &verbose, NULL, NULL },
    { "", CLE_T_STRING, "input sound file name",
	  NULL, &inpath, NULL, NULL },
    { "", CLE_T_STRING, "output compressed file name",
	  NULL, &outpath, NULL, NULL },
    { "", CLE_T_END, NULL,	
	  NULL, NULL, NULL, NULL }
};
 */
/* again, null entry terminates list */

/* A cleSpecialSetFn: */
int cleTokenSetFn(const char *arg, void *where, const char *tok, void *data);
    /* set an int value from a CLE_TOKEN table, return 1 if arg used, 0 if 
       no arg used, -1 if error (unrecog) */

void cleUsage(CLE_ENTRY *entries, const char *programName);
    /* print a usage message to stderr, based on the CLE_ENTRY table */

int cleSetDefaults(CLE_ENTRY *entries);
    /* go through whole entries table setting up the defaults first */

int cleHandleArgs(CLE_ENTRY *entries, int argc, char *argv[]);
    /* parse a set of command line arguments according to a structure */

int cleExtractArgs(CLE_ENTRY *entries, int* pargc, char *argv[], 
		   int noExtraArgs);
   /* parse a set of command line arguments according to a structure.
      If noExtraArgs is not set, allow for a variable number of arguments 
      that we don't parse; return modified argc and argv pointing to the 
      residue. Return value is number of actual error objected to. */

