/***************************************************************\
*	sndrsmp.c (was res.c) (after resmp.c)			*
*	Change the sampling rate of a file with sinc interp	*
*	This version works to/from disk & uses integer approx   *
*	It also uses the modularised resampler and resmbuffer   *
*	23feb91 dpwe						*
*   1993dec10 dpwe Modified to allow stdin/stdout               *
* 1997feb18 dpwe@icsi.berkeley.edu  				*
*	Folded in to SPRACHworks release of dpwelib		*
* $Header: /u/drspeech/repos/dpwelib/sndrsmp.c,v 1.17 2011/03/09 02:06:07 davidj Exp $
\***************************************************************/

#include <dpwelib.h>
#include <math.h>

#include <snd.h>
#include <resambuf.h>

#include <sndfutil.h>	/* for ConvertSampleBlockFormat */
#include <genutils.h>	/* for SubsExtn */

#include <assert.h>

/* constants  (bufsizes in samples, not bytes) */
#define INBUFSIZE    32768
#define OUTBLOCKSIZE  8192
#define RS_SHORTMAX  32767	/* scaling float <> short */
#define DFLT_LOBES	  16		/* size of sinc kernel */
#define DFLT_ACC	(0.001)	/* match sr to 0.1 % by default */
#define DFLT_PROP	(0.9)	/* proportion of nyquist */

/* static function prototypes */
static void ResampleFileToFile(char *inPath, char *outPath, 
			       FLOATARG exfact, FLOATARG outrate, 
			       FLOATARG acc, FLOATARG gain, FLOATARG prop, 
			       int fixHdr);
static long SendFunction(int chanArg, SAMPL *buf, long size, int  flag);
static void DoSend(void);
static long ResampleOpenFiles(SOUND *inSnd, SOUND *outSnd, 
			      FLOATARG outRate, FLOATARG acc, FLOATARG prop);
static void Usage(void);


/* static global variables */
       char *programName;
static int  err = 0;

static int lobes = DFLT_LOBES;
static char defExt[32];

static int useStdIn  = FALSE;
static int useStdOut = FALSE;

static int verbose = FALSE;

/* hold names of requested soundfile formats */
static char  *inSFfmt = NULL;
static char  *outSFfmt = NULL;

#if defined(__MWERKS__)
#include <SIOUX.h>
extern short InstallConsole(short fd);
#include <console.h>
#endif /* MACINTOSH */

int
main(argc,argv)
    int 	argc;
    char	**argv;
    {
    char	*inPath, *outPath;
    float	exfact, outrate, acc = DFLT_ACC;
    float	gain = 1.0, prop = DFLT_PROP;
    /* acc is resampling ratio match accuracy (default 0.1%) */
    int 	fixHdr = 0;	/* whether to change sr in op hdr */
    int 	i, err=0;

#if defined(__MWERKS__)
    /* Set options for CodeWarrior SIOUX package */
/*    SIOUXSettings.autocloseonquit = false;
    SIOUXSettings.showstatusline = true;
    SIOUXSettings.asktosaveonclose = false;
    InstallConsole(0);
    SIOUXSetTitle("\psndrsmp"); */

    argc = ccommand(&argv);
#endif /* macintosh */

    programName = argv[0];
    inPath = NULL;
    outPath = NULL;
    outrate = exfact = 0.0;

    i = 0;
    while(++i<argc && err == 0)
	{
	char c, *token, *arg, *nextArg;
	int  argUsed;

	token = argv[i];
	if(*token++ == '-')
	    {
	    if(i+1 < argc)	nextArg = argv[i+1];
		else			nextArg = "";
	    argUsed = 0;
		if(*token == '\0') {
		    /* '-' alone means stdin/stdout */
		    if(inPath == NULL) {
			/* no inpath yet specified, so use stdin */
			useStdIn = 1;
			/* set a dummy name, so we know input is handled */
			inPath = "-";
		    } else {
			/* assume it refers to output */
			useStdOut = 1;
		    }
		}
	    else while(c = *token++) {
		if(*token)	arg = token;
		else		arg = nextArg;
		switch(c)
		    {
		case 'f':
		    exfact = atof(arg);	argUsed = 1;
		    if( exfact == 0 )
			fprintf(stderr,"%s: bad factor %s\n",
				programName, arg);
		    /* actual error will be picked up later */
		    break;
		case 'g':
		    gain = atof(arg);	argUsed = 1;
		    if( gain <= 0 || gain > 1000000)
			fprintf(stderr,"%s: bad gain %s\n",
				programName, arg);
		    /* actual error will be picked up later */
		    break;
		case 's':
		case 'r':
		    outrate = atof(arg); argUsed = 1;
		    if(outrate == 0)
			fprintf(stderr,"%s: bad rate %s\n",
				programName, arg);
		    /* fact of error maybe trapped later */
		    if(outrate < 100) {
			fprintf(stderr,"%s: warn: sr of %f taken as kHz\n",
				programName, outrate);
			outrate = outrate * 1000;
		    }
		    /* fact of error maybe trapped later */
		    break;
		case 'a':
		    acc = 0.01*atof(arg); argUsed = 1; /* convert percent */
		    if(acc <= 0.0)
			fprintf(stderr,"%s: bad accuracy %% %s\n",
				programName, arg);
		    /* fact of error maybe trapped later */
		    break;
		case 'p':
		    prop = atof(arg);	argUsed = 1;
		    if( prop > 2.0 || prop < 0.1 ) {
			fprintf(stderr,"%s: bad nyquist proportion %s\n",
				programName, arg);
			err = 1;
		    }
		    break;
		case 'm':
		    lobes = atoi(arg)/2; argUsed = 1; /* convert percent */
		    if(lobes < 1)  {
			fprintf(stderr,"%s: -m %s (mults) must be even, >=2\n",
				programName, arg);
			err = 1;
		    }
		    break;
		case 'o':	useStdOut = 1; break;
		case 'v':	verbose = 1; break;
		case 'h':   fixHdr = 1;  break;
		case 'S':	inSFfmt = arg; argUsed = 1; break;
		case 'T':	outSFfmt = arg; argUsed = 1; break;
		default:	fprintf(stderr,"%s: unrec option %c\n",
					programName, c);
		                err = 1; break;
		    }
		if(argUsed)
		    {
		    if(arg == token)  token = ""; /* force end of this token */
		    else	      ++i;	  /* arg==next argv, skip it */
		    }
		}
	    }
	else
	    {
	    if(inPath == NULL)
		inPath = argv[i];
	    else if(outPath == NULL)
		outPath = argv[i];
	    else
		{
		fprintf(stderr,"%s: excess argument %s\n", 
			programName, argv[i]);
		err = 1;
		}
	    }
	}
    if( (exfact == 0 && outrate == 0)||(exfact != 0 && outrate != 0))
	{
	fprintf(stderr,"%s: you must specify exactly one of -f or -r\n",
		programName);
	err = 1;
	}
    else if(outrate == 0)
	{
	if(exfact <.001 || exfact > 1000)
	    {
	    fprintf(stderr,
		    "%s: Factor %f out of range \n", programName, exfact);
	    err = 1;
	    }
	}
    else if(outrate < 1 || outrate > 10000000)
	{
	fprintf(stderr,"%s: Rate %f out of range\n", programName, outrate);
	err = 1;
	}
    if(gain <= 0 || gain > 1000000)
	{
	fprintf(stderr,"%s: Gain %f out of range\n", programName, gain);
	err = 1;
	}



    if( err || inPath == NULL ) /* insufficient arguments */
	Usage();

    /* Now have a reasonably well checked scenario */
    ResampleFileToFile(inPath, outPath, exfact, outrate, acc, gain, prop, fixHdr);

    exit(0);
    }

static void ResampleFileToFile(inPath, outPath, exfact, outrate,
			       acc, gain, prop, fixHdr)
    char *inPath;
    char *outPath;
    FLOATARG exfact;
    FLOATARG outrate;
    FLOATARG acc;	/* match rates this well */
    FLOATARG gain;
    FLOATARG prop;
    int  fixHdr;        /* if nonzero, do NOT modify output SR in hdr */
    {
    SOUND	*inSnd, *outSnd;
    long	nsegs = 0, totsent = 0, sent, inframes, outframes = SF_UNK_LEN;
    int	 	dsize, chans, format;
    int	        i, err = 0;

    inSnd = sndNew(NULL);

    if(inSFfmt != NULL) {   /* user requested particular infile trtmt */
	if(!sndSetSFformatByName(inSnd, inSFfmt)) {
	    fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
		    programName, inSFfmt);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");
	    exit(1);
	}
    }
    if ( (inSnd = sndOpen(inSnd, inPath, "r")) == NULL) {
	fprintf(stderr, "%s: couldn't read '%s'\n", programName, inPath);
	exit(-1);
    }

    if( verbose && inSnd != NULL) {
	sndPrint(inSnd, stderr, "input sound");
    }
    /* setup format */
    sndSetUformat(inSnd, SFMT_SHORT);
    if (gain != 1.0) {
	sndSetUgain(inSnd, gain);
    }
    if(outrate == 0)
	outrate = sndGetSrate(inSnd) * exfact;
    else
	exfact  = outrate/sndGetSrate(inSnd);

    chans  = sndGetChans(inSnd);
    dsize  = SFSampleSizeOf(format);
    outSnd = sndNew(inSnd);
    sndSetSrate(outSnd, fixHdr?sndGetSrate(inSnd):outrate);
    if ( (inframes = sndGetFrames(inSnd)) != SF_UNK_LEN) {
	/* figure out the actual size of the file to be written */
	int ip, op;
	float foutframes;

	FindGoodRatio(outrate/sndGetSrate(inSnd), &ip, &op, acc);
	foutframes = (((float)inframes * ip /*+ op - 1*/) / op);
	outframes = (long)(foutframes+0.01);
	sndSetFrames(outSnd, outframes);
	if(verbose)
	    fprintf(stderr, "Predicting %f (%ld) frames out..\n", foutframes, outframes);
    }

    if(outSFfmt != NULL) {   /* user requested particular outfile trtmt */
	if(!sndSetSFformatByName(outSnd, outSFfmt)) {
	    fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
		    programName, outSFfmt);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");
	    exit(1);
	}
    } else if (inSFfmt != NULL) { /* use input sound spec */
	assert(sndSetSFformatByName(outSnd, inSFfmt));
    } else {  /* no formats specified - copy imputation from input */
	sndSetSFformat(outSnd, sndGetSFformat(inSnd));
    }
    
    if (outPath == NULL && !useStdOut)  {
	if(useStdIn)	{
	    useStdOut = TRUE;	/* default op for stdin */
	} else {
	    /* set up default file extension */
	    strcpy(defExt, ".rs");
	    strcat(defExt, sndfExtn);

	    outPath = (char *)malloc(strlen(inPath)+strlen(defExt)+1);
	    SubstExtn(inPath, defExt, outPath);
	}
    }
    if(outPath == NULL)  {
	assert(useStdOut);
	outPath = "-";
    }
    if (sndOpen(outSnd, outPath, "w") == NULL) {
	fprintf(stderr, "%s: unable to write '%s'\n", programName, outPath);
	exit(-1);
    }

    while (err == 0 && sndNext(inSnd)) {
	sent = ResampleOpenFiles(inSnd, outSnd, outrate, acc, prop);

	if(outframes != SF_UNK_LEN \
	   && sent != outframes) {	/* pad file with zeros if too short */
	    short ss[SNDF_MAXCHANS];
	    int i;
	    int extra = outframes - sent;
	    for(i=0;i<chans; ++i)  ss[i] = 0;
	    if(verbose)
		fprintf(stderr, "** predicted %ld output frames but wrote %ld\n", 
			outframes, sent);
	    if (extra > 0) {
		for(i = 0; i<extra; ++i) {
		    sndWriteFrames(outSnd, ss, 1);
		    ++sent;
		}
	    } else {
		sndSetFrames(outSnd,sent);
	    }
	}
	if ( verbose ) {
	    sndPrint(outSnd, stderr, "output sound");	
	}
	totsent += sent;
	++nsegs;
	/* err = (sndNext(outSnd) != 0); */
	sndNext(outSnd);
    }
    sndClose(outSnd);
    sndClose(inSnd);
    if ( verbose ) {
	fprintf(stderr, "Wrote %ld output frames (%ld segment%s) to %s\n", totsent, nsegs, nsegs==1?"":"s", outPath); 
    }
}

static SOUND *outFile = NULL;
/* static variables for output buffer management */
static int    chans;
static short  *outBuffer;
static long   obfSize;
static long   outBlockSize;
static long   chanBufd[SNDF_MAXCHANS];
static long   framesent[SNDF_MAXCHANS];

static long SendFunction(chanArg, buf, size, flag)
    int chanArg;
    SAMPL *buf;
    long size;
    int  flag;
    {
    int  chan = (int)chanArg;

/*    Sampl2Short(buf, 1, outBuffer+(chans*chanBufd[chan])+chan,
	       chans, size);
 */
    int i;
    for(i = 0; i < size; ++i) {
	*(outBuffer+chan+chans*(chanBufd[chan]+i)) 
	    = (short)(((float)RS_SHORTMAX)* SCALEDOWN(*buf++));
    }

    chanBufd[chan] += size;
    return size;	/* return number of points accepted (tb dropped) */
    }

static void DoSend()
{
    int c;

    if(outFile != NULL)
	sndWriteFrames(outFile, outBuffer, chanBufd[0]);
    for(c = 0; c<chans; ++c) {
	framesent[c] += chanBufd[c]; chanBufd[c] = 0;
    }
}


static long ResampleOpenFiles(inSnd, outSnd, outRate, acc, prop)
    SOUND *inSnd;
    SOUND *outSnd;
    FLOATARG	outRate;	/* actual output srate */
    FLOATARG	acc;		/* how close to match frqs */
    FLOATARG	prop;
{
    int 	c, done = 0;
    SAMPL 	*rdinBuf,*inBuf,*outBuf;
    long	read, ibSize = INBUFSIZE;
    int 	ip, op = 1.0/acc;	/* largest denominator to try */
    int         *ifixar;
    int	        maxOblox;
    long	obBsize, totalRead = 0, actLen;
    RsmpDescType	*rdp;
    TrigRSBufType	*tba[SNDF_MAXCHANS];
    int 	debug = 3;

    chans = sndGetChans(outSnd);
    if(chans > SNDF_MAXCHANS) {
	fprintf(stderr,"%s: sorry, max %d channels (not %d)\n", 
		programName, SNDF_MAXCHANS, chans);
	return;
    }

    FindGoodRatio(outRate/sndGetSrate(inSnd), &ip, &op, acc);
    if(verbose) {
	fprintf(stdout, "ratio %d:%d = %f (to %f %%)-> act sr %.1f (nom %.1f)\n", 
		ip, op, (float)ip/(float)op, 100.0*acc, ip*sndGetSrate(inSnd)/op, 
		/* outRate */ sndGetSrate(outSnd));
    }
    outFile = outSnd;		/* setup global for SendFunction */
    rdinBuf = (SAMPL *)malloc(sizeof(SAMPL)*ibSize*chans);
    inBuf = (SAMPL *)malloc(sizeof(SAMPL)*ibSize);
    outBlockSize = OUTBLOCKSIZE;
    obBsize = sizeof(short)*chans*(outBlockSize+ibSize*ip/op);
    outBuffer = (short *)malloc(obBsize);
    rdp = NewResampKernel(op, ip, lobes, (float)1, prop); 

    actLen = MinResampleBufLen(rdp, outBlockSize, ibSize);

    for (c = 0; c<chans; ++c) {
	/* chnSz,emSz,actLen,fill,chnFn,chnAr,emFn,emAr,rdp,desc */
	tba[c] = NewTrigRSBuf(1L, outBlockSize, actLen, 0L,
			      NULL, NULL, SendFunction, c, 
			      rdp, NULL);
	chanBufd[c] = 0;
	framesent[c] = 0;
    }

    if(sndGetFrames(inSnd) != SF_UNK_LEN) {
	if(verbose)
	    fprintf(stdout,"%s: apparently %ld frames to read\n",
		    programName, sndGetFrames(inSnd));
    }
    while(!done) {
	short *sbuf = (short *)rdinBuf;
	int i;
	
	read = sndReadFrames(inSnd, sbuf, ibSize);
	sbuf += read*chans;
	for(i = chans*read-1; i >=0; --i) {
	    *(rdinBuf+i) = ((float)(*(--sbuf)))*(1.0/RS_SHORTMAX);
	}
       	if(read < ibSize)	done = 1;	/* last read */

	totalRead += read;
    /*  if(debug==2)	ClearSampls(rdinBuf, 1, read);
	++debug;
     */
	for(c = 0; c<chans; ++c) {
	    /* CopySampls(rdinBuf+c, chans, inBuf, 1, read, 1);	 */
	    int i;
	    for(i = 0; i < read; ++i) {
		inBuf[i] = *(rdinBuf+c+chans*i);
	    }
 	    FeedTrigRSBuf(tba[c], inBuf, read , 0);
	}
	DoSend();
	if(verbose) {
	    fprintf(stdout,"%8ld",totalRead);
	    fflush(stdout);
	}
    }

    for(c = 0; c<chans; ++c)
	FeedTrigRSBuf(tba[c], inBuf, 0L, 1);

    DoSend();

    if(verbose) fprintf(stdout,"\n");

    /* Release memory */
    for (c = 0; c<chans; ++c) {
	FreeTrigRSBuf(tba[c]);
    }
    FreeResampKernel(rdp);
    free(outBuffer);
    free(inBuf);
    free(rdinBuf);

    return framesent[0];
}

static void Usage()		/* print syntax & exit */
    {
    fprintf(stderr,
    "usage: %s [-{s|r} rate|-f fctr][-a a%%][-h][-v] insound [outsound]\n", 
	    programName);
    fprintf(stderr,"where (defaults in brackets)\n");
    fprintf(stderr," -r rate  or\n");
    fprintf(stderr," -s rate    resample to this sampling rate\n");
    fprintf(stderr," -f fctr    resample by this factor multiplied by SR\n");
    fprintf(stderr," -a acc%%    match resampling ratio to this percent acc  (%.2f%%)\n", 100.0*DFLT_ACC);
    fprintf(stderr," -p prop    proportion of Nyquist for low pass (%.2f)\n", DFLT_PROP);
    fprintf(stderr," -g gain    scaling of audio data (1.0)\n");
    fprintf(stderr," -m mults   this many multiplies per point              (%d)\n", 2*DFLT_LOBES);
    fprintf(stderr," -h         do NOT modify sampling rate in output header\n");
    fprintf(stderr," -v         verbose mode\n");
    fprintf(stderr," -S sffmt   force input soundfile to be treated as fmt\n");
    fprintf(stderr," -T sffmt   force output soundfile to be treated as fmt\n");
    fprintf(stderr,"            sffmt can be: ");
    SFListFormats(stderr); fprintf(stderr,"\n");
    fprintf(stderr," insound    input soundfile    ('-' uses stdin, (stdout))\n");
    fprintf(stderr," [outsound] output soundfile   (instem+'.rs%s')\n",sndfExtn);
    exit(1);
    }
