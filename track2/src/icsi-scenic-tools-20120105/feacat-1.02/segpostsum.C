//
// segpostsum.C - summarize segments of posterior probabilities
// 
// post-processing of NN posterior output to try and make a classification 
// of whole segments
//
// 2002-01-26 dpwe@ee.columbia.edu
// $Header: /u/drspeech/repos/feacat/segpostsum.C,v 1.6 2010/10/29 03:41:53 davidj Exp $

#include <stdio.h>
#include <assert.h>
#include <math.h>	/* for rint() */
#include <strings.h>	/* for bzero() */

#include <QN_streams.h>
#include <QN_PFile.h>
#include <QN_HTKstream.h>
#include <QN_SRIfeat.h>
#include <QN_MiscStream.h>
#include <QN_RapAct.h>
#include <QN_camfiles.h>
#include <QN_AsciiStream.h>

#include <QN_filters.h>		// for QN_InFtrStream_JoinFtrs

extern "C" {
#include <dpwelib/cle.h>
}

#define LOG2 0.69314718

static struct {
    char* out_file;
  char *ref_file;		// correct answers for scoring
    int in_type;		// flag for type of input stream
    int data_width;		// elements per frame, for Cam files
    int debug;			// Debug level.
    int verbose;		// for -verbose
    int help;			// for -help
  int prob_dom; 		// domain for averaging probs
  float prob_exp; 		// sum probs raised to this power
  int conf_meas;		// which confidence measure
  float conf_min;		// floor of acceptable confidence values
  float sd_wt;			// score is mean + sd_wt*sd
} config;

static char *programName;

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
// Recent feature additions
    FTYPE_ASCII,
    FTYPE_HTK, 
    FTYPE_RAWBE,
    FTYPE_RAWLE
};

CLE_TOKEN ftTable[] = {
    { "u?nknown", FTYPE_UNK} , 
    { "pf?ile",   FTYPE_PFILE} , 
    { "r?apbin",  FTYPE_RAPBIN} ,
    { "raph?ex",  FTYPE_RAPHEX} ,
    { "rapa?scii",  FTYPE_RAPASCII} ,
    { "h?tk", FTYPE_HTK } ,
    { "raw?be", FTYPE_RAWBE } ,
    { "rawle", FTYPE_RAWLE } ,
    { "o?nlftr",  FTYPE_ONLFTR} , 
    { "pr?e",     FTYPE_PRE} , 
    { "ln?a8",    FTYPE_LNA8} ,
    { "a?scii",	  FTYPE_ASCII} ,
    { NULL, 0}
};

enum {
  PDOM_LIN = 0, 
  PDOM_LOG
};

CLE_TOKEN pdTable[] = {
  { "li?near", PDOM_LIN }, 
  { "lo?garithmic", PDOM_LOG }, 
  { NULL, 0}
};

enum {
  CONF_UNIF = 0,
  CONF_PMAX, 
  CONF_MARG, 
  CONF_INVENT, 
  CONF_NEGENT
};

CLE_TOKEN cmTable[] = {
  { "u?niform", CONF_UNIF }, 
  { "max?prob", CONF_PMAX }, 
  { "mar?gin", CONF_MARG }, 
  { "inv?entropy", CONF_INVENT }, 
  { "neg?entropy", CONF_NEGENT }, 
  { NULL, 0}
};

#define DFLT_FORMAT "lna"

CLE_ENTRY clargs [] = {
  { "Posterior segment classification summarization",
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
    { "segpostsum version " PACKAGE_VERSION, 
      CLE_T_USAGE, NULL, NULL, NULL, NULL }, 
    { "-ip?format", CLE_T_TOKEN, "Input stream format", 
      DFLT_FORMAT, &config.in_type, NULL, ftTable }, 
    { "-o?utfile", CLE_T_STRING, "Output segment scores file", 
      "-", &config.out_file, NULL, NULL },
    { "-r?effile", CLE_T_STRING, "File of correct classes for each seg", 
      "", &config.ref_file, NULL, NULL },
    { "-w?idth|-fw?idth|-ftrw?idth", 
      CLE_T_INT, "Elements per frame (for Cambridge formats)", 
      "0", &config.data_width, NULL, NULL }, 

  { "-pr?obs", CLE_T_TOKEN, "Probability averaging domain", 
    "lin", &config.prob_dom, NULL, pdTable },
  { "-ex?ponent", CLE_T_FLOAT, "Power that probabilities are raised to before summing", 
    "1.0", &config.prob_exp, NULL, NULL },
  { "-c?onf", CLE_T_TOKEN, "Confidence measure", 
    "uniform", &config.conf_meas, NULL, cmTable }, 
  { "-m?inconf", CLE_T_FLOAT, "Lowest usable confidence value", 
    "0", &config.conf_min, NULL, NULL }, 
  { "-s?d_wt", CLE_T_FLOAT, "For seg scoring, score is mean + sd_wt*sd", 
    "0", &config.sd_wt, NULL, NULL }, 

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
	if (stream == NULL) {
	    fprintf(stderr, "fopen_gz: couldn't fopen('%s','%s')\n", 
		    filename, mode);
	}
    }

    return stream;
}

QN_InFtrStream *NewInStream(const char *filename, int type, int indexed, 
			    int *pwidth, FILE **pfile) {
    // Open a QN_InXxxStream in one of the specified types
    QN_InFtrStream *rslt = NULL;
    int debug = config.debug;
    char *dtag = programName;
    FILE *file = NULL;
    int width = *pwidth;
    int nftrs   = width;
    int nlabels = 0;

    file = fopen_gz(filename, "r");

    if (file == NULL) {
	fprintf(stderr, "NewInStream: couldn't read '%s'\n", filename);
	if (pfile)	*pfile = NULL;
	return NULL;
    }

    switch (type) {
    case FTYPE_PFILE:
	rslt = new QN_InFtrLabStream_PFile(debug, dtag, file, indexed);
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
    case FTYPE_RAWLE:
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: must specify width for RAWLE\n");
	} else {
	    rslt = new QN_InFtrStream_raw(debug, dtag, file, QNRAW_FLOAT32, QNRAW_LITTLEENDIAN, width, indexed);
	}
	break;
    case FTYPE_RAWBE:
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: must specify width for RAWBE\n");
	} else {
	    rslt = new QN_InFtrStream_raw(debug, dtag, file, QNRAW_FLOAT32, QNRAW_BIGENDIAN, width, indexed);
	}
	break;
    case FTYPE_ASCII:
	/* only demand width be > 0 for labcat */
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: must specify width for ASCII\n");
	}
	rslt = new QN_InFtrLabStream_Ascii(debug, dtag, file, 
					   nftrs, nlabels, indexed, 
					   NULL);
	break;
    case FTYPE_ONLFTR:
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: must specify width for ONLFTR\n");
	} else {
	    rslt = new QN_InFtrLabStream_OnlFtr(debug, dtag, file, 
						width, indexed);
	}
	break;
    case FTYPE_PRE:
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: must specify width for PRE\n");
	} else {
	    rslt = new QN_InFtrLabStream_PreFile(debug, dtag, file, 
						 width, indexed);
	}
	break;
    case FTYPE_LNA8:
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: must specify width for LNA8\n");
	} else {
	    rslt = new QN_InFtrLabStream_LNA8(debug, dtag, file, 
					      width, indexed);
	}
	break;
    default:
	fprintf(stderr, "NewInStream: unknown type code %d\n", type);
	break;
    }

    // Read the actual width, check it or set it
    if (rslt) {
	width = rslt->num_ftrs();
	*pwidth = width;
	if (width <= 0) {
	    fprintf(stderr, "NewInStream: input '%s' has zero width!\n", 
		    filename);
	    delete rslt;
	    rslt = NULL;
	}
    }

    if (rslt == NULL) {
	// failed to open as a stream
	fprintf(stderr, "NewInStream: abandoning input\n");
	fclose(file);
	file = NULL;
    }
    
    // Return the file handle too
    if (pfile)	*pfile = file;

    return rslt;
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
    int getNextTok(char **ret);
    int getNextInt(int *ret);
    // Shorthand to close our file (even though we didn't open it)
    void close(void) {
	fclose(file);
    }
};
const char *tokFile::WS = " \t\n";

int tokFile::getNextTok(char **ret) {
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

main(int argc, char *argv[])
{
    int err = 0;
    int nipfiles;
    //char **ipfiles;
    const char **ipfiles;
    const char *oneipfile;
    const char *defaultip = "-";
    int ecode = 0;
    int i;

    programName = argv[0];

    // Handle the args
    cleSetDefaults(clargs);

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
	oneipfile = defaultip;
	ipfiles = &oneipfile;
    } else {
	nipfiles = argc-1;
	//ipfiles = argv+1;
	//assert(nipfiles <= MAXIPFILES);
	ipfiles = new const char *[nipfiles];
	for(int i = 0; i<nipfiles; ++i) {
	  ipfiles[i] = argv[i+1];
	}
    }

    QN_InFtrStream *instream = NULL;
    FILE *outfile = NULL;
    FILE *ipfile;
    FILE *reffile = NULL;
    tokFile *reftoks;

    if (strcmp(config.out_file, "-") == 0) {
      outfile = stdout;
    } else {
      outfile = fopen_gz(config.out_file, "w");
    }
    if (outfile == NULL) {
      fprintf(stderr, "%s: couldn't open '%s' for writing\n", 
	      programName, config.out_file);
      exit(1);
    }

    if (config.ref_file && strlen(config.ref_file) > 0) {
      reffile = fopen_gz(config.ref_file, "r");
      reftoks = new tokFile(reffile, "reference classes");
    }

    float *frame_buf = NULL;
    float *sum_x = NULL;
    float *sum_xx = NULL;
    float N, fsum, prob, ent, pmax, pmax2, conf;

    int filenum;
    int indexed = 0;
    int uttnum = 0;
    int totutts = 0;
    int totcorrutts = 0;
    int segid;

    int nzframes, totframes, tottotframes = 0;

    int width = config.data_width;
    float exp = config.prob_exp;
    float sd_wt = config.sd_wt;

    int correct, ncorrect, totncorrect = 0;

    for (filenum = 0; filenum < nipfiles; ++filenum) {
      
      const char *ipfilename = ipfiles[filenum];

      instream = NewInStream(ipfilename, config.in_type, indexed, 
			     &width, &ipfile);
      if(instream == NULL) {
	fprintf(stderr, "unable to create input stream\n");
	exit(1);
      }

      if (frame_buf == NULL) {
	frame_buf = new float[width];
	sum_x = new float[width];
	sum_xx = new float[width];
      }

      uttnum = 0;

      // Step forward to next utterance
      while ( (segid = instream->nextseg()) != QN_SEGID_BAD) {

	// Retrieve the correct class for this utt, if we have a ref file
	if (reffile) {
	  if (reftoks->getNextInt(&correct) == 0) {
	    fprintf(stderr, "Warn: ran out of ref tokens at file %d utt %d\n", 
		    filenum, uttnum);
	    delete reftoks;
	    fclose(reffile);
	    reffile = 0;
	  }
	}

	    // See if we can find the number of frames in this utterance
	    int utt_frames = instream->num_frames(uttnum);
	    if (utt_frames == QN_SIZET_BAD) {
		// Couldn't read length, so use a very large number
		utt_frames = INT_MAX;
	    }
	    
	    bzero(frame_buf, width*sizeof(float));
	    bzero(sum_x, width*sizeof(float));
	    bzero(sum_xx, width*sizeof(float));
	    N = 0;
	    totframes = 0;
	    nzframes = 0;
	    ncorrect = 0;

	    // Read the frames for this utterance
	    int framenum = -1;
	    int got = -1;
	    int i;
	    int maxclass;

	    while (got && framenum < utt_frames) {
	      got = instream->read_ftrs(1, frame_buf);

	      if (got) {
		framenum += got;
		++totframes;

		// Calculate the entropy, find 2 top probs
		pmax = frame_buf[0]; pmax2 = frame_buf[1]; maxclass = 0;
		if (pmax2 > pmax) {
		  pmax = frame_buf[1]; pmax2 = frame_buf[0]; maxclass = 1;
		}
		fsum = 0;
		for (i = 0; i < width; ++i ) {
		  prob = frame_buf[i];
		  fsum += prob;
		  if (i > 1) {
		    if (prob >= pmax) {
		      pmax2 = pmax;
		      pmax = prob;
		      maxclass = i;
		    } else if (prob > pmax2) {
		      pmax2 = prob;
		    }
		  }
		}
		if (fsum < 0.95 || fsum > 1.05) {
		  fprintf(stderr, "Warn: file=%d utt=%d frame=%d fsum=%f\n", filenum, uttnum, framenum, fsum);
		}
		ent = 0;
		for (i = 0; i < width; ++i) {
		  prob = frame_buf[i]/fsum;
		  ent += prob*-log(prob)/LOG2;
		}		  

		// figure the confidence weighting
		switch (config.conf_meas) {
		case CONF_UNIF:
		  conf = 1.0;
		  break;
		case CONF_PMAX:
		  conf = pmax;
		  break;
		case CONF_MARG:
		  conf = pmax - pmax2;
		  break;
		case CONF_INVENT:
		  conf = 1/ent;
		  break;
		case CONF_NEGENT:
		  conf = -ent;
		  break;
		default:
		  fprintf(stderr, "Error: unknown CM code %d\n", config.conf_meas);
		  exit(1);
		}
		conf = conf - config.conf_min;
		if (conf < 0) {
		  conf = 0;
		} else {
		  // keep track of how many frames we actually use
		  ++nzframes;
		}

		// Accumulate the statistics
		for (i = 0; i < width; ++i) {
		  prob = frame_buf[i];
		  if (config.prob_dom == PDOM_LOG) {
		    prob = log(prob);
		  }
		  if (exp != 1.0) {
		    prob = pow(prob, exp);
		  }
		  sum_x[i] += conf*prob;
		  sum_xx[i] += conf*prob*prob;
		}
		N += conf;

		// figure the frame accuracy if ref file available
		if (reffile) {
		  if (maxclass == correct) {
		    ++ncorrect;
		  }
		}
	      }
	    }
	    // did we end cleanly?
	    if (got == 0) {
	      if (config.verbose) {
		fprintf(stderr, "%s: Warning: unexpected end of "
			"utterance %d at %d in %s\n", 
			programName, uttnum, framenum, 
			ipfiles[filenum]);
	      }
	    }
	    // Write to ouptut
	    if (N == 0) {
	      // no confidence, all sums will be zero
	      N = 1.0;
	    }
	    fprintf(outfile, "Utt: %d", uttnum);
	    // calc mean and SD, find max
	    float maxscore = 0, score;
	    maxclass = 0;
	    for (i = 0; i < width; ++i) {
	      // put sd in sum_xx array
	      sum_xx[i] = pow(sqrt(fabs(sum_xx[i]/N-sum_x[i]/N*sum_x[i]/N)), 1.0/exp);
	      // put mean in sum_x array
	      sum_x[i] = pow(sum_x[i]/N, 1.0/exp);
	      score = sum_x[i] + sd_wt*sum_xx[i];
	      if (i == 0 || score > maxscore) {
		maxclass = i;
		maxscore = score;
	      }
	    }
	    if (reffile) {
	      fprintf(outfile, " FA: %.1f%%", 100.0*ncorrect/totframes);
	      fprintf(outfile, " rec %d ref %d %s", maxclass, correct, (maxclass==correct)?"right":"**wrong**");
	      if (maxclass==correct) {
		++totcorrutts;
	      }
	    }
	    fprintf(outfile, "\n");
	    fprintf(outfile, "Mean: ");
	    for (i = 0; i < width; ++i) {
	      fprintf(outfile, "%f ", pow(sum_x[i]/N, 1.0/exp));
	    }
	    fprintf(outfile, "\n  SD: ");
	    for (i = 0; i < width; ++i) {
	      fprintf(outfile, "%f ", pow(sqrt(fabs(sum_xx[i]/N-sum_x[i]/N*sum_x[i]/N)), 1.0/exp));
	    }
	    fprintf(outfile, "\n");

	    if (config.verbose) {
	      fprintf(stderr, "file %d utt %d used frames %d/%d total conf %f\n", filenum, uttnum, nzframes, totframes, N);
	    }
	    ++uttnum;
	    ++totutts;
	    tottotframes += totframes;
	    totncorrect += ncorrect;
      }
      delete instream;
      fclose(ipfile);
    }
    fclose(outfile);
    if (reffile) {
      //      if (config.verbose) {
	fprintf(stderr, "%d utts FA=%.1f%% SegAcc=%.1f%%\n", 
		totutts, 100.0*totncorrect/tottotframes, 
		100.0*totcorrutts/totutts);
	// }
      delete reftoks;
      fclose(reffile);
    }
}

