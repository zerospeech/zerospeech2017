//
// Range.C
//
// Another implementation of the range specification
// based on Jeff's and Eric's.
//
// 1998sep29 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacat/Range.C,v 1.17 2010/10/29 03:41:52 davidj Exp $

// Ranges are a way of specifying a range of integers (indexes) 
// with an ascii string.  For instance, you could say:
//   1,2,3,5,6,7,9,10,11
// or
//   1-3,5-7,9-11
// or 
//   1-11/4,8
// where numbers after a slash are omitted.
// You can also repeat terms, and present them in any order:
//   5,5,5,4,4,4,3,3,3
// although the definitions after a slash are repetition- and 
// order-insensitive.

// The full syntax is:
//      RANGESPEC -> @FILENAME|RANGELIST	Top level can be file of ixs
//  	RANGELIST -> RANGETERM[;RANGELIST]	Semicolon-separated terms
//	RANGETERM -> SEQUENCE[/SEQUENCE]	Items after slash are excluded
//	SEQUENCE -> RANGE[,SEQUENCE]		Comma-separated terms 
//	RANGE -> [POSITION][[:NUMBER]:[POSITION]] Matlab range specification
//	POSITION -> [^]NUMBER			Jeff's from-the-end format
// "-" is a synonym for ":", SPACE is a synonym for "," and 
// RETURN is a synonym for ";". 

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "Range.H"

#ifdef DEBUG
#define DBGFPRINTF(a)	fprintf a
#else /* !DEBUG */
#define DBGFPRINTF(a)	/* nothing */
#endif /* DEBUG */

RangeNode** FindLastRangeList(RangeNode** last) {
    while(*last != NULL) {last = &((*last)->next); }
    return last;
}

unsigned int RangeListSize(RangeList list) {
    unsigned int len = 0;
    while(list != NULL) {list = list->next; ++len; }
    return len;
}

int RangeNode::SubtractVal(int val) {
    // Knock out one value from this node, which may involve splitting
    // it into two nodes, which will be silently inserted into the list.
    fixup_end();
    if ( !(val < start || val > end || ((val-start)%step) != 0)) {
	if (val == start) {
	    start += step;
	} else if (val == end) {
	    end -= step;
	} else {
	    // Need to split the list
	    next = new RangeNode(next, val + step, end, step);
	    end = val - step;
	}
    }
    return (n_pts() > 0);
}

int RangeNode::SubtractRange(RangeNode *sub) {
    // Try to subtract an entire range
    if (sub->n_pts() == 1) {
	return SubtractVal(sub->start);
    } else if ( (sub->start < start && sub->end < start) \
		|| (sub->start > end && sub->end > end) ) {
	// No overlap, just ignore
    } else if (step == sub->step || step == -sub->step) {
	// steps are synchronized.  Only effect if same phase
	if ( (start%step) == (sub->start % step) ) {
	    int start_index = (sub->start - start)/step;
	    int end_index = (sub->end - start)/step;
	    if (end_index < start_index) { 
		int t = start_index; start_index = end_index; end_index = t;
	    }
	    if (start_index <= 0 && end_index >= ((int)n_pts())-1) {
		// everything wiped out - make n_pts() == 0
		end = start - step;
	    } else if ( start_index > 0 && end_index < ((int)n_pts())-1) {
		// fully-included range - have to split list
		next = new RangeNode(next, start+step*(end_index+1), end,step);
		end = start+step*(start_index - 1);
	    } else if ( start_index > 0) {
		// just an initial sublist
		end = start+step*(start_index - 1);
	    } else {
		assert(end_index < ((int)n_pts())-1);
		start += step*(end_index+1);
	    }
	}
	// else nothing
    } else {
	// The overlap and they have different steps - do it 
	// point by point
	int i;
	for (i = sub->start; i <= sub->end; i += sub->step) {
	    SubtractVal(i);
	}
    }
    return (n_pts() > 0);
}

void FreeRangeList(RangeList list) {
    RangeNode *doomed;
    while(list) {
	doomed = list;
	list = doomed->next;
	delete doomed;
    }
}

int CanonicalRangeList(RangeList *plist) {
    // Remove null nodes, possibly merge adjacent nodes
    RangeNode *cur, *nxt;

    while (*plist) {
	cur = *plist;
	if (cur->n_pts() <= 0) {
	    // Null list - strip
	    *plist = cur->next;
	    delete cur;
	} else {
	    cur->fixup_end();
	    nxt = cur->next;
	    if (nxt != NULL) {
		if (cur->n_pts() == 1 && nxt->n_pts() == 1) {
		    cur->step = nxt->start - cur->start;
		    if (cur->step == 0)	cur->step = 1;
		}
		if ( (cur->end + cur->step == nxt->start) \
		     && (nxt->n_pts() == 1 || nxt->step == cur->step) ) {
		    // Absorb next pt, or ranges run together
		    cur->end = nxt->end;
		    cur->next = nxt->next;
		    delete nxt;
		    cur->fixup_end();
		} else {
		    plist = &(cur->next);
		}
	    } else {
	        plist = &(cur->next);
	    }
	}
    }
    return 1;
}

const char *Range::colons = ":-";   // valid separators for matlab range specs
const char *Range::commas = ", \t"; // valid separators for number lists
const char *Range::slashes = "/";   // valid separators of exclude lists
const char *Range::semis  = ";\n";  // valid top-level term separators
const char *Range::space = " \t";   // non-linebreak WS
const char *Range::WS = " \t\n\r";  // any WS
const char *Range::alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";


int Range::_parseNumber(int *pval) {
    // Grab the next number from the string
    // ignore leading WS
    int fromEnd = 0;
    int scale = 1;
    int val = 0;
    int len = 0;

    DBGFPRINTF((stderr, "_parseNumber: at '%s'\n", pos));

    pos += strspn(pos, space);
    char c = *pos;
    if (c == '^') {
	fromEnd = 1;
	++pos;
	c = *pos;
    }
    if (c == '-') {
	scale = -1;
	++pos;
	c = *pos;
    }
    if (c == '\0') {
	errmsg = "expecting number, hit EOS";
	DBGFPRINTF((stderr, "_parseNumber:%s @ '%s'\n", errmsg, pos));
	return 0;
    }
    while (c >= '0' && c <= '9') {
	val = 10*val + (c - '0');
	++len;
	++pos;
	c = *pos;
    }
    if (len == 0) {
	errmsg = "number not found";
	DBGFPRINTF((stderr, "_parseNumber:%s @ '%s'\n", errmsg, pos));
	return 0;
    }
    val = scale * val;
    if (fromEnd) {
	if (max_val == RNG_VAL_BAD) {
	    errmsg = "max_val not specified for ^ syntax";
	    DBGFPRINTF((stderr, "_parseNumber:%s @ '%s'\n", errmsg, pos));
	    return -1;
	}
	val = max_val - 1 - val;
    }
    DBGFPRINTF((stderr, "got number %d\n", val));
    *pval = val;
    return 1;
}

int Range::_parseMatlabRange(int *pstart, int *pstep, int *pend) {
    // Parse a matlab-style [start][:step][:[end]] string, 
    // which includes 1  1:5  1:2:10  :4  and :
    // Missing numbers take defaults from min_val and max_val
    int start = 0 , end = 0, step = 0, rc;

    DBGFPRINTF((stderr, "_parseRange: at '%s'\n", pos));

    // Either the first character is a colon, or it's a number
    // .. but skip leading WS
    pos += strspn(pos, space);
    char c = *pos;
    if (c == '\0') {
	errmsg = "expecting Matlab range, hit EOS";
	DBGFPRINTF((stderr, "_parseRange:%s @ '%s'\n", errmsg, pos));
	return 0;
    }
    // Try to get first number, OK to fail
    if ( (rc=_parseNumber(&start)) < 0) {
	// fatal error in number
	return rc;
    } else if (rc == 0) {
	// Next chr *must* be colon
	c = *pos;
	if (c == '\0' || strchr(colons, c) == NULL) {
	    errmsg = "Matlab range does not start with colon or number";
	    DBGFPRINTF((stderr, "_parseRange:%s @ '%s'\n", errmsg, pos));
	    return 0;
	}
	// It was a colon, but don't move over it, so we see it again blw
	//++pos;
	// default the first val
	if (min_val == RNG_VAL_BAD) {
	    errmsg = "min_val not specified for default range";
	    DBGFPRINTF((stderr, "_parseRange:%s @ '%s'\n", errmsg, pos));
	    return -1;
	}
	start = min_val;
    }
    // Parse up to two subseqent ":number" terms
    int i;
    int read_end = 0;
    end = start;
    for (i = 0; i < 2; ++i) {
	// Skip ws
	pos += strspn(pos, space);
	// Do we have a colon?
	c = *pos;
	if (c != '\0' && strchr(colons, c)) {
	    // We have a colon.  If this is the second pass, push the 
	    // first pass to the step
	    if (i > 0) {
		if (read_end == 0) {
		    // I think this means "::" found - just ignore
		    fprintf(stderr, "defaulted step (:: found?)\n");
		} else {
		    step = end;
		}
	    }
	    // Step over colon
	    ++pos;
	    // Either grab a number, or it defaults
	    if ( (rc=_parseNumber(&end)) < 0) {
		return rc;
	    } else if (rc == 0) {
		// Didn't find a number, so take default end
		if (max_val == RNG_VAL_BAD) {
		    errmsg = "max_val not specified for default range";
		    DBGFPRINTF((stderr, "_parseRange:%s @ '%s'\n", errmsg, pos));
		    return -1;
		}
		end = max_val - 1;
	    } else {
		read_end = 1;
	    }
	}
    }
    // Maybe choose appropriate default step
    if (step == 0) {
	if (end < start)	step = -1;
	else			step = 1;
    }
    // Should now have it all
    *pstart = start;
    *pend = end;
    *pstep = step;
    DBGFPRINTF((stderr, "got mat range %d:%d:%d @ '%s'\n", start, step, end, pos));
    return 1;
}

int Range::_parseRange(int *pstart, int *pstep, int *pend) {
    // Accept either a special-case token or a Matlab range
    // Special-case tokens
    int rc;
    int start, step, end;

    DBGFPRINTF((stderr, "_parseRange: at '%s'\n", pos));

    // Skip leading WS
    pos += strspn(pos, space);
    
    int slen = strspn(pos, alpha);
    if (slen == 3 && strncmp(pos, "all", slen) == 0) {
	if (min_val == RNG_VAL_BAD) {
	    errmsg = "min_val not specified for sequence \"all\"";
	    DBGFPRINTF((stderr, "_parseRange:%s @ '%s'\n", errmsg, pos));
	    return -1;
	}
	start = min_val;
	if (max_val == RNG_VAL_BAD) {
	    errmsg = "max_val not specified for sequence \"all\"";
	    DBGFPRINTF((stderr, "_parseRange:%s @ '%s'\n", errmsg, pos));
	    return -1;
	}
	end = max_val - 1;
	step = 1;
	pos += slen;
    } else if ( (slen == 3 && strncmp(pos, "nil", slen) == 0) \
		|| (slen == 4 && strncmp(pos, "none", slen) == 0)) {
	// Construct the sequence empty but consume the token
	start = 0; step = 1; end = -1;
	pos += slen;
    } else {
	// None of the ascii tokens - let it be Matlab ranges
	if ( (rc=_parseMatlabRange(&start, &step, &end)) <= 0) {
	    return rc;
	}
    }
    *pstart = start;
    *pstep = step;
    *pend = end;
    return 1;
}


int Range::_parseSequence(RangeList *seq) {
    // Read a sequence of matlab-ranges, return as a linked list of new
    // RangeNodes
    int start, end, step, rc;

    DBGFPRINTF((stderr, "_parseSeq: at '%s'\n", pos));

    // Have we seen a real preceding separator? 1=yes, -1=no, 0=maybe
    int seensep = 0;

    // Skip leading WS
    pos += strspn(pos, space);
    
    do {
	if ( (rc=_parseRange(&start, &step, &end)) < 0) {
	    return rc;
	} else if (rc == 1) {
	    // Found one, add it to the list
	    *seq = new RangeNode(*seq, start, end, step);
	    seq = &((*seq)->next);
	    // OK for another sequence to follow without extra sep
	    seensep = 0;
	} else if (seensep == 1) {
	    errmsg = "sequence separator not followed by sequence";
	    DBGFPRINTF((stderr, "_parseSeq:%s @ '%s'\n", errmsg, pos));
	    return -1;
	} else {
	    // No range seen, so don't expect another
	    seensep = -1;
	}
	// If next chr is valid seq separator, try again
	char c = *pos;
	if (c != '\0' && strchr(space, c)) {
	    seensep = 0;
	    pos += strspn(pos, space);
	    c = *pos;
	}
	if (c != '\0' && strchr(commas, c)) {
	    seensep = 1;
	    ++pos;
	    c = *pos;
	}
	DBGFPRINTF((stderr, "seqLoop: seensep=%d @ '%s'\n", seensep, pos));
    } while (seensep != -1);

    return 1;
}

int Range::_parseTerm(RangeList *seq) {
    // Parse the full SEQUENCE [/SEQUENCE] syntax
    RangeList positive = NULL, negative = NULL;
    int rc;

    DBGFPRINTF((stderr, "_parseTerm: at '%s'\n", pos));

    // Skip leading WS
    pos += strspn(pos, space);
    
    // Read first sequence
    if ( (rc = _parseSequence(&positive)) <= 0) {
	DBGFPRINTF((stderr, "_parseTerm:%s @ '%s'\n", errmsg, pos));
	return rc;
    }
    // Maybe a slash now?
    pos += strspn(pos, space);
    char c = *pos;
    if (c != '\0' && strchr(slashes, c)) {
	++pos;
	// Read negative range
	if ( (rc = _parseSequence(&negative)) <= 0) {
	    DBGFPRINTF((stderr, "_parseTerm:%s @ '%s'\n", errmsg, pos));
	    return rc;
	}
    }
    // Combine positive and negative sequences
    SubtractSeqs(&positive, negative);
    *seq = positive;
    FreeRangeList(negative);
    DBGFPRINTF((stderr, "_parseTerm:OK @ '%s'\n", pos));
    return 1;
}

int Range::_parseSpec(RangeList *seq) {
    // Parse the top level TERM [; TERM] syntax

    // Have we seen a real preceding separator? 1=yes, -1=no, 0=maybe
    int seensep = 1, rc;

    DBGFPRINTF((stderr, "_parseSpec: at '%s'\n", pos));

    do {
	// Read first term
	if ( (rc = _parseTerm(seq)) <= 0) {
	    DBGFPRINTF((stderr, "_parseSpec:%s @ '%s'\n", errmsg, pos));
	    return rc;
	}
	seq = FindLastRangeList(seq);
	// If next chr is valid seq separator, try again
	char c = *pos;
	seensep = -1;
	if (c != '\0' && strchr(space, c)) {
	    pos += strspn(pos, space);
	    c = *pos;
	}
	if (c != '\0' && strchr(semis, c)) {
	    seensep = 1;
	    ++pos;
	    c = *pos;
	}
    } while (seensep != -1);
    DBGFPRINTF((stderr, "_parseSpec:OK @ '%s'\n", pos));
    return 1;
}

int Range::_compileDefStr(void) {
    // Compile the range specification stored in the def_str
    int ok = 1;

    if(rangeList) FreeRangeList(rangeList);
    rangeList = NULL;
    pos = def_string;
    errmsg = "(unknown error)";
    ok = _parseSpec(&rangeList);
    if (ok == 1 && strlen(pos) > 0) {
	errmsg = "unparseable residue";
	ok = -1;
    }
    if (ok != 1) {
	FreeRangeList(rangeList);
	rangeList = NULL;
	_reportParseError(def_string);
	return 0;
    }
    CanonicalRangeList(&rangeList);
    return 1;
}

char *fgets_realloc(char **pbuf, int *pbufsize, FILE *file) {
    // Like fgets, but buf will be successively reallocated 
    // to accommodate lines that exceed the current buffer.
    // On entry, *pbuf is an existing buffer, allocated with 
    // new char[] (or NULL to allocate internally).  *pbufsize 
    // is the length of the preallocated buffer (or 0 to 
    // allocate default internally.  One whole line is 
    // read from file and returned in *pbuf as well as the 
    // return code.  On error, NULL is returned.
    int alreadygot = 0;
    int oldbufsize;
    char *rc;

    if(*pbuf == NULL) {
	if (*pbufsize < 1) { *pbufsize = BUFSIZ; }
	*pbuf = new char [*pbufsize];
    }
    (*pbuf)[0] = '\0';
    do {
        rc = fgets((*pbuf)+alreadygot, (*pbufsize)-alreadygot, file);
	oldbufsize = *pbufsize;
	alreadygot = strlen(*pbuf);
	if (alreadygot == oldbufsize-1) {
	    /* ran out of buffer - have to realloc */
	    char *newbuf;
	    *pbufsize *= 2;
	    newbuf = new char[*pbufsize];
	    memcpy(newbuf, *pbuf, oldbufsize);
	    delete [] *pbuf;
	    *pbuf = newbuf;
	    fprintf(stderr, "fgets_realloc: to %d buf chrs\n", *pbufsize);
	}
    } while (alreadygot == oldbufsize-1);
    // return null only if last call to fgets did AND we got nothing 
    // overall
    if (strlen(*pbuf) > 0) {
	rc = *pbuf;
    }
    return rc;
}

int Range::_compileSpecFile(char *filename) {
    // Spec is contained in a file.  Compile it all at once
    int bufsize = BUFSIZ;
    char *buf = new char[bufsize];
    FILE *infile;
    int ok, len, lineno = 0, rc = 0;
    RangeList *rngListEndPtr = NULL;
    char errmsgbuf[128];

    if(rangeList) FreeRangeList(rangeList);
    rangeList = NULL;
    rngListEndPtr = &rangeList;

    if ( (infile = fopen(filename, "r")) == NULL) {
	fprintf(stderr, "Range::_compileFileSpec: can't read '%s'\n", 
		filename);
	goto exit;
    }

    while (!feof(infile)) {
	// Read in a whole line from the file, into a magic expanding 
	// buffer
	fgets_realloc(&buf, &bufsize, infile);
	// on EOF, buf comes back zero len, which is OK
	len = strlen(buf);
	
	if (len > 0) {
	    ++lineno;

	    // Squash terminal CR if you find it
	    if (buf[len-1] == '\n') {
		buf[len-1] = '\0';
		--len;
	    }

	    // If first non-white chr is a '#', skip the line as a comment
	    char *c = buf + strspn(buf, " \n\t\r");
	    if (*c != '#') {

		// Parse it as a term, on the end of the current range
		pos = buf;
		errmsg = "(unknown error)";
		ok = _parseSpec(rngListEndPtr);

		if (ok == 1 && strlen(pos) > 0) {
		    errmsg = "unparseable residue";
		    ok = -1;
		}

		// Clean up & report if parse error
		if (ok != 1) {
		    FreeRangeList(rangeList);
		    rangeList = NULL;
		    sprintf(errmsgbuf, "%s @ line %d of %s", 
			    errmsg, lineno, filename);
		    errmsg = errmsgbuf;
		    _reportParseError(buf);
		    goto exit;
		}

		// Step on where to append next line
		rngListEndPtr = FindLastRangeList(rngListEndPtr);

	    }
	}
    }

    // Optimize list
    CanonicalRangeList(&rangeList);

  exit:
    // Clean up and exit
    if (infile) fclose(infile);
    delete [] buf;
    return rc;
}

void Range::_reportParseError(char *def_str) {
    // Print a report of the parse error using built-in data
    fprintf(stderr, "Range::Parse error: %s\n", errmsg);
    fprintf(stderr, ">> %s\n", def_str);
    char fmtstr[32];
    sprintf(fmtstr, ">> %%%lds\n", (pos-def_str));
    fprintf(stderr, fmtstr, "^");
}

void Range::PrintRanges(const char *tag/*=NULL*/, FILE *stream/*=stderr*/) {
    // For debug, print out the actual ranges specified
    RangeNode *node = rangeList;
    fprintf(stream, "**Ranges '%s' (npts=%u):\n", tag?tag:"", length());
    while(node) {
	fprintf(stream, "  %d:%d:%d\n", node->start, node->step, node->end);
	node = node->next;
    }
}

unsigned int Range::length(void) {
    // Total number of values specified by a list
    unsigned int size = 0;
    RangeList list = rangeList;
    while(list) {
	size += list->n_pts();
	list = list->next;
    }
    return size;
}

int Range::index(int ix) {
    // Value of ix'th index in list
    RangeList list = rangeList;
    int n_pts;
    int iix = ix;

    while(list) {
	n_pts = list->n_pts();
	if (iix < n_pts) {
	    return list->start + iix * list->step;
	}
	list = list->next;
	iix -= n_pts;
    }    
    // fell through!
    fprintf(stderr, "Error: wanted value %d but list length %d\n", 
	    ix, length());
    abort();
}

int SubtractSeqs(RangeList *ppos, RangeList neg) {
    // Go through making the subtractions from a range list
    RangeNode **pnextpos;
    RangeNode *doomed;

    // double loop
    while(neg) {
	pnextpos = ppos;
	while(*pnextpos) {
	    if ( (*pnextpos)->SubtractRange(neg) == 0) {
		// Signal to dispose of this node
		doomed = *pnextpos;
		*pnextpos = doomed->next;
		delete doomed;
	    } else {
		pnextpos = &((*pnextpos)->next);
	    }
	}
	neg = neg->next;
    }
    return 1;
}

int Range::SetLimits(int minval/*=0*/, int ulimval/*=RNG_VAL_BAD*/) {
    // Set new limits & recompute range, if necessary
    int rc = 0;
    if (min_val != minval || max_val != ulimval) {
	min_val = minval;
	max_val = ulimval;
	if (def_string != NULL) {
	    // Retrigger parsing with new limits
	    rc = SetDefStr(def_string);
	}
    }
    return rc;
}

int Range::SetDefStr(const char *spec) {
    // Set/change the range definition string
    int rc = 0;

    if (spec != def_string) {
	if (def_string) free(def_string);
	if (spec) {
	    def_string = strdup(spec);
	} else {
	    def_string = NULL;
	}
    }
    //    if (def_string == NULL) {
    //	// NULL def_str equivalent to ALL? (to be like Jeff's range)
    //def_string = strdup("all");
    //}
    if (def_string) {
	// Is it a file name, or a true range?
	if (def_string[0] == '@') {
	    /* type = RNG_TYPE_FILE; */
	    if ( (rc = _compileSpecFile(def_string+1)) > 0) {
		type = RNG_TYPE_LIST;
	    }	    
	} else {
	    if ( (rc = _compileDefStr()) > 0) {
		type = RNG_TYPE_LIST;
	    }
	}
    } else {
	def_string = NULL;
    }
    return rc;
}

int Range::first(void) {
    // it's an error to call first() or last() on an empty list
    assert(rangeList);
    return rangeList->start;
}

int Range::last(void) {
    RangeList list = rangeList;
    // it's an error to call first() or last() on an empty list
    assert(rangeList);
    while(list->next) {
	list = list->next;
    }
    return list->end;
}

int Range::contains(int val) {
    // return the *number of times* val occurs in the list
    int count = 0;
    RangeList list = rangeList;
    while(list) {
	if (list->step > 0) {
	    if (val >= list->start && val <= list->end 
		&& ((val - list->start) % list->step) == 0) {
		++count;
	    }
	} else {
	    if (val <= list->start && val >= list->end 
		&& ((list->start - val) % -(list->step)) == 0) {
		++count;
	    }
	}
	list = list->next;
    }
    return count;
}

int Range::full(void) {
    // A full range goes straight from min to max
    int rc = 0;
    if (rangeList != NULL 
	&& rangeList->next == NULL
	&& rangeList->start == min_val 
	&& rangeList->end == max_val - 1
	&& rangeList->step == ((min_val < max_val)?1:-1) ) {
	rc = 1;
    }
    return rc;
}

Range::Range(const char *defstr /*=NULL*/, int minval /*=0*/, 
	     int ulimval /*=RNG_VAL_BAD*/) {
    def_string = NULL;
    rangeList = NULL;
    type = RNG_TYPE_NONE;
    min_val=0; max_val=0;
    SetLimits(minval, ulimval);
    SetDefStr(defstr?defstr:"all");
}

Range::~Range() {
    if(def_string) free(def_string);
    if(rangeList)  FreeRangeList(rangeList);
}

Range::iterator::iterator (const Range& rng) 
    : filename(0), file_handle(0)   // implicit, but pleases purify
{
    reset(rng);
}

Range::iterator::iterator (const iterator& it) {
    // Copy constructor.  Tricky if we've got an open file
    cur_value = it.cur_value;
    atEnd = it.atEnd;
    myrange = it.myrange;

    cur_node = it.cur_node;

    filename = it.filename;
    if (it.file_handle) {
	if ( (file_handle = fopen(filename, "r")) == NULL) {
	    fprintf(stderr, "iterator::copycon: unable to reopen '%s'\n", 
		    filename);
	    abort();
	}
    } else {
	file_handle = NULL;
    }
    buflen = it.buflen;
    buf = NULL;
    bufpos = 0;
    bufline = 0;
}

Range::iterator::~iterator(void) {
    if (file_handle != NULL) {
	fclose(file_handle);
	file_handle = NULL;
    }
}

int Range::iterator::read_next (int *result, FILE *file_handle, const char* filename) {
    // Read the next int from a file
    // Read another number from the list file.  File needs just to be 
    // white-space delimited, but can have comment lines starting with "#"
    // Next read value is put into *result.  Return 1 on success, 0 at EOF
/*    static int buflen = 1024;
    static char *buf = NULL;
    static int bufpos = 0;
    static int bufline = 0; */

    const char *WS = " \t\n\r";

    if (buf == NULL) {
	buf = new char[buflen];
	buf[0] = '\0';
	bufpos = 0;
    }
    
    while (buf[bufpos] == '\0') {
	// need a new line
	char *ok = fgets(buf, buflen, file_handle);
	if (!ok) {
	    // EOF hit, nothing more got
	    return 0;
	}
	int got = strlen(buf);
	while (buf[got-1] != '\n' && !feof(file_handle)) {
	    // because line didn't end with EOL, we assume we ran out 
	    // of buffer space - realloc & keep going
	    assert(got == buflen-1);
	    int newbuflen = 2*buflen;
	    char *newbuf = new char[newbuflen];
	    memcpy(newbuf, buf, got);
	    delete [] buf;
	    buf = newbuf;
	    buflen = newbuflen;
	    fgets(buf+got, buflen, file_handle);
	    got = strlen(buf);
	}
	++bufline;
	// strip the trailing CR
	if (buf[got-1] == '\n') {
	    buf[got-1] = '\0';
	}
	// OK, now we've got a new line, continue
	bufpos = strspn(buf, WS);
	// but if it's a comment line, skip it by pretending it's a blank line
	if (buf[bufpos] == '#') {
	    buf[bufpos] = '\0';
	}
    }

    // Get the next number in the line buffer & advance pointer
    char *end;
    int val = strtol(buf+bufpos, &end, 10);

    // If unparseable, end points to start
    if (end == buf+bufpos) {
	fprintf(stderr, "unable to parse token '%s' as an integer at line %d of file '%s'\n", end, bufline, filename);
	return 0;
    }

    // Got the number
    *result = val;
    bufpos = end-buf;
    return 1;
}

int Range::iterator::reset (void) {
    return reset(*myrange);
}

int Range::iterator::reset (const Range& rng) {
    cur_node = NULL;
    file_handle = NULL;
    atEnd = 0;
    myrange = &rng;
    if (myrange->GetType() == RNG_TYPE_FILE) {
	filename = myrange->GetDefStr()+1;
	if ( (file_handle = fopen(filename, "r")) == NULL) {
	    fprintf(stderr, "Range::iterator: unable to read file '%s'\n", 
		    filename);
	    exit(-1);
	}
	buflen = 1024;
	buf = NULL;
	bufpos = 0;
	bufline = 0;
	if ( read_next(&cur_value, file_handle, filename) == 0) {
	    atEnd = 1;
	}
    } else {
	// Assume regular range
	cur_node = rng.getRangeList();
	if (cur_node) {
	    cur_value = cur_node->start;
	} else {
	    // Empty list
	    cur_value = 0;
	    atEnd = 1;
	}
    }
    return cur_value;
}

int Range::iterator::step_by(int n) {
    // Step on n steps, return resulting val
    if (file_handle) {
	int i = 0;
	while (i++ < n) {
	    next_el();
	}
    } else if (cur_node) {
	// short cuts
	int rem_this_node = (cur_node->end - cur_value)/cur_node->step;
	while (n > rem_this_node) {
	    cur_node = cur_node->next;
	    n -= rem_this_node;
	    if (cur_node == NULL) {
		cur_value = 0;
		atEnd = 1;
		break;
	    }
	    cur_value = cur_node->start;
	    rem_this_node = (cur_node->end - cur_value)/cur_node->step;
	}
	if (cur_node) {
	    cur_value = cur_value + cur_node->step * n;
	}
    }
    return cur_value;
}

int Range::iterator::next_el(void) {
    if (file_handle) {
	// Assume a file type
	if ( read_next(&cur_value, file_handle, filename) == 0) {
	    atEnd = 1;
	}
    } else if (cur_node) {
	if (cur_value == cur_node->end) {
	    // Need to move on to next node
	    cur_node = cur_node->next;
	    if (cur_node) {
		cur_value = cur_node->start;
	    } else {
		cur_value = 0;
		atEnd = 1;
	    }
	} else {
	    cur_value += cur_node->step;
	}
    }
    return cur_value;
}

int Range::iterator::current_range_finite(void) const {
    // When stepping down an iterator, sometimes you get an index which 
    // doesn't exist, and then you may want to know if the current 
    // range has explicitly-defined ends, or if one of them has 
    // been defaulted to INT_MAX or INT_MIN, in which case it's no 
    // surprise that something ran out (needed for feacat 'all' ranges 
    // on unindexed files).
    int finite = 1;
    // Actually, it's INT_MAX-1 that we really want to look for, because 
    // a range 'all' with min=0 and max=INT_MAX actually results 
    // in the range 0:1:(INT_MAX-1), because max is the value you never 
    // actually get to (in the current scheme).
    if (cur_node->end == INT_MAX || cur_node->end == INT_MIN \
	 || cur_node->end == INT_MAX-1 || cur_node->end == INT_MIN+1) {
	finite = 0;
    }
    return finite;
}


#ifdef MAIN
// Test code - just parse argv[1] and print the debug

int max_val = RNG_VAL_BAD;
int min_val = RNG_VAL_BAD;
char *spec = NULL;
int indx = RNG_VAL_BAD;
int debug = 0;
int quiet = 0;
int commas = 0;
int horizontal = 0;

extern "C" {
#include <dpwelib/cle.h>
}

CLE_ENTRY clargs [] = {
    { "Range object tester iterates a specified range", 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
    { "-u?pper", CLE_T_INT, "Default range upper limit", 
      NULL, &max_val, NULL, NULL }, 
    { "-l?ower", CLE_T_INT, "Default range lower limit", 
      NULL, &min_val, NULL, NULL }, 
    { "-i?ndex", CLE_T_INT, "Print just n'th value from list (from 0)", 
      NULL, &indx, NULL, NULL }, 
    { "-c?ommas", CLE_T_BOOL, "Separate each value with commas", 
      "0", &commas, NULL, NULL }, 
    { "-h?orizontal", CLE_T_BOOL, "Print ennumerated range horizontally", 
      "0", &horizontal, NULL, NULL }, 
    { "-d?ebug", CLE_T_BOOL, "Show internal representation of range", 
      "0", &debug, NULL, NULL }, 
    { "-q?uiet", CLE_T_BOOL, "Suppress printing the ennumerated range values", 
      "0", &quiet, NULL, NULL }, 
    { "<range>", CLE_T_USAGE, "Range specification string", NULL, 
      NULL, 0,0},
    { NULL, CLE_T_END, NULL, NULL, NULL, NULL, NULL }
};

main(int argc, char **argv) {
    char *programName;
    int err = 0;

    programName = argv[0];
    cleSetDefaults(clargs);
    err = cleExtractArgs(clargs, &argc, argv, 0);

    if (argc > 1) {spec = argv[1];}

    if(err || spec == NULL) {
	cleUsage(clargs, programName);
	exit(-1);
    }

    Range r(spec, min_val, max_val+1);
    if (debug) {
	r.PrintRanges(spec,stdout);
	// Is it full?
	fprintf(stdout, "first=%d last=%d full=%d contains(0)=%d\n",
		r.first(), r.last(), r.full(), r.contains(0));
    }

    if (indx != RNG_VAL_BAD) {
	int sby=5;
	fprintf(stderr, "list[%d] = %d\n", indx, r.index(indx));
	Range::iterator it = r.begin();
	int i;
	for (i = 0; i < indx; ++i) {
	    ++it;
	}
	int v1 = *it;
	it.step_by(sby);
	fprintf(stderr, "list[%d+%d] = %d..%d\n", indx, sby, v1, *it);
    }
    
    if (!quiet) {
	int first = 1;
	for (Range::iterator it = r.begin(); 
	     !it.at_end(); it++) {
	    // Handle any inter-term separator
	    if (first) {first=0;} else {
		if (commas) {
		    fprintf(stdout, ",");
		} else if (horizontal) {
		    fprintf(stdout, " ");
		}
		if (!horizontal) {
		    fprintf(stdout, "\n");
		}
	    }
	    // Print the actual term
	    fprintf(stdout, "%d", *it);
	}
	// Always a final return
	fprintf(stdout, "\n");
    }
}

#endif /* def MAIN */
