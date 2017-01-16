//
// ParamListFile.H
//
// Definitions for module that handles lists of parameters
// e.g. VTN parameters for each segment in a training set.
// Part of the "feacalc" feature calculation system.
//
// 2001-04-23 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/ParamListFile.H,v 1.2 2012/01/05 20:30:15 dpwe Exp $

#include <stdio.h>

class ParamListFile {
    // Interface to a file of parameters
private:
    FILE *file;
    char *filename;
    int buflen;
    char *buf;
    int bufpos;
    int linect;
    static const char *WS;

public:
    ParamListFile(char *a_filename);
    ~ParamListFile();

    int getNextInt(int *ret);
    int getNextFloat(float *ret);
    int getNextTok(char **ret);
};

