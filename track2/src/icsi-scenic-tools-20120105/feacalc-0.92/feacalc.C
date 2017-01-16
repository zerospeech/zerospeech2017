//
// feacalc.C
//
// Main routine for feacalc - new version of rasta that 
// allows more kinds of features to be made, straight into pfiles
//
// 1997jul23 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/feacalc.C,v 1.16 2002/03/18 21:10:38 dpwe Exp $

// Local includes
#include "OutputFile.H"
#include "InputFile.H"
#include "FileList.H"
#include "CmdLineStuff.H"
#include "Rasta.H"
#include "ParamListFile.H"

int main(int argc, char **argv) {
    char *programName = argv[0];
    CL_VALS *clv;

    /* flags to pass as byteswap to get be/le output */
#ifdef WORDS_BIGENDIAN
    int swap_for_be = 0;
    int swap_for_le = 1;
#else 
    int swap_for_be = 1;
    int swap_for_le = 0;
#endif /* WORDS_BIGENDIAN */

    clv = HandleCmdLine(&argc, &argv);

    // Figure where we're getting our files from
    FileList *filelist;
    // cleExtractArgs leaves argv[0] as the programName
    // remaining tokens are list of input files to use.
    if (clv->uselists) {
	// take any excess arguments as list file names, in order
	filelist = new FileList_Files(argc-1, argv+1);
    } else if (argc == 2 && strcmp(argv[1], "-") == 0) {
	filelist = new FileList_Stream(stdin);
    } else if (argc > 1) {
	filelist = new FileList_Argv(argc-1, argv+1);
    } else {
	fprintf(stderr, "%s: no list or input filenames given\n", programName);
	exit(-1);
    }
    // Tell it what the mapping command is
    filelist->SetFileCmd(clv->filecmd, clv->wavdirectory, clv->wavextension);
    // Set up any ranging info
    if (clv->rangeRate > 0.0) {
	filelist->SetRanging(1, 1.0/clv->rangeRate, 
			     clv->rngStartOffset, clv->rngEndOffset);
    }
    // Maybe use VTN parameter file
    ParamListFile *vtnfile = NULL;
    if (clv->vtnfile && strlen(clv->vtnfile)) {
	vtnfile = new ParamListFile(clv->vtnfile);
    }

    // Create the processing chain
    FtrCalc *ftrcalc = new FtrCalc_Rasta(clv);

    // 2001-12-04: provide for fractional sample hop
    int stepCents = (int)rint(100.0*(clv->stepTime*clv->sampleRate/1000.0 - (float)ftrcalc->steplen));
    if (stepCents != 0) {
      fprintf(stderr, "%s: Warn: non-integer hop size (stepCents=%d)\n", programName, stepCents);
    }

    // Open the output file
    OutputFile *output;
    if(clv->outfilefmt == OUTFF_PFILE) {
	output = new OutputFile_Pfile(clv->outpath, ftrcalc->nfeats);
    } else if(clv->outfilefmt == OUTFF_SRI) {
	output = new OutputFile_SRI(clv->outpath, ftrcalc->nfeats);
    } else if(clv->outfilefmt == OUTFF_ONLINE) {
	output = new OutputFile_Online(clv->outpath, ftrcalc->nfeats, swap_for_be);
    } else if(clv->outfilefmt == OUTFF_ONLINE_SWAPPED) {
	output = new OutputFile_Online(clv->outpath, ftrcalc->nfeats, swap_for_le);
    } else if(clv->outfilefmt == OUTFF_RAW) {
	output = new OutputFile_Raw(clv->outpath, ftrcalc->nfeats, swap_for_be);
    } else if(clv->outfilefmt == OUTFF_RAW_SWAPPED) {
	output = new OutputFile_Raw(clv->outpath, ftrcalc->nfeats, swap_for_le);
    } else if(clv->outfilefmt == OUTFF_ASCII) {
	output = new OutputFile_Ascii(clv->outpath, ftrcalc->nfeats);
    } else if(clv->outfilefmt == OUTFF_HTK) {
	output = new OutputFile_HTK(clv->outpath, ftrcalc->nfeats, swap_for_be, clv->stepTime);
    } else if(clv->outfilefmt == OUTFF_HTK_SWAPPED) {
	output = new OutputFile_HTK(clv->outpath, ftrcalc->nfeats, swap_for_le, clv->stepTime);
    } else {
	fprintf(stderr, "%s: Unknown outfmt %d\n", programName, clv->outfilefmt);
	exit(-1);
    }

    // Run through each file
    FileEntry *inputFileEnt;
    InputFile *input;
    floatVec samples, frame;
    int nframes, gotsamps;
    int totutts = 0;
    float vtnwarp = 1.0;

    while( (clv->nutts < 0 || totutts < clv->firstutt+clv->nutts) &&
	   (inputFileEnt = filelist->GetNextFileEntry()) != NULL) {
	// Pull in next VTN parameter
	if (vtnfile) {
	    if ( vtnfile->getNextFloat(&vtnwarp) == 0 ) {
		fprintf(stderr, "Unable to get vtnwarp parameter for utterance %d (using %.1f)\n", totutts, vtnwarp);
		// but continue anyway?
	    }
	    //fprintf(stderr, "vtnwarp=%f\n", vtnwarp);
	}

	if(totutts < clv->firstutt) {
	    if (clv->verbose > 0) {
		fprintf(stderr, "%s: skipping %s..\n", programName, 
			inputFileEnt->name);
	    }
	} else {
	    if(clv->verbose > 0) {
		fprintf(stderr, "%s: Processing %s..\n", programName, 
			inputFileEnt->name);
	    }
	    input = new InputFile(inputFileEnt->name, 
				  ftrcalc->framelen, ftrcalc->steplen, stepCents, 
				  inputFileEnt->startTime, inputFileEnt->lenTime, inputFileEnt->channel, 
				  ftrcalc->padlen, 
				  clv->hpf_fc_int/(float)1000, ftrcalc->dozpad,
				  clv->ipformatname);
	    if(input == NULL || !(input->OK()) ) {
		fprintf(stderr, "%s: couldn't open input file '%s'\n", 
			programName, inputFileEnt->name);
		// clean up output??
		break;
	    }
	    if(input->sampleRate != clv->sampleRate) {
		fprintf(stderr, "%s: warning: srate of %s reported as %.0f; using %.0f\n", programName, inputFileEnt->name, input->sampleRate, clv->sampleRate);
	    }
	    // don't need this any more now?
	    delete inputFileEnt;
	    inputFileEnt = NULL;

	    output->WriteUttStart(NULL);
	    clv->vtnWarp = vtnwarp;
	    ftrcalc->Reset(clv);
	    nframes = 0;
	    while(!input->Eof()) {
		gotsamps = input->GetFrames(samples);
		if(gotsamps == ftrcalc->framelen) {	/* don't do short */
		    frame = ftrcalc->Process(&samples);
		    if(frame.Len()) {
			output->WriteFrame(frame);
			++nframes;
		    }
		}
	    }
	    // flush the processing state
	    while(frame.Len()) {
		frame = ftrcalc->Process(NULL);
		if(frame.Len()) {
		    output->WriteFrame(frame);
		    ++nframes;
		}
	    }
	    // finish up this input file
	    delete input;
	    output->WriteUttEnd();
	    if(clv->verbose > 0) {
		fprintf(stderr, "%s: Finished (%d frames).\n", programName, nframes);
	    }
	}
	++totutts;
    }
    delete output;
    delete filelist;
    if(clv->verbose > 0) {
	fprintf(stderr, "%s: completed %d utterance%s\n", programName, 
		totutts, (totutts != 1)?"s":"");
    }
    exit(0);
}

