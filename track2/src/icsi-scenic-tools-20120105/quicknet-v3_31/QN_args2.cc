#ifndef NO_RCSID
const char *QN_args2_rcsid =
  "$Header: /u/drspeech/repos/quicknet2/QN_args2.cc,v 1.5 2011/03/15 02:10:02 davidj Exp $";
#endif

#include <assert.h>
#include <math.h>	/* atof */
#include <stdlib.h>	/* atoi? */
#include <string.h>	/* strdup() */
#include "QN_args2.h"

#define QN_ARG2_STRLEN 256

#ifdef DEBUG
#define DBGFPRINTF(a)	fprintf a
#else /* !DEBUG */
#define DBGFPRINTF(a)	/* nothing */
#endif /* DEBUG */


#ifndef QN_HAVE_STRDUP
extern char *strdup(const char *s1);
#endif  /* QN_HAVE_STRDUP */

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
#define QN_ARG2_RIGHT_MARGIN 76

static void coPrint(FILE *stream, int *curcol, const char *str, int first, int rest)
{   /* copy the string <s> to output <stream>.  Start the first line at
       column <first>; perform word-wrapping, starting subsequent lines 
       at column <rest>. */
    int rm = QN_ARG2_RIGHT_MARGIN;
    int i, len, skip, crlen;
    int startcol = first;
    char partstr[QN_ARG2_STRLEN];
    char *s;

    while (*str) {
	/* copy up to first carriage return */
	crlen = strcspn(str, "\n");
	assert(crlen < QN_ARG2_STRLEN);
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
    const char *s;
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

static void cleMakeTokenVal(char *val, QN_Arg2Token *tktab)
{   /* make a value string of the form "{a|b|c}" from a token table */
    QN_Arg2Token *tk = tktab;

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
    while ( (t = (char *) strchr(s,':')) != NULL ) {
	y = strtol(s, &t, 10);
	if (*t != ':') {
	    /* found something else */
	    fprintf(stderr, "cleScanTime: error at chr %ld of '%s'\n", (long) (t - s), s);
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
#define DFLTS_RCOL   (QN_ARG2_RIGHT_MARGIN)

void cleUsage(QN_Arg2Entry *entries, char *programName)
{   /* print a usage message to stderr, based on the QN_Arg2Entry table */
    QN_Arg2Entry *ent = entries;
    char val[QN_ARG2_STRLEN], str[QN_ARG2_STRLEN];
    const char *flag, *usage, *deflt;
    int curcol = 0;

    fprintf(stderr, "%s: command line flags (defaults in parens):\n", 
	    programName);
    while(ent->type != QN_ARG2_NOMOREARGS)  {
	flag = ent->flag; usage = ent->usage; deflt = ent->deflt;
	if (!flag) flag = "";
	if (!usage) usage = "";
	switch(ent->type)  {
	case QN_ARG2_INT:		strcpy(val, "<int>");	break;
	case QN_ARG2_LONG:	strcpy(val, "<long>");	break;
	case QN_ARG2_FLOAT:	strcpy(val, "<float>"); break;
	case QN_ARG2_TIME:	strcpy(val, "<hh:mm:ss.sss>"); break;
	case QN_ARG2_BOOL:	strcpy(val, "");	break;
	case QN_ARG2_INVBOOL:	strcpy(val, "");	break;
	case QN_ARG2_STR:	strcpy(val, "<string>");    break;
	case QN_ARG2_SPECIAL:	strcpy(val, "<arg>");	break;
	case QN_ARG2_TOKEN:	
	    cleMakeTokenVal(val, (QN_Arg2Token *)ent->specData);	
	    break;
	/* Totally ignore the flag portion of T_USAGE */
	case QN_ARG2_DESC:	strcpy(val, ""); break;	

	default:
	    fprintf(stderr, "cleUsage: unknown type %d\n", ent->type);
	    exit(1);
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

int QN_arg2_setdefaults(QN_Arg2Entry *entries)
{  /* go through whole entries table setting up the defaults first */
    QN_Arg2Entry *ent = entries;
    char *arg;
    int err = 0;

    while(ent->type != QN_ARG2_NOMOREARGS)  {
	/* Boolean types have implicit defaults, regardless of the dflt fld */
	if( (arg = ent->deflt) != NULL \
	    || ent->type == QN_ARG2_BOOL \
	    || ent->type == QN_ARG2_INVBOOL)  {
	    switch(ent->type)  {
	    case QN_ARG2_BOOL:
		*(int *)(ent->where) = 0; break;
	    case QN_ARG2_INVBOOL:
		*(int *)(ent->where) = 1; break;

	    case QN_ARG2_INT:
		{ int i; err = (sscanf(arg, "%d", &i) != 1); 
		  if (!err) *(int *)(ent->where) = i; }   break;
	    case QN_ARG2_LONG:
		{ long l; err = (sscanf(arg, "%ld", &l) != 1); 
		  if(!err) *(int *)(ent->where) = l; }    break;
	    case QN_ARG2_FLOAT:
		{ float f; err = (sscanf(arg, "%f", &f) != 1); 
		  if(!err) *(float *)(ent->where) = f; }  break;
	    case QN_ARG2_TIME:
		{ float f; err = (cleScanTime(arg, &f) != 1); 
		  if(!err) *(float *)(ent->where) = f; }  break;
	    case QN_ARG2_STR:
		*(char **)(ent->where) = strdup(arg); break;
	    case QN_ARG2_TOKEN:
		err = cleTokenSetFn(arg, ent->where, ent->flag, ent->specData);
		break;
	    case QN_ARG2_SPECIAL:
		err = (*(ent->specialfn))(arg, ent->where, ent->flag,
					  ent->specData);
		break;
	    case QN_ARG2_DESC:
		break;
	    default:
		fprintf(stderr, "QN_arg2_setdefaults: unknown CLE_TYPE %d\n", ent->type);
		exit(1);
	    }
	}
	++ent;
    }
    return err;
}

static int cleMatchPat(char *arg, const char *multipat, int *pused, char **ppat, 
		       int *pval)
{   /* do the necessary expansions to match arg against the multipat;
       return 1 if it actually matched else 0.
       Set *pused to 0 if the arg should not be consumed. 
       Set *ppat to the actual pattern that matched.
       *pval takes the actual value for %d/%f flags. */
    int minlen;
    const char *state = NULL;
static char pat[QN_ARG2_STRLEN];	/* gets returned in *ppat, so make static ?? */
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

int cleTokenSetFn(char *arg, void *where, char *fromflag, void *cle_tokens)
{   /* set an int value from a QN_Arg2Token table, return 1 if arg used, 0 if 
       no arg used, -1 if error (unrecog) */
    int *pint = (int *)where;
    QN_Arg2Token *tk = (QN_Arg2Token *)cle_tokens;
    int found = 0;
    int used;
    char *pat;

    while(!found && tk->string != NULL) {
	found = cleMatchPat(arg, tk->string, &used, &pat, pint);
	if( found && (strcmp(pat, "%d")) != 0) {
	    if (strcmp(pat, "%f") == 0) {
		/* scale by argument and write as int */
		*pint = (int) ((float) tk->code * *((float *)pint));
	    } else {
		*pint = tk->code;
	    }
	}
	++tk;
    }
    if(found)	return used;
    else	return -1;
}

int cleHandleArgs(QN_Arg2Entry *entries, int argc, char *argv[])
{  /* parse a set of command line arguments according to a structure */
    int err = cleExtractArgs(entries, &argc, argv, 1);
    return err;
}

int cleExtractArgs(QN_Arg2Entry *entries, int* pargc, char *argv[], 
		   int noExtraArgs)
{  /* parse a set of command line arguments according to a structure.
      If noExtraArgs is not set, allow for a variable number of arguments 
      that we don't parse; return modified argc and argv pointing to the 
      residue. Return value is number of actual error objected to. */
    int i, errs = 0, anerr, rc, argUsed, tokDone, toklen, minlen;
    size_t flaglen;
    int blankIndex = 0;     /* use each blank index just once */
    int blankCount;
    char *next, *arg, *tok;
    char tokbuf[QN_ARG2_STRLEN];
    char flag[QN_ARG2_STRLEN];
    QN_Arg2Entry *ent;
    int outargc = 1;
    int argc = *pargc;
    const char *state;
    int tokcontinued = 0;	/* flag that reparsing remainder */
    char *programName = argv[0];

    /* first, set all defaults - we can overwrite them later */
    /* QN_arg2_setdefaults(entries); */  /* no, let program do this if it wants */
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
	while(!tokDone && ent->type != QN_ARG2_NOMOREARGS)  {
	    state = NULL;
	    while(!tokDone && ent->type != QN_ARG2_DESC && \
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
		    case QN_ARG2_INT:
		    { int x; anerr = (arg==NULL) || (sscanf(arg, "%d", &x) != 1); 
		      if (!anerr) *(int *)(ent->where) = x; }   
		    argUsed = 1; break;
		    case QN_ARG2_LONG:
		    { long l; anerr = (arg==NULL) || (sscanf(arg, "%ld", &l) != 1); 
		      if(!anerr) *(int *)(ent->where) = l; }
		    argUsed = 1; break;
		    case QN_ARG2_FLOAT:
		    { float f; anerr = (arg==NULL) || (sscanf(arg, "%f", &f) != 1); 
		      if(!anerr) *(float *)(ent->where) = f; }
		    argUsed = 1; break;
		    case QN_ARG2_TIME:
		    { float f; anerr = (arg==NULL) || (cleScanTime(arg, &f) != 1); 
		      if(!anerr) *(float *)(ent->where) = f; }
		    argUsed = 1; break;
		    case QN_ARG2_STR:
			if(arg==NULL) { anerr=1; } else {
			/* free any default value */
			if (*(char **)ent->where)  free(*(char **)ent->where);
			*(char **)(ent->where) = strdup(arg); argUsed = 1; }
			break;
		    case QN_ARG2_BOOL:
			++(*(int *)(ent->where)); break;
		    case QN_ARG2_INVBOOL:
			--(*(int *)(ent->where)); break;
		    case QN_ARG2_TOKEN:
			rc = cleTokenSetFn(arg,ent->where, tok, ent->specData);
			if(rc == 1) argUsed = 1; else if(rc != 0) anerr = 1;
			break;
		    case QN_ARG2_SPECIAL:
			rc = (*(ent->specialfn))(arg, ent->where, tok,
						 ent->specData);
			if(rc == 1) argUsed = 1; else if(rc != 0) anerr = 1;
			break;
		    default:
			fprintf(stderr, "cleExtractArgs: unknown CLE_TYPE %d\n", ent->type);
			exit(1);
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

QN_Arg2Token testTable[] = {
    { "*F", 0 }, 	/* -rasta no */
    { "j?ah", 1 }, 
    { "cj?ah", 2 }, 
    { "*T|log|", 3 },	/* -rasta yes or -rasta log or -rasta */
    { NULL, 0}
};

enum {AWCMP_NONE, AWCMP_MPEG_LAYER_I, AWCMP_MPEG_LAYER_II, AWCMP_MULTIRATE};
QN_Arg2Token algoTable[] = {
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

QN_Arg2Entry clargs[] = {
    { "-A|--algo?rithm", QN_ARG2_TOKEN,  "algorithm type",
	  DFLT_ALGO, &algoType, NULL, algoTable },
    { "-F?ixbr", QN_ARG2_FLOAT, "fixed bitrate",
	  QUOTE(DFLT_FXBR), &fixedBR, NULL, NULL },
    { "-K|-nokbd", QN_ARG2_BOOL, "ignore the keyboard interrupt",
	  NULL, &nokbd, NULL, NULL },
    { "-D|--de?bug", QN_ARG2_INT, "debug level", 
          NULL, &debug, NULL, NULL },
    { "", QN_ARG2_STR, "input sound file name",
	  NULL, &inpath, NULL, NULL },
    { "", QN_ARG2_STR, "output compressed file name",
	  "", &outpath, NULL, NULL },
    { "", QN_ARG2_NOMOREARGS, NULL,	
	  NULL, NULL, NULL, NULL }
};

int main(int argc, char **argv) {

    /* test that NULL means don't set a default, but "0" sets to zero */
    debug = -99;
    nokbd = -1;
    fixedBR = -1.0;
    QN_arg2_setdefaults(clargs);
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

