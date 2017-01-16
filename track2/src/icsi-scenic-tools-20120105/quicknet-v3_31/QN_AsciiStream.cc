const char* QN_AsciiStream_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_AsciiStream.cc,v 1.9 2010/10/29 02:20:37 davidj Exp $";

// This file contains some miscellaneous routines for handling 
// LabFtr_Ascii streams - segment data represented as lines of ascii.

#include <QN_config.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_Logger.h"

#include "QN_AsciiStream.h"

//
// QN_TokenMap (phoneset definition)
//

int QN_TokenMap::readPhset(const char *filename) {
    // Read a phoneset file into the array
    // first, lose existing data
    clear();

    FILE *f = fopen(filename, "r");
    if (f==NULL) {
	log.error("can't open phset '%s' for read", filename);
	return 0;	/* actually, log.error exits */
    }
    char buf[256];
    if (!fgets(buf, 256, f)) {
	fclose(f);
	log.error("can't read phset '%s'", filename);
	return 0;	/* actually, log.error exits */
    }
    int slen;
    char *tok, *eptr, *rest;
    slen = strlen(buf);
    // remove trailing cr
    if (slen && buf[slen-1] == '\n') {
	buf[slen-1] = '\0';
    }
    // first line should be token count
    ntokens = strtol(buf, &eptr, 10);
    if (eptr == buf || *eptr != '\0') {
	// couldn't parse number
	log.error("reading phset '%s': first line '%s' isn't count\n", 
		  filename, buf);
	fclose(f);
	ntokens = 0;
	return 0;	/* actually, log.error exits */
    }
    // allocate array
    tokenArray = new char*[ntokens];
    // read in the tokens
    int redtoks = 0;
    while (!feof(f)) {
	if (fgets(buf, 256, f)) {
	    slen = strlen(buf);
	    // remove trailing cr
	    if (slen && buf[slen-1] == '\n') {
		buf[slen-1] = '\0';
	    }
	    // find the token
	    tok = buf + strspn(buf, " \t");
	    slen = strcspn(tok, " \t\n\r");
	    if (slen > 0 && redtoks < ntokens) {
		// hold ptr to rest of line
		if ((int)strlen(tok) > slen) {
		    rest = tok+slen+1;
		} else {
		    rest = NULL;
		}
		// just truncate at first ws
		tok[slen] = '\0';
		// .. and copy
		tokenArray[redtoks] = strdup(tok);
		// If there is a rest, check it
		if (rest) {
		    int restnum = strtoul(rest, NULL, 10);
		    if (restnum != redtoks) {
			log.log(QN_LOG_PER_RUN, 
				"reading phset '%s': rest of line '%s' after tok '%s' not expected num %d", filename, rest, tok, redtoks);
			// but go on
		    }
		}
		++redtoks;
	    } else {
		log.error("phset '%s' has too many tokens (%d) with '%s'\n", 
			  filename, redtoks, buf);
	    }
	}
    }
    fclose(f);
    if (ntokens < redtoks) {
	log.error("read phset '%s' but got only %d of %d\n", 
		  filename, redtoks, ntokens);
    }
    ntokens = redtoks;
    return(ntokens);
}

int QN_TokenMap::writePhset(const char *filename) {
    // Write as a phoneset file
    FILE *f = fopen(filename, "w");
    if (!f) {
	log.error("can't open phset '%s' for write\n", filename);
	return(0);
    }
    // write the total number of phons
    fprintf(f, "%d\n", ntokens);
    // write each token
    int i;
    for(i=0; i<ntokens; ++i) {
	fprintf(f, "%s %d\n", tokenArray[i], i);
    }
    // all done
    fclose(f);
    return(ntokens);
}

int QN_TokenMap::lookup(const char *tok) {
    // search the map for one matching
    int i;
    for (i=0; i < ntokens; ++i) {
	if(strcmp(tok, tokenArray[i]) == 0)	return i;
    }
    // didn't find it
    return -1;
}

void QN_TokenMap::clear(void) {
    // Free up existing map
    int i; 
    for(i=0; i<ntokens; ++i) {
	free(tokenArray[i]);
    }
    delete [] tokenArray; 
    ntokens=0; 
    tokenArray=NULL;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_Ascii
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrLabStream_Ascii::QN_InFtrLabStream_Ascii(int a_debug, const char* a_dbgname,
			 FILE* a_file, 
			 size_t a_num_ftrs, size_t a_num_labs, 
			 int a_indexed, QN_TokenMap *a_tokmap /* = NULL */)
    : log(a_debug, "QN_InFtrLabStream_Ascii", a_dbgname), 
      file(a_file), 
      indexed(a_indexed), 
      num_ftr_cols(a_num_ftrs), 
      num_lab_cols(a_num_labs), 
      current_seg(-1), 
      current_frame(0),
      line(NULL), 
      tokmap(a_tokmap)
{
    log.log(QN_LOG_PER_RUN, "Started reading AsciiFile '%s'.", 
	    QN_FILE2NAME(file));
    if (indexed) {
	log.error("Cannot use Ascii streams ('%s') in indexed mode.", 
	    QN_FILE2NAME(file));
    }
    // Allocate initial line buffer
    linelen = QN_ASC_DEFAULT_LINELEN;
    line = (char*)malloc(linelen);
    if (line == NULL) {
	log.error("Out of memory trying to allocate %d bytes for ascii"
		  " input buffer for file '%s'.", 
		  linelen, QN_FILE2NAME(file));
    }
    line[0] = '\0';
}

QN_InFtrLabStream_Ascii::~QN_InFtrLabStream_Ascii() {
    if (line) {
	free(line);
    }
}

char *QN_InFtrLabStream_Ascii::my_fgets(char *s, int* pn, FILE *stream) {
    // like fgets but will realloc *s as much as necessary and 
    // adjust *pn accordingly
    char *e;
    int len, got = 0;
    s[0] = '\0';
    while (1) {
	e = fgets(s+got, *pn-got, stream);
	// Did we get it all?
	len = strlen(s+got);
	if (len > 0 && s[got+len-1] == '\n') {
	    // Got up to EOL chr; OK
	    // DON'T return the newline
	    s[got+len-1] = '\0';
	    return s;
	} else {
	    // didn't get EOL
	    // Is it EOF?
	    if (feof(stream)) {
		// Hit EOF, so just return whatever we got
		return s;
	    }
	    // Not EOF, no CR - must be buffer full
	    assert(got+len == (*pn - 1));
	    // double the buffer size
	    *pn = 2 * *pn;
	    //fprintf(stderr, "my_fgets: len=%d\n", *pn);
	    //s = (char *)realloc(s, *pn);
	    char *new_s = (char *)malloc(*pn);
	    if ( new_s == NULL ) {
		log.error("Out of memory trying to allocate %d bytes for ascii"
			  " input buffer for file '%s'.", 
			  *pn, QN_FILE2NAME(file));
	    }
	    strcpy(new_s, (const char *)s);
	    free(s);
	    s = new_s;

	    got += len;
	}
	// loop around and try again
    }
}

static int get_int(char *s, int *pos, int *pval) {
    // Read an integer from the front of s.  But must be followed by 
    // a space or EOL.  Return 1 on success, 0 on failure, value in *pval.
    // *pos is the start point within s, which is updated to the following 
    // char
    int nchrs = 0;
    /* int found = */ sscanf(s+*pos, "%d%n", pval, &nchrs);
    *pos += nchrs;
    //    return found;
    return (nchrs>0);
}

static int get_tok(char *s, int *pos, int *pval, QN_TokenMap *tokmap) {
    // Read a token from the front of s, which must be know to the 
    // tokmap, and return its numeric code from tokmap in *pval, updating
    // pos accordingly.  Return 1 on success, 0 if couldn't parse.
    int startpos = *pos + strspn(s + *pos, " \n\t\r");
    int toklen = strcspn(s+startpos, " \n\t\r");
    if (toklen && tokmap) {
	// Terminate the string temporarily
	int c = s[startpos+toklen];
	s[startpos+toklen] = '\0';
	// Look it up
	int code = tokmap->lookup(s+startpos);
	s[startpos+toklen] = c;
	if (code != -1) {
	    /* found it! */
	    *pval = code;
	    *pos = startpos+toklen;
	    return 1;
	}
    }
    /* didn't find */
    return 0;
}

static int get_float(char *s, int *pos, float *pval) {
    // Read an float from the front of s.  But must be followed by 
    // a space or EOL.  Return 1 on success, 0 on failure, value in *pval.
    // *pos is the start point within s, which is updated to the following 
    // char
    int nchrs = 0;
    int found = sscanf(s+*pos, "%f%n", pval, &nchrs);
    *pos += nchrs;
    return found;
}

size_t QN_InFtrLabStream_Ascii::read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs) {
    // Read the next line(s) from the ascii file
    size_t got = 0, remain = cnt;
    int i, redseg, redframe, ec, pos;
    int dummyi;
    float dummyf;
    float *fltptr = &dummyf;

    while (remain) {
	// Unconsumed line from last attempt will still be in *line, 
	// else read the next one
	if (strlen(line) == 0) {
	    line = my_fgets(line, &linelen, file);
	}
	pos = 0;
	// Line starts with utterance and frame number
	ec = get_int(line, &pos, &redseg);
	if (ec == 0) {
	    // Like EOF?
	    if (feof(file)) {
		break;
	    } else {
		// maybe skip skippable lines
		pos += strspn(line+pos, " \t");
		// if first chr is "#", skip it
		if (strlen(line+pos) > 0 && line[0] != '#') {
		    log.error("unrecognized line '%s' at frame %d of seg %d"
			      " in file '%s'", 
			      line, current_frame, 
			      current_seg, QN_FILE2NAME(file));
		}
	    }
	    // else read another line
	} else {
	    if (redseg == current_seg) { 
		// still in current frame
		if (!get_int(line, &pos, &redframe)) {
		    log.error("no frame number at frame %d in segment %d"
			      " of file '%s'", redseg, current_frame, 
			      current_seg, QN_FILE2NAME(file));
		}
		if (redframe != current_frame) {
		    log.error("got frame number %d expecting %d in segment %d"
			      " of file '%s'", redframe, current_frame, 
			      current_seg, QN_FILE2NAME(file));
		}
		// seg and frame OK - read ftrs
		for(i = 0; i < (int)num_ftr_cols; ++i) {
		    if (ftrs) fltptr = ftrs++;
		    if (!get_float(line, &pos, fltptr)) {
			log.error("error reading ftr at col %d of frame %d "
				  " in segment %d of file '%s'", 
				  i, current_frame, 
				  current_seg, QN_FILE2NAME(file));
		    }
		}
		// read labs
		for(i = 0; i < (int)num_lab_cols; ++i) {
		    // Maybe try to read as symbol
		    if (tokmap == NULL \
			|| !get_tok(line, &pos, &dummyi, tokmap)) {
			// Otherwise, just read as number
			if (!get_int(line, &pos, &dummyi)) {
			    log.error("error reading lab '%s' at col %d of"
				      " frame %d in segment %d of file '%s'", 
				      line+pos, i, current_frame, 
				      current_seg, QN_FILE2NAME(file));
			}
		    }
		    if (labs) *labs++ = (QNUInt32)dummyi;
		}
		// check to EOL
		pos += strspn(line+pos, " \t");
		if (strlen(line+pos) > 0) {
		    log.error("extra stuff '%s' at end of frame %d of seg %d"
			      " in file '%s'", 
			      line+pos, current_frame, 
			      current_seg, QN_FILE2NAME(file));
		}
		// Done with this frame
		++current_frame;
		++got;
		--remain;
		// Make sure we don't read this line again next time
		line[0] = '\0';

	    } else if (redseg == current_seg + 1) {
		// Looking at start of next segment - end this reading
		break;
	    } else {
		log.error("saw segment number %d in segment %d of file"
			  " '%s'", redseg, current_seg, QN_FILE2NAME(file));
	    }
	}
    }
    return got;
}

int QN_InFtrLabStream_Ascii::get_pos(size_t* segnop, size_t* framenop) {
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (current_seg==-1) {
	segno = QN_SIZET_BAD;
	frameno = QN_SIZET_BAD;
    } else {
	segno = (size_t) current_seg;
	frameno = (size_t) current_frame;
	assert(current_frame>=0);
    }
    if (segnop!=NULL)
	*segnop = segno;
    if (framenop!=NULL)
	*framenop = frameno;
    log.log(QN_LOG_PER_SENT, "Currently at sentence %lu, frame %lu.",
	    (unsigned long) segno, (unsigned long) frameno);
    return QN_OK;
}

QN_SegID QN_InFtrLabStream_Ascii::nextseg() {
    // Step on to start of next seg
    int get = 999;
    int got = get;

    while (got == get) {
	got = read_ftrslabs(get, NULL, NULL);
    }
    // short return means we saw EOS
    ++current_seg;
    current_frame = 0;
    // was it EOF?
    if (feof(file)) {
	return QN_SEGID_BAD;
    }
    
    return (QN_SegID) 0;
}

int QN_InFtrLabStream_Ascii::rewind() {
    // Move to the start of the data and initialise our own file offset.
    if (fseek(file, 0, SEEK_SET)!=0) {
	log.error("Rewind failed to move to start of data in "
		 "'%s'.",
		 QN_FILE2NAME(file));
    }
    current_seg = -1;
    current_frame = 0;
    log.log(QN_LOG_PER_EPOCH, "At start of PFile.");
    return 0;
}



////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_Ascii - access a File for output
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_OutFtrLabStream_Ascii::QN_OutFtrLabStream_Ascii(int a_debug, 
						   const char* a_dbgname,
						   FILE* a_file, 
						   size_t a_num_ftrs, 
						   size_t a_num_labs, 
						   int a_indexed, 
						   QN_TokenMap *a_tokmap,
						   int a_oldstyle)
    : log(a_debug, "QN_OutFtrLabStream_Ascii", a_dbgname), 
      file(a_file), 
      indexed(a_indexed), 
      num_ftr_cols(a_num_ftrs), 
      num_lab_cols(a_num_labs), 
      current_seg(0), 
      current_frame(0),
      tokmap(a_tokmap),
      oldstyle(a_oldstyle)
{
    log.log(QN_LOG_PER_RUN, "Started writing AsciiFile '%s'.", 
	    QN_FILE2NAME(file));
    if (indexed) {
	log.error("Cannot use Ascii streams ('%s') in indexed mode.", 
	    QN_FILE2NAME(file));
    }
}

QN_OutFtrLabStream_Ascii::~QN_OutFtrLabStream_Ascii() {
}

void QN_OutFtrLabStream_Ascii::write_ftrslabs(size_t cnt, const float* ftrs,
					      const QNUInt32* labs) {
    // Write some frames. ftrs and labs can be null, meaning fill with zeros
    int i;
    float dummyf = 0;
    unsigned int dummyi = 0;

    while (cnt-- > 0) {
	// Write the frame header
	fprintf(file, "%ld %ld", current_seg, current_frame);
	// Write features
	for (i = 0; i < (int)num_ftr_cols; ++i) {
	    if (ftrs)
		dummyf = *ftrs++;
	    if (oldstyle)
		fprintf(file, " %f", dummyf);
	    else
		fprintf(file, " %g", dummyf);
	}
	// Write labels
	for (i = 0; i < (int)num_lab_cols; ++i) {
	    if (labs) dummyi = (unsigned int)*labs++;
	    const char *s;
	    if (tokmap && (s = tokmap->index(dummyi)) != NULL ) {
		fprintf(file, " %s", s);
	    } else {
		fprintf(file, " %u", dummyi);
	    }
	}
	// EOL
	fprintf(file, "\n");
	++current_frame;
    }
}

void QN_OutFtrLabStream_Ascii::doneseg(QN_SegID /* segid */) {
    ++current_seg;
    current_frame = 0;
}

