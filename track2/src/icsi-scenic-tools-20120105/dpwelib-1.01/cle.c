/***************************************************************\
*   cle.c							*
*   Table-driven command-line argument parsing			*
*   orignally implemented for yympeg				*
*   dpwe 1994dec12						*
\***************************************************************/

#include "dpwelib.h"
#include <assert.h>
#include <math.h>	/* atof */
#include <stdlib.h>	/* atoi? */
#include <string.h>	/* strdup() */

#include "cle.h"

#define CLE_STRLEN 256

#ifndef HAVE_STRDUP
extern char *strdup(const char *s1);
#endif  /* HAVE_STRDUP */

/* a couple of functions for printing nicely columnated output to 
   a file/stream. */

static void coMoveToCol(FILE *stream, int *curcol, int targcol)
{   /* Write white-space to <stream> until the cursor is at <targcol>. 
       Insert a newline if necessary.  <curcol> is our 'private' state. */
    if (*curcol > targcol) {
	fputc('\n', stream);
	(*curcol) = 0;
    }
    /* could use tab, but why bother? */
    while (*curcol < targcol) {
	fputc(' ', stream);
	++(*curcol);
    }
}

static int coWrapPrefix(char *s, int n, int *step)
{   /* return the length of a prefix of string <s>, not greater than <n>, 
       which attempts to end just before a whitespace.  In <step>, 
       return the number of characters to advance in <s> to get to the 
       following non-whitespace. */
    char ws[] = " \t";
    int len = 0;
    char *t = s;
    int nWs, nNonWs, nLastWs = 0;

    while (*t) {
	/* length of next non-ws */
	nNonWs = strcspn(t, ws);
	/* length of subsequent ws */
	nWs = strspn(t+nNonWs, ws);
	if (nNonWs+nWs == 0  /* end of string */ \
	    || (len > 0 && len+nLastWs+nNonWs > n) 
               /* nonempty string abt to wrap */) {
	    break;
	} else {
	    /* include this bit in the string */
	    len += nLastWs+nNonWs;
	    /* save the following Ws to add onto len next time around */
	    nLastWs = nWs;
	    /* skip the string to the start of the next non-ws */
	    t += nNonWs+nWs;
	}
    }
    /* If we couldn't fit the wrap, len will be bigger than n;
       chop it if so */
    if (len > n) {
	len = n;
	nLastWs = 0;	/* no skip for following WS if truncated */
    }
    /* <len> is how much to print.  But len+nLastWs is how much 
       to skip to get to the next non-WS to print.  Return this 
       in *step. */
    if (step) {
	*step = len + nLastWs;
    }
    return len;
}

/* At what column to do wrapping */
#define RIGHT_MARGIN 76

static void coPrint(FILE *stream, int *curcol, const char *str, int first, int rest)
{   /* copy the string <s> to output <stream>.  Start the first line at
       column <first>; perform word-wrapping, starting subsequent lines 
       at column <rest>. */
    int rm = RIGHT_MARGIN;
    int i, len, skip, crlen;
    int startcol = first;
    char partstr[CLE_STRLEN];
    char *s;

    while (*str) {
	/* copy up to first carriage return */
	crlen = strcspn(str, "\n");
	assert(crlen < CLE_STRLEN);
	strncpy(partstr, str, crlen);
	partstr[crlen] = '\0';
	s = partstr;
	while(*s) {
	    /* move to the correct indent */
	    coMoveToCol(stream, curcol, startcol);
	    /* find out how much we can print */
	    len = coWrapPrefix(s, rm - *curcol, &skip);
	    /* print it out */
	    for (i=0; i < len; ++i) {
		fputc(s[i], stream);
		++(*curcol);
	    }
/*    fprintf(stderr, "\ncoPrint: s='%s' curcol=%d len=%d skip=%d\n", 
		    s, *curcol, len, skip);
 */
	    /* move on for rest */
	    s += skip;
	    startcol = rest;
	}
	/* end of line.  Should we <cr>? */
	if (str[crlen] == '\n') {
	    fputc('\n', stream);
	    *curcol = 0;
	    /* make sure we skip the <cr> too */
	    ++crlen;
	}
	/* move onto next line in string */
	str += crlen;
    }
}

/* --- end of column-printout (co) routines --- */

static int cleNextPattern(const char *fmtstring, char* rtnstring, 
			  const char **state)
{   /* Convert a string containing multiple patterns of the form
       "-ras*ta|-R" into a succession of strings, "-rasta", "-R" 
       on successive calls, also returning the minimum match lengths
       (4, 2).  <rtnstring> points to pre-allocated return space 
       to match; <state> is a string pointer used as state, which 
       should initially contain NULL.
       Returns -1 when no more patterns. */
    int firsttime = 0;
    int fldlen, patlen, minlen;
    char *s;
    /* check the state ptr is OK */
    if (!state) {
	fprintf(stderr, "cleNextPat: <state> cannot be NULL\n"); exit(-1);
    }
    if (*state == NULL) { *state = fmtstring; firsttime = 1; }
    if (*state < fmtstring || *state > (fmtstring+strlen(fmtstring))) {
	fprintf(stderr, "cleNextPat: <state> is nonsense\n"); exit(-1);
    }
    if (!firsttime) {
	if (**state == '\0') {
	    /* reached end of string */
	    return -1;
	} else if (**state != '|') {
	    fprintf(stderr, "cleNextPat: unexpected <state>\n"); exit(-1);
	}
	/* step over the separator */
	++*state;
    }
    /* state is valid; extract the pattern */
    rtnstring[0] = '\0';
    fldlen = strlen(*state);
    if ( (s = strchr(*state, '|')) != NULL) {
	/* There's another field after this one */
	fldlen = s - *state;
    }
    patlen = fldlen;
    minlen = fldlen;
    if ( (s = strchr(*state, '?')) != NULL \
	&& (s - (*state)) < fldlen ) {
	/* There's an optional part in this field */
	minlen = s - (*state);
	/* copy it without the '?' */
	strncpy(rtnstring, *state, minlen);
	strncpy(rtnstring+minlen, (*state)+minlen+1, fldlen-minlen-1);
	patlen = fldlen - 1;
    } else {
	patlen = fldlen;
	strncpy(rtnstring, *state, patlen);
    }
    rtnstring[patlen] = '\0';
    /* advance the state */
    *state += fldlen;
    return minlen;
}

static void cleMakeTokenVal(char *val, CLE_TOKEN *tktab)
{   /* make a value string of the form "{a|b|c}" from a token table */
    CLE_TOKEN *tk = tktab;

    strcpy(val, "{");
    while(tk->string != NULL) {
	strcat(val, tk->string);
	strcat(val, "|");
	++tk;
    }
    /* chop off the last bar */
    val[strlen(val)-1] = '\0';
    strcat(val, "}");
}

static int cleScanTime(const char *val, float *secs)
{  /* convert a time in [[hh:]mm:]ss[.sss] into secs as a float.  Return 0 if bad formatting, 1 on success */
    const char *s;
    char *t;
    float x, y;
    int err;

    s = val;
    x = 0;
    while ( (t = strchr(s,':')) != NULL ) {
	y = strtol(s, &t, 10);
	if (*t != ':') {
	    /* found something else */
	    fprintf(stderr, "cleScanTime: error at chr %ld of '%s'\n", (unsigned long) (t - s), s);
	    return 0;
	}
	x = 60*x + y;
	/* start just beyond colon */
	s = t+1;
    }
    /* rest is seconds */
    err = (sscanf(s, "%f", &y) != 1);
    if (err) {
	return 0;
    }
    x = 60*x+y;
    *secs = x;
    return 1;
}	

/* Column to start flag/values */
#define FLAG_COL     2
/* Column to start definitions */
#define DEFIN_COL    20
/* right-alignment column for default vals */
#define DFLTS_RCOL   (RIGHT_MARGIN)

void cleUsage(CLE_ENTRY *entries, const char *programName)
{   /* print a usage message to stderr, based on the CLE_ENTRY table */
    CLE_ENTRY *ent = entries;
    char val[CLE_STRLEN], str[CLE_STRLEN];
    const char *flag, *usage, *deflt;
    int curcol = 0;

    fprintf(stderr, "%s: command line flags (defaults in parens):\n", 
	    programName);
    while(ent->type != CLE_T_END)  {
	flag = ent->flag; usage = ent->usage; deflt = ent->deflt;
	if (!flag) flag = "";
	if (!usage) usage = "";
	switch(ent->type)  {
	case CLE_T_INT:		strcpy(val, "<int>");	break;
	case CLE_T_LONG:	strcpy(val, "<long>");	break;
	case CLE_T_FLOAT:	strcpy(val, "<float>"); break;
	case CLE_T_TIME:	strcpy(val, "<hh:mm:ss.sss>"); break;
	case CLE_T_BOOL:	strcpy(val, "");	break;
	case CLE_T_INVBOOL:	strcpy(val, "");	break;
	case CLE_T_STRING:	strcpy(val, "<string>");    break;
	case CLE_T_SPECIAL:	strcpy(val, "<arg>");	break;
	case CLE_T_TOKEN:	
	    cleMakeTokenVal(val, (CLE_TOKEN *)ent->specData);	
	    break;
	/* Totally ignore the flag portion of T_USAGE */
	case CLE_T_USAGE:	strcpy(val, ""); break;	

	default:
	    fprintf(stderr, "cleUsage: unknown type %d\n", ent->type);
	    abort();
	}
	sprintf(str, "%s %s", flag, val); 
	coPrint(stderr, &curcol, str, FLAG_COL, 0);
	coPrint(stderr, &curcol, usage, DEFIN_COL, DEFIN_COL+2);
	if(deflt)  {	/* argument has default defined */
	    sprintf(str, "(%s)", deflt);
	    /* figure col from strlen(str) for right-alignment */
	    coPrint(stderr, &curcol, str, DFLTS_RCOL-strlen(str), 0);
	}
	coPrint(stderr, &curcol, "\n", 0, 0);
	++ent;
    }
}

int cleSetDefaults(CLE_ENTRY *entries)
{  /* go through whole entries table setting up the defaults first */
    CLE_ENTRY *ent = entries;
    const char *arg;
    int err = 0;

    while(ent->type != CLE_T_END)  {
	/* Boolean types have implicit defaults, regardless of the dflt fld */
	if( (arg = ent->deflt) != NULL \
	    || ent->type == CLE_T_BOOL \
	    || ent->type == CLE_T_INVBOOL)  {
	    switch(ent->type)  {
	    case CLE_T_BOOL:
		*(int *)(ent->where) = 0; break;
	    case CLE_T_INVBOOL:
		*(int *)(ent->where) = 1; break;

	    case CLE_T_INT:
		{ int i; err = (sscanf(arg, "%d", &i) != 1); 
		  if (!err) *(int *)(ent->where) = i; }   break;
	    case CLE_T_LONG:
		{ long l; err = (sscanf(arg, "%ld", &l) != 1); 
		  if(!err) *(int *)(ent->where) = l; }    break;
	    case CLE_T_FLOAT:
		{ float f; err = (sscanf(arg, "%f", &f) != 1); 
		  if(!err) *(float *)(ent->where) = f; }  break;
	    case CLE_T_TIME:
		{ float f; err = (cleScanTime(arg, &f) != 1); 
		  if(!err) *(float *)(ent->where) = f; }  break;
	    case CLE_T_STRING:
		*(char **)(ent->where) = strdup(arg); break;
	    case CLE_T_TOKEN:
		err = cleTokenSetFn(arg, ent->where, ent->flag, ent->specData);
		break;
	    case CLE_T_SPECIAL:
		err = (*(ent->specialfn))(arg, ent->where, ent->flag,
					  ent->specData);
		break;
	    case CLE_T_USAGE:
		break;
	    default:
		fprintf(stderr, "cleSetDefaults: unknown CLE_TYPE %d\n", ent->type);
		abort;
	    }
	}
	++ent;
    }
    return err;
}

static int cleMatchPat(const char *arg, const char *multipat, int *pused, char **ppat, 
		       int *pval)
{   /* do the necessary expansions to match arg against the multipat;
       return 1 if it actually matched else 0.
       Set *pused to 0 if the arg should not be consumed. 
       Set *ppat to the actual pattern that matched.
       *pval takes the actual value for %d/%f flags. */
    int minlen;
    const char *state = NULL;
static char pat[CLE_STRLEN];	/* gets returned in *ppat, so make static ?? */
    int found = 0;
    int arglen = (arg)?strlen(arg):0;

	/* Special cases: 
	   1. token string like "abc?def" matches
	       abc, abcd, abcde, abcdef BUT NOT a, ab, abce etc.
	   2. token string "" matches anything, but shouldn't be 
	      regarded as consuming the following token.
        xx 3. token string "abc*" matches abc followed by anything
	   4. token string "*T" matches "1", "true", "yes"
	   5. token string "*F" matches "0", "false", "no"
	   6. token string "%d" allows an argument as decimal string.
	   */
    while(!found && (minlen = cleNextPattern(multipat, pat, &state)) > -1) {
	if( (strcmp(pat, "*T")) == 0) {      /* recurse on special patterns */
	    found = cleMatchPat(arg, "y?es|t?rue|1", pused, ppat, pval);
	} else if( (strcmp(pat, "*F")) == 0) {
	    found = cleMatchPat(arg, "n?o|f?alse|0", pused, ppat, pval);
	} else if( (strcmp(pat, "%d")) == 0) {
	    int val;
	    if(arg && sscanf(arg, "%d", &val) == 1) {
		found = 1;
		if(pval)	*(int*)pval = val;
		if(pused)	*pused = (strlen(pat) > 0);
		if(ppat)	*ppat = pat;
	    }
	} else if( (strcmp(pat, "%f")) == 0) {
	    float val;
	    if(arg && sscanf(arg, "%f", &val) == 1) {
		found = 1;
		if(pval)	*(float*)pval = val;
		if(pused)	*pused = (strlen(pat) > 0);
		if(ppat)	*ppat = pat;
	    }
	} else if( strlen(pat) == 0 \
	          || (arglen >= minlen && strncmp(arg, pat, arglen)==0) ) {
	    /* null string matches "no arg" - don't consume the arg */
	    found = 1;
	    if(pused)	*pused = (strlen(pat) > 0);
	    if(ppat)	*ppat = pat;
	}
    }
    return found;
}

int cleTokenSetFn(const char *arg, void *where, const char *fromflag, void *cle_tokens)
{   /* set an int value from a CLE_TOKEN table, return 1 if arg used, 0 if 
       no arg used, -1 if error (unrecog) */
    int *pint = (int *)where;
    CLE_TOKEN *tk = (CLE_TOKEN *)cle_tokens;
    int found = 0;
    int used;
    char *pat;

    while(!found && tk->string != NULL) {
	found = cleMatchPat(arg, tk->string, &used, &pat, pint);
	if( found && (strcmp(pat, "%d")) != 0) {
	    if (strcmp(pat, "%f") == 0) {
		/* scale by argument and write as int */
		*pint = tk->code * *(float *)pint;
	    } else {
		*pint = tk->code;
	    }
	}
	++tk;
    }
    if(found)	return used;
    else	return -1;
}

int cleHandleArgs(CLE_ENTRY *entries, int argc, char *argv[])
{  /* parse a set of command line arguments according to a structure */
    int err = cleExtractArgs(entries, &argc, argv, 1);
    return err;
}

int cleExtractArgs(CLE_ENTRY *entries, int* pargc, char *argv[], 
		   int noExtraArgs)
{  /* parse a set of command line arguments according to a structure.
      If noExtraArgs is not set, allow for a variable number of arguments 
      that we don't parse; return modified argc and argv pointing to the 
      residue. Return value is number of actual error objected to. */
    int i, j, errs = 0, anerr, rc, argUsed, tokDone, flaglen, toklen, minlen;
    int blankIndex = 0;     /* use each blank index just once */
    int blankCount;
    char *next, *arg, *tok;
    char tokbuf[CLE_STRLEN];
    char flag[CLE_STRLEN];
    CLE_ENTRY *ent;
    int outargc = 1;
    int argc = *pargc;
    char *pat;
    const char *state;
    int tokcontinued = 0;	/* flag that reparsing remainder */
    const char *programName = argv[0];

    /* first, set all defaults - we can overwrite them later */
    /* cleSetDefaults(entries); */  /* no, let program do this if it wants */
    i = 0;
    while(i + 1 - tokcontinued<argc)  {
	if (!tokcontinued) 
	    tok = argv[++i];	/* grab the next arg; preincrement
				   ensures we skip argv[0] */
	next = (i+1<argc)?argv[i+1]:NULL;
	ent = entries; 
	blankCount = 0;	argUsed = 0; tokDone = 0; anerr = 0; tokcontinued = 0;
	DBGFPRINTF((stderr, "cle(%d): testing '%s' (then '%s')\n", \
		    i, tok, next?next:"(NULL)"));
	while(!tokDone && ent->type != CLE_T_END)  {
	    state = NULL;
	    while(!tokDone && ent->type != CLE_T_USAGE && \
		  (minlen = cleNextPattern(ent->flag, flag, &state)) > -1) {
		toklen = strlen(tok);
		flaglen = strlen(flag);
		DBGFPRINTF((stderr, "cle: trying '%s' ('%s', %d)\n", \
			    tok,flag,minlen));
		if(flaglen == 0)
		    ++blankCount;    /* keep track of which blank field */
		if( /* Allow empty flags (untagged args) to match any (new) 
		       string, but not if they start with '-' unless they 
		       are exactly "-" */
		    ( flaglen == 0 \
		      && blankCount > blankIndex \
		      && ( tok[0] != '-' || tok[1] == '\0' ) ) \
		    /* Match all the tok, at least up to minlen of the flag */
		    || ((toklen >= minlen) \
			&& (strncmp(tok, flag, toklen) == 0)) \
		    /* special case: 2-chr flags only need match first bit */
		    || ( flaglen == 2 \
			 && flag[0] == '-' \
			 && ( tok[0] == '-' && tok[1] == flag[1]) ) ) {

		    /*** We matched a flag pattern ***/
		    tokDone = 1;

		    if(flaglen == 0) {		    
			++blankIndex;   /* mark this blank field as used */
			arg = tok;	/* for blank flds, tok is arg */
		    } else if(flag[0]=='-' && flaglen==2 
			      && strlen(tok)>flaglen) {
			/* simple "-X" flag was matched as prefix:
			   separate out the remainder as an arg, 
			   or the next tok. */
			arg = tok+flaglen;
		    } else {
			arg = next;
		    }
		    switch(ent->type)  {
		    case CLE_T_INT:
		    { int x; anerr = (arg==NULL) || (sscanf(arg, "%d", &x) != 1); 
		      if (!anerr) *(int *)(ent->where) = x; }   
		    argUsed = 1; break;
		    case CLE_T_LONG:
		    { long l; anerr = (arg==NULL) || (sscanf(arg, "%ld", &l) != 1); 
		      if(!anerr) *(int *)(ent->where) = l; }
		    argUsed = 1; break;
		    case CLE_T_FLOAT:
		    { float f; anerr = (arg==NULL) || (sscanf(arg, "%f", &f) != 1); 
		      if(!anerr) *(float *)(ent->where) = f; }
		    argUsed = 1; break;
		    case CLE_T_TIME:
		    { float f; anerr = (arg==NULL) || (cleScanTime(arg, &f) != 1); 
		      if(!anerr) *(float *)(ent->where) = f; }
		    argUsed = 1; break;
		    case CLE_T_STRING:
			if(arg==NULL) { anerr=1; } else {
			/* free any default value */
			if (*(char **)ent->where)  free(*(char **)ent->where);
			*(char **)(ent->where) = strdup(arg); argUsed = 1; }
			break;
		    case CLE_T_BOOL:
			++(*(int *)(ent->where)); break;
		    case CLE_T_INVBOOL:
			--(*(int *)(ent->where)); break;
		    case CLE_T_TOKEN:
			rc = cleTokenSetFn(arg,ent->where, tok, ent->specData);
			if(rc == 1) argUsed = 1; else if(rc != 0) anerr = 1;
			break;
		    case CLE_T_SPECIAL:
			rc = (*(ent->specialfn))(arg, ent->where, tok,
						 ent->specData);
			if(rc == 1) argUsed = 1; else if(rc != 0) anerr = 1;
			break;
		    default:
			fprintf(stderr, "cleExtractArgs: unknown CLE_TYPE %d\n", ent->type);
			abort;
		    }
		    if(anerr)  {
			fprintf(stderr, 
			  "%s: error reading '%s'(%s) as %s(%s)\n",
			  programName, tok, (arg==NULL)?"NULL":arg, ent->flag, ent->usage);
			++errs;
		    }
		    DBGFPRINTF((stderr, \
				"cle(%d): matched %s %s as %s %s (%s %s)\n", \
				i, tok, next?next:"(NULL)", flag, \
				argUsed?arg:"", ent->flag, ent->usage));
		}
	    }
	    ++ent;
	}
	if(!tokDone)  {	/* this token was not recognized at all */
	    float dummy;
	    if(noExtraArgs \
	       || (tok[0] == '-' && strlen(tok) > 1 \
		   && sscanf(tok, "%f", &dummy) == 0) ) {
		/* report an error if we don't expect any unrecognized 
		   args - or even if we do, but the tok is of "-foo" form, 
		   *unless* it's a valid number (e.g. -123) */
		fprintf(stderr, "%s: '%s' not recognized\n", programName, tok);
		++errs;
	    } else {
		/* just copy the missed tok to output */
		argv[outargc] = tok;
		++outargc;
	    }
	} else if(!argUsed) {
	    /* we recognized a token but it didn't use the arg - 
	       any arg is thus a candidate for the next token */
	    if(arg != next) {
		/* arg was not just the next token - so set it as a cont'n */
		/* fakely prepend the '-' (or ...) */
		tokbuf[0] = flag[0];
		strcpy(tokbuf+1, arg);
		/* flag looptop not read next arg */
		tokcontinued = 1;
		tok = tokbuf;
	    }
	} else {
	    /* tok used the arg.  If it was the next tok, skip it */
	    if(arg == next) {
		++i;
	    }
	}
    }
    *pargc = outargc;

/*  if(errs)
	cleUsage(entries, argv[0]);
 */
    return errs;
}
	
#ifdef MAIN
/* test code */

CLE_TOKEN testTable[] = {
    { "*F", 0 }, 	/* -rasta no */
    { "j?ah", 1 }, 
    { "cj?ah", 2 }, 
    { "*T|log|", 3 },	/* -rasta yes or -rasta log or -rasta */
    { NULL, 0}
};

enum {AWCMP_NONE, AWCMP_MPEG_LAYER_I, AWCMP_MPEG_LAYER_II, AWCMP_MULTIRATE};
CLE_TOKEN algoTable[] = {
    { "-|*F", AWCMP_NONE },
    { "mpeg2|mp2",  AWCMP_MPEG_LAYER_II },
    { "mr"   ,  AWCMP_MULTIRATE },
    /* must come last because has null field (meaning no arg) */
    { "*T|mpeg?1|mp1|", AWCMP_MPEG_LAYER_I },
    { NULL, 0 }
};

#define DFLT_ALGO "no"
#define DFLT_FXBR 192000
int algoType;
float fixedBR;
int nokbd = -1;
int debug = -99;
char *inpath = NULL, *outpath;

CLE_ENTRY clargs[] = {
    { "-A|--algo?rithm", CLE_T_TOKEN,  "algorithm type",
	  DFLT_ALGO, &algoType, NULL, algoTable },
    { "-F?ixbr", CLE_T_FLOAT, "fixed bitrate",
	  QUOTE(DFLT_FXBR), &fixedBR, NULL, NULL },
    { "-K|-nokbd", CLE_T_BOOL, "ignore the keyboard interrupt",
	  NULL, &nokbd, NULL, NULL },
    { "-D|--de?bug", CLE_T_INT, "debug level", 
          NULL, &debug, NULL, NULL },
    { "", CLE_T_STRING, "input sound file name",
	  NULL, &inpath, NULL, NULL },
    { "", CLE_T_STRING, "output compressed file name",
	  "", &outpath, NULL, NULL },
    { "", CLE_T_END, NULL,	
	  NULL, NULL, NULL, NULL }
};

int main(int argc, char **argv) {

    /* test that NULL means don't set a default, but "0" sets to zero */
    debug = -99;
    nokbd = -1;
    fixedBR = -1.0;
    cleSetDefaults(clargs);
    if (debug != -99) {
	fprintf(stderr, "debug defaulted to %d\n", debug);
	exit(-1);
    }
    if (nokbd != 0) {	/* implicit defaults for bools? */
	fprintf(stderr, "nokbd defaulted to %d\n", nokbd);
	exit(-1);
    }
    if (fixedBR != DFLT_FXBR) {
	fprintf(stderr, "fixBR defaulted to %f\n", fixedBR);
	exit(-1);
    }

    if(cleHandleArgs(clargs, argc, argv)) {
	cleUsage(clargs, argv[0]);
	exit(-1);
    }
    printf("algoType=%d fixedBR=%.1f nokbd=%d debug=%d\n", 
	   algoType, fixedBR, nokbd, debug);
    printf("inpath=%s\n", inpath?inpath:"(NULL)");
    printf("outpath=%s\n", outpath?outpath:"(NULL)");
    exit(0);
}
#endif /* MAIN */

