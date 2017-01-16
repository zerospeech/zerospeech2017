// 
// wavs2onlaudio
//
// Read audio waveform files from a list of IDs and write them out 
// as an online audio stream.
//
// Uses the IO classes developed for "feacalc", a C++ interface to 
// rasta.  Meant to be drop-in compatible with bedk's 
// drspeech_sphere2onlaudio, but to handle the wider range of 
// soundfile formats supported by feacalc (via snd(3), via sndf(3)) - 
// especially gzipp'd files.
//
// 1997nov11 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/wavs2onlaudio.C,v 1.9 2012/01/05 20:30:15 dpwe Exp $

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "QN_args.h"

// Local includes
#include "OutputFile.H"
#include "InputFile.H"
#include "FileList.H"

// dpwelib include
#include <Sound.H>

// constants
// Minimum size of blocks of samples to transfer
#define MIN_BLOCK_FRAMES	1000

float sf = 8000;
float rangerate = 0;
float rngstartoffs = 0;
float rngendoffs = 0;
float zeropad = 0;
int utt_start = 0;
int utt_count = -1;
int ioblocking = 8000;
char *infilename = NULL;
char *wavfilecmd = NULL;
char *wavdirectory = NULL; /* instead of filecmd, start in this directory */
char *wavextension = NULL; /* instead of filecmd, append this extension */
char *ipsffmt = NULL;

QN_ArgEntry argtab[] =
{
  { NULL,
    "Construct an online audio stream from a list of wav file IDs",
    QN_ARG_DESC },
  { "infilename", "List of wavfiles to process",
    QN_ARG_STR, &infilename, QN_ARG_REQ },
  { "ipsffmt", "sndf(3) name for forced input soundfile typing", 
    QN_ARG_STR, &ipsffmt, QN_ARG_OPT },
  { "utt_start",  "First utterance in the wavfile list to process",
    QN_ARG_INT, &utt_start,  QN_ARG_OPT },
  { "utt_count",  "Number of utterances to process",
    QN_ARG_INT, &utt_count,  QN_ARG_OPT },
  { "ioblocking", "Maximum size chunk of data to process",
    QN_ARG_INT, &ioblocking, QN_ARG_OPT },
  { "sf",         "Sampling frequency of wavfiles",
    QN_ARG_FLOAT, &sf, QN_ARG_OPT },
  { "filecmd",    "Command to build wavfilename from utid %u",
    QN_ARG_STR, &wavfilecmd, QN_ARG_OPT },
  { "wavdir",    "directory containing wav files (instead of filecmd)", 
    QN_ARG_STR, &wavdirectory, QN_ARG_OPT }, 
  { "wavext",    "extension to complete wav file names (instead of filecmd)", 
    QN_ARG_STR, &wavextension, QN_ARG_OPT }, 
  { "rangerate",  "Flag & scaling of start/end range filename", 
    QN_ARG_FLOAT, &rangerate, QN_ARG_OPT },
  { "rngstartoffset",  "Constant added to range start before scaling", 
    QN_ARG_FLOAT, &rngstartoffs, QN_ARG_OPT },
  { "rngendoffset",  "Constant added to range end before scaling", 
    QN_ARG_FLOAT, &rngendoffs, QN_ARG_OPT },
  { "zeropad",  "Add this many ms of pure zeros at each end of each file", 
    QN_ARG_FLOAT, &zeropad, QN_ARG_OPT },

  { NULL, NULL, QN_ARG_NOMOREARGS }
};

int main(int argc, const char** argv)
{
    char* progname;               // The name of the program, set by QN_args

    // Process arguments
    QN_initargs(&argtab[0], &argc, &argv, &progname);

    // Check ipsffmt, if any
    if (ipsffmt && strlen(ipsffmt) > 0) {
	int id = SFFormatToID(ipsffmt);
        if (id == SFF_UNKNOWN) {
	    fprintf(stderr, "ipsffmt='%s' is unknown, use:\n", ipsffmt);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");
	    exit(1);
	}
    }
    // Figure where we're getting our files from
    FileList *filelist;
    // Where the files are going
    Sound *input, *output;

    // Open input (just one list file at present..)
    //    char *listfilelist[1];
    //    listfilelist[0] = infilename;
    //    filelist = new FileList_Files(1, listfilelist);
    filelist = new FileList_File(infilename);
    // Tell it what the mapping command is
    filelist->SetFileCmd(wavfilecmd,wavdirectory,wavextension);
    // Set up ranging if wanted
    if (rangerate > 0.0) {
	filelist->SetRanging(1, 1.0/rangerate, rngstartoffs, rngendoffs);
    }

    // Open output
    int chans = 1;
    Format fmt = Sfmt_SHORT;
    const char *opsffmt = "PCM/EbAbb";
    const char *opfilename = "-";	// i.e. stdout
    output = new Sound("w", sf, chans, fmt);
    // set up the soundfile format
    output->setSFformatByName(opsffmt);
    if (!output->open(opfilename)) {
	fprintf(stderr, "%s: ** couldn't open '%s' for output\n", 
		progname, opfilename);
	exit(-1);
    }
    // set the transfer format as the same as the file data format
    output->setFormat(fmt);

    // allocate the transfer buffer
    int blockframes = ioblocking / (chans * sizeof(short));
    if (blockframes < MIN_BLOCK_FRAMES) {
	blockframes = MIN_BLOCK_FRAMES;
    }
    short *sampbuf = new short[chans * blockframes];

    // Scan to the first utterance to process
    int utt_N  = 0;
    FileEntry *inputFileEnt;
    while ( (utt_count < 0) || (utt_N < utt_start + utt_count)) {
	inputFileEnt = filelist->GetNextFileEntry();
	if (inputFileEnt == NULL) {
	    DBGFPRINTF((stderr, "hit end of list at # %d\n", utt_N));
	    if (!(utt_count < 0)) {
		fprintf(stderr, "%s: ** unexpected end of file list reached at utt_num %d\n", progname, utt_N);
	    }
	    // else we were running to the end of the list & this is it
	    utt_count = utt_N - utt_start - 1;
	    if (utt_count < 0)	utt_count = 0;
	    if (utt_N < utt_start)	utt_start = utt_N;
	    continue;
	}
	DBGFPRINTF((stderr, "got file '%s'\n", inputFileEnt->name));
	// Process this file if in-bounds
	if (utt_N >= utt_start) {
	    input = new Sound("r", sf, chans, fmt);
	    if (ipsffmt && strlen(ipsffmt) > 0) {
	        input->setSFformatByName(ipsffmt);
	    }
	    if (!input->open(inputFileEnt->name)) {
		fprintf(stderr, "%s: ** couldn't read wavfile '%s' (#%d)\n", 
			progname, inputFileEnt->name, utt_N);
		exit(-1);
	    }
	    if (sf == 0) {
		// specifying as zero means just read it from the sound file
		sf = input->getSampleRate();
	    }
	    if (sf != input->getSampleRate()) {
		fprintf(stderr, "%s: Warning: file '%s' is being treated as sf=%.0f, but header claims %.0f\n", progname, inputFileEnt->name, sf, input->getSampleRate());
		// but continue anyway
	    }
	    input->setFormat(fmt);
	    // Set up chan mode too
	    input->setChannels(1);
	    input->setChanMode(Scmd_CHAN0+inputFileEnt->channel);
	    // seek to any specified start
	    int zpadframes, startpos, startpad = 0, pos = 0;
	    zpadframes = (int)rint(sf*zeropad/1000.0);
	    startpos = (int)rint(sf*inputFileEnt->startTime);
	    if (startpos > 0) {
		input->frameSeek(startpos);
	    } else {
		// need to prepad this sound with some zeros
		startpad = -startpos;
	    }
	    // transfer the frames
	    int got = -1, toget = blockframes;
	    int remain = -1;
	    if (inputFileEnt->lenTime > 0.0) {
		remain = (int)rint(sf*inputFileEnt->lenTime);
		// compensate for the pre-zero-pad too
		remain += zpadframes;
	    }
	    while(got != 0) {
		if(remain >= 0) {
		    toget = min(blockframes, remain);
		}
		if (pos < startpad+zpadframes) {
		    // prepadding with zero for -ve startTime
		    got = min(startpad+zpadframes-pos, toget);
		    memset(sampbuf, 0, got*chans*sizeof(short));
		} else {
		    got = input->readFrames(sampbuf, toget);
		}
		output->writeFrames(sampbuf, got);
		DBGFPRINTF((stderr, "copied %d frames\n", got));
		remain -= got;
		pos += got;
	    }
	    // pad out to requested length with zeros
	    if (zpadframes || remain > 0) {
		if (remain < 0) remain = 0;
		remain += zpadframes;
		memset(sampbuf, 0, blockframes*chans*sizeof(short));
		while(remain > 0) {
		    got = min(blockframes, remain);
		    output->writeFrames(sampbuf, got);
		    DBGFPRINTF((stderr, "zpadded %d frames\n", got));
		    remain -= got;
		    pos += got;
		}
	    }

	    // close up the files
	    delete input;
	    output->next();
	}
	++utt_N;
	DBGFPRINTF((stderr, "utt_n=%d utt_start=%d utt_count=%d\n", 
		    utt_N, utt_start, utt_count));
    }
    // finish up
    delete output;
    delete [] sampbuf;
    delete filelist;

    return 0;
}
