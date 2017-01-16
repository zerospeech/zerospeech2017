/***************************************************************\
*	sndrfmt.c						*
*   Reformat a soundfile in any sense				*
*   dpwe 20feb91						*
*   Attempt to include channel, srate and dataformat changing	*
*   dpwe 02dec91						*
\***************************************************************/

#include <dpwelib.h>
#include <math.h>	/* for atof() prototype */
#include <sndfutil.h>
#include <genutils.h>
#include <vecops.h>
#include <byteswap.h>
#include <pushfileio.h>

#ifdef THINK_C
#include <tc_main.h>
char sfdir_path[] = "Frida:dpwe:sound:";
#endif

/* add header defaults */
#define DEF_SRATE	44100
#define DEF_CHANS	1
#define DEF_FMT		SFMT_SHORT

#define CPBUFSIZE 65536L	/* char copy buffer */

/* static function prototype */
static void MyConvertChannels PARG((float *inbuf, float *outbuf, 
				    int inchs, int ouchs, long nsamps));
static void ProcessCLArgs PARG((int argc, char **argv));
static void usage PARG((void));
static void DoSndRfmt PARG((FILE *inf));

/* static data */

char *programName;

static int   chans = 0;
static float srate = 0.0;
static float durn  = 0.0;
static int   durnF = 0;
static float endT  = 0.0;
static int   endF  = 0;
static float skip  = 0.0;
static int   skipF = 0;
static int   skipby= 0;
static char  formc = '\0';
static char  srcformc = '\0';
static int   formt = 0;
static int   srcformt = 0;
static float gain  = 1.0;
static char  *info = "";
static int   infoSz= 0;
static int   query = 0;
static int   rvrs  = 0;
static int   strip = 0;
static int   adhdr = 0;
static int   force = 0;
static int   heder = 0;
static int   useStdout = 0;
static int   useStdin  = 0;
static char  defExtPart[] = ".rf";
static char  defExt[32];
static char  *inPath = NULL;
static char  *outPath = NULL;
static int   abbotClip = 0;
static int   verbose = 0;
/* options to allow specification of channel treatment */
static int   chanmap[SNDF_MAXCHANS];
static int   chanmapsize = 0;
/* hold names of requested soundfile formats */
static char  *inSFfmt = NULL;
static char  *outSFfmt = NULL;

static void DoAbbotClip(short *data, int count)
{   /* quick hack - go through clipping 0x8000s to 0x8001's
       to permit this short data to be used in abbot pipes */
    while(count--) {
	if (*data == -32768 /* 0x8000 */) {
	    *data = -32767  /* 0x8001 */;
	}
	++data;
    }
}

int
main(argc,argv)
    int 	argc;
    char	**argv;
    {
    FILE *inf;
    int fmt, mode;

    programName = argv[0];

    /* handle command line arguments, set up static globals */
    ProcessCLArgs(argc, argv);

    if(verbose)
	{
	fprintf(stderr, "%s: chans %d format %d (%c) src fmt %d (%d)\n", 
		programName, chans, formt, formc, srcformt, srcformc);
	fprintf(stderr, "%s: chanmapsize = %d (%d%d%d%d)\n", 
		programName, chanmapsize, chanmap[0], chanmap[1], 
		chanmap[2], chanmap[3]);
	fprintf(stderr, "%s: srate %8.0f skip %8.4f (%d fr) dur %8.4f\n", 
		programName, srate, skip, skipF, durn);
	fprintf(stderr, "%s: gain %8.4f infosz %d info '%s'\n", 
		programName, gain, infoSz, info);
	fprintf(stderr, "%s: qry %d rvs %d str %d adh %d hdr %d sop %d aclip %d\n",
		programName, query, rvrs, strip, adhdr, heder, useStdout, abbotClip);
	fprintf(stderr, "%s: dfxp '%s' sfEx '%s' inP '%s' outP '%s'\n", 
		programName, defExtPart, sndfExtn, inPath, 
		(outPath==NULL)?"(null)":outPath);
	}
    /* sanity checks */
    if( strip && (heder || adhdr) )
	{  fprintf(stderr, "%s: cannot strip header if adding or editing it\n",
		   programName);	exit(1);	}

    /* open input file and set up specifications of output soundfile */
    if(useStdin) {
	inf = stdin;
	inPath = "(stdin)";
    } else {
	if( (inf = fopen(inPath, "rb")) == NULL) {
	    fprintf(stderr, "%s: couldn't open file %s\n",
		    programName, inPath);
	    exit(1);	
	}
    }

    while ( (fmt = SFIdentify(inf, &mode)) != SFF_ZLEN) {
	if (!adhdr && inSFfmt == NULL) {
	    SFSetFormat(inf, fmt, mode);
	}
	DoSndRfmt(inf);
    }

    fclose(inf);
    exit(0);
}

static void DoSndRfmt(FILE *inf)
{   /* actually do the command.  Governed mostly by globals.
       If <inf> is treated as a sndf file, it will have 
       SFFlushToEof called on it before we return, somehow. */
    int 	err = 0;
    SFSTRUCT	*inSnd = NULL, *outSnd;
    FILE	*outf;
    char 	*cpbuf;		/* pointer to copying buffer */
    long	cpbssiz;	/* copy buffer size in samples */
    long	ipSamps;	/* samples read returned by fread */
    int 	bpsamp;		/* bytes per (output) sample */
    int		actChans;	/* actual channels at var. stages */

    long 	outBytes = 0L;	/* total bytes written to output */
    int		iskipfr, oskipfr; /* frames to skip at in & out rates */

    if(skipby > 0) {
	/* Skip bytes of input file before even thinking about a header */
	seek_by_read(inf, skipby);
    }
    if(!adhdr) {		/* if we have an input header file to read.. */
	if(inSFfmt != NULL) {   /* user requested particular infile trtmt */
	    if(SFSetDefaultFormatByName(inSFfmt) == 0) {
		fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
			programName, inSFfmt);
		SFListFormats(stderr);
		fprintf(stderr, "\n");
		exit(1);
	    }
	}
	if( err = SFAllocRdHdr(inf, &inSnd) )
	    SFDie(err, inPath);
	/* set up defaults of output sound if not on command line */
	if(chans == 0)		chans = inSnd->channels;
	if(srate == 0)		srate = inSnd->samplingRate;
	if(srcformt == 0)	srcformt = inSnd->format;
	if(formt == 0)		formt = inSnd->format;
	if(infoSz== 0)	  {	info  = inSnd->info;
				infoSz= SFInfoSize(inSnd);	}
    } else {			/* no input sound header, use other defaults */
	if(chans == 0)		chans = DEF_CHANS;
	if(srate == 0)		srate = DEF_SRATE;
	if(srcformt == 0)	srcformt = DEF_FMT;
	if(formt == 0)		formt = DEF_FMT;
	/* info is set up to default */
    }

    /* Now input file is open, and output sound details are spec'd */
    if( query ) {	/* just looking - do not want to write output file */
	long samps = SFFlushToEof(inf, inSnd->format);
	if (samps != SF_UNK_LEN) {
	    if (inSnd->dataBsize == SF_UNK_LEN) {
		/* didn't know size until flushed to end */
		inSnd->dataBsize = samps * SFSampleSizeOf(inSnd->format);
	    } else if (inSnd->dataBsize 
		       != samps * SFSampleSizeOf(inSnd->format)) {
		fprintf(stderr, 
			"sndrfmt: header claimed %ld frames, but %ld read\n",
			inSnd->dataBsize/SFSampleFrameSize(inSnd), 
			samps / inSnd->channels);
	    }
	}
	PrintSNDFinfo(inSnd, inPath, SFFormatName(SFGetFormat(inf, NULL)), 
		      stderr);
	return;
    }

    /* Create structure for output sound */
    if( err = SFAlloc(&outSnd, SF_UNK_LEN, formt, srate, chans, infoSz) )
	SFDie(err, NULL);

    /* set up info field (info defaults empty, set in CL or from inSnd) */
    memcpy(outSnd->info, info, infoSz);
    /* condition two versions of skip - if time specified, takes precedence */
    if(skip != 0.0)  {
	float israte = srate;
	if(inSnd != NULL)	israte = inSnd->samplingRate;
	iskipfr = israte * skip;
	oskipfr = outSnd->samplingRate * skip;
    } else {
	iskipfr = oskipfr = skipF;
    }
    if(verbose) 
	fprintf(stderr, "inskipfr=%d, outskipfr=%d\n",iskipfr, oskipfr);
    /* map end to durn, if specified */
    if (endT > 0.0) {
	float skipT, israte = srate;
	if(inSnd != NULL)	israte = inSnd->samplingRate;
	skipT = ((float)iskipfr)/israte;
	durn = endT - skipT;
    }
    if (endF > 0) {
	durnF = endF - iskipfr;
    }
    /* make best estimate of output file length (outSnd->dataBsize) */
    if(durn > 0.0) {	/* if a duration is specified, defines outlen */
	outSnd->dataBsize = durn * outSnd->samplingRate *
	    outSnd->channels * SFSampleSizeOf(outSnd->format);
    } else if (durnF > 0) {
	outSnd->dataBsize = durnF * outSnd->channels * SFSampleSizeOf(outSnd->format);
    } else {			/* output len derived from input len */
	if(adhdr) {		/* no input sound - just raw data */
	    long ibytes = GetFileSize(inf);
		
	    if(ibytes == -1)	/* return if GetFileSize fails */
		outSnd->dataBsize = SF_UNK_LEN;	/* explicit.. */
	    else {		/* got a size, need to scale by format sizes */
		outSnd->dataBsize = SFSampleFrameSize(outSnd) * 
		    ((ibytes-skipby) / (SFSampleSizeOf(srcformt)*chans) 
		     - iskipfr);
		if(outSnd->dataBsize < 0)
		    outSnd->dataBsize = 0;
	    }
	} else if(inSnd->dataBsize == SF_UNK_LEN) { /* inp size unknown */
	    outSnd->dataBsize = SF_UNK_LEN;
	} else {			/* figure modified outsound size */
	    long	inSmps, outSmps;

	    inSmps = inSnd->dataBsize / 
		(inSnd->channels * SFSampleSizeOf(srcformt));
	    inSmps -= iskipfr;
	    if(inSmps < 0)	inSmps = 0;
	    /* output samples is scaled by ratio of sampling rates */
	    outSmps = (long)rint(outSnd->samplingRate * 
			     (float)inSmps / inSnd->samplingRate);
	    outSnd->dataBsize = outSmps * outSnd->channels * 
		SFSampleSizeOf(outSnd->format);

	}
    }

    /* now we need an output file */
    if(outSFfmt != NULL) {	/* user requested particular outfile trtmt */
	if(SFSetDefaultFormatByName(outSFfmt) == 0) {
	    fprintf(stderr, "%s: soundfile format '%s' not known, need\n",
		    programName, outSFfmt);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");	    
	    exit(1);
	}
    }
    /* set up default file extension, now the output file name is known */
    strcpy(defExt, defExtPart);
    strcat(defExt, sndfExtn);

    if(outPath == NULL)	 {	/* no output filename -> stdout or build one */
	char *inNameStem = "stdin"; /* by default .. */

	if(useStdout)
	    outPath = "(stdout)";
	else{
	    if(useStdin == 0)
		inNameStem = inPath; /* use actual name if is one */
	    outPath = (char *)malloc(strlen(inNameStem)+strlen(defExt));
	    SubstExtn(inNameStem, defExt, outPath);
	}
    } else {
	useStdout = 0;		/* no stdout if outpath specified */
    }
    /* Open whichever output file */
    if(useStdout) {
	outf = stdout;
    } else {
	outf = fopen(outPath, "w+b"); /* SFWrHdr needs to read */
    }
    if( outf == NULL ) {
	SFDie(SFE_NOPEN, outPath);
    }

    if( verbose ) {
	if(inSnd != NULL) {
	    fprintf(stderr, "%s: Input sound:\n", programName);
	    PrintSNDFinfo(inSnd, inPath, SFFormatName(SFGetFormat(inf, NULL)), stderr);
	}
    }

    /* output file is open.  Write header if desired */
    if (!strip) {
	/* Make sure we don't try to write MSWAVE or AIFF as mulaw */
	int id = SFGetFormat(outf, NULL);
	if(outSnd->format == SFMT_ULAW
	   && (id == SFF_AIFF || id == SFF_MSWAVE)) {
	    fprintf(stderr, "%s: mulaw not supported in output format - using shorts\n", programName);
	    outSnd->format = SFMT_SHORT;
	    if(outSnd->dataBsize != SF_UNK_LEN) {
		outSnd->dataBsize *= 2;	/* = SFSampleSizeOf(SFMT_SHORT)/SFSampleSizeOf(SFMT_MULAW) */
	    }
	}
	if(err = SFWriteHdr(outf, outSnd, 0))
	    SFDie(err, outPath);
	outBytes += outSnd->headBsize;
    }

    if(verbose) {
	fprintf(stderr, "%s: Output sound%s:\n", programName,
		strip?" (headerless)":"" );
	PrintSNDFinfo(outSnd, outPath, SFFormatName(SFGetFormat(outf, NULL)), stderr);
    }
    /* allocate read buffer */
    if( (cpbuf = (char *)malloc(CPBUFSIZE)) == NULL) {
	fprintf(stderr, "%s: cpbuf malloc fail for %ld\n",
		programName, CPBUFSIZE);	abort();
    }
    bpsamp = SFSampleSizeOf(outSnd->format);
    cpbssiz= CPBUFSIZE / bpsamp;

    if(adhdr) {		/* adding header to raw -> just copy rest of file */
	int bpssamp = SFSampleSizeOf(srcformt);
	long trbsz = cpbssiz;
	long thistime, toskip = iskipfr*chans;

	if(bpssamp > bpsamp) {
	    cpbssiz = CPBUFSIZE/bpssamp;
	}

	/* skip some input data */
	while(toskip > 0)  {
	    /* skip input data */
	    thistime = MIN(toskip, cpbssiz);
	    my_fread(cpbuf, bpssamp, thistime, inf);
	    toskip -= thistime;
	}
	/* add padding to output data */
	toskip = -oskipfr*chans;
	memset(cpbuf, 0, CPBUFSIZE);
	while(toskip > 0)  {
	    thistime = MIN(toskip, cpbssiz);
	    SFfwrite(cpbuf, outSnd->format, thistime, outf);
	    outBytes += bpsamp*thistime;
	    toskip -= thistime;
	}
	/* now process the actual data */
	while( (ipSamps = my_fread(cpbuf, bpssamp, cpbssiz, inf))>0 ) {
	    if(rvrs)
		ConvertBufferBytes(cpbuf, 
				   ipSamps*bpsamp,
				   2,
				   BM_BYTEREV);
	    if(srcformt != outSnd->format)
		ConvertSampleBlockFormat(cpbuf, cpbuf, 
					 srcformt, outSnd->format,
					 ipSamps);

	    if(abbotClip && outSnd->format == SFMT_SHORT) {
		DoAbbotClip((short *)cpbuf, ipSamps);
	    }
	    SFfwrite(cpbuf, outSnd->format, ipSamps, outf);
	    outBytes += bpsamp*ipSamps;
	}
    } else if(heder) {		/* just changing header -> rewrite soundfile */
	/* -V, -A have no effect with -h */
	if(rvrs || abbotClip )	
	    fprintf(stderr, 
		    "%s: reverse/abbotClip flags ignored; only touching header\n", 
		    programName);
	while( (ipSamps = SFfread(cpbuf, outSnd->format, cpbssiz, inf))>0 ) {
	    SFfwrite(cpbuf, outSnd->format, ipSamps, outf);
	    outBytes += bpsamp*ipSamps;
	}
    } else {			/* we are mandated to change the data */
	float	*opbuf;
	long	opSamps;
	long    opSampsToGo;
	long	opbBsize;
	int	rschans, c;

	/* revise number of points we can fit in cp buffer */
	cpbssiz= CPBUFSIZE / MAX(bpsamp, SFSampleSizeOf(srcformt));

	if(outSnd->dataBsize != SF_UNK_LEN)
	    opSampsToGo = outSnd->dataBsize / (bpsamp * outSnd->channels);
	else
	    opSampsToGo = -1L;

	/* maximum size of a frame is (largest sample format) * (most channels)
	 * (sample rate expansion if any (outRate > inRate)) 
	 * (sample frames read at once) */
	opbBsize = MAX(SFSampleSizeOf(srcformt), 
		       SFSampleSizeOf(outSnd->format));
	opbBsize = MAX(opbBsize, sizeof(float)) * 
	    MAX(inSnd->channels, outSnd->channels) *
		(long)(MAX(outSnd->samplingRate/inSnd->samplingRate, 1.0)
		       * (float)cpbssiz);
	if( (opbuf = (float *)malloc(opbBsize))==NULL ) {
	    fprintf(stderr, "%s: opbuf malloc fail for %ld\n",
		    programName, opbBsize);	abort();
	}

	/* how many chans will we be resampling? */
	rschans = MIN(inSnd->channels, outSnd->channels);

	/* ensure we read an integral number of sample frames (all chans) */
	cpbssiz = inSnd->channels * (long)(cpbssiz / inSnd->channels);
	/* we have to load samples (not sample frames) so that SFfread 
	   knows how to convert them, if needed */
	if(iskipfr > 0)	{	/* skip over input sound if req'd */
	    if(SFfseek(inf, SFSampleSizeOf(srcformt)*inSnd->channels *
		       iskipfr, SEEK_CUR) 
	       == -1) {			/* couldn't seek, do reads instead */
		long inSampsToGo, inSamps, redSamps;

		inSampsToGo = iskipfr;
		redSamps = -1;
		while(inSampsToGo > 0 && redSamps != 0) {
		    if( (inSamps = cpbssiz/inSnd->channels) > inSampsToGo)
			inSamps = inSampsToGo;
		    redSamps = SFfread(cpbuf, srcformt, 
				       inSamps * inSnd->channels, inf);
		    redSamps /= inSnd->channels;
		    inSampsToGo -= redSamps;
		}
	    } /* either got to end of input or skipped time */
	} else if(oskipfr < 0.0) {	/* pad front with silence */
	    long	padSamps;

	    padSamps = -oskipfr;
	    if(opSampsToGo >= 0 
	       && padSamps > opSampsToGo)	padSamps = opSampsToGo;
    	    memset(opbuf, 0, cpbssiz*outSnd->channels*sizeof(float));
	    ConvertSampleBlockFormat((char *)opbuf, (char *)opbuf, 
				     SFMT_FLOAT, outSnd->format,
				     cpbssiz * outSnd->channels);
	    while(padSamps > 0) {
		opSamps = cpbssiz/outSnd->channels;
		if(opSamps > padSamps)	opSamps = padSamps;
		padSamps -= opSamps; /* at zero - ends op */
		if(opSampsToGo >= 0)
		    opSampsToGo -= opSamps;
		opSamps *= outSnd->channels; /* frames back to samples */
		if(strip)	/* writing a raw file - no filter */
		    err = fwrite(opbuf, bpsamp, opSamps, outf)<opSamps;
		else		/* sound file may be filtered */
		    err = SFfwrite(opbuf,outSnd->format,opSamps,outf)<opSamps;
		if(err)
		    SFDie(SFE_WRERR, outPath);
		outBytes += opSamps * bpsamp;
	    }
	}
	    
	while( opSampsToGo != 0 && 
	      (ipSamps = SFfread(cpbuf, srcformt, cpbssiz, inf)) > 0 ) {
	    float *wbuf;

	    if(rvrs)
		ConvertBufferBytes(cpbuf, 
				   ipSamps*SFSampleSizeOf(srcformt),
				   2 /* SFSampleSizeOf(srcformt) */ , 
				   BM_BYTEREV);
	    ipSamps /= inSnd->channels;	/* sample FRAMES read */
	    actChans = inSnd->channels;
	    if(gain!=1.0 || inSnd->channels != outSnd->channels
	       || chanmapsize > 0 
	       || inSnd->samplingRate != outSnd->samplingRate 
	       || srcformt != outSnd->format)  {
		/* IF we need to convert the raw data at all.. */
		/* Let's just work in floats .. */
		ConvertSampleBlockFormat(cpbuf, (char *)opbuf, 
					 srcformt, SFMT_FLOAT,
					 ipSamps * actChans);

		/* do ch.cnvrsion (in place) if means less data to resample */
		if(inSnd->channels > outSnd->channels)  {
		    MyConvertChannels(opbuf, opbuf, 
				      inSnd->channels, outSnd->channels,
				      ipSamps);
		    actChans = outSnd->channels;
		}

		/* Resampling goes HERE! */
		/* also, still need to implement duration and skip */
		/* should put test somewhere so doesn't do any of this for 
		   simple strip (no format changing) */
		if(inSnd->samplingRate != outSnd->samplingRate) {
		    fprintf(stderr,"%s: srate conversion not implemented\n",
			    programName);	exit(1);	
		} else {
		    if(gain != 1.0)
			VSMul(opbuf, 1, gain, ipSamps*actChans);
		    opSamps = ipSamps;
		}

		/* do chan. conversion (in place) if we didn't before */
		if(inSnd->channels < outSnd->channels
		   || (inSnd->channels == outSnd->channels && chanmapsize>0)) {
		    MyConvertChannels(opbuf, opbuf, 
				      inSnd->channels, outSnd->channels,
				      opSamps);
		    actChans = outSnd->channels;
		}

		/* convert to final data format */
		ConvertSampleBlockFormat((char *)opbuf, (char *)opbuf, 
					 SFMT_FLOAT, outSnd->format,
					 opSamps * outSnd->channels);
		wbuf = opbuf;
	    } else {		/* just passed the data through */
		opSamps = ipSamps;
		wbuf = (float *)cpbuf;
	    }
	    if(opSampsToGo > 0)	{ 
		/* only write exactly opsampstogo maximum */
		if(opSamps > opSampsToGo)	opSamps = opSampsToGo;
		opSampsToGo -= opSamps;	/* at zero - ends op */
	    }

	    opSamps *= outSnd->channels; /* frames back to samples */
	    if(abbotClip && outSnd->format == SFMT_SHORT) {
		DoAbbotClip((short *)wbuf, opSamps);
	    }
	    if(strip)		/* writing a raw file - no filter */
		err = fwrite(wbuf, bpsamp, opSamps, outf)<opSamps;
	    else		/* sound file may be filtered */
		err = SFfwrite(wbuf, outSnd->format, opSamps, outf)<opSamps;
	    if(err)
		SFDie(SFE_WRERR, outPath);
	    outBytes += opSamps * bpsamp;
	}
	if(opSampsToGo > 0) {	/* more output - pad with zeros */
    	    memset(opbuf, 0, cpbssiz*sizeof(float));
	    ConvertSampleBlockFormat((char *)opbuf, (char *)opbuf, 
				     SFMT_FLOAT, outSnd->format,
				     cpbssiz * outSnd->channels);
	    while(opSampsToGo > 0) {
		opSamps = cpbssiz/outSnd->channels;
		if(opSamps > opSampsToGo)	opSamps = opSampsToGo;
		opSampsToGo -= opSamps;	/* at zero - ends op */
		opSamps *= outSnd->channels; /* frames back to samples */
		if(strip)	/* writing a raw file - no filter */
		    err = fwrite(opbuf, bpsamp, opSamps, outf)<opSamps;
		else		/* sound file may be filtered */
		    err = SFfwrite(opbuf,outSnd->format,opSamps,outf)<opSamps;
		if(err)
		    SFDie(SFE_WRERR, outPath);
		outBytes += opSamps * bpsamp;
	    }
	}
    }

    fflush(outf);
    if(!strip)			/* rewrite header if there is one */
	SFCloseWrHdr(outf, NULL);
    else
	fclose(outf);
    if(verbose) {
	fprintf(stderr, "%s: wrote output file %s (%ld bytes)\n",
		programName, outPath, outBytes);
    }

    if (!adhdr) {
	/* make sure reached EOF on input (unless input is raw) */
	SFFlushToEof(inf, inSnd->format);
    }
    return;
}

static void MyConvertChannels(float *inbuf, float *outbuf, 
			      int inchs, int ouchs, long nsamps)
{	/* implement fancy channel conversion options */
    long i;
    int  ch;
    float	samp[SNDF_MAXCHANS+1];	/* temp store for in-place swap */

/*    fprintf(stderr, "inb %lx, oub %lx, inch %d, ouch %d, ns %ld\n", 
	    inbuf, outbuf, inchs, ouchs, nsamps);
 */
    if(chanmapsize == 0)  {  /* no fancy options, do default stereo<>mono */
	ConvertFlChans(inbuf, outbuf, inchs, ouchs, nsamps);
    }
    else {
	for(ch = 0; ch < SNDF_MAXCHANS+1; ++ch)
	    samp[ch] = 0;
	if(inchs > ouchs) {   /* getting shorter: start at front */
	    for(i = 0; i < nsamps; ++i)  {
		for(ch = 0; ch < inchs; ++ch)  {
		    samp[1+ch] = *inbuf++; /* read each channel */
		}
		/* then write out convoluted.. */
		for(ch = 0; ch < ouchs; ++ch)  {
		    if(ch < chanmapsize)  {
			*outbuf++ = samp[chanmap[ch]];
		    }
		    else
			*outbuf++ = 0;
		}
	    }
	} else {	/* getting longer: start at end */
	    outbuf += ouchs * nsamps;
	    inbuf += inchs * nsamps;
	    for(i = 0; i < nsamps; ++i)  {
		for(ch = inchs; --ch >= 0 ; )  {
		    samp[1+ch] = *--inbuf; /* read each channel */
		}
		/* then write out convoluted.. */
		for(ch = ouchs; --ch >= 0; )  {
		    if(ch < chanmapsize)  {
			*--outbuf = samp[chanmap[ch]];
		    }
		    else
			*--outbuf = 0;
		}
	    }
	}
    }
}

static void ProcessCLArgs(argc, argv)
    int 	argc;
    char	**argv;
    {		/* set up system globals per command line arguments */
    int 	i = 0;
    int 	err = 0;

    if(argc == 2)	query = 1; /* sndrfmt fname   defaults -q */
    while(++i<argc && err == 0)
	{
	char c, *token, *arg, *nextArg;
	int  argUsed;

	token = argv[i];
	if(*token++ == '-')
	    {
	    if(i+1 < argc)
		nextArg = argv[i+1];
	    else
		nextArg = "";
	    argUsed = 0;
	    if(*token == '\0') {
		/* just '-' alone - stands for stdin or stdout */
		if(inPath == NULL) {
		    inPath = "(stdin)";	/* so we know inPath spec'd */
		    useStdin = 1;
		} else {
		    useStdout = 1;
		}
	    } else while(c = *token++) {
	/*	if(NumericQ(token))	arg = token;	*/
		if(strlen(token))	arg = token;
		else			arg = nextArg;
		switch(c)
		    {
		    char *chanarg;
		    int  chani;

		case 'c':	chans = atoi(arg); argUsed = 1; 
		    /* Munsi Hack for channel options */
		    chanarg = strpbrk(arg, "lLrRsStTzZnN");
		    if(chanarg != NULL)  {
			for(chani = 0; chani < MIN(strlen(chanarg), SNDF_MAXCHANS);
			    ++chani)  {
			    if(chanarg[chani] == 'l' || chanarg[chani] == 'L')
				chanmap[chani] = 1;
			    else 
			     if(chanarg[chani] == 'r' || chanarg[chani] == 'R')
				    chanmap[chani] = 2;
			    else 
			     if(chanarg[chani] == 's' || chanarg[chani] == 'S')
				    chanmap[chani] = 3;
			    else 
			     if(chanarg[chani] == 't' || chanarg[chani] == 'T')
				    chanmap[chani] = 4;
			}
			chanmapsize = chani;
		    }
		    break;
		case 'r':	srate = atof(arg); argUsed = 1; break;
		case 'd':	durn  = atof(arg); argUsed = 1; break;
		case 'D':	durnF = atof(arg); argUsed = 1; break;
		case 'e':	endT  = atof(arg); argUsed = 1; break;
		case 'E':	endF  = atof(arg); argUsed = 1; break;
		case 'k':	skip  = atof(arg); argUsed = 1; break;
		case 'K':	skipF = atoi(arg); argUsed = 1; break;
		case 'X':	skipby= atoi(arg); argUsed = 1; break;
		case 'f':	formc = *arg;      argUsed = 1; break;
		case 'F':	srcformc = *arg;   argUsed = 1; break;
		case 'g':	gain  = atof(arg); argUsed = 1; break;
		case 'i':	info  = arg;	infoSz= strlen(info);
		                argUsed = 1; break;
		case 'S':	inSFfmt = arg; argUsed = 1; break;
		case 'T':	outSFfmt = arg; argUsed = 1; break;
		case 'q':	query = 1; break;
		case 'v': 	verbose = 1; break;
		case 'h':	heder = 1; break;
		case 'a':	adhdr = 1; break;
		case 's':	strip = 1; break;
		case 'y':	force = 1; break;
		case 'o':	useStdout = 1; break;
		case 'V':	rvrs = 1; break;
		case 'A':	abbotClip = 1; break;
		default:	fprintf(stderr,"%s: unrec option %c\n",
					programName, c);
		    err = 1; break;
		    }
		if(argUsed)
		    {
		    if(arg == token)	token = ""; /* no more from token */
		    else		++i; /* skip arg we used */
		    arg = ""; argUsed = 0;
		    }
		}
	    }
	else{
	    if(inPath == NULL)		inPath = argv[i];
	    else if(outPath == NULL)	outPath = argv[i];
	    else{  fprintf(stderr,"%s: excess arg %s\n", programName, argv[i]);
		   err = 1;  }
	    }
	}
    if(formc)
	switch(formc)
	    {
	case 'o':	formt = SFMT_CHAR_OFFS;	break;
	case 'a':	formt = SFMT_ALAW;	break;
	case 'u':	formt = SFMT_ULAW;	break;
	case 'c':	formt = SFMT_CHAR;	break;
	case 's':	formt = SFMT_SHORT;	break;
	case 'l':	formt = SFMT_LONG;	break;
	case 'f':	formt = SFMT_FLOAT;	break;
	case 'd':	formt = SFMT_DOUBLE;	break;
	default:
	    fprintf(stderr,"%s: unrec format key %c (not o/a/u/c/s/l/f/d)\n",
		    programName, formc);
	    err = 1;	break;
	    }
    if(srcformc)
	switch(srcformc)
	    {
	case 'o':	srcformt = SFMT_CHAR_OFFS;	break;
	case 'a':	srcformt = SFMT_ALAW;	break;
	case 'u':	srcformt = SFMT_ULAW;	break;
	case 'c':	srcformt = SFMT_CHAR;	break;
	case 's':	srcformt = SFMT_SHORT;	break;
	case 'l':	srcformt = SFMT_LONG;	break;
	case 'f':	srcformt = SFMT_FLOAT;	break;
	case 'd':	srcformt = SFMT_DOUBLE;	break;
	default:
	    fprintf(stderr,"%s: unrec format key %c (not o/a/u/c/s/l/f/d)\n",
		    programName, srcformc);
	    err = 1;	break;
	    }
    if(err || inPath == NULL)
	usage();		/* never returns */
    }

static void usage()		/* print syntax & exit */
    {
    fprintf(stderr,
    "usage: %s [-c chMODE][-r rate][-d durn][-f outfmt][-F srcfmt][-g gain]\n",
	    programName);
    fprintf(stderr,
    "               [-i info][-q][-h][-s][-a][-v] insound [outsound]\n");
    fprintf(stderr,"where (defaults for -a in brackets, else from inSnd)\n");
    fprintf(stderr," -c chMODE convert into <ch> channels            (%d)\n", 
	    DEF_CHANS);
    fprintf(stderr,"           optional MODE (no space) selects which chans of input go to output\n");
    fprintf(stderr,"           -c 2RL swaps stereo channels; -c 4ZLZZ puts mono as 2nd in quad.\n");
    fprintf(stderr,"           without MODE, mixes or replicates\n");

    fprintf(stderr,
	    " -r rate  resample to sampling rate <rate>       (%5.0f)\n", 
	    (float)DEF_SRATE);
    fprintf(stderr," -d durn  extend/chop to duration <durn>         (eof)\n");
    fprintf(stderr," -D durfr extend/chop to duration <durfr> frames (eof)\n");
    fprintf(stderr," -e end   absolute endpoint in time              (eof)\n");
    fprintf(stderr," -E endfr absolute endpoint in sample frames     (eof)\n");
    fprintf(stderr," -k skip  drop (pad if <0) the first <skip> secs   (0)\n");
    fprintf(stderr," -K skipfr  drop/pad the first <skipfr> FRAMES     (0)\n");
    fprintf(stderr," -X skipby  skip BYTES at start of file (bfr hdr)  (0)\n");
    fprintf(stderr,
	    " -f fmt   convert to data format           o/c/a/u/(s)/l/f/d\n");
    fprintf(stderr,
	    " -F fmt   force source data format         (from header)\n");
    fprintf(stderr," -g gain  multiply by factor <gain>              (1.0)\n");
    fprintf(stderr," -i info  add <info> as ascii info field         ('')\n");
    fprintf(stderr," -q       query existing header (default)\n");
    fprintf(stderr," -h       just change header, leave data as is\n");
    fprintf(stderr," -s       strip off header, write (reformatted) data\n");
    fprintf(stderr," -a       add new header in front of data\n");
    fprintf(stderr," -v       verbose mode\n");
    fprintf(stderr," -V       swap bytes in data words\n");
    fprintf(stderr," -S sffmt force input soundfile to be treated as fmt\n");
    fprintf(stderr," -T sffmt force output soundfile to be treated as fmt\n");
    fprintf(stderr,"          sffmt can be: ");
    SFListFormats(stderr); fprintf(stderr,"\n");
    fprintf(stderr," insound  input soundfile or data ('-' takes stdin)\n");
    fprintf(stderr,
	    " outsound output sound ('-' uses stdout)              (instem+'%s%s')\n",
	    defExtPart, sndfExtn);
    exit(1);
    }


