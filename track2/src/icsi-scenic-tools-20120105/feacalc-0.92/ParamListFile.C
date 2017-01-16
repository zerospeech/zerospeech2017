//
// ParamListFile.C
//
// Module that handles files containing lists of parameters.
// Part of the "feacalc" feature calculation system.
//
// 2001-04-23 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/ParamListFile.C,v 1.3 2012/01/05 20:30:15 dpwe Exp $

#include "ParamListFile.H"
#include <stdlib.h>	// for free()
#include <string.h>
#include <assert.h>

const char *ParamListFile::WS = " \t\n";

ParamListFile::ParamListFile(char *a_filename) {
    file = fopen(a_filename, "r");
    if (file == NULL) {
      fprintf(stderr, "ParamListFile: cannot read '%s'\n", a_filename);
      exit(1);
    }
    filename = strdup(a_filename);
    buflen = 1024;
    buf = new char[buflen];
    bufpos = 0;
    buf[bufpos] = '\0';
    linect = 0;
}

ParamListFile::~ParamListFile() {
    delete [] buf;
    free(filename);
    fclose(file);
}

int ParamListFile::getNextTok(char **ret) {
    // Pull another space-delimited token from an open file handle.
    // File needs just to be white-space
    // delimited, but can have comment lines starting with "#"
    // Pointer to token string written to *ret. 
    // Return 1 on success, 0 at EOF

    while (buf[bufpos] == '\0') {
	// need a new line
	char *ok = fgets(buf, buflen, file);
	if (!ok) {
	    // EOF hit, nothing more got
	    return 0;
	}
	int got = strlen(buf);
	while (buf[got-1] != '\n' && !feof(file)) {
	    // because line didn't end with EOL, we assume we ran out 
	    // of buffer space - realloc & keep going
	    assert(got == buflen-1);
	    int newbuflen = 2*buflen;
	    char *newbuf = new char[newbuflen];
	    memcpy(newbuf, buf, got);
	    delete [] buf;
	    buf = newbuf;
	    buflen = newbuflen;
	    fgets(buf+got, buflen, file);
	    got = strlen(buf);
	}
	++linect;
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

    // Find the next space after the token
    // (dlbufpos already points to non-WS)
    int toklen = strcspn(buf+bufpos, WS);
    // I think this has to work
    assert(toklen > 0);

    // Save the result
    *ret = buf+bufpos;

    if (buf[bufpos+toklen] != '\0') {
	// There's more after the tok
	// terminate the string at terminal WS
	buf[bufpos+toklen] = '\0';	
	// Skip over the terminated, returned token
	bufpos += toklen+1;
	// Advance pointer to look at following non-WS next time
	bufpos += strspn(buf+bufpos, WS);
    } else {
	// This token is last in string - point to end of buffer for next time
	bufpos += toklen;
    }
    return 1;
}

int ParamListFile::getNextInt(int *ret) {
    // Read another number from the deslen file as the desired length
    // in frames of the next frame.  File needs just to be white-space
    // delimited, but can have comment lines starting with "#"
    // Next read value is put into *pdeslen.  Return 1 on success, 0 at EOF
    int rc = 0;
    // Get the next number in the line buffer & advance pointer
    char *str, *end;

    if ( rc = getNextTok(&str) ) {
	int val = strtol(str, &end, 10);

	// If unparseable, end points to start
	if (end == str) {
	    fprintf(stderr, "unable to parse token '%s' as an integer "
		    "at line %d in file '%s'\n", str, linect, filename);
	    return 0;
	}

	// Got the number
	*ret = val;
    }
    return rc;
}

int ParamListFile::getNextFloat(float *ret) {
    // Read another number from the deslen file as the desired length
    // in frames of the next frame.  File needs just to be white-space
    // delimited, but can have comment lines starting with "#"
    // Next read value is put into *pdeslen.  Return 1 on success, 0 at EOF
    int rc = 0;
    // Get the next number in the line buffer & advance pointer
    char *str, *end;

    if ( rc = getNextTok(&str) ) {
	float val = strtod(str, &end);

	// If unparseable, end points to start
	if (end == str) {
	    fprintf(stderr, "unable to parse token '%s' as a float "
		    "at line %d in file '%s'\n", str, linect, filename);
	    return 0;
	}

	// Got the number
	*ret = val;
    }
    return rc;
}


