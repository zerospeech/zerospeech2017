/*******************************************************\
*   sndcmp.c						*
*   Compare two soundfiles				*
*   dpwe 1992jun01					*
* 1997feb15 dpwe@icsi.berkeley.edu 			*
*	prepped for SPRACHworks release			*
* $Header: /u/drspeech/repos/dpwelib/sndcmp.c,v 1.13 2011/03/09 01:35:03 davidj Exp $
\*******************************************************/

#include <dpwelib.h>
#include <math.h>
/* #include <samplops.h> */
#include <vecops.h>
#include <genutils.h>
#include <snd.h>

#ifdef THINK_C
#include <tc_main.h>
char sfdir_path[] = "Frida:dpwe:sound:";
#endif

/* #define CPBUFSIZE 16384L	/* char copy buffer */
#define CPBUFSIZE 32768L	/* char copy buffer */

/* Types */

typedef struct 		/* structure for gathering stats on a soundfile */
    {
    long 	n;	/* number of samples */
    double	sum;	/* sum of all */
    double	ssq;	/* sum of squares */
    } SNDSTATS;

/* Static function prototypes */
       void ResetStats PARG((SNDSTATS *stats));
       void ProcessFrames PARG((char *p1, char *p2, long len, int fmt, 
				int chans, SNDSTATS *s1stats, 
				SNDSTATS *s2stats, SNDSTATS *difstats, 
				int scndq));
       void ReportResults PARG((FILE *stream,SNDSTATS *stats, char *title));
static void usage PARG((void));
       FILE *RawSFOpenAllocRdHdr PARG((char *name, SFSTRUCT **psnd));

/* Externs */

/* Static global variables */
       char *programName;
static int debug = 1;

static double startTime = 0.0;
static double duration  = -1.0;	/* i.e. EOF */
static int   useStdout = 0;
static char  *inPath1 = NULL;
static char  *inPath2 = NULL;
static char  *outPath = NULL;
static long  delay = 0;		/* skew between files */
/* SNDF format specifiers */
static char  *inSFfmt = NULL;
static char  *outSFfmt = NULL;

main(argc,argv)
    int 	argc;
    char	**argv;
    {
    int 	i = 0;
    int 	err = 0;
    SOUND	*snd1,*snd2, *outSnd;
    SNDSTATS	*s1stats, *s2stats, *difstats;
    long 	bufferByteSize = CPBUFSIZE;
/*  char 	buf1[CPBUFSIZE], buf2[CPBUFSIZE];  */
    char 	*buf1, *buf2;
    long	red1, red2, red, thistime, bufferFrameSize;
    long	doneFrames;
    long 	totalFrames;
    int  	frByts, smByts, chans, done = 0;
    double	inDurn = 0;
	int 	forceChans = 0;
	int     nodiff = 1;
	float   gain = 1.0;

    programName = argv[0];

    i = 0;
    while(++i<argc && err == 0)
	{
	char c, *token, *arg, *nextArg;
	int  argUsed;

	token = argv[i];
	if(*token++ == '-' && *token != '\0')   /* allow '-' thru as name */
	    {
	    if(i+1 < argc)
		nextArg = argv[i+1];
	    else
		nextArg = "";
	    argUsed = 0;
	    while(c = *token++)
		{
		if(NumericQ(token))	arg = token;
		else			arg = nextArg;
		switch(c)
		    {
		case 'd':	delay     = atoi(arg); argUsed = 1; break;
	/*	case 't':	startTime = atof(arg); argUsed = 1; break;
		case 'o':	useStdout = 1; break;
		case 'c':   forceChans = atoi(arg); argUsed = 1; break; */
		case 'g':       gain = atof(arg); argUsed = 1; break;
		case 'S':	inSFfmt = arg; argUsed = 1; break;
		case 'T':	outSFfmt = arg; argUsed = 1; break;
		default:	fprintf(stderr,"%s: unrec option %c\n",
					programName, c);
		                err = 1; break;
	    }
		if(argUsed)
		    {
		    if(arg == token)	token = "";   /* no more from token */
		    else		++i;	      /* skip arg we used */
		    arg = ""; argUsed = 0;
		    }
		}
	    }
	else{
	    if(inPath1 == NULL)		inPath1 = argv[i];
	    else if(inPath2 == NULL)	inPath2 = argv[i];
	    else if(outPath == NULL)	outPath = argv[i];
	    else{  fprintf(stderr,"%s: excess arg %s\n", programName, argv[i]);
		   err = 1;  }
	    }
	}

    if(err || inPath1 == NULL /* || inPath2 == NULL */ )
	usage();		/* never returns */

/*    if(outPath == NULL)
/*	{
/*	outPath = (char *)malloc(strlen(inPath1)+strlen(DEF_EXT));
/*	SubstExtn(inPath1, DEF_EXT, outPath);
/*	}
 */

    /* Map default input name to special token */
    if ( inPath1 && strcmp(inPath1,"-") == 0) {
	inPath1 = SF_STDIN_STR;
    } else if ( inPath2 && strcmp(inPath2,"-") == 0) {
	inPath2 = SF_STDIN_STR;
    }
    if ( outPath && strcmp(outPath,"-") == 0) {
	outPath = SF_STDOUT_STR;
    }

    snd1 = sndNew(NULL);
    snd2 = sndNew(NULL);

    sndSetUformat(snd1, SFMT_FLOAT);
    sndSetUformat(snd2, SFMT_FLOAT);

    if (gain != 1.0) {
	sndSetUgain(snd1, gain);
    }

    if(inSFfmt != NULL) {   /* user requested particular infile trtmt */
	if(sndSetSFformatByName(snd1, inSFfmt) == 0 || sndSetSFformatByName(snd2, inSFfmt) == 0) {
	    fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
		    programName, inSFfmt);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");
	    exit(1);
	}
    }
    if( (snd1 = sndOpen(snd1, inPath1, "r")) == NULL)  {
	SFDie(SFE_NOPEN, inPath1);
    }

    if(inPath2)  {
	if( (snd2 = sndOpen(snd2, inPath2, "r")) == NULL)  {
	    SFDie(SFE_NOPEN, inPath1);
	}
    }
    if(forceChans > 0)  {
	sndSetUchans(snd1, forceChans);
	if(inPath2)  {	
	    sndSetUchans(snd2, forceChans);
	}
    }
    if(inPath2)  {
	if(sndGetUchans(snd1) != sndGetUchans(snd2)) {
	    fprintf(stderr, "%s: incompatible soundfile types in %s and %s\n",
		   programName, inPath1, inPath2);	exit(-1);	}
    }

    chans = sndGetUchans(snd1);
    s1stats = (SNDSTATS *)Mymalloc(chans, sizeof(SNDSTATS), "snd 1 stats");
    s2stats = (SNDSTATS *)Mymalloc(chans, sizeof(SNDSTATS), "snd 2 stats");
    difstats = (SNDSTATS *)Mymalloc(chans, sizeof(SNDSTATS), "diffce stats");
	buf1 = TMMALLOC(char, bufferByteSize, "sndcmp:buf1");
	buf2 = TMMALLOC(char, bufferByteSize, "sndcmp:buf2");
    frByts = sndBytesPerFrame(snd1);		/* bytes/frame */
    smByts = frByts/chans;	/* bytes/sample */
    bufferFrameSize = bufferByteSize / frByts;

    if(delay > 0)	/* positive delay => skip some of sound1 */
	{
	fprintf(stderr, "%s: seeking on %s to skip delay\n",
		programName, inPath1);
	sndFrameSeek(snd1, delay, SEEK_CUR);
	}
    else if(inPath2 && delay < 0)
	{
	fprintf(stderr, "%s: seeking on %s to skip delay\n",
		programName, inPath2);
	sndFrameSeek(snd2, -delay, SEEK_CUR);
	}
	
    totalFrames = sndGetFrames(snd1);
    if (totalFrames != SF_UNK_LEN && delay > 0) {
	totalFrames -= delay;
    }
    if ( inPath2 && sndGetFrames(snd2) != SF_UNK_LEN ) {
	if ( totalFrames == SF_UNK_LEN \
	     || (sndGetFrames(snd2) - MAX(0, -delay)) < totalFrames) {
	    totalFrames = sndGetFrames(snd2) - MAX(0, -delay);
	}
    }

    if(outPath != NULL)	{
	outSnd = sndNew(snd1);
	if(outSFfmt != NULL) {   /* user requested particular outfile trtmt */
	    if(sndSetSFformatByName(outSnd, outSFfmt) == 0) {
		fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
			programName, outSFfmt);
		SFListFormats(stderr);
		fprintf(stderr, "\n");
		exit(1);
	    }
	}
	sndSetUformat(outSnd, SFMT_FLOAT);
	if( (outSnd = sndOpen(outSnd, outPath, "w")) == NULL) {
	    SFDie(err, outPath);
	}
    }

    for(i=0; i<chans; ++i) {
	ResetStats(s1stats+i);
	ResetStats(s2stats+i);
	ResetStats(difstats+i);
    }
    if(totalFrames == SF_UNK_LEN) {
	fprintf(stderr, "UNKNOWN frames to process.  Each dot is %ld frames:\n", 
		bufferFrameSize);
    } else {
	fprintf(stderr, "%ld frames to process.  Each dot is %ld frames:\n", 
		totalFrames, bufferFrameSize);
    }
    done = 0;
    doneFrames = 0;
    while(!done) {
	thistime = bufferFrameSize;
	if (totalFrames >= 0 && thistime > (totalFrames - doneFrames)) {
	    thistime = totalFrames - doneFrames;
	}
	red1 = sndReadFrames(snd1, buf1, thistime);
	if (0) {  /* debug */
	    float sum = 0;
	    int i;
	    for (i = 0; i < red1; ++i) {
		sum += buf1[i];
	    }
	    fprintf(stderr, "read %ld frames, mean=%f\n", red1, sum/red1);
	}
	if(inPath2)  {
	    red2 = sndReadFrames(snd2, buf2, thistime);
	} else {
	    red2 = red1;	/* fudge */
	}
	if( (red = MIN(red1, red2)) < thistime) {
	    /* hit EOF */
	    done = 1;
	    if (totalFrames != SF_UNK_LEN) {
		fprintf(stderr, "%s: wanted %ld of %d, got %ld\n",
			programName, thistime, frByts, red);
		SFDie(SFE_RDERR, NULL);
	    }
	}
	fprintf(stderr, ".");	fflush(stderr);
	ProcessFrames(buf1, buf2, red, sndGetUformat(snd1), chans, 
		      s1stats, s2stats, difstats, (inPath2!=NULL));
	if(outPath != NULL)
	    sndWriteFrames(outSnd, buf1, red);
	doneFrames += red;
	if (totalFrames != SF_UNK_LEN && doneFrames >= totalFrames) {
	    done = 1;
	}
    }

    fprintf(stderr, "\n");

    for(i=0; i< chans; ++i)
	{
	fprintf(stdout, "Channel %d:\n", i);
	ReportResults(stdout, s1stats+i, "Sound1");
	if(inPath2)  {
	    ReportResults(stdout, s2stats+i, "Sound2");
	    ReportResults(stdout, difstats+i, "Diffce");
	    if (difstats[i].ssq != 0) nodiff = 0;
	}
	}
    sndClose(snd1);
    if(inPath2)
	sndClose(snd2);
    if(outPath != NULL)
	sndClose(outSnd);
    /* ensure clean exit, exit nonzero if files differ */
    if (nodiff) {
	exit(0);
    } else {
	exit(1);
    }
}

void ResetStats(stats)
    SNDSTATS *stats;
    {
    stats->n = 0;
    stats->ssq = stats->sum = 0.0;
    }

void GatherStats(stats, data, len, skip)
    SNDSTATS *stats;
    float    *data;
    long     len;
    int      skip;
    {
    long     l;
    double    d;

    for(l=0; l<len; ++l)
	{
	d = *data;
	stats->sum += d;
	stats->ssq += d*d;
	++(stats->n);
	data += skip;
	}
    }

void ProcessFrames(p1, p2, len, fmt1, chans, s1stats, s2stats, difstats, scndq)
    char *p1;
    char *p2;
    long len;
    int fmt1;
    int chans;
    SNDSTATS *s1stats, *s2stats, *difstats;
    int scndq;		/* is there a second sound ? */
    {
	/*     short *ps1, *ps2;  */
    int   ch;
    long  i;
static float *fb1, *fb2, *dfb;
static long fbSiz = 0;
    double s1, s2, d;
    
    int fmt2=fmt1;	/* for now */

    if(fbSiz < len)
	{
	if(fbSiz > 0)	{  
	    /* Myfree((char *)fb1, "ProcFrm1");  
	       Myfree((char *)fb2, "ProcFrm2");  */
	    Myfree((char *)dfb, "ProcFrmD");
	}
	/* 	fb1 = (float *)Mymalloc(len, chans*sizeof(float), "prcss floatbuf 1");
	fb2 = (float *)Mymalloc(len, chans*sizeof(float), "prcss floatbuf 2"); */
	dfb = (float *)Mymalloc(len, chans*sizeof(float), "prcss diffbuf");
	fbSiz = len;
	}

    fb1 = (float *)p1;
    fb2 = (float *)p2;

    /* Short2Float((short *)p1, 1, fb1, 1, chans*len); */
    /* 2005-03-18: always float, now snd.c */
    /*    ConvertSampleBlockFormat((char *)p1, (char *)fb1, 
			     fmt1, SFMT_FLOAT, chans*len); */
    if(scndq)  {
	/* Short2Float((short *)p2, 1, fb2, 1, chans*len); */
	/* CopyFloats(fb2, 1, dfb, 1, chans*len, FWD); */
	/* ConvertSampleBlockFormat((char *)p2, (char *)fb2, 
				 fmt2, SFMT_FLOAT, chans*len); */
	memcpy(dfb, fb2, chans*len*sizeof(float));
	VSMul(dfb, 1, (FLOATARG)-1.0, chans*len);
	VVAdd(fb1, 1, dfb, 1, chans*len); /* form difference */
    }

    for(ch = 0; ch < chans; ++ch)
	{
	GatherStats(s1stats+ch, fb1+ch, len, chans);
	if(scndq)  {
	    GatherStats(s2stats+ch, fb2+ch, len, chans);	
	    GatherStats(difstats+ch, dfb+ch, len, chans);
	}
	}
    if(scndq) {	/* send back diff */
	/* Float2Short(dfb, 1, (short *)p1, 1, chans*len); */
	/*	ConvertSampleBlockFormat((char *)dfb, (char *)p1, 
				 SFMT_FLOAT, SFMT_SHORT, chans*len); */
	memcpy(p1, dfb, chans*len*sizeof(float));
    }
    }

void ReportResults(stream, stats, title)
    FILE *stream;
    SNDSTATS *stats;
    char *title;
    {
    double mean, avsq, var, sd, rms, scale = 32768.0;

    mean = stats->sum/(double)stats->n;
    avsq = stats->ssq/(double)stats->n;
    var  = avsq - mean*mean;
    sd   = sqrt(var);	rms = sqrt(avsq);
    fprintf(stream, "Stats for %s (%ld pts):", title, stats->n);
/*    fprintf(stream, "mean = %6.3f;  var = %6.3f;  power = %6.3f\n", 
	    1000.0*mean, 1000.0*var, 1000.0*avsq); 	*/
    fprintf(stream, "mean = %6.3f;  sd = %8.2f;  rms = %8.2f\n", 
	    scale*mean, scale*sd, scale*rms); 	
    }


static void usage()		/* print syntax & exit */
    {
    fprintf(stderr,
    "usage: %s [-d delay] [-g gain] sound1 sound2 [outsound]\n",
	    programName);
    fprintf(stderr,"where (defaults in brackets)\n");
    fprintf(stderr," -d delay ignore first <d> frames of sound1, if > 0 \n");
    fprintf(stderr," -g gain    apply gain to sound1 ONLY\n");
    fprintf(stderr," -S sffmt   force input soundfile to be treated as fmt\n");
    fprintf(stderr," -T sffmt   force output soundfile to be treated as fmt\n");
    fprintf(stderr,"            sffmt can be: ");
    SFListFormats(stderr); fprintf(stderr,"\n");
    fprintf(stderr," sound1, sound2  input soundfiles to compare\n");
    fprintf(stderr," outsound  difference = sound1 - sound2 \n");
    exit(1);
    }


