/*
 * sndcat.c
 *	
 * Copy between soundfiles using the snd.h library.
 *
 * 1998jun02 dpwe@icsi.berkeley.edu
 * $Header: /u/drspeech/repos/dpwelib/sndcat.c,v 1.18 2011/03/09 02:06:06 davidj Exp $
 */

#include <dpwelib.h>	/* std includes, defs like stdio.h */
#include <stdlib.h>
#include <snd.h>	/* soundfile handling functions */
#include <cle.h>


char *outputfilename = NULL;

int verb = FALSE;	/* verbosity flag */
int query = FALSE;	/* just querying flag */

/*int sfifmt = 0;		/* input soundfile format requested */
/*int sfofmt = 0;		/* output soundfile format requested */
char *sfifmtname = NULL;
char *sfofmtname = NULL;
float skiptime = 0.0;	/* seconds to skip */
float durtime = 0.0;	/* duration in seconds */
float endtime = 0.0;	/* end time in seconds */
int skipframes = 0;	/* frames to skip */
int durframes = 0;	/* total frames */
int endframe = 0;	/* absolute end frame */
int chans = 0;		/* output chans */
int chanmode = SCMD_MONO;	/* should channel reduction mix or extract? */
float srate = 0.0;	/* output srate */
int sampfmt = 0;	/* output sample format */
float gain = 1.0;	/* gain */
int help = 0;

int domulti = 0;	/* attempt to read multiple sounds in stream */
int nomerge = 0;	/* don't merge multiple soundfiles into one */

CLE_TOKEN sampfmtTable[] = {
    { "c?har", SFMT_CHAR }, 
    { "o?ffsetchar", SFMT_CHAR_OFFS }, 
    { "a?law", SFMT_ALAW }, 
    { "u?law|mu?law", SFMT_ULAW }, 
    { "s?hort|i16", SFMT_SHORT }, 
    { "m?ed|i24", SFMT_24BL }, 
    { "l?ong|i32", SFMT_LONG }, 
    { "f?loat|f32", SFMT_FLOAT }, 
    { "d?ouble|f64", SFMT_DOUBLE },
    { NULL, 0}
};

CLE_TOKEN chanmodeTable[] = {
    { "m?ix|mo?no", SCMD_MONO }, 
    { "a|A|0|l?eft|L?EFT", SCMD_CHAN0 }, 
    { "b|B|1|r?ight|R?IGHT", SCMD_CHAN1 }, 
    { "c|C|2", SCMD_CHAN2 }, 
    { "d|D|3", SCMD_CHAN3 }, 
    { "e|E|4", SCMD_CHAN4 }, 
    { "f|F|5", SCMD_CHAN5 }, 
    { "g|G|6", SCMD_CHAN6 }, 
    { "h|H|7", SCMD_CHAN7 }, 
    { NULL, 0}
};

CLE_ENTRY clargs[] = {
    { "-o?utputfile", CLE_T_STRING, "Output file (NOTE NEW SYNTAX!)", 
      "-", &outputfilename, NULL, NULL },

    { "-S|-ip?format", CLE_T_STRING, "input sound file format (sndf token)", 
      NULL, &sfifmtname, NULL, NULL}, 
    { "-T|-op?format", CLE_T_STRING, "output sound file format (sndf token)", 
      NULL, &sfofmtname, NULL, NULL}, 

    { "-k|-sk?ip", CLE_T_TIME, "skip this many seconds of initial sound", 
      "0.0", &skiptime, NULL, NULL },
    { "-d?uration", CLE_T_TIME, "force output duration to this many secs", 
      "0.0", &durtime, NULL, NULL }, 
    { "-e?ndtime", CLE_T_TIME, "stop copying at this point in file (secs)", 
      "0.0", &endtime, NULL, NULL }, 
    { "-K|-skipf?rames", CLE_T_INT, "skip this many sample frames initially", 
      "0", &skipframes, NULL, NULL },  
    { "-D|-durf?rames", CLE_T_INT, "force duration to this many frames", 
      "0", &durframes, NULL, NULL }, 
    { "-E|-endf?rame", CLE_T_INT, "absolute endpoint in samples", 
      "0", &endframe, NULL, NULL }, 
    { "-c?hans|-chann?els", CLE_T_INT, "force output to this many channels", 
      "0", &chans, NULL, NULL }, 
    { "-chanm?ode|-cm?ode", CLE_T_TOKEN, "if converting to/from mono, set mode", 
      "mono", &chanmode, NULL, chanmodeTable }, 
    { "-f?ormat|-fmt", CLE_T_TOKEN, "output sample format", 
      NULL, &sampfmt, NULL, sampfmtTable }, 
    { "-r?ate|-sr?ate|-s?amplerate", CLE_T_FLOAT, "force output sample rate", 
      NULL, &srate, NULL, NULL }, 
    { "-g?ain", CLE_T_FLOAT, "apply gain factor to data", 
      "1.0", &gain, NULL, NULL },
    { "-n?omerge", CLE_T_BOOL, "don't merge all sounds into a single unsegmented output sound, but write segment separators inbetween", 
      "FALSE", &nomerge, NULL, NULL }, 
    { "-q?uery", CLE_T_BOOL, "just query soundfile(s)", 
      "FALSE", &query, NULL, NULL }, 
    { "-v?erbose", CLE_T_BOOL, "verbose information reporting", 
      "FALSE", &verb, NULL, NULL }, 
    { "-mu?lti", CLE_T_BOOL, "attempt to read multiple soundfiles from stream", 
      "FALSE", &domulti, NULL, NULL }, 
    { "-h?elp", CLE_T_BOOL, "print help message & exit", "FALSE", &help, NULL, NULL },


    { "[inputfiles ...]", CLE_T_USAGE, NULL, 
     "-", NULL, NULL}, 
    { NULL, CLE_T_END, NULL, NULL, NULL, NULL, NULL }
};

int main(int argc, char *argv[])
{
    int 	i = 0;
    int		err;
    int 	j;
    char *programName = argv[0];
    SOUND *isnd = NULL, *osnd = NULL;
    SOUND *isndtplt;	/* to hold default sfifmt */
    int ipchans, ipformat, ipframes, ipsffmt;
    float ipsrate, lastipsrate = 0;
    char **pipfname;
    char *ipfname = NULL;
    float *buf = NULL;
    int bufsize = 32768;
    int bufbytes;
    int remains, doneframes, totdoneframes, got, toget;
    int firsttime;
    int filestodo, filesdone;
    int lenset;
    int domerge = 1;

    /*  Handle the args */
    cleSetDefaults(clargs);
    err = cleExtractArgs(clargs, &argc, argv, 0);
    if(err) {
	cleUsage(clargs, programName);
	exit(-1);
    }
    if(help) {
	cleUsage(clargs, programName);
	exit(0);
    }

    /* post-process */
    if (argc > 1) {
        pipfname = &(argv[1]);
	filestodo = argc - 1;
    } else {
	/* No input files specified - use stdin */
	ipfname = "-";
	pipfname = &ipfname;
	filestodo = 1;
    }
    filesdone = 0;
    totdoneframes = 0;

    if (nomerge) {
	domerge = 0;
    }

    /* Input soundfile fmt name; accept "unk?nown" meaning to guess */
    if (sfifmtname && \
	strncmp(sfifmtname, "unknown", strlen(sfifmtname)) != 0) {
        if (!SFSetDefaultFormatByName(sfifmtname)) {
	    fprintf(stderr, "sndffmt: '%s' is not recognized.  Try:\n", 
		    sfifmtname);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");
	    cleUsage(clargs, programName);
	    exit(-1);
	}
    } else {
        /* no input type specified, so set to guess */
	SFSetDefaultFormat(SFF_UNKNOWN);
    }	

    /* preset the input format as requested (alternative to SF* stuff) */
    isndtplt = sndNew(NULL);
    if (sfifmtname) sndSetSFformatByName(isndtplt, sfifmtname);

    /* cycle through all the input file names */
    while ( filesdone < filestodo ) {

	ipfname = *pipfname++;

	isnd = sndNew(isndtplt);
	if( (isnd = sndOpen(isnd, ipfname, "r")) == NULL)  {
	    /* could not open as sound file */
	    fprintf(stderr, "%s: couldn't open '%s' for input\n", 
		    programName, ipfname);
	    exit(-1);

	} else {

	    /* now we have ipsrate, can maybe convert times to frames */
	    ipsrate = sndGetSrate(isnd);
	    if (lastipsrate == 0) {
		lastipsrate = ipsrate;
	    } else if (ipsrate != lastipsrate) {
		/* different sample rate of sucessive sounds. 
		   Not a problem unless we're sticking them together */
		if (domerge) {
		    fprintf(stderr, "%s: warn: sample rate of input changed from %.0f to %.0f\n", programName, lastipsrate, ipsrate);
		}
	    }
	    if (skipframes == 0 && skiptime > 0.0) {
		skipframes = (int)(rint(ipsrate * skiptime));
	    }
	    /* durframes and durtime take precedence over endtime */
	    if (durtime == 0.0 && durframes == 0 \
		&& endtime > 0.0 && endtime > skiptime) {
		durtime = endtime - skiptime;
	    }
	    if (durframes == 0 && durtime > 0.0) {
		durframes = (int)(rint(ipsrate * durtime));
	    }
	    /* endframe is respected only if endtime and dur* are not set */
	    if (durframes == 0 && endframe > 0 && endframe > skipframes) {
		durframes = endframe - skipframes;
	    }
	    ipchans   = sndGetChans(isnd);
	    if (chans > 0)  {
		sndSetUchans(isnd, chans);   /* force e.g. to mono */
		ipchans = chans;
	    }
	    /* Set channel mode too (or only for mapping to 1 ?) */
	    sndSetChanmode(isnd, chanmode);

	    bufbytes = sizeof(float)*ipchans*bufsize;
	    if( (buf = malloc(bufbytes)) == NULL) {
		fprintf(stderr, "error allocating buffer\n");
		exit (-1);
	    }
	    memset(buf, 0, bufbytes);

	    if (!query) {
		if (osnd == NULL) {
		    /* create output sound */
		    osnd = sndNew(isnd);
		    if (srate > 0) {	sndSetSrate(osnd, srate);	}
		    if (chans > 0) {	sndSetChans(osnd, chans);	}
		    if (sampfmt > 0) {	sndSetFormat(osnd, sampfmt);	}
		}
		if (osnd->isOpen && !domerge) {
		    /* checkpointing; do this *before* setting the length */
		    sndNext(osnd);	
		}
		lenset = 0;
		if (durframes > 0) {    
		    sndSetFrames(osnd, durframes);  
		    lenset = 1;
		} else {
		    int iframes = sndGetFrames(isnd);
		    if (iframes != SF_UNK_LEN && (filestodo == 1 || !domerge)) {
			sndSetFrames(osnd, iframes - skipframes);
			lenset = 1;
		    }
		}
		if (!osnd->isOpen) {
		    ipsffmt = SFGetFormat(isnd->file, NULL);
		    if(sfofmtname) {
  		        if (!sndSetSFformatByName(osnd, sfofmtname)) {
			    fprintf(stderr, "sndfofmt: '%s' is not recognized.  Try:\n", 
				    sfofmtname);
			    SFListFormats(stderr);
			    fprintf(stderr, "\n");
			    cleUsage(clargs, programName);
			    exit(-1);
			}
		    } else {
			sndSetSFformat(osnd, ipsffmt);
		    }
		    if (sndOpen(osnd, outputfilename, "w") == NULL) {
			fprintf(stderr, "%s: couldn't write to '%s'\n", 
				programName, outputfilename);
			exit(-1);
		    }
		}
	    }

	    /* sndNext is a flag that checks to see if there are 
	       more soundfiles on this single pipe */
	    firsttime = 1;
	    while (err == 0 && (firsttime || (domulti && sndNext(isnd)))) {
		firsttime = 0;

		if(verb) {
		    sndPrint(isnd, stderr, "");
		}

		ipchans   = sndGetChans(isnd);
		ipformat  = sndGetFormat(isnd);
		ipframes  = sndGetFrames(isnd);
		ipsrate   = sndGetSrate(isnd);

		if (!query || domulti) {

		    if(skipframes) {
			sndFrameSeek(isnd, skipframes, SEEK_SET);
		    }
		    if(durframes > 0) {
			if (ipframes > durframes) {
			    ipframes = durframes;
			} else if (ipframes < 0) {
			    ipframes = durframes;
			}
		    }
		    if (srate > 0)	
			ipsrate = srate;	/* override file's sr */
		    if (chans > 0)  {
			sndSetUchans(isnd, chans);   /* force e.g. to mono */
			ipchans = chans;
		    }

		    doneframes = 0;
		    /* actually copy the samples from file to output */
		    if(ipframes > 0)
			remains = ipframes;
		    else
			remains = -1;
		    do {
			if(remains > 0)
			    toget = MIN(bufsize, remains);
			else
			    toget = bufsize;
			got = sndReadFrames(isnd, buf, toget);

			if (gain != 1.0) {
			    for (j = 0; j < got * ipchans; ++j) {
				buf[j] *= gain;
			    }
			}

			if(osnd) sndWriteFrames(osnd, buf, got);

			doneframes += got;
			if(remains > 0)
			    remains -= got;
		    } while(remains != 0 && got > 0);

		    /* pad out end with zeros if req'd duration longer */
		    if (durframes > 0 && doneframes < durframes) {
			for (j = 0; j < bufsize; ++j) {
			    buf[j] = 0;
			}
			while (doneframes < durframes) {
			    remains = durframes - doneframes;
			    got = MIN(bufsize, remains);
			    if(osnd) sndWriteFrames(osnd, buf, got);
			    doneframes += got;
			}
		    }

		    if(verb) {
			fprintf(stderr, "(%d frames)\n", doneframes);
		    }
		    totdoneframes += doneframes;
		}
	    }
	}
	if (osnd && !lenset && !domerge) {
	    /* attempt to write length retrospectively */
	    sndSetFrames(osnd, doneframes);
	    lenset = 1;
	}
	++filesdone;
    }
    if (osnd && !lenset) {
	/* write length only at the very end */
	sndSetFrames(osnd, totdoneframes);
    }
    if(buf)	free(buf);
    if(osnd)	sndClose(osnd);
    if(isnd)	sndClose(isnd);

    exit(0);
}
