//
// FileList.H
//
// Definitions for module that handles lists of filenames.
// Part of the "feacalc" feature calculation system.
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/FileList.H,v 1.7 2012/01/05 20:30:15 dpwe Exp $

#include <stdio.h>

class FileEntry {
    // Returns from a file list - encapsulates a file name and optional 
    // ranging information i.e. a subset of samples within that file
public:
    char *name;
    float startTime;
    float lenTime;
    int channel;

    FileEntry(const char *fn, float start=0, float len=0, int chan=0);
    // copy constructor?
    ~FileEntry(void);
};

class FileList {
    // Abstract class for different kinds of lists of file IDs
protected:
    char *fileCmd;
    char *wavdirectory;
    char *wavextension;
    char *MaybeMapUtID(char *s);
    int ParseRanging(char **ptail, float *pstart, float *plen, int *pchan);
    char *GetNextFileNameFromFile(FILE *f, float *pstart, float *plen, int *pchan);
    int fileNum;
    int isRanged;	// should we look for ranging info in this filelist?
    double rangeScale;	// constant to convert ranging info to seconds
    double rangeStartOffset;	// add to start range index before scaling
    double rangeEndOffset; 	// add to range end index before scaling
public:
    FileList(void);
    virtual ~FileList(void);

    // Set the command that maps from uttIDs to full paths
    void SetFileCmd(char *cmd, char *dir, char *ext);
    // Setup flag & info for ranging data
    void SetRanging(int flag, float scale=0.0, 
		    float startoffs=0, float endoffs=0) {
	isRanged = flag; rangeScale = scale; 
	rangeStartOffset = startoffs; rangeEndOffset = endoffs;
    }
    // Return the next input file to open, or NULL for end of list
    virtual FileEntry* GetNextFileEntry(void) = 0;
};

class FileList_File : public FileList {
    // File list that reads filenames from one file
private:
    FILE *file;
public:
    FileList_File(char *listpath);
    ~FileList_File(void);
    FileEntry * GetNextFileEntry(void);
};

class FileList_Files : public FileList {
    // File list that reads filenames from one or more files
private:
    FILE *file;
    char **filenames;
    int nfilenames;
    int nextfilename;
public:
    FileList_Files(int argc, char **argv);
    ~FileList_Files(void);
    virtual FileEntry* GetNextFileEntry(void);
};

class FileList_Stream : public FileList {
    // Fake filelist object for reading files from a stream (e.g. Abbot)
private:
    FILE *stream;
public:
    FileList_Stream(FILE *stream);
    virtual FileEntry* GetNextFileEntry(void);
};

class FileList_Argv : public FileList {
    // Filelist object that regurgitates from a argc/argv list
private:
    int nargs;
    int nextarg;
    char **args;
public:
    FileList_Argv(int argc, char **argv);
    ~FileList_Argv(void);
    virtual FileEntry* GetNextFileEntry(void);
};

