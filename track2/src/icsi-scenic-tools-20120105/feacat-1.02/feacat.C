//
// feacat.C
//
// 'Universal' feature-stream-to-feature-stream junction.
// Uses the quicknet libraries to allow promiscuous conversions between 
// any supported quicknet FtrStream types (fixed-width streams of 
// features broken into utterances).
//
// Optionally also effects padding/trimming of each utterance, 
// subsetting of utterances and subsetting of features
//
// 1998jun08 dpwe@icsi.berkeley.edu
 static const char *rcsid="$Header: /u/drspeech/repos/feacat/feacat.C,v 1.48 2010/10/29 03:41:52 davidj Exp $";

// This same source file generates both feacat, the feature file conversion
// program, and labcat, the same thing for label files.  They both use 
// very similar QN_Stream access, so it's easier to have them in one 
// file and use cpp to alternate between them.  The key symbol is 
// LABCAT

#ifdef LABCAT

// We are compiling labcat
#define TYPENAME "label"
#define QN_INSTREAM QN_InLabStream
#define QN_OUTSTREAM QN_OutLabStream
#define NEWINSTREAM NewInLabStream
#define ELTYPE QNUInt32
#define READFN read_labs
#define WRITEFN write_labs
#define DFLT_FORMAT "ilab"

#else /* !LABCAT - we're doing feacat */

#define TYPENAME "feature"
#define QN_INSTREAM QN_InFtrStream
#define QN_OUTSTREAM QN_OutFtrStream
#define NEWINSTREAM NewInFtrStream
#define ELTYPE float
#define READFN read_ftrs
#define WRITEFN write_ftrs
#define DFLT_FORMAT "pfile"

#endif /* LABCAT / feacat */

#include <stdio.h>
#include <assert.h>
#include <math.h>	/* for rint() */

#include <QN_streams.h>
#include <QN_PFile.h>
#include <QN_Strut.h>
#include <QN_ILab.h>
#include <QN_camfiles.h>
#include <QN_RapAct.h>
#include <QN_AsciiStream.h>
#include <QN_HTKstream.h>
#include <QN_SRIfeat.h>
#include <QN_MiscStream.h>
#include <QN_filters.h>	

extern "C" {
#include <dpwelib/cle.h>
}

// Jeff's range specification class
#ifndef HAVE_BOOL
#ifndef BOOL_DEFINED
enum bool { false = 0, true = 1 };
#define BOOL_DEFINED
#endif
#endif

#include "Range.H"

static struct {
    const char* oldstyle_in_file;
    const char* out_file;
    int in_type;		// flag for type of input stream
    int out_type;		// flag for type of output stream
    const char *deslenfilename;	// name of opt. file of desired output seg lens
    int des_type;               // deslenfile is list, or prototype
    const char *skipfilename;		// name of opt. file of initial skips
    const char *utt_range_def;	// which utterances to process
    const char *data_range_def;	// which elements to pass
    const char *pfr_range_def;	// per-utterance frame range
    int data_width;		// elements per frame, for Cam files
    int padframes;		// pad (>0) or trim (<0) frames on each utt
    int indexedin;		// used indexed access to input files
    int indexedout;		// used indexed access to output files
    int repeatutts;		// repeat each utterance this number of times
    int mergeutts;		// write it all out as one long utt
    int query;			// just querying files
    int debug;			// Debug level.
    int verbose;		// for -verbose
    int help;			// for -help
    int lists;			// input files are lists of data file names
    const char *outlistfilename;	// list of per-utterance output file names
    float steptime;		// milliseconds per frame step (for HTK period)
    int typecode;		// HTK output data type code
    const char* sriftrname;		// Name of SRI feature (type "cepfile" if "")
    int transform;		// code for feature transform (log, exp...)
    const char *phoneset;		// phoneset file defines symbols
    int usefnsepstr;            // respect "file1//file2" syntax for pasting
} config;

static const char *programName;

// How many dummy values to pad/strip from start of each UNIIO frame?
#define UNIIO_N_DUMMY 1
// Now just one - but have to make first feature column be energy


// Tokens for defining data stream types
enum {
    FTYPE_UNK = 0, 
// ICSI stream formats
    FTYPE_PFILE, 
    FTYPE_RAPBIN, 
    FTYPE_RAPHEX, 
    FTYPE_RAPASCII, 
// Cambridge stream formats
    FTYPE_ONLFTR, 
    FTYPE_PRE, 
    FTYPE_LNA8, 
// New ICSI label-only format
    FTYPE_ILAB,
// Recent feature additions
    FTYPE_ASCII,
    FTYPE_ASCIIF,		// Like ASCII but uses %f
    FTYPE_HTK, 
    FTYPE_UNIIO,
    FTYPE_STRUT,
    FTYPE_SRI, 
    FTYPE_RAWBE,
    FTYPE_RAWLE, 
    FTYPE_LIST
};

CLE_TOKEN ftTable[] = {
    { "u?nknown", FTYPE_UNK} , 
    { "pf?ile",   FTYPE_PFILE} , 
    { "st?rut",   FTYPE_STRUT} , 
#ifndef LABCAT
    { "r?apbin",  FTYPE_RAPBIN} ,
    { "raph?ex",  FTYPE_RAPHEX} ,
    { "rapa?scii",  FTYPE_RAPASCII} ,
    { "h?tk", FTYPE_HTK } ,
    { "sr?i", FTYPE_SRI } ,
    { "uni?io", FTYPE_UNIIO } ,
    { "raw?be", FTYPE_RAWBE } ,
    { "rawle", FTYPE_RAWLE } ,
#endif /* LABCAT */
    { "o?nlftr",  FTYPE_ONLFTR} , 
    { "pr?e",     FTYPE_PRE} , 
    { "ln?a8",    FTYPE_LNA8} ,
#ifdef LABCAT
    { "i?lab", 	  FTYPE_ILAB} ,
#endif /* LABCAT */
    { "a?scii",	  FTYPE_ASCII} ,
    { "asciif",   FTYPE_ASCIIF} ,
    { "li?st",	  FTYPE_LIST} ,
    { NULL, 0}
};

// Table for options that can take a true/false argument, but 
// default to true if specified (-ix, -ox)
CLE_TOKEN defTrueTable[] = {
    { "*F", 0 }, 
    { "%d", 0 },	/* special case: take the integer value */
    { "*T|", 1},	/* empty string treated as true */
    { NULL, 0 }
};

// Table of valid tokens for transform option
// They're bits, so they can be or'd together
enum {
    TRF_NONE     = 0, 
    TRF_LOG      = 0x0001, 
    TRF_EXP      = 0x0002, 
    TRF_SOFTMAX  = 0x0004, 
    TRF_DCT      = 0x0008,
    TRF_NORM     = 0x0010,
    TRF_SAFELOG  = 0x0020
};

CLE_TOKEN transformTable[] = {
    { "none", TRF_NONE }, 
    { "l?og", TRF_LOG },
    { "sa?felog", TRF_SAFELOG },
    { "e?xp", TRF_EXP },
    { "s?oftmax", TRF_SOFTMAX },
    { "d?ct", TRF_DCT },
    { "norm", TRF_NORM },
    { NULL, 0 }
};

CLE_ENTRY clargs [] = {
    { "Universal " TYPENAME " stream converter with trimming/padding", 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
#ifdef LABCAT
    { "labcat version " PACKAGE_VERSION, 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
#else
    { "feacat version " PACKAGE_VERSION, 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
#endif
    { "-i?nfile", CLE_T_STRING, "Old-style input " TYPENAME " filename spec (-i is redundant)", 
      "", &config.oldstyle_in_file, NULL, NULL },
    { "-ip?format", CLE_T_TOKEN, "Input stream format", 
      DFLT_FORMAT, &config.in_type, NULL, ftTable }, 
    { "-op?format", CLE_T_TOKEN, "Output stream format", 
      DFLT_FORMAT, &config.out_type, NULL, ftTable }, 
    { "-o?utfile", CLE_T_STRING, "Output " TYPENAME " file", 
      "-", &config.out_file, NULL, NULL },
    { "-sr|-s?entrange|-ur|-u?ttrange", CLE_T_STRING, "Sentence range spec", 
      "all", &config.utt_range_def, NULL, NULL }, 
    {
#ifdef LABCAT
	"-w?idth|-lw?idth|-labw?idth", 
#else /* feacat */
	"-w?idth|-fw?idth|-ftrw?idth", 
#endif /* LABCAT */
      CLE_T_INT, "Elements per frame (for Cambridge formats)", 
      "0", &config.data_width, NULL, NULL }, 
    { 
#ifdef LABCAT
	"-lr|-l?abrange|-lab?elrange", 
#else /* feacat */
	"-fr|-f?trrange|-fea?turerange", 
#endif /* LABCAT */
      CLE_T_STRING, TYPENAME " range spec", 
      "all", &config.data_range_def, NULL, NULL }, 
    { "-pr|-pe?ruttrange|-fra?merange", CLE_T_STRING, "Per-utterance frame range spec", 
      "all", &config.pfr_range_def, NULL, NULL }, 
    { "-p?adframes", CLE_T_INT, "Pad (> 0) or trim (< 0) frames at each end of each utterance", 
      "0", &config.padframes, NULL, NULL }, 
    { "-r?epeatutts", CLE_T_INT, "Repeat each utterance this many times", 
      "1", &config.repeatutts, NULL, NULL }, 
    { "-m?ergeutts", CLE_T_BOOL, "Merge all frames into one output segment", 
      "0", &config.mergeutts, NULL, NULL }, 
    { "-des?lenfile|-dl?f", CLE_T_STRING, "File of desired output segment lengths", 
      "", &config.deslenfilename, NULL, NULL }, 
    { "-dlt?ype|-dt", CLE_T_TOKEN, "Format of deslen file", 
      "list", &config.des_type, NULL, ftTable }, 
    { "-sk?ipfile|-sf", CLE_T_STRING, "File of per-segment initial frame skips", 
      "", &config.skipfilename, NULL, NULL }, 
    { "-l?ists", CLE_T_BOOL, "Input files are lists of data file names", 
      "0", &config.lists, NULL, NULL }, 
    { "-ol?ist|-outl?ist|-outfi?lelist", CLE_T_STRING, "File of per-segment output file names", 
      "", &config.outlistfilename, NULL, NULL }, 
#ifndef LABCAT /* HTK options for feacat only */
    { "-st?eptime|-peri?od", CLE_T_FLOAT, "Frame period (milliseconds) (for HTK header)",
      "10.0", &config.steptime, NULL, NULL }, 
    { "-htk?code", CLE_T_INT, "Type code (for HTK header)", 
      "9", &config.typecode, NULL, NULL },
    { "-srif?trname", CLE_T_STRING, "Feature name (for SRI header)", 
      "", &config.sriftrname, NULL, NULL },
    /* Transform also applies to feacat only */
    { "-tr?ansform", CLE_T_TOKEN, "Apply a fixed transform to ftr vals", 
      "none", &config.transform, NULL, transformTable }, 
#else /* LABCAT - phoneset options for labcat only */
    { "-ph?onesetfile", CLE_T_STRING, "Phoneset definition file (for ascii)", 
      "", &config.phoneset, NULL, NULL },    
#endif /* LABCAT */
    { "-ind?exedin|-ix", CLE_T_TOKEN, "Use indexed access to input files", 
      NULL, &config.indexedin, NULL, defTrueTable }, 
    { "-indexedo?ut|-ox", CLE_T_TOKEN, "Use indexed access to output file", 
      NULL, &config.indexedout, NULL, defTrueTable }, 
    { "-used?oubleslash", CLE_T_BOOL, "Use // syntax to paste files", 
      "0", &config.usefnsepstr, NULL, NULL }, 
    { "-q?uery", CLE_T_BOOL, "Just report details of input files", 
      "0", &config.query, NULL, NULL }, 
    { "-d?ebug", CLE_T_INT, "Debug level", 
      "0", &config.debug, NULL, NULL }, 
    { "-v?erbose", CLE_T_BOOL, "Verbose status reporting", 
      "0", &config.verbose, NULL, NULL }, 
    { "-h?elp", CLE_T_BOOL, "Print usage message", 
      "0", &config.help, NULL, NULL }, 
    { "[infile ...] (default to stdin)", CLE_T_USAGE, NULL, 
      NULL, NULL, NULL},

    { NULL, CLE_T_END, NULL, NULL, NULL, NULL, NULL }
};

FILE *fopen_gz(const char *filename, const char *mode) {
    /* Cover for fopen, will use popen and gzip if the filename 
       ends in .gz */
    FILE *stream;

    size_t len = strlen(filename);
    if (len>3 && strcmp((filename+len-3), ".gz") == 0) {
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
	if (stream == NULL) {
	    fprintf(stderr, "fopen_gz: couldn't fopen('%s','%s')\n", 
		    filename, mode);
	}
    }

    return stream;
}

QN_InLabStream *NewInLabStream(const char *filename, int type, int indexed, 
			 QN_TokenMap *tokmap, 
			 int *pwidth, FILE **pfile, int allowzwidth = 0) {
    // Open a QN_InXxxStream in one of the specified types
    QN_InLabStream *rslt = NULL;
    int debug = config.debug;
    const char *dtag = programName;
    FILE *file = NULL;
    int width = *pwidth;
    int nftrs   = width;
    int nlabels = 1;

    file = fopen_gz(filename, "r");

    if (file == NULL) {
	fprintf(stderr, "NewInLabStream: couldn't read '%s'\n", filename);
	if (pfile)	*pfile = NULL;
	return NULL;
    }

    switch (type) {
    case FTYPE_PFILE:
	rslt = new QN_InFtrLabStream_PFile(debug, dtag, file, indexed);
	break;
    case FTYPE_ILAB:
	rslt = new QN_InLabStream_ILab(debug, dtag, file, indexed);
	break;
    case FTYPE_ASCII:
    case FTYPE_ASCIIF:
	rslt = new QN_InFtrLabStream_Ascii(debug, dtag, file, 
					   nftrs, nlabels, indexed, 
					   tokmap);
	break;
    case FTYPE_ONLFTR:
	if (width <= 0) {
	    fprintf(stderr, "NewInLabStream: must specify width for ONLFTR\n");
	} else {
	    rslt = new QN_InFtrLabStream_OnlFtr(debug, dtag, file, 
						width, indexed);
	}
	break;
    case FTYPE_PRE:
	if (width <= 0) {
	    fprintf(stderr, "NewInLabStream: must specify width for PRE\n");
	} else {
	    rslt = new QN_InFtrLabStream_PreFile(debug, dtag, file, 
						 width, indexed);
	}
	break;
    case FTYPE_LNA8:
	if (width <= 0) {
	    fprintf(stderr, "NewInLabStream: must specify width for LNA8\n");
	} else {
	    rslt = new QN_InFtrLabStream_LNA8(debug, dtag, file, 
					      width, indexed);
	}
	break;
    default:
	fprintf(stderr, "NewInLabStream: unknown type code %d\n", type);
	break;
    }

    // Read the actual width, check it or set it
    if (rslt) {
	width = rslt->num_labs();
	if (*pwidth != 0) {
	    //	    if (width != *pwidth) {
	    //	fprintf(stderr, "NewInLabStream: ERROR: %s has %d elements, but %d requested\n", filename, width, *pwidth);
	    //	delete rslt;
	    //	rslt = NULL;
	    //}
	}
	*pwidth = width;
	if (width <= 0 && !allowzwidth) {
	    fprintf(stderr, "NewInLabStream: input '%s' has zero width!\n", 
		    filename);
	    delete rslt;
	    rslt = NULL;
	}
    }

    if (rslt == NULL) {
	// failed to open as a stream
	fprintf(stderr, "NewInLabStream: abandoning input\n");
	fclose(file);
	file = NULL;
    }
    
    // Return the file handle too
    if (pfile)	*pfile = file;

    return rslt;
}

QN_InFtrStream *NewInFtrStream(const char *filename, int type, int indexed, 
			 QN_TokenMap *tokmap, 
			 int *pwidth, FILE **pfile, int allowzwidth = 0) {
    // Open a QN_InXxxStream in one of the specified types
    QN_InFtrStream *rslt = NULL;
    int debug = config.debug;
    const char *dtag = programName;
    FILE *file = NULL;
    int width = *pwidth;
    int nftrs   = width;
    int nlabels = 0;

    file = fopen_gz(filename, "r");

    if (file == NULL) {
	fprintf(stderr, "NewInFtrStream: couldn't read '%s'\n", filename);
	if (pfile)	*pfile = NULL;
	return NULL;
    }

    switch (type) {
    case FTYPE_PFILE:
	rslt = new QN_InFtrLabStream_PFile(debug, dtag, file, indexed);
	break;
    case FTYPE_STRUT:
	rslt = new QN_InFtrStream_Strut(debug, dtag, file);
	break;
    case FTYPE_SRI:
	rslt = new QN_InFtrStream_SRI(debug, dtag, file);
	break;
    case FTYPE_RAPBIN:
	rslt = new QN_InFtrStream_Rapact(debug, dtag, file, \
					 QN_STREAM_RAPACT_BIN, indexed);
	break;
    case FTYPE_RAPHEX:
	rslt = new QN_InFtrStream_Rapact(debug, dtag, file, \
					 QN_STREAM_RAPACT_HEX, indexed);
	break;
    case FTYPE_RAPASCII:
	rslt = new QN_InFtrStream_Rapact(debug, dtag, file, \
					 QN_STREAM_RAPACT_ASCII, indexed);
	break;
    case FTYPE_HTK:
	rslt = new QN_InFtrStream_HTK(debug, dtag, file, indexed);
	break;
    case FTYPE_UNIIO:
  	rslt = new QN_InFtrStream_UNIIO(debug, dtag, file, indexed, UNIIO_N_DUMMY);
	break;
    case FTYPE_RAWLE:
	if (width <= 0) {
	    fprintf(stderr, "NewInFtrStream: must specify width for RAWLE\n");
	} else {
	    rslt = new QN_InFtrStream_raw(debug, dtag, file, QNRAW_FLOAT32, QNRAW_LITTLEENDIAN, width, indexed);
	}
	break;
    case FTYPE_RAWBE:
	if (width <= 0) {
	    fprintf(stderr, "NewInFtrStream: must specify width for RAWBE\n");
	} else {
	    rslt = new QN_InFtrStream_raw(debug, dtag, file, QNRAW_FLOAT32, QNRAW_BIGENDIAN, width, indexed);
	}
	break;
    case FTYPE_ASCII:
    case FTYPE_ASCIIF:
	/* only demand width be > 0 for labcat */
	if (width <= 0) {
	    fprintf(stderr, "NewInFtrStream: must specify width for ASCII\n");
	}
	rslt = new QN_InFtrLabStream_Ascii(debug, dtag, file, 
					   nftrs, nlabels, indexed, 
					   tokmap);
	break;
    case FTYPE_ONLFTR:
	if (width <= 0) {
	    fprintf(stderr, "NewInFtrStream: must specify width for ONLFTR\n");
	} else {
	    rslt = new QN_InFtrLabStream_OnlFtr(debug, dtag, file, 
						width, indexed);
	}
	break;
    case FTYPE_PRE:
	if (width <= 0) {
	    fprintf(stderr, "NewInFtrStream: must specify width for PRE\n");
	} else {
	    rslt = new QN_InFtrLabStream_PreFile(debug, dtag, file, 
						 width, indexed);
	}
	break;
    case FTYPE_LNA8:
	if (width <= 0) {
	    fprintf(stderr, "NewInFtrStream: must specify width for LNA8\n");
	} else {
	    rslt = new QN_InFtrLabStream_LNA8(debug, dtag, file, 
					      width, indexed);
	}
	break;
    default:
	fprintf(stderr, "NewInFtrStream: unknown type code %d\n", type);
	break;
    }

    // Read the actual width, check it or set it
    if (rslt) {
	width = rslt->num_ftrs();
	if (*pwidth != 0) {
	    //	    if (width != *pwidth) {
	    //	fprintf(stderr, "NewInFtrStream: ERROR: %s has %d elements, but %d requested\n", filename, width, *pwidth);
	    //	delete rslt;
	    //	rslt = NULL;
	    //}
	}
	*pwidth = width;
	if (width <= 0 && !allowzwidth) {
	    fprintf(stderr, "NewInFtrStream: input '%s' has zero width!\n", 
		    filename);
	    delete rslt;
	    rslt = NULL;
	}
    }

    if (rslt == NULL) {
	// failed to open as a stream
	fprintf(stderr, "NewInFtrStream: abandoning input\n");
	fclose(file);
	file = NULL;
    }
    
    // Return the file handle too
    if (pfile)	*pfile = file;

    return rslt;
}

/* Let's use two slashes as the separator for multiple pasted files */
#define FNSEPSTR "//"

QN_INSTREAM *NewInStreamMulti(char *filenames, 
			      int type, int indexed, 
			      QN_TokenMap *tokmap, 
			      int *pwidth, 
			      FILE ***pfiles, int *pnfiles) {
    /* If filename is actually several files, try to open them 
       all in parallel */
    QN_INSTREAM *rslt = NULL;
    int totfiles = 1;
    int totwidth = 0;

    int debug = config.debug;
    const char *dtag = programName;

    char *sp = filenames;

    // How many filenames are there?
    if (config.usefnsepstr) {
	while ( (sp = strstr(sp, FNSEPSTR)) != NULL) {
	    ++totfiles;
	    sp += strlen(FNSEPSTR);
	}
    }

    // Allocate space for files
    *pfiles = new FILE*[totfiles];
    FILE **pfile = *pfiles;

    // OK, build the merge, one at a time
    sp = filenames;

    while(sp) {
	// Pull out this filename
	int len = strlen(sp);

	char *spend = NULL;
	if (config.usefnsepstr) {
	  spend = strstr(sp, FNSEPSTR);
	  if (spend) 	len = spend - sp;
	}

	char *fn = new char[len + 1];
	strncpy(fn, sp, len);
	fn[len] = '\0';

	// Build this file
	int width = *pwidth;
	QN_INSTREAM *newstr = NEWINSTREAM(fn, type, indexed, tokmap, 
					  &width, pfile);
	delete [] fn;

	// Maybe merge into result
	if (rslt) {
#ifndef LABCAT
	    // Pasting only for features
	    rslt = new QN_InFtrStream_JoinFtrs(debug, dtag, *rslt, *newstr);
#else
	    fprintf(stderr, "%s: ERROR: no pasting for labels (%s)\n", 
		    dtag, filenames);
	    exit(-1);	    
#endif /* !LABCAT */
	} else {
	    rslt = newstr;
	}

	// Track total width
	totwidth += width;
	// update where to write file ptrs
	++pfile;

	// Skip filename pointer onto next one
	if (spend) spend = spend + strlen(FNSEPSTR);
	sp = spend;
    }

    *pwidth = totwidth;
    *pnfiles = totfiles;

    return rslt;
}

QN_OUTSTREAM *NewOutStream(const char *filename, int type, int indexed, 
			   QN_TokenMap *tokmap, 
			   int width, FILE **pfile) {
    // Open a QN_OutXxxStream in one of the specified types
    QN_OUTSTREAM *rslt = NULL;
    int debug = config.debug;
    const char *dtag = programName;
    FILE *file = NULL;
#ifdef LABCAT
    int nftrs   = 0;
    int nlabels = width;
#else /* feacat */
    int nftrs   = width;
    int nlabels = 0;
#endif /* LABCAT */
    const char *sriftrname = config.sriftrname;
    int online = 0;
    const char *writemode = "w";

    if (strcmp(sriftrname, "")==0)
	sriftrname = NULL;
    /* HTK, ilab, SRI and QuickNet need to seek back to rewrite headers, so 
       mode must be w+; this means we can't write gz files directly, 
       so only use it when we have to. */
#ifdef LABCAT
    if (type == FTYPE_PFILE || type == FTYPE_ILAB)
#else /* FEACAT */
    if (type == FTYPE_PFILE || type == FTYPE_HTK || type == FTYPE_SRI)
#endif /* LABCAT */
      writemode = "w+";

    file = fopen_gz(filename, writemode);
    if (file == NULL) {
	fprintf(stderr, "NewOutStream: couldn't write '%s'\n", filename);
	if (pfile) *pfile = NULL;
	return NULL;
    }
    if (file == stdout) {
	online = 1;
    }

    switch (type) {
    case FTYPE_PFILE:
	rslt = new QN_OutFtrLabStream_PFile(debug, dtag, file, nftrs, 
					    nlabels, indexed);
	break;
#ifndef LABCAT
    case FTYPE_RAPBIN:
	rslt = new QN_OutFtrStream_Rapact(debug, dtag, file, nftrs, 
					  QN_STREAM_RAPACT_BIN, online);
	break;
    case FTYPE_RAPHEX:
	rslt = new QN_OutFtrStream_Rapact(debug, dtag, file, nftrs, 
					  QN_STREAM_RAPACT_HEX, online);
	break;
    case FTYPE_RAPASCII:
	rslt = new QN_OutFtrStream_Rapact(debug, dtag, file, nftrs, 
					  QN_STREAM_RAPACT_ASCII, online);
	break;
    case FTYPE_ONLFTR:
	rslt = new QN_OutFtrStream_OnlFtr(debug, dtag, file, nftrs);
	break;
    case FTYPE_SRI:
	rslt = new QN_OutFtrStream_SRI(debug, dtag, file, nftrs,
				       sriftrname);
	break;
    case FTYPE_HTK:
	rslt = new QN_OutFtrStream_HTK(debug, dtag, file, nftrs, 
				       config.typecode, 
				       (int)rint(config.steptime*10000));
	/* HTK period is in units of 100ns, so to get the correct 
	   value for our real number of ms, we multiply by 10,000 */
	break;
    case FTYPE_UNIIO:
	rslt = new QN_OutFtrStream_UNIIO(debug, dtag, file, nftrs, UNIIO_N_DUMMY);
	break;
    case FTYPE_LNA8:
	rslt = new QN_OutFtrLabStream_LNA8(debug, dtag, file, nftrs, online);
	break;
    case FTYPE_RAWLE:
	rslt = new QN_OutFtrStream_raw(debug, dtag, file, QNRAW_FLOAT32, QNRAW_LITTLEENDIAN, nftrs, indexed);
	break;
    case FTYPE_RAWBE:
	rslt = new QN_OutFtrStream_raw(debug, dtag, file, QNRAW_FLOAT32, QNRAW_BIGENDIAN, nftrs, indexed);
	break;
#else /* LABCAT */
    case FTYPE_ONLFTR:
    case FTYPE_LNA8:
    case FTYPE_RAWLE:
    case FTYPE_RAWBE:
	fprintf(stderr, "NewOutStream: illegal output type code %d\n", type);
	break;
    case FTYPE_ILAB:
	// int maxlabel = 255;
	rslt = new QN_OutLabStream_ILab(debug, dtag, file, \
					255 /* maxlabel */, indexed);
	break;
#endif /* LABCAT */
    case FTYPE_ASCII:
	rslt = new QN_OutFtrLabStream_Ascii(debug, dtag, file, 
					    nftrs, nlabels, 
					    indexed, tokmap);
	break;
    case FTYPE_ASCIIF:
	rslt = new QN_OutFtrLabStream_Ascii(debug, dtag, file, 
					    nftrs, nlabels, 
					    indexed, tokmap, true);
	break;
    case FTYPE_PRE:
	rslt = new QN_OutFtrLabStream_PreFile(debug, dtag, file, nftrs);
	break;
    default:
	fprintf(stderr, "NewOutStream: unknown type code %d\n", type);
	break;
    }

    if (rslt == NULL) {
	// failed to open as a stream
	fprintf(stderr, "NewOutStream: abandoning output\n");
	fclose(file);
	file = NULL;
    }

    // Return the file handle too
    if (pfile) *pfile = file;

    return rslt;
}

class InputStream {
    // Handle all the local stuff associated with each input file
public:
    InputStream(const char *fname, int type, int indexed, int width, 
		int verb = 0);
    ~InputStream(void);

    // Covers to QN_INSTREAM
    int set_pos(int utt, int mode) { return instream->set_pos(utt, mode); }
    int nextseg(void)              { return instream->nextseg(); }
    size_t  num_frames(int utt)    { return instream->num_frames(utt); }
    size_t  readfn(int nfr, ELTYPE *buf) { return instream->READFN(nfr, buf); }

    QN_INSTREAM* instream;
    char *filename;
    FILE **infiles;
    int ninfiles;
    ELTYPE *data_buffer;
    size_t num_utts;
    int tot_frames;
    int uttnum;
    int ipwidth;

    int verbose;

    // Class methods
    static InputStream *getInputStream(const char *fname, int type, 
				       int indexed, int width, int verb = 0);
    static void closeAll(void);

    // Class slots
#define MAX_INPUT_STREAMS 4
    static InputStream *inputs[MAX_INPUT_STREAMS];
    static int ninputs;
    static const char *debugName;
    static int buf_frames;
    static QN_TokenMap *tokmap;
};

int InputStream::ninputs = 0;
int InputStream::buf_frames = 0;
InputStream* InputStream::inputs[MAX_INPUT_STREAMS];
const char *InputStream::debugName = "feacat/labcat";
QN_TokenMap *InputStream::tokmap = NULL;

void InputStream::closeAll(void) {
    // Close all open input streams
    int i;
    for (i = 0; i < ninputs; ++i) {
	delete inputs[i];
	inputs[i] = NULL;
    }
    ninputs = 0;
}

InputStream *InputStream::getInputStream(const char *fname, int type, 
    int indexed, int width, 
					 int verb /* = 0 */) {
    // Either retrieve an open file from the cache, or open it
    int i;
    int which = -1;
    InputStream* strm = NULL;

    for (i=0; i<ninputs; ++i) {
	if (strcmp(inputs[i]->filename, fname)==0) {
	    which = i;
	    break;
	}
    }

    if (which == -1) {
	// Didn't find it, so add it (or reuse bottom of pile)

	if (ninputs == MAX_INPUT_STREAMS) {
	    // Drop one off the bottom of the pile
	    --ninputs;
	    delete inputs[ninputs];
	}

	// Open this new file
	which = ninputs;
	inputs[ninputs++] = new InputStream(fname, type, indexed, width, verb);
    }

    // move "which" to the top of the pile
    strm = inputs[which];
    for (i = which; i > 0; --i) {
	inputs[i] = inputs[i-1];
    }
    inputs[0] = strm;

    return strm;
}


InputStream::InputStream(const char *fname, int type, int indexed, int width, 
			 int verb /* = 0 */) {

    filename = strdup(fname);
    verbose = verb;

    instream = NewInStreamMulti(filename, type, indexed, tokmap, &width, 
				&infiles, &ninfiles);

    // Bail if open failed
    if (instream == NULL) { 
  	fprintf(stderr, "%s: ERROR: Unable to open input file '%s'\n", 
  		debugName, filename);
  	exit(-1);
    }

    num_utts = instream->num_segs();
    tot_frames = instream->num_frames(QN_ALL), 

    ipwidth = width;

//    fprintf(stderr, "InputStream::ctr: new data_buffer of %dx%d for '%s'\n", 
//	    buf_frames, ipwidth, filename);

    data_buffer = new ELTYPE[buf_frames*ipwidth];

    // Counter of utterance within this file, preset to -1
    // to ensure nextseg() called before any reads.
    uttnum = -1;

    if (verbose) {
	char num_utts_str[32];
	if (num_utts == -1) {
	    sprintf(num_utts_str, "an unknown number of");
	} else {
	    sprintf(num_utts_str, "%ld", num_utts);
	}
	fprintf(stderr, "%s: input file '%s' contains %s utterances\n", 
		debugName, filename, num_utts_str);
    }
}

InputStream::~InputStream(void) {
//    fprintf(stderr, "InputStream::dtr(fn='%s',instr=0x%lx,inf=0x%lx)\n", 
//	    filename, instream, infiles[0]);
    if (instream != NULL) {
	// Maybe close open input file
	delete instream;
	instream = NULL;
	int i;
	for (i = 0; i < ninfiles; ++i) {
	    fclose(infiles[i]);
	}
    }
    if (data_buffer) {
	delete [] data_buffer;
    }
    free(filename);
}

class tokFile {
    // A class that accesses a text file containing a list of 
    // white-space separated tokens, which may be interpreted as 
    // plain strings or as integers
public:
    FILE *file;
    char *filename;
    int buflen;
    char *buf;
    int bufpos;
    int linect;
    static const char *WS;

    tokFile(FILE* a_file, const char *a_name = "(unknown)") {
	file = a_file;
	filename = strdup(a_name);
	buflen = 1024;
	buf = new char[buflen];
	bufpos = 0;
	buf[bufpos] = '\0';
	linect = 0;
    }
    ~tokFile(void) {
	free(filename);
	delete [] buf;
    }
    int getNextTok(const char **ret);
    int getNextInt(int *ret);
    // Shorthand to close our file (even though we didn't open it)
    void close(void) {
	fclose(file);
    }
};
const char *tokFile::WS = " \t\n";

int tokFile::getNextTok(const char **ret) {
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

int tokFile::getNextInt(int *ret) {
    // Read another number from the deslen file as the desired length
    // in frames of the next frame.  File needs just to be white-space
    // delimited, but can have comment lines starting with "#"
    // Next read value is put into *pdeslen.  Return 1 on success, 0 at EOF
    int rc = 0;
    // Get the next number in the line buffer & advance pointer
    const char *str;
    char *end;

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

int do_dct(float *src, float *dst, int width, int nout=0) {
    // Perform a DCT on a float vector
    // Cache the coefficients so repeated calls of the same size 
    // are efficient
    static float *costab = NULL;
    static int tabsize = 0;
    static float *tmpbuf = NULL;	// copy input in case in place

    if (tabsize != 2*(width-1)) {
	tabsize = 2*(width-1);
	if (costab != NULL) delete [] costab;
	costab = new float[tabsize];
	if (tmpbuf != NULL) delete [] tmpbuf;
	tmpbuf = new float[width];
	// Set up costab
	int i;
	float *pf = costab;
	double pionk = M_PI/(width-1);
	for(i=0; i < tabsize; ++i) {
	    *pf++ = cos(pionk*i);
	}
    }

    int i,j,ix, sgn = 1;
    double tmp;
    float *pf;
    
    if (nout == 0) nout = width;

    // Make a copy of the input, so we can overwrite it if dst == src
    pf = tmpbuf;
    for (i = 0; i < width; ++i) {
	*pf++ = *src++;
    }

    for (j = 0; j < nout; ++j) {
	tmp = tmpbuf[0] + sgn*tmpbuf[width-1];
	sgn = -sgn;
	ix = j;
	pf = tmpbuf+1;
	for (i = 1; i < width-1; ++i) {
	    /* calculate inner product against cos bases */
	    /* tmp += 2 * logspec->values[i] * cos(pionk*i*j); */
	    tmp += 2*(*pf++) * costab[ix];
	    ix += j;	if(ix >= tabsize)	ix -= tabsize;
	}
	*dst++ = tmp/(width-1);
    }

    return nout;
}

main(int argc, char *argv[])
{
    int err = 0;
    int nipfiles;
    //const char **ipfiles;
    //const char *ipfiles[MAXIPFILES];
    const char **ipfiles;
    const char *anipfile;
    int  *startUtt;
    const char *defaultip = "-";
    int ecode = 0;
    int i;

    programName = argv[0];
    // Put this where the input file handling class can find it too
    InputStream::debugName = programName;

    // Handle the args
    cleSetDefaults(clargs);

    // Preset indexed flags to "don't know"
    config.indexedin = -1;
    config.indexedout = -1;

    err = cleExtractArgs(clargs, &argc, argv, 0);
    if(err || config.help) {
	cleUsage(clargs, programName);
	exit(-1);
    }

    // Leftover is taken as one or more infiles - unless -i ipfile was
    // specified
    
    if (argc < 2) {
	// nothing on command line - read from stdin
	nipfiles = 1;
	if (config.oldstyle_in_file && strlen(config.oldstyle_in_file)) {
	  //ipfiles = &config.oldstyle_in_file;
	  anipfile = config.oldstyle_in_file;
	  ipfiles = &anipfile;
	} else {
	  //ipfiles = &defaultip;
	  anipfile = defaultip;
	  ipfiles = &anipfile;
	}
    } else {
	if (config.oldstyle_in_file && strlen(config.oldstyle_in_file)) {
	    fprintf(stderr, "%s: archaic use of '-i %s' not allowed with "
		    "extra filenames ('%s ...')\n", programName, 
		    config.oldstyle_in_file, argv[1]);
	    cleUsage(clargs, programName);
	    exit(-1);
	}
	nipfiles = argc-1;
	//ipfiles = argv+1;
	//assert(nipfiles <= MAXIPFILES);
	ipfiles = new const char *[nipfiles];
	for(int i = 0; i<nipfiles; ++i) {
	  ipfiles[i] = argv[i+1];
	}
    }

    // If the ipfiles are coming from a list, read it all in now
    if (config.lists) {
	int dfbufsize = 100;	// default input table size
	const char **datafiles = new const char*[dfbufsize];
	int ndatafiles = 0;

	// each of the ip files is a list file we have to read through
	tokFile *ilistfile = NULL;
	FILE *ipffp;
	const char *fname;
	int i;
	for (i = 0; i < nipfiles; ++i) {
	    if ( (ipffp = fopen_gz(ipfiles[i], "r")) == NULL) {
		fprintf(stderr, "%s: Error: Unable to input data list "
			"file '%s'\n", programName, ipfiles[i]);
		exit(-1);
	    }
	    ilistfile = new tokFile(ipffp, ipfiles[i]);
	    // Pull all the tokens from this file
	    while (ilistfile->getNextTok(&fname) != 0) {
		if (ndatafiles >= dfbufsize) {
		    // Need to realloc the buffer
		    int newdfbufsize = 2*dfbufsize;
		    const char **newdatafiles = new const char*[newdfbufsize];
		    memcpy(newdatafiles, (const void *)datafiles, 
			   sizeof(char*)*dfbufsize);
		    delete [] datafiles;
		    datafiles = newdatafiles;
		    dfbufsize = newdfbufsize;
		}
		datafiles[ndatafiles++] = strdup(fname);
	    }
	    // Close the tokfile
	    delete ilistfile;
	    fclose(ipffp);
	}
	// OK, read tokens from all input files - pass to prog
	nipfiles = ndatafiles;
	//ipfiles = datafiles;
	//assert(nipfiles <= MAXIPFILES);
	ipfiles = new const char *[nipfiles];
	for(int i = 0; i<nipfiles; ++i) {
	  ipfiles[i] = datafiles[i];
	}
    }

    QN_TokenMap *phoneset = NULL;
#ifdef LABCAT
    // If a phoneset file is specified (which can only happen for labcat)
    // set it up now.
    if (config.phoneset && strlen(config.phoneset)) {
	phoneset = new QN_TokenMap();
	if (phoneset->readPhset(config.phoneset) <= 0) {
	    fprintf(stderr, "%s: unable to read phoneset file '%s'\n", 
		    programName, config.phoneset);
	    delete phoneset;
	    phoneset = NULL;
	    // DON'T continue
	    exit(-1);
	}
    }
#endif /* LABCAT */
    // Set up this tokmap as the default for input, output
    InputStream::tokmap = phoneset;

    // We assemble an array of the logical index of each specified input 
    // file so that we can index around in one long logical pfile.  
    // The first value is always 0; the (nipfiles+1) value is the 
    // total number of utterances.
    startUtt = new int[nipfiles+1];
    startUtt[0] = 0;
    for (i = 1; i < nipfiles+1; ++i) {
	// preset to indicate unknown length
	startUtt[i] = INT_MAX;
    }

    // If indexed in/out not specified, use intelligent defaults
    if (config.indexedin < 0) {
	if (config.in_type == FTYPE_PFILE || config.in_type == FTYPE_ILAB) {
	    config.indexedin = 1;
	} else {
	    config.indexedin = 0;
	}
    }
    if (config.indexedout < 0) {
	if (config.out_type == FTYPE_PFILE || config.out_type == FTYPE_ILAB) {
	    config.indexedout = 1;
	} else {
	    config.indexedout = 0;
	}
    }

    // Repeating utterances only works if input is indexed
    if (config.repeatutts > 1) {
	if (!config.indexedin) {
	    fprintf(stderr, "%s: Warning: repeatutts of %d specified "
		    "without -indexedin\n", programName, config.repeatutts);
	}
    }

    InputStream *instream = NULL;
    QN_OUTSTREAM* outstream = NULL;
    FILE *outfile = NULL;
    Range *utt_range = NULL;
    Range *pfr_range = NULL;
    Range *data_range = NULL;
    int opwidth = -1;
    int padframes = config.padframes;
    int ipfileno = -1, lastIpfileno;
    // track frames per file
    int frameslasteof = 0;

    // Buffer for reading frames in
    // If we're trimming (padframes < 0), have to introduce that much
    // delay so we can see the end of the utterance far enough ahead.
    InputStream::buf_frames = 1 + ((padframes<0)? -padframes : 0);
    ELTYPE *op_frame    = NULL;

    // If it *is* indexed input, maybe scan through the input files to 
    // get their lengths
    if (config.indexedin) {
	for (i = 0; i < nipfiles; ++i) {
	    instream = InputStream::getInputStream(ipfiles[i], config.in_type, 
				      config.indexedin, config.data_width, 
				      config.verbose);
	    // Try to set up the index
	    size_t num_utts = instream->num_utts;
	    if (startUtt[i] != INT_MAX && num_utts != QN_SIZET_BAD) {
		startUtt[i+1] = 
		    startUtt[i] + num_utts;
	    }
	}
    }

    utt_range = new Range(config.utt_range_def, 0, startUtt[nipfiles]);
    if (config.debug) {
	utt_range->PrintRanges("utterance range");
    }
    // Construct this now (rather than at top of its loop) to avoid 
    // compile warnings about goto close_in skipping constructors
    Range::iterator urit = utt_range->begin(); 

    // declare these now so that the goto won't skip them (& upset compiler)
    int skipframes = 0;
    tokFile *skipfile = NULL;
    tokFile *outlistfile = NULL;
    int totutts = 0, totframes = 0;

    // First open the desired length file, if any
    int deslen = INT_MAX;	// default value won't chop any utts
    tokFile *deslenfile = NULL;
    QN_InFtrStream *desftrfile = NULL;
    if (config.deslenfilename && strlen(config.deslenfilename) > 0) {
	FILE *deslenfp;
	if ( (deslenfp = fopen_gz(config.deslenfilename, "r")) == NULL) {
	    fprintf(stderr, "%s: Error: Unable to read desired lengths "
		    "file '%s'\n", programName, config.deslenfilename);
	    ecode = -1;
	    goto close_in;
	}
	if (config.des_type  == FTYPE_LIST) {
	  deslenfile = new tokFile(deslenfp, config.deslenfilename);
	} else {
	  int dummy; /* width */
	  fclose(deslenfp);
	  desftrfile = NewInFtrStream(config.deslenfilename, config.des_type, 
				   1 /*indexed*/, NULL, &dummy, &deslenfp, 1);
	  deslenfile = (tokFile *)-1;
	}
    }

    // Similarly for the initial skips file
    if (config.skipfilename && strlen(config.skipfilename) > 0) {
	FILE *skipfp;
	if ( (skipfp = fopen_gz(config.skipfilename, "r")) == NULL) {
	    fprintf(stderr, "%s: Error: Unable to read initial skips "
		    "file '%s'\n", programName, config.skipfilename);
	    ecode = -1;
	    goto close_in;
	}
	skipfile = new tokFile(skipfp, config.skipfilename);
    }

    // Similarly for the output filename list
    if (config.outlistfilename && strlen(config.outlistfilename) > 0) {
	if ( config.out_file && strlen(config.out_file) \
	     && strcmp(config.out_file, "-") != 0) {
	    fprintf(stderr, "%s: Error: you cannot specify both an output "
		    "filename '%s' and a list '%s'\n", programName, 
		    config.out_file, config.outlistfilename);
	    ecode = -1;
	    goto close_in;
	    
	}
	FILE *outlistfp;
	if ( (outlistfp = fopen_gz(config.outlistfilename, "r")) == NULL) {
	    fprintf(stderr, "%s: Error: Unable to read out name list "
		    "'%s'\n", programName, config.outlistfilename);
	    ecode = -1;
	    goto close_in;
	}
	outlistfile = new tokFile(outlistfp, config.outlistfilename);
    }

    int framenum;
    int done, segid, utt_frames;
    int rq_framenum, got, first_frame;
    // Actual counts of frames read in and written out post iterator
    int frame_count_in, frame_count_out;
    // circular buffer head/tail
    int buf_ix_in, buf_ix_out;

    for ( ; !urit.at_end(); urit++) {

	int ut_rg_num = *urit;
	int ut_done = 0;

	while (!ut_done) {
	    // We repeat the process of choosing which file 
	    // contains the current segment so that we can 
	    // skip through several files when we are learning 
	    // the actual number of utterances in each 

	    // Make sure this utterance is within range, to best of knwlg
	    if (startUtt[nipfiles] != INT_MAX \
		&& ut_rg_num >= startUtt[nipfiles]) {

		/* If the user explicitly requested this utterance, then 
		   it's an error for it to be outside the files.  But if 
		   it's just that we guessed the total number of files, 
		   then only report if verbose. */
		if (urit.current_range_finite()) {
		    fprintf(stderr, "%s: ERROR: requested utterance index %d "
			    "exceeds total utterance count of %d\n", 
			    programName, ut_rg_num, startUtt[nipfiles]);
		    ecode = -1;
		}
		goto close_out;
	    }
	    // Figure out which ip file we think this lies in
	    lastIpfileno = ipfileno;
	    ipfileno = 0;
	    while (ipfileno < nipfiles \
		   && startUtt[ipfileno+1] != INT_MAX \
		   && startUtt[ipfileno+1] <= ut_rg_num) {
		++ipfileno;
	    }

	    if (config.debug > 0) {
		fprintf(stderr, "%s: attempting to read logical utterance "
			"%d (of %u) in file '%s'...\n", programName, 
			ut_rg_num, utt_range->length(), ipfiles[ipfileno]);
	    }

	    // If we've switched file, reopen
	    if (ipfileno != lastIpfileno) {

		if (lastIpfileno != -1 && config.query) {
		    // print details of last file
		    fprintf(stdout, "%ld sentences, %d frames, %d " 
			    TYPENAME "s.\n", instream->num_utts, 
			    instream->tot_frames,
			    instream->ipwidth);
		}

		// Open (or retrieve) the next input file
		instream = InputStream::getInputStream(ipfiles[ipfileno], 
				       config.in_type, config.indexedin, 
				       config.data_width, config.verbose);
		// Bail if open failed
		if (instream == NULL) { 
		    ecode = -1;
		    goto close_out;
		};

		if (config.query) {
		    fprintf(stdout, "%s\n", ipfiles[ipfileno]);
		}

		// Maybe set up the index here if we happen to know how
		if (startUtt[ipfileno] != INT_MAX \
		    && instream->num_utts != QN_SIZET_BAD) {
		    startUtt[ipfileno+1] 
			= startUtt[ipfileno] + instream->num_utts;
		}

		// Make sure we know to rebuild the data_range
		if (data_range) { 
		    delete data_range;
		    data_range = NULL;
		}		

		// If we opened a new input file, restart the utterance 
		// processing, in case the index has been expanded 
		// and this utterance is now thought to be in a later file.
		continue;
	    }

	    // If this is the first pass, or if we just opened a new file, 
	    // we'll need to re-compile the data_range_spec
	    if (data_range == NULL) {

		data_range = new Range(config.data_range_def, 
				       0, instream->ipwidth);
		
		int new_opwidth = data_range->length();
		
		// Is the new output width compatible with any previous one?
		if (opwidth > -1 && new_opwidth != opwidth) {
		    fprintf(stderr, "%s: ERROR: cannot switch output width "
			    "from %d to %d for input %s\n", 
			    programName, opwidth, data_range->length(), 
			    ipfiles[ipfileno]);
		    ecode = -1;
		    goto close_out;
		}

		opwidth = new_opwidth;
	    }
		
	    // We now have an open input file, so we can open the output 
	    // file, if we haven't already.
	    if (outstream == NULL) {

		// No output on -q
		if (!config.query) {
		    // Now can open the output
		    const char *fname = config.out_file;
		    if (outlistfile != NULL) {
			// Retrieve next output file name from list
			if ( outlistfile->getNextTok(&fname) == 0) {
			    fprintf(stderr, "%s: ERROR: ran out of output "
				    "filenames from %s after %d utts\n", 
				    programName, config.outlistfilename, 
				    totutts);
			    ecode = -1;
			    goto close_out;
			}
		    }
		    outstream = NewOutStream(fname, config.out_type, 
					     config.indexedout, 
					     phoneset, opwidth, 
					     &outfile);
		    if (outstream == NULL) {
			ecode = -1;
			goto close_in;
		    };
		}
		
		if (op_frame == NULL) {
		    op_frame = new ELTYPE[opwidth];
		}
	    }

	    // Step forward to the utterance specified by the range
	    segid = 0;	/* i.e. != QN_SEGID_BAD */
	    if ( config.indexedin \
		 || ut_rg_num <= startUtt[ipfileno] + instream->uttnum) {
		// Need to seek backwards, or have seek available
		int from = instream->uttnum;
		instream->uttnum = ut_rg_num - startUtt[ipfileno];
		if (instream->set_pos(instream->uttnum, 0) == QN_SEGID_BAD) {
		    fprintf(stderr, "%s: Error: unable to seek from utt %d "
			    "to %d (logical %d) in file '%s'\n", programName, 
			    from, instream->uttnum, 
			    startUtt[ipfileno]+instream->uttnum, 
			    ipfiles[ipfileno]);
		    ecode = -1;
		    goto close_out;
		}
	    } else {
		// Just stepping forwards, seek not available
		while (startUtt[ipfileno] + instream->uttnum < ut_rg_num \
		       && segid != QN_SEGID_BAD) {
		    segid = instream->nextseg();
		    ++instream->uttnum;
		}
	    }
	    if (segid == QN_SEGID_BAD) {
		// hit EOF; check outside uttnum loop so that break will end
		// processing of this input file
		if (config.verbose  && !config.query) {
		    fprintf(stderr, "%s: Warning: unanticipated end of file "
			    "'%s' at utterance %d (logical %d)\n", 
			    programName, ipfiles[ipfileno], instream->uttnum, 
			    startUtt[ipfileno] + instream->uttnum);
		}
		
		// Maybe record this file's EOF if we don't already know it
		if (startUtt[ipfileno+1] == INT_MAX) {
		    instream->num_utts = instream->uttnum;
		    startUtt[ipfileno+1] = 
			startUtt[ipfileno] + instream->num_utts;
		}
		// And maybe record the number of frames in the file
		if (instream->tot_frames <= 0) {
		    instream->tot_frames = totframes - frameslasteof;
		    frameslasteof = totframes;
		}

		// Skip rest of utterance processing - need to go to next file
		continue;
	    }
	
	    // OK, utterance is found, now process it.
	    
	    // See if we can find the number of frames in this utterance
	    utt_frames = instream->num_frames(instream->uttnum);
	    if (utt_frames == QN_SIZET_BAD) {
		// Couldn't read length, so use a very large number
		utt_frames = INT_MAX;
	    }
	    
	    int thisframes;
	    int rpt_count;
	    for (rpt_count = 0; rpt_count < config.repeatutts; ++rpt_count) {
		// Need to seek back to start on subsequent passes
		if (rpt_count > 0) {
		    if (instream->set_pos(instream->uttnum, 0) 
			    == QN_SEGID_BAD) {
			fprintf(stderr, "%s: Error: unable to rewind utt "
				"%d in file '%s'\n", programName, 
				instream->uttnum, 
				ipfiles[ipfileno]);
			ecode = -1;
			goto close_out;
		    }
		}
		
		// Maybe read a desired length for this o/p segment
		if (deslenfile) {
		  // Have a file of desired output utterance framelens
		  if (config.des_type  == FTYPE_LIST) {
		    if (!deslenfile->getNextInt(&deslen)) {
			fprintf(stderr, "%s: Error: ran out of desired "
				"lengths from file '%s' at output utt %d\n", 
				programName, config.deslenfilename, totutts);
			ecode = -1;
			goto close_out;
		    }
		  } else {
		    // cloning existing file
		    deslen = desftrfile->num_frames(totutts);
		    if (deslen == QN_SIZET_BAD) {
			fprintf(stderr, "%s: Error: ran out of desired "
				"segs to copy lens from file '%s' at output utt %d\n", 
				programName, config.deslenfilename, totutts);
			ecode = -1;
			goto close_out;
		    }
		  }
		}
		// .. and maybe read an initial skip for this seg
		if (skipfile) {
		    if (!skipfile->getNextInt(&skipframes)) {
			fprintf(stderr, "%s: Error: ran out of initial skips "
				"from file '%s' at output utt %d\n", 
				programName, config.skipfilename, totutts);
			// Close the file just in case it's salvagable
			ecode = -1;
			goto close_out;
		    }
		}

		// Initialize the frame iterator for this utterance
		if (pfr_range)   delete pfr_range;
		pfr_range = new Range(config.pfr_range_def, 0, 
				      MIN(deslen, utt_frames-skipframes));

		// Read the frames for this utterance
		thisframes = 0;
		framenum = -1;
		got = -1;
		// (index into circular data_buffer)
		buf_ix_in = 0;
		buf_ix_out = 0;
		// count of actual frames indicated by iterator
		frame_count_in = 0;
		frame_count_out = 0;
		// don't emit until this frame
		first_frame = (padframes > 0)? 0 : -padframes;

		for (Range::iterator prit = pfr_range->begin(); 
		     !prit.at_end() \
			 && thisframes < deslen; prit++) {
		    // read next specified frame
		    rq_framenum = *prit + skipframes;

		    if (rq_framenum < framenum \
			|| (config.indexedin && (rq_framenum > framenum+1)) ) {
			// Need to seek back (or fwd) to find this frame
			//fprintf(stderr, "Seeking from %d to %d\n", 
			//	  framenum, rq_framenum);
			if (instream->set_pos(instream->uttnum, rq_framenum) \
			          == QN_SEGID_BAD) {
			    fprintf(stderr, "%s: Error: unable to seek back "
				    "to frame %d in utt %d in file '%s'\n", 
				    programName, rq_framenum, 
				    instream->uttnum, 
				    ipfiles[ipfileno]);
			    ecode = -1;
			    goto close_out;
			}
			framenum = rq_framenum;
			// .. but then back off one to trick next step
			// into reading it.
			--framenum;
		    }
		    while (got && framenum < rq_framenum) {
			got = instream->readfn(1, instream->data_buffer 
					        + instream->ipwidth*buf_ix_in);
			framenum += got;
		    }
		    if (got == 0) {
			// hit end of this segment, so we're done
			if (config.verbose && !config.query) {
			    fprintf(stderr, "%s: Warning: unexpected end of "
				    "utterance %d at %d in %s\n", 
				    programName, instream->uttnum, framenum, 
				    ipfiles[ipfileno]);
			}
			break;

		    } else {
			// Step on the input circular buffer counter
			buf_ix_in  = (buf_ix_in+1) % InputStream::buf_frames;
			// increase tally of frames actually passed by iterator
			++frame_count_in;

			// Have we filled the circular buffer (hence ready to 
			// emit)?
			if (frame_count_in >= InputStream::buf_frames) {

			    // Arrange to skip the first few emissions 
			    // if we're trimming
			    if (frame_count_out >= first_frame) {

				// copy this frame into the output buf
				i = 0;
				for (Range::iterator frit = data_range->begin(); 
				     !frit.at_end(); frit++) {
				    op_frame[i++] 
					= instream->data_buffer[buf_ix_out
							  * instream->ipwidth 
							        + *frit];
				}

#ifndef LABCAT
				// Apply any selected feature transformation 
				// to buf about to be written
				if (config.transform != TRF_NONE) {
				    int i, len = data_range->length();
				    float *pf = op_frame;
				    if (config.transform == TRF_LOG) {
					for (i = 0; i < len; ++i){
					    *pf = log(*pf);
					    ++pf;
					}
				    } else if (config.transform == TRF_EXP) {
				      for (i = 0; i < len; ++i){
					*pf = exp(*pf);
					++pf;
				      }
				    } else if (config.transform==TRF_SOFTMAX) {
				      double x, sum = 0.0, invsum;
				      for (i = 0; i < len; ++i){
					x = exp(*pf);
					sum += x;
					*pf++ = x;
				      }
				    } else if (config.transform == TRF_DCT) {
				      do_dct(pf, pf, len);
				      pf += len;
				    } else if (config.transform == TRF_NORM) {
				      // do it later
				      pf += len;
				    } else if (config.transform == TRF_SAFELOG) {
				      // add on smallest possible float so that we don't 
				      // get log of zero and hence -inf in the output
				      float tiny = 1.175494e-38;
				      for (i = 0; i < len; ++i){
					*pf = log(*pf + tiny);
					++pf;
				      }
				    } else {
				      fprintf(stderr, 
					      "Unknown transform# %d\n", 
					      config.transform);
				      exit(-1);
				    }
				    if (config.transform == TRF_SOFTMAX || \
					config.transform == TRF_NORM) {
					/* Now normalize */
					double x, sum = 0.0, invsum;
					for (i=0; i<len; ++i) {
					    x = *--pf;
					    sum += x;
					}
					invsum = 1.0 / sum;
					for (i=0; i<instream->ipwidth; ++i) {
					    *pf++ *= invsum;
					}
				    }
				}				
#endif /* !LABCAT */

				// Write out this frame
				if(outstream) outstream->WRITEFN(1, op_frame);
				++totframes;
				++thisframes;

				// Repeat the first frame several times if padding
				if (frame_count_out == first_frame 
				    && padframes > 0) {
				    for (i = 0; i < padframes; ++i) {
					if (outstream) 
					    outstream->WRITEFN(1, op_frame);
					++totframes;
					++thisframes;
				    }
				}
			    }
			    // Step on the output circular buffer counter
			    buf_ix_out = (buf_ix_out+1) % InputStream::buf_frames;
			    // tally of frames that have finished the 
			    // circular buffer
			    ++frame_count_out;
			}
		    }
		}

		// We reached the end of the utterance, but pad the 
		// tail if we're padding
		int actpadframes = padframes;
		// Desired length specification takes precedence 
		// over padding if it is used
		if (deslenfile) {
		    if (thisframes < deslen) {
			actpadframes = deslen-thisframes;
		    } else {
			actpadframes = 0;
		    }
		}
		// Write out any padding frames as repeat of last
		if (actpadframes > 0) {
		    for (i = 0; i < actpadframes; ++i) {
			if (outstream)
			    outstream->WRITEFN(1, op_frame);
			++totframes;
			++thisframes;
		    }
		}

		// Flag output that this utterance is finished
		if (outstream) { 
		    if (!config.mergeutts) {
		        outstream->doneseg(segid);
		    }
		    if (outlistfile != NULL) {
			// We're writing each output seg to a separate 
			// file, so I guess we're done with this one
			if (config.mergeutts) {
			    // only terminate segments at very end if merging
			    outstream->doneseg(segid);
			}
			delete outstream;
			fclose(outfile);
			outstream = NULL;
		    }
		}
		++totutts;

		// Read through remaining frames in this utterance to discard
		if (!config.indexedin) {
		    while (got) {
			got = instream->readfn(InputStream::buf_frames, 
					       instream->data_buffer);
			framenum += got;
		    }
		} else {
		    // already know where it ends
		    framenum = utt_frames-1;
		}
		// Report
		if (config.verbose) {
		    if (config.query) {
			fprintf(stdout, "%d %d\n", instream->uttnum, 
				framenum+1);
		    } else {
			fprintf(stderr, "%s: Finished "
			    "utterance %d in %s of %d frames (%d written)\n", 
			    programName, instream->uttnum, 
			    ipfiles[ipfileno], framenum+1, thisframes);
		    }
		}
	    }  /* for (rptcount...) loop */
	    ut_done = 1;
	} /* while (!ut_done) */
    }


close_out:
    // Before exit, print stats of final file (if query)
    if (ipfileno != -1 && config.query) {
	// print details of last file
	fprintf(stdout, "%ld sentences, %d frames, %d " 
		TYPENAME "%s.\n", instream->num_utts, 
		instream->tot_frames,
		instream->ipwidth,
		(instream->ipwidth==1)?"":"s");
    }

    // All done - close streams
    if (outstream) {
	if (config.mergeutts) {
	    // only terminate segments at very end if merging
	    outstream->doneseg(segid);
	}
	delete outstream;
	fclose(outfile);
    }
    // other cleanup
    if (deslenfile) {
	if (config.des_type  == FTYPE_LIST) {
	  deslenfile->close();
	  delete deslenfile;
	} else {
	}
    }
    if (skipfile) {
	skipfile->close();
	delete skipfile;
    }
    if (outlistfile) {
	outlistfile->close();
	delete outlistfile;
    }
    delete [] op_frame;
    delete utt_range;

    if (config.verbose) {
	fprintf(stderr, "%s: %d utts (%d frames) of %d " TYPENAME "%s", 
		programName, totutts, totframes, opwidth, (opwidth==1)?"":"s");
	if (!config.query) {
	    if (outlistfile) {
		fprintf(stderr, " written to files in %s", 
			config.outlistfilename);
	    } else {
		fprintf(stderr, " written to %s", config.out_file);
	    }
	}
	fprintf(stderr, " (ecode %d)\n", ecode);
    }

close_in:
    InputStream::closeAll();

    exit (ecode);
}

