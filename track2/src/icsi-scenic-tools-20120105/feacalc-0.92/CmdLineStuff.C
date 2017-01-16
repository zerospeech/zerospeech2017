//
// CmdLineStuff.C
//
// Stuff for handling the command-line options for feacalc.
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/CmdLineStuff.C,v 1.20 2007/08/30 20:30:51 janin Exp $

#include "CmdLineStuff.H"

#include <time.h>	/* for ctime stamp in history file */

extern "C" {
#include <dpwelib/sndf.h>
#ifdef FALSE
/* These get defined in rasta.h, so avoid the error */
#undef TRUE
#undef FALSE
#endif /* FALSE */
#include <rasta.h>	// to get JAH_DEFAULT & HPF_FC
#ifndef TRUE
/* Make sure they got defined */
#error "rasta.h did not redefine TRUE!"
#endif /* TRUE */
}

// The global set where we write them (as referenced in the CL_ENT table below)
extern CL_VALS clv;

CLE_TOKEN rastaTable[] = {
    { "*F", RASTA_NONE }, 	/* -rasta no */
    { "j?ah", RASTA_JAH }, 
    { "cj?ah", RASTA_CJAH }, 
    { "*T|log|", RASTA_LOG },	/* -rasta yes or -rasta log or -rasta */
    { NULL, 0}
};

CLE_TOKEN plpTable[] = {
    { "*F", PLP_NONE }, 
    { "%d", 0 },		/* -plp 12 etc. */
    { "*T|", PLP_DEFAULT},	/* -plp yes or -plp alone */
    { NULL, 0 }
};

CLE_TOKEN outTable [] = {
    { "lin?ear", DOMAIN_LIN }, 
    { "log?arithmic", DOMAIN_LOG },
    { "cep?stra", DOMAIN_CEPSTRA },
    { NULL, 0}
};

CLE_TOKEN hpfTable[] = {
    { "*F", 0 },
    { "%f", 1000 }, 
    { "*T|", (int)(1000*HPF_FC) },	/* -hpf is the same as -hpf yes */
    { NULL, 0}
};

CLE_TOKEN outFileFmtTable [] = {
    { "p?file", OUTFF_PFILE }, 
    { "o?nline", OUTFF_ONLINE },
    { "r?aw", OUTFF_RAW },
    { "s?wappedraw", OUTFF_RAW_SWAPPED }, 
    { "swappedo?nline", OUTFF_ONLINE_SWAPPED }, 
    { "a?scii", OUTFF_ASCII }, 
    { "h?tk", OUTFF_HTK }, 
    { "htks?wapped", OUTFF_HTK_SWAPPED }, 
    { "sr?i", OUTFF_SRI }, 
    { NULL, 0}
};

/* verbosity level */
CLE_TOKEN verbTable [] = {
    { "q?uiet", 0 }, 
    { "*F|n?ormal", 1 },
    { "*T|", 2 }, 
    { NULL, 0}
};

/* filterbank scale options */
CLE_TOKEN freqTable [] = {
    { "a?uditory|b?ark", 0}, 
    { "m?el|h?tk", 1}, 
    { NULL, 0} 
};

/* filter shape options */
CLE_TOKEN fshapeTable [] = {
    { "tri?angular", 0}, 
    { "trap?ezoidal", 1}, 
    { NULL, 0} 
};

/* generic -opt {yes|no} table */
CLE_TOKEN boolTable [] = {
    { "*T", 1 }, 
    { "*F", 0 },
    { NULL, 0}
};

// Declare our special syntax-checking function
int ipformatSetFn(char *arg, void *where, char *flag, void *data);

CLE_ENTRY clargs[] = {
    { "Audio feature calculation program",
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
    { "feacalc version " PACKAGE_VERSION, 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
   { "-r?asta|--ras?ta", CLE_T_TOKEN, "rasta processing style", 
      "no", &clv.rasta, NULL, rastaTable }, 
    { "-plp", CLE_T_TOKEN, "plp model selection/order", 
      QUOTE(TYP_MODEL_ORDER), &clv.plp, NULL, plpTable },
    { "-com?press", CLE_T_TOKEN, "should we cube-root-compress & weight?", 
      "yes", &clv.cmpwt, NULL, boolTable },
    { "-dom?ain", CLE_T_TOKEN, "output feature domain", 
      "lin", &clv.domain, NULL, outTable},
    { "-cep?sorder", CLE_T_INT, "order of output cepstrum (default plp+1)",
      "0", &clv.cepsorder, NULL, NULL }, 
    { "-nu?mfilts", CLE_T_INT, "number of filters in initial filterbank",
      "0", &clv.nFilts, NULL, NULL }, 
    { "-slim?spect", CLE_T_TOKEN, "discard the 'junk' spectral-edge channels", 
      "yes", &clv.slimspect, NULL, boolTable },
    { "-frq?axis", CLE_T_TOKEN, "which kind of frequency axis to use", 
      "bark", &clv.doMel, NULL, freqTable },
    { "-filt?shape|-filt?ershape", CLE_T_TOKEN, "which kind of frequency axis to use", 
      "trap", &clv.filtershape, NULL, fshapeTable },
    { "-j?ah", CLE_T_FLOAT, "value of j for constant-jah (cjah) rasta", 
      QUOTE(JAH_DEFAULT), &clv.jah, NULL, NULL }, 
    { "-up?perrasfrq", CLE_T_FLOAT, "upper cutoff-zero freq for rasta filter",
      QUOTE(UPPER_CUTOFF_FRQ), &clv.fcupper, NULL, NULL }, 
    { "-lo?werrasfrq", CLE_T_FLOAT, "lower cutoff-3dB freq for rasta filter",
      QUOTE(LOWER_CUTOFF_FRQ), &clv.fclower, NULL, NULL }, 
    { "-hist?ory", CLE_T_STRING, "name of jah history file", 
      "", &clv.histpath, NULL, NULL },
    { "-readh?istory", CLE_T_BOOL, "initialize from the jah history file", 
      "0", &clv.readhist, NULL, NULL }, 
    { "-map?pingfile|-mapf?ile", CLE_T_STRING, "jah mapping input text file", 
      "", &clv.mapfile, NULL, NULL }, 
    { "-vt?nfile", CLE_T_STRING, "name of VTN parameters file", 
      "", &clv.vtnfile, NULL, NULL },
    { "-win?dowtime", CLE_T_FLOAT, "duration of analysis window (msec)", 
      "25", &clv.windowTime, NULL, NULL }, 
    { "-step?time", CLE_T_FLOAT, "increment between windows (msec)", 
      "10", &clv.stepTime, NULL, NULL }, 
    { "-pad", CLE_T_BOOL, "pad first & last windows by reflecting samples", 
      "0", &clv.doPad, NULL, NULL }, 
    { "-zero?padtime", CLE_T_FLOAT, "pad start&end with this many ms of zero", 
      "0", &clv.zpadTime, NULL, NULL }, 
    { "-hpf?ilter", CLE_T_TOKEN, "high-pass filter the input to remove DC (at this freq)", 
      "no", &clv.hpf_fc_int, NULL, hpfTable },
    { "-dith?er", CLE_T_BOOL, "add 1 bit of dither noise", 
      "0", &clv.doDither, NULL, NULL },
    { "-delta?order", CLE_T_INT, "order of delta calculation (0..2)", 
      "2", &clv.deltaOrder, NULL, NULL }, 
    { "-deltawin", CLE_T_INT, "time points for delta calculation", 
      "9", &clv.deltaWindow, NULL, NULL }, 
    { "-list?s", CLE_T_BOOL, "file names are lists of input file ids", 
      "", &clv.uselists, NULL, NULL}, 
    { "-sent_s?tart", CLE_T_INT, "skip this many files before starting", 
      "0", &clv.firstutt, NULL, NULL},
    { "-sent_c?ount", CLE_T_INT, "maximum number of files to process", 
      "-1", &clv.nutts, NULL, NULL},
    { "-range?rate", CLE_T_FLOAT, "sampling rate of range limits if any", 
      "0", &clv.rangeRate, NULL, NULL },
    { "-rngst?artoffset", CLE_T_FLOAT, "offset for pre-scaling range start", 
      "0", &clv.rngStartOffset, NULL, NULL },
    { "-rngend?offset", CLE_T_FLOAT, "offset for pre-scaling range end", 
      "0", &clv.rngEndOffset, NULL, NULL },
    { "-ip?format", CLE_T_STRING, "input sound file format (sndf token)", 
      "", &clv.ipformatname, NULL, NULL}, 
    { "-sam?plerate|-sr", CLE_T_FLOAT, "sampling rate of input sounds", 
      "8000", &clv.sampleRate, NULL, NULL }, 
    { "-nyq?uistrate", CLE_T_FLOAT, "optionally bandlimit input to this frq", 
      "0", &clv.nyqRate, NULL, NULL }, 
    { "-file?cmd", CLE_T_STRING, "command to map file ids to paths", 
      "", &clv.filecmd, NULL, NULL}, 
    { "-wavd?irectory", CLE_T_STRING, "directory containing wav files (instead of filecmd)", 
      "", &clv.wavdirectory, NULL, NULL}, 
    { "-wave?xtension", CLE_T_STRING, "extension to complete wav file names (instead of filecmd)", 
      "", &clv.wavextension, NULL, NULL}, 
    { "-o?utput", CLE_T_STRING, "write output to this file", 
      "-", &clv.outpath, NULL, NULL}, 
    { "-op?format|-format", CLE_T_TOKEN, "output file format", 
      "pfile", &clv.outfilefmt, NULL, outFileFmtTable}, 
    //    { "-strut?mode", CLE_T_BOOL, "hacks for strut compatibility (IGNORED)", 
    //      "0", &clv.strutmode, NULL, NULL },
    { "-cls?ave", CLE_T_TOKEN, "save command line in a history file?", 
      "yes", &clv.cmdHistory, NULL, boolTable}, 
    { "-clh?istname", CLE_T_STRING, "name of command-line history file", 
      "./.feacalc_hist", &clv.cmdHistName, NULL, NULL },
    { "-v?erbose|--ver?bose", CLE_T_TOKEN, "verbose reporting", 
      "normal", &clv.verbose, NULL, verbTable }, 
    { "-h?elp", CLE_T_BOOL, "print usage message", 
      "0", &clv.help, NULL, NULL }, 

    { "inputfile [inputfile ...] or '-' for stdin", CLE_T_USAGE, NULL, 
     NULL, NULL, NULL}, 
    { NULL, CLE_T_END, NULL, NULL, NULL, NULL, NULL }
};

// cle extension function to accept the sndf output file type flag
int ipformatSetFn(char *arg, void *where, char *flag, void *data) 
{   /* User specified *arg as the ipformat.  We should write to 
       the integer clv field at *where. */
    int *pid = (int *)where;

    *pid = SFF_UNKNOWN;
    if (arg && strlen(arg)) {
	if (!SFSetDefaultFormatByName(arg)) {
	    fprintf(stderr, "ipformat: '%s' is not recognized.  Try:\n", arg);
	    SFListFormats(stderr);
	    return -1;
	} else {
	    *pid = SFFormatToID(arg);
	}
    }
    return 1;
}

CL_VALS clv;

CL_VALS *HandleCmdLine(int *pargc, char*** pargv)
{   /* Handle setting the arguments as well as recording the command line 
       in a history file if requested. */
    int err;
    char *cmdLine;
    int argc = *pargc;
    char **argv = *pargv;
    char *programName = argv[0];

    // Save a copy of the cmd line
    {
	int i, len = 0;
	char quote[2];

	quote[0] = '\0';	quote[1] = '\0';

	for (i = 0; i < argc; ++i) {
	    len += strlen(argv[i])+1;
	    if (strchr(argv[i], ' ') != NULL) {
		// allocate space for quotes
		len += 2;
	    }
	}
	cmdLine = new char[len];
	strcpy(cmdLine, argv[0]);
	for (i = 1; i < argc; ++i) {
	    strcat(cmdLine, " ");
	    // maybe add a quote chr? (made space for it above)
	    if (strchr(argv[i], ' ') != NULL) {
		if (strchr(argv[i], '\"') == NULL) {
		    quote[0] = '\"';
		} else {	// last hope
		    quote[0] = '\'';
		}
		strcat(cmdLine, quote);
	    }
	    strcat(cmdLine, argv[i]);
	    if (quote[0]) {
		strcat(cmdLine, quote);
		quote[0] = '\0';
	    }
	}
    }

    // Handle the args
    cleSetDefaults(clargs);
    err = cleExtractArgs(clargs, &argc, argv, 0);
    if(err || clv.help) {
	cleUsage(clargs, programName);
	exit(-1);
    }

    // Fixup the ipformat name
    if(ipformatSetFn(clv.ipformatname, &clv.ipformat, NULL, NULL) < 0) {
	cleUsage(clargs, programName);
	exit(-1);
    }

    // set up the output argc & argv as the ones left over by cleExtract
    *pargc = argc;
    *pargv = argv;	// actually, I guess it already was...

    // Maybe append this command to the history line..?
    if(clv.cmdHistory) {
	FILE *cmdHfile = fopen(clv.cmdHistName, "a");
	if(!cmdHfile) {
	    fprintf(stderr, "%s: unable to write command history to '%s'\n", 
		    programName, clv.cmdHistName);
	} else {
	    time_t now = time(0);
	    fprintf(cmdHfile, "%s @ %s", cmdLine, ctime(&now));
	    fclose(cmdHfile);
	}
    }
    delete [] cmdLine;

    return &clv;
}
