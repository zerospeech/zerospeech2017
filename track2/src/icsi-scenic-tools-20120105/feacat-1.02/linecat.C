//
// linecat.C
// 
// Select lines from a text file by parsing a range definition.
// - to correspond to feacat, labcat etc but for one-line-per-utt text files.
// 
// 1999feb28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacat/linecat.C,v 1.7 2010/10/29 03:41:52 davidj Exp $

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" {
#include <dpwelib/cle.h>
}

#include "Range.H"

static struct {
    char *outfilename;
    char *rangespec;
    char *commentpattern;
    int indexbase;
    int seekable;
    int repeatlines;
    int debug;
    int verbose;
} config;

CLE_ENTRY clargs[] = {
    { "Per-line text file range selection and rearrangement", 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
    { "linecat version " PACKAGE_VERSION, 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
    { "-o?utfile", CLE_T_STRING, "Output text file", 
      "-", &config.outfilename, NULL, NULL }, 
    { "-r?angespec|-sr|-ur", CLE_T_STRING, "Range specification for output lines", 
      "all", &config.rangespec, NULL, NULL },
    { "-c?ommentpattern", CLE_T_STRING, "Regexp to detect comment lines to ignore", 
      "", &config.commentpattern, NULL, NULL }, 
    { "-ind?exbase|-ixb", CLE_T_INT, "Effective index for first line of first file", 
      "0", &config.indexbase, NULL, NULL },
    { "-s?eekable", CLE_T_INT, "Input files are seekable and should be prescanned", 
      "0", &config.seekable, NULL, NULL },
    { "-rep?eatlines", CLE_T_INT, "Repeat each output line this many times", 
      "1", &config.repeatlines, NULL, NULL },
    { "-v?erbose", CLE_T_BOOL, "Verbose progress reporting", 
      "0", &config.verbose, NULL, NULL }, 
    { "-d?ebug", CLE_T_INT, "Debug level", 
      "0", &config.debug, NULL, NULL },
    { "[infile ...] (default to stdin)", CLE_T_USAGE, NULL, 
      NULL, NULL, NULL},

    { NULL, CLE_T_END, NULL, NULL, NULL, NULL, NULL }
};

/* after feacat.C - should put in a library! */
static FILE *fopen_gz(const char *filename, const char *mode) {
    /* Cover for fopen, will use popen and gzip if the filename 
       ends in .gz */
    FILE *stream;

    if (strcmp(filename+MAX(0,strlen(filename)-3), ".gz") == 0) {
	/* It's a gzipped file, so read/write through a popen to gzip */
	char buf[BUFSIZ];

	if (mode[0] == 'r') {
	    sprintf(buf, "gzip --stdout --decompress --force %s", filename);
	} else if (mode[0] == 'w') {
	    sprintf(buf, "gzip --stdout --force  > %s", filename);
	} else {
	    fprintf(stderr, "fopen_gz(%s): mode of '%s' is not 'r' or 'w'\n", 
		    filename, mode);
	    abort();
	}

	stream = popen(buf, mode);

	if(stream == NULL) {
	    fprintf(stderr, "fopen_gz('%s'): couldn't popen('%s','%s')\n", 
		    filename, buf, mode);
	    exit(-1);
	}

    } else if (strcmp(filename, "-")==0) {
	if (mode[0] == 'r') {
	    stream = stdin;
	} else if (mode[0] == 'w') {
	    stream = stdout;
	} else {
	    fprintf(stderr, "fopen_gz(\"-\"): mode of '%s' is not 'r' or 'w'\n", 
		    mode);
	    abort();
	}

  } else {
      stream = fopen(filename, mode);
      if(stream == NULL) {
	  fprintf(stderr, "fopen_gz: couldn't fopen('%s','%s')\n", 
		  filename, mode);
      }
  }

  return stream;
}

static char *my_fgets(char *s, int* pn, FILE *stream) {
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
	    s = (char *)realloc(s, *pn);
	    if ( s == NULL ) {
		fprintf(stderr, "my_fgets: out of memory trying to read %d chrs\n", *pn);
		abort();
	    }
	    got += len;
	}
	// loop around and try again
    }
}

class InputStream {
private:
    FILE *file;
    char *filename;
    int seekable;
    int currentline;
    int verbose;
    int debug;
public:
    InputStream(int skb=1, int vrb=0, int dbg=0) { 
	seekable=skb; verbose = vrb; debug = dbg;
	file = NULL; filename = NULL; 
	if (commentpat && strlen(commentpat)) {
	    fprintf(stderr, "commentpat not yet supported\n");}
    }
    ~InputStream() { close(); }

    int open(const char *name) {
	int rc = 0;
	close();
	if ( (file = fopen_gz(name, "r")) ) {
	    filename = strdup(name);
	    rc = 1;
	    currentline = 0;
	} else {
	    fprintf(stderr, "InputStream::open: couldn't read '%s'\n", name);
	    rc = 0;
	}
	if (seekable) {
	    calcNumLines();
	} else {
	    numlines = -1;
	}
	return rc;
    }
    void close(void) {
	if (file != NULL) {
	    if (file != stdin) {
		fclose(file);
	    }
	    file = NULL;
	    free(filename);
	    filename = NULL;
	}
    }
    void reset(void) {
	//rewind(file);
#ifdef HAVE_FSEEKO
	if (fseeko(file, 0, SEEK_SET) < 0) {
#else
	if (fseek(file, 0, SEEK_SET) < 0) {
#endif
	    fprintf(stderr, "ERROR: can't seek backwards at line %d of '%s' (try -seek 0)\n", currentline, filename);
	    exit(1);
	}
	currentline = 0;
    }
    void skipline(void) {
	char c;
	int t = 0, i;
	do {
	    i = fread(&c, 1, 1, file);
	    t += i;
	} while(c != '\n' && !feof(file));
	if ( t > 0 ) {
	    ++currentline;
	}
	if (feof(file)) {
	    numlines = currentline;
	}
    }
    int eof(void) {
	return feof(file);
    }
    void calcNumLines(void) {
	// Count the number of valid lines in this file
	while(!eof()) {
	    skipline();
	}
	numlines = currentline;
    }
    int linenum(void) { return currentline; }
    char *getline(int n, char *s, int *slen) {
	// Read a particular line
	char *rslt;
	if (n < currentline) {
	    reset();
	}
	while (n > currentline && !eof()) {
	    skipline();
	}
        rslt = my_fgets(s, slen, file);
	if (strlen(s)) {
	    ++currentline;
	}
	if (feof(file)) {
	    numlines = currentline;
	}
	if (debug) {
	    fprintf(stderr, "getline('%s',%d): numlins=%d strlen=%ld eof=%d\n", 
                    filename, n, numlines, strlen(rslt), eof());
	}
        return rslt;
    }

    int numlines;

    static char *commentpat;
};

char *InputStream::commentpat = NULL;

char *programName = NULL;

main(int argc, char *argv[])
{
    int err = 0;
    int nipfiles;
    //char **ipfiles;
    //const char *ipfiles[MAXIPFILES];
    const char **ipfiles;
    const char *anipfile;
    int  *startUtt;
    const char *defaultip = "-";
    int ecode = 0;
    int totlines = 0;
    int i;

    programName = argv[0];

    // Handle the args
    cleSetDefaults(clargs);

    err = cleExtractArgs(clargs, &argc, argv, 0);
    if(err) {
	cleUsage(clargs, programName);
	exit(-1);
    }

    // Copy over the commentpattern
    InputStream::commentpat = config.commentpattern;

    // Leftover is taken as one or more infiles - unless -i ipfile was
    // specified
    
    if (argc < 2) {
	// nothing left on command line - read from stdin
	nipfiles = 1;
	//ipfiles[0] = defaultip;
	anipfile = defaultip;
	ipfiles = &anipfile;
    } else {
	nipfiles = argc-1;
	//ipfiles = argv+1;
	//assert(nipfiles <= MAXIPFILES);
	ipfiles = new const char *[nipfiles];
	for(int i = 0; i<nipfiles; ++i) {
	  ipfiles[i] = argv[i+1];
	}
    }
    // We assemble an array of the logical index of each specified input 
    // file so that we can index around in one long logical file.  
    // The first value is always 0; the (nipfiles+1) value is the 
    // total number of utterances.
    startUtt = new int[nipfiles+1];
    startUtt[0] = config.indexbase;
    for (i = 1; i < nipfiles+1; ++i) {
	// preset to indicate unknown length
	startUtt[i] = INT_MAX;
    }

    InputStream *instream = new InputStream(config.seekable, config.verbose, config.debug);
    FILE* outstream = NULL;
    Range *line_range = NULL;
    int ipfileno = -1, lastIpfileno;

    // Buffer for reading lines in (must be malloc'd for realloc in my_fgets)
    int ipbuflen = 1024;
    char *ipbuf = (char *)malloc(ipbuflen);

    // Pre-scan the lengths of the input files
    for (i = 0; i < nipfiles; ++i) {
	if ( !instream->open(ipfiles[i]) ) {
	    fprintf(stderr, "%s: couldn't read input file '%s'\n", 
		    programName, ipfiles[i]);
	    exit(-1);
	}	
	ipfileno = i;
	// Try to set up the index
	int numlines = instream->numlines;
	if (startUtt[i] != INT_MAX && numlines >= 0) {
	    startUtt[i+1] = 
		startUtt[i] + numlines;
	}
	if (config.verbose) {
	    fprintf(stderr, "%s: ip file '%s' has %d lines\n", 
		    programName, ipfiles[i], numlines);
	}
    }

    line_range = new Range(config.rangespec, 
			   config.indexbase, startUtt[nipfiles]);
    if (config.debug) {
	line_range->PrintRanges("lines range");
    }

    // Construct this now (rather than at top of its loop) to avoid 
    // compile warnings about goto close_in skipping constructors
    Range::iterator lrit = line_range->begin(); 

    // Open the output file
    if ( !(outstream = fopen_gz(config.outfilename, "w")) ) {
	fprintf(stderr, "%s: couldn't open '%s' for writing\n", 
		programName, config.outfilename);
	goto close_in;
    }

    for ( ; !lrit.at_end(); lrit++) {

	int line_rg_num = *lrit;
	int line_done = 0;

	while (!line_done) {
	    // We repeat the process of choosing which file 
	    // contains the current segment so that we can 
	    // skip through several files when we are learning 
	    // the actual number of utterances in each 

	    // Make sure this utterance is within range, to best of knwlg
	    if (startUtt[nipfiles] != INT_MAX \
		&& line_rg_num >= startUtt[nipfiles]) {

		/* If the user explicitly requested this utterance, then 
		   it's an error for it to be outside the files.  But if 
		   it's just that we guessed the total number of files, 
		   then only report if verbose. */
		if (lrit.current_range_finite()) {
		    fprintf(stderr, "%s: ERROR: requested line index %d "
			    "exceeds total line count of %d\n", 
			    programName, line_rg_num, startUtt[nipfiles]);
		    ecode = -1;
		}
		goto close_out;
	    }
	    // Figure out which ip file we think this lies in
	    lastIpfileno = ipfileno;
	    ipfileno = -1;
	    while (ipfileno < nipfiles \
		   && startUtt[ipfileno+1] != INT_MAX \
		   && startUtt[ipfileno+1] <= line_rg_num) {
		++ipfileno;
	    }

	    if (ipfileno == -1) {
		fprintf(stderr, "%s: requested line %d which is before the first line (#%d)\n", programName, line_rg_num, startUtt[0]);
		goto close_out;
	    }

	    if (config.debug > 0) {
		fprintf(stderr, "%s: attempting to read logical line "
			"%d (of %u) in file '%s'...\n", programName, 
			line_rg_num, line_range->length(), ipfiles[ipfileno]);
	    }

	    // If we've switched file, reopen
	    if (ipfileno != lastIpfileno) {

		// Open (or retrieve) the next input file
		// Bail if open failed
		if (instream->open(ipfiles[ipfileno]) < 0) {
		    ecode = -1;
		    goto close_out;
		};

		// Maybe set up the index here if we happen to know how
		if (startUtt[ipfileno] != INT_MAX \
		    && instream->numlines >= 0) {
		    startUtt[ipfileno+1] 
			= startUtt[ipfileno] + instream->numlines;
		}

		// If we opened a new input file, restart the utterance 
		// processing, in case the index has been expanded 
		// and this utterance is now thought to be in a later file.
		continue;
	    }
	
	    // Now open in the right file?
	    ipbuf = instream->getline(line_rg_num - startUtt[ipfileno], 
				      ipbuf, &ipbuflen);
	    if (strlen(ipbuf) == 0 && instream->eof()) {
		// We hit EOF
		if (config.debug) {
		    fprintf(stderr, "%s: EOF seen in '%s' at line %d\n", 
			    programName, ipfiles[ipfileno], 
			    instream->numlines);
		}
		assert(instream->numlines >= 0 && instream->numlines \
		       <= line_rg_num - startUtt[ipfileno]);
		startUtt[ipfileno+1] = startUtt[ipfileno] + instream->numlines;
		continue;
	    }

	    int rptcount;
	    for (rptcount = 0; rptcount < config.repeatlines; ++rptcount) {

		// Write to output
		fputs(ipbuf, outstream);
		// and EOL
		fputc('\n', outstream);
		++totlines;

		// Report
		if (config.verbose) {
		    fprintf(stderr, "%s: Finished line %d in %s\n", 
			    programName, instream->linenum(), 
			    ipfiles[ipfileno]);
		}
	    }  /* for (rptcount...) loop */
	    line_done = 1;
	} /* while (!line_done) */
    }

close_out:
    // All done - close streams
    fclose(outstream);
    delete line_range;

    if (config.verbose) {
	fprintf(stderr, "%s: %d lines written to %s (ecode %d)\n", 
		programName, totlines, 
		config.outfilename, ecode);
    }

close_in:
    delete instream;

    free(ipbuf);

    exit (ecode);
}
