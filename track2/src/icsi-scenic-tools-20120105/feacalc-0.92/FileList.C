//
// FileList.C
//
// Module that handles lists of filenames.
// Part of the "feacalc" feature calculation system.
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/FileList.C,v 1.11 2012/01/05 20:30:15 dpwe Exp $

#include "FileList.H"
#include <stdlib.h>	// for free()
#include <string.h>
#include <unistd.h>	// for exec()

#include "ExecUtils.H"	// for filecmd exec stuff
#include <assert.h>

#ifndef STDOUT 
#define STDOUT 1
#endif /* STDOUT */

FileEntry::FileEntry(const char *fn, float start/*=0*/, float len/*=0*/, int chan/*=0*/) {
    name = strdup(fn); startTime = start; lenTime = len; channel=chan;
}

FileEntry::~FileEntry(void) {
    free(name);
}

FileList::FileList(void) {
    fileCmd = NULL;
    wavdirectory = NULL;
    wavextension = NULL;
    fileNum = 0;
    isRanged = 0;
    rangeScale = 0.0;
}

FileList::~FileList(void) {
    if(fileCmd) {
	free(fileCmd);
	fileCmd = NULL;
    }
    if(wavdirectory) {
	free(wavdirectory);
	wavdirectory = NULL;
    }
    if(wavextension) {
	free(wavextension);
	wavextension = NULL;
    }
}

void FileList::SetFileCmd(char *cmd, char *dir, char *ext) {
    // Define the command string to execute to map an utID to a file name
    if(fileCmd) {
	free(fileCmd);
	fileCmd = NULL;
    }
    if(wavdirectory) {
	free(wavdirectory);
	wavdirectory = NULL;
    }
    if(wavextension) {
	free(wavextension);
	wavextension = NULL;
    }
    if(cmd && strlen(cmd)) {	// leave it clear if it's zero len
	fileCmd = strdup(cmd);
    }
    if(dir && strlen(dir)) {
	wavdirectory = strdup(dir);
    }
    if(ext && strlen(ext)) {
	wavextension = strdup(ext);
    }
}

char *FileList::MaybeMapUtID(char *s) {
    // Apply the fileCmd to the utID s, if we have one.
    // also prepend/postpend wavdirectory and wavextension if we have them
    // BEFORE applying fileCmd.
    // Always return a freshly-allocated string.
    char tmp[1024];
    char *ret;

    if (wavdirectory || wavextension) {
	if (wavdirectory) {
	    int len;
	    strcpy(tmp, wavdirectory);
	    len = strlen(tmp);
	    /* force it to have a directory separator at the end */
	    if (tmp[len-1] != '/') {
		tmp[len] = '/';
		tmp[len+1] = '\0';
	    }
	} else {
	    tmp[0] = '\0';
	}
	strcat(tmp, s);
	if (wavextension) {
	    strcat(tmp, wavextension);
	}
	s = tmp;
    }
    if(fileCmd) {
	// Run the fileCmd to map the name
	char fullcmd[1024];

	if(strstr(fileCmd, "%s") != NULL \
	    || strstr(fileCmd, "%u") != NULL) {
	    // fileCmd includes "%s" or "%u" - do expansion to the utterance ID
	    // %u is now the preferred version
	    char *pcsubs[2];
	    pcsubs[0] = s;	pcsubs[1] = s;
	    assert(exuExpandPercents(fileCmd, fullcmd, 1024, 
				     2, "su", pcsubs));
	} else {
	    // just append the utid to the file cmd
	    assert(strlen(fileCmd)+strlen(s)+1 < 1024);
	    strcpy(fullcmd, fileCmd);
	    strcat(fullcmd, s);
	}
	s = exuExecCmd(fullcmd);
	if(s == NULL || strlen(s) == 0) {
	    fprintf(stderr, "MapUtID: fileCmd %s failed\n", fullcmd);
	    exit(-1);
	}
	/* exuExecCmd returns a new string */
	ret = s;
    } else {
	ret = strdup(s);
    }
    return ret;
}

int FileList::ParseRanging(char **ptail, float *pstart, float *plen, int *pchan) {
    // Extract range info from post-filename list line, if needed.
    // return 1 if OK, 0 to warn with problem parsing line, -1 for fatal
    char *t = *ptail;
    if (isRanged) {
	// Look for ranging info - two numbers after filename
	double st, ed;
	char *eptr1 = NULL, *eptr2 = NULL;
	if(t) {
	    st = strtod(t, &eptr1) + rangeStartOffset;
	    ed = strtod(eptr1, &eptr2) + rangeEndOffset;
	}
	// if unable to parse numbers, eptr2 will be set to eptr1
	if (eptr2 == eptr1) {
	    fprintf(stderr, "Unable to parse range limits in '%s'\n", *ptail);
	    return -1;
	}
	// set range flags
	if (pstart) *pstart = rangeScale * st;
	if (plen)   *plen   = rangeScale * (ed-st);
	// default chan
	if (pchan)  *pchan  = 0;
	*t = '\0';
	// where to look for rest of line
	t = eptr2;
	t = t+strspn(t, " \t\n\r");
	// Check for channel index
	if (t[0] != '\0' \
	    && (t[1] == '\0' || t[1] == ' ' \
		|| t[1] == '\t' || t[1] == '\r' || t[1] == '\n')) {
	    /* A single nonblank following chr is a channel spec */
	    if (*t == '0' || *t == 'A' || *t == 'a' \
		|| *t == 'L' || *t == 'l') {
		if (pchan) *pchan = 0;
	    } else if (*t == '1' || *t == 'B' || *t == 'b' \
		       || *t == 'R' || *t == 'r') {
		if (pchan) *pchan = 1;
	    } else if (*t == '2' || *t == 'C' || *t == 'c') {
		if (pchan) *pchan = 2;
	    } else if (*t == '3' || *t == 'D' || *t == 'd') {
		if (pchan) *pchan = 3;
	    } else if (*t == '4' || *t == 'E' || *t == 'e') {
		if (pchan) *pchan = 4;
	    } else if (*t == '5' || *t == 'F' || *t == 'f') {
		if (pchan) *pchan = 5;
	    } else if (*t == '6' || *t == 'G' || *t == 'g') {
		if (pchan) *pchan = 6;
	    } else if (*t == '7' || *t == 'H' || *t == 'h') {
		if (pchan) *pchan = 7;
	    } else {
		fprintf(stderr, "Unable to parse range channel in '%s'\n", *ptail);
		return -1;
	    }
	    ++t;
	}
	*ptail = t;
    } else {
	if (pstart) *pstart = 0;
	if (plen) *plen = 0;
	if (pchan) *pchan = 0;
    }
    if(t) {
	// shouldn't be anything after the filename, permit spaces
	char *nexts = t+strspn(t, " \t\n\r");
	if (*nexts != '\0') {
	    return 0;	// print warning, but not fatal
	}
    }
    // return OK
    return 1;
}

char *FileList::GetNextFileNameFromFile(FILE *f, float *pstart, float *plen, int *pchan) {
    // Read a filename from a file and return it.  Used by FileList_File
    // and FileList_Files
    char line[256];
    char *s, *t;
    int found = 0;
    while(!found) {
	if(fgets(line, 256, f) == NULL) {
	    // EOF
	    return NULL;
	}
	// remove terminating CR (makes error messages print out nicely)
	if (*(line + strlen(line)-1) == '\n') {
	    *(line + strlen(line)-1) = '\0';
	}
	// We're looking for a nonblank line that doesn't start with #
	s = line+strspn(line, " \t\n\r");
	if(strlen(s) && s[0] != '#') {
	    found = 1;
	    // Terminate it at the first blank or whatever
	    t = strpbrk(s, " \t\n\r");
	    int rrc = ParseRanging(&t, pstart, plen, pchan);
	    // check return code from range parsing
	    if (rrc < 1) {
		// error or warning
		fprintf(stderr, "%s: garbled list line '%s'\n", 
			(rrc < 0)?"Error":"Warning", line);
		if (rrc < 0) {
		    // fatal error
		    exit(-1);
		} else {
		    fprintf(stderr, "   (but filename extracted)\n", s);
		}
		// make sure file name is terminated before error msg
		if (t) *t = '\0';
	    }
	}
    }
    return MaybeMapUtID(s);
}

FileList_File::FileList_File(char *fname) {
    // Read a file that gives a list of utterance IDs/filenames
    file = fopen(fname, "r");
    if(file == NULL) {
	fprintf(stderr, "FileList_File: couldn't open list file %s\n", fname);
	exit(-1);
    }
}

FileList_File::~FileList_File(void) {
    if (file) {
	fclose(file);
    }
}

FileEntry *FileList_File::GetNextFileEntry(void) {
    float start, len;
    int chan;
    FileEntry *fe = NULL;
    char *name = GetNextFileNameFromFile(file, &start, &len, &chan);
    if (name) {
	++fileNum;
	fe = new FileEntry(name, start, len, chan);
	free(name);
    }
    return fe;
}

FileList_Files::FileList_Files(int argc, char *argv[]) {
    // Make a copy of the argv elements passed
    int i;
    filenames = new char* [argc];
    nfilenames = argc;
    nextfilename = 0;
    for(i = 0; i<argc; ++i) {
        filenames[i] = strdup(argv[i]);
    }
    file = NULL;
}

FileList_Files::~FileList_Files(void) {
    // free up the store we allocated
    int i;
    if (file != NULL) {
	fclose(file);
    }
    for (i = 0; i<nfilenames; ++i) {
	free(filenames[i]);
    }
    delete [] filenames;
}

FileEntry *FileList_Files::GetNextFileEntry(void) {
    // Return the next file from the list
    char *name = NULL;
    const char *fname = "";
    float start, len;
    int chan;

    while (name == NULL && (file != NULL || nextfilename < nfilenames)) {
	if (file == NULL) {
	    fname = filenames[nextfilename++];
	    // specifying "-" means read utids from stdin
	    if (strcmp(fname, "-") == 0) {
		file = stdin; 
		// will attempt to close stdin now - oh well
	    } else {
		file = fopen(fname, "r");
	    }
	    if(file == NULL) {
		fprintf(stderr, "FileList_Files: couldn't open list file %s\n", fname);
		exit(-1);
	    }
	}
	name = GetNextFileNameFromFile(file, &start, &len, &chan);
//	fprintf(stderr, "FileList_Files::GetNextFileName: fname='%s', name='%s'\n", fname, (name==NULL)?"(NULL)":name);
	if (name == NULL) {
	    // Hit EOF in that file, so try next
	    fclose(file);
	    file = NULL;
	}
    }
    FileEntry *fe = NULL;
    if (name) {
	++fileNum;
	fe = new FileEntry(name, start, len, chan);
	free(name);
    }
    return fe;
}

FileList_Stream::FileList_Stream(FILE *s) {
    stream = s;
}

FileEntry* FileList_Stream::GetNextFileEntry(void) {
    // return a dummy UNLESS stream is at EOF, in which case return NULL

    if(feof(stream) || ferror(stream)) {
	return NULL;
    }
    ++fileNum;

    //return "(stream)";
    // snd.h will automatically interpret the filename "-" as stdin, so use it
    return new FileEntry("-");
}

FileList_Argv::FileList_Argv(int argc, char *argv[]) {
    // Make a copy of the argv elements passed
    int i;
    args = new char* [argc];
    nargs = argc;
    nextarg = 0;
    for(i = 0; i<argc; ++i) {
	args[i] = strdup(argv[i]);
    }
}

FileList_Argv::~FileList_Argv(void) {
    // free up the store we allocated
    int i;
    for (i = 0; i<nargs; ++i) {
	free(args[i]);
    }
    delete [] args;
}

FileEntry *FileList_Argv::GetNextFileEntry(void) {
    // Return the next file from the list
    if(nextarg >= nargs) {
	return NULL;
    }
    ++fileNum;
    char *fn = MaybeMapUtID(args[nextarg++]);
    FileEntry *fe = new FileEntry(fn);
    free(fn);
    return fe;
}
