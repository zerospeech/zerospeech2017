/*
 * danmix.c
 * Mix together soundfiles
 * DAn's version
 * 1993sep19 dpwe
 */

#include <dpwelib.h>
#include <math.h>	/* atof() (?) */
#include <genutils.h>	/* for fSize(), NumericQ() and SubstExtn() */
#include <snd.h>
#include <vecops.h>

#define DFLT_EXTN ".mix"

static void usage(void);	/* print syntax & exit */
void DanMix(SOUND *in1, double scale1, double time1, 
	    SOUND *in2, double scale2, double time2, SOUND *out);
    /* read files in1 and in2; add them together and write out to out;
       Continue until sndGetFrames(out) frames are written. 
       Each sound is scaled by <scaleN> and prepended by <timeN>'s worth 
       of zeros.  If <timeN> is less than zero, some of start is skipped. */

 char *programName;

static double f1 = 1.0;
static double f2 = 1.0;
static double t1 = 0.0;
static double t2 = 0.0;
static double dur = 0.0;
static int    deschans = 0;

char *sfifmtname = NULL;
char *sfofmtname = NULL;

int
main(argc, argv)
int argc;
char *argv[];
{
    char *inPath1 = NULL;
    char *inPath2 = NULL;
    char *outPath = NULL;
    int     i = 0;
    int     err = 0;
    SOUND   *in1, *in2, *out;
    long    nframes;
    double  srate;
    int ipsffmt;

    programName = argv[0];
    i = 0;
    while(++i<argc && err == 0)  {
	char c, *token, *arg, *nextArg;
	int  argUsed;

	token = argv[i];
	if(*token++ == '-')  {
	    if(i+1 < argc)  nextArg = argv[i+1];
	    else	nextArg = "";
	    argUsed = 0;
	    while(c = *token++)  {
		if(NumericQ(token)) arg = token;
		else		arg = nextArg;
		switch(c)  {
	    /*	case 'd':   data = atoi(arg); argUsed = 1; break;
		case 'v':   verb = 1; break;
	     */
		case 'f':   f1 = atof(arg); argUsed = 1; break;
		case 'g':   f2 = atof(arg); argUsed = 1; break;
		case 'k':   t1 = atof(arg); argUsed = 1; break;
		case 'l':   t2 = atof(arg); argUsed = 1; break;
		case 'd':   dur = atof(arg); argUsed = 1; break;
		case 'c':   deschans = atoi(arg); argUsed = 1; break;
		case 'S':   sfifmtname = arg; argUsed = 1; break;
		case 'T':   sfofmtname = arg; argUsed = 1; break;
		
		default:    fprintf(stderr,"%s: unrec option %c\n",
				    programName, c);
		    err = 1; break;
		}
		if(argUsed) {
		    if(arg == token)	token = "";   /* no more from token */
		    else	++i;	      /* skip arg we used */
		    arg = ""; argUsed = 0;
		}
	    }
	} else {
	    if(inPath1 == NULL)		inPath1 = argv[i];
	    else if(inPath2 == NULL)	inPath2 = argv[i];
	    else if(outPath == NULL)	outPath = argv[i];
	    else{ fprintf(stderr,"%s: excess arg %s\n", programName, argv[i]);
		  err = 1;  }
	}
    }
    /* other tests on args */

    if(err || inPath2 == NULL)
    usage();	    /* never returns */

    if(outPath == NULL)  {
	outPath = TMMALLOC(char, strlen(inPath1)+strlen(DFLT_EXTN),
			   "danmix:op");
	strcpy(outPath, inPath1);
	SubstExtn(inPath1, DFLT_EXTN, outPath);
    }

    /* Input soundfile fmt name; accept "unk?nown" meaning to guess */
    if (sfifmtname && \
	strncmp(sfifmtname, "unknown", strlen(sfifmtname)) != 0) {
        if (!SFSetDefaultFormatByName(sfifmtname)) {
	    fprintf(stderr, "sndffmt: '%s' is not recognized.  Try:\n", 
		    sfifmtname);
	    SFListFormats(stderr);
	    fprintf(stderr, "\n");
	    exit(-1);
	}
    } else {
        /* no input type specified, so set to guess */
	SFSetDefaultFormat(SFF_UNKNOWN);
    }
    /* record what we ended up with, so we can reset after opening */
    ipsffmt = SFGetDefaultFormat();

    if( (in1 = sndOpen(NULL, inPath1, "r")) == NULL)  {
	fprintf(stderr, "%s: cannot open '%s' as input soundfile\n",
		programName, inPath1);
	usage();
    }

    /* reset the soundfile format type again - no need to check here */
    SFSetDefaultFormat(ipsffmt);

    if( (in2 = sndOpen(NULL, inPath2, "r")) == NULL)  {
	fprintf(stderr, "%s: cannot open '%s' as input soundfile\n",
		programName, inPath2);
	usage();
    }
    /* check compatability of two sounds */
    if(deschans>0)  {
	sndSetUchans(in1, deschans);
	sndSetUchans(in2, deschans);
    } else if(sndGetChans(in1) != sndGetChans(in2))  {
	fprintf(stderr, "%s: WARNING: %d chans in 2nd sound being coerced to %d like 1st\n",
		programName, sndGetChans(in2), sndGetChans(in1));
	sndSetUchans(in2, sndGetChans(in1));
    }
    if(sndGetSrate(in1) != sndGetSrate(in2))  {
	fprintf(stderr, "%s: WARNING: 2nd sound has sr of %.0f Hz; " \
		"output is %.0f Hz\n",
		programName, sndGetSrate(in2), sndGetSrate(in1));
	/* continue none-the-less */
    }
    /* spawn parameters from first input (including sffmt, now works) */
    out = sndNew(in1);
    if(deschans>0)
	sndSetChans(out, deschans);
    srate = sndGetSrate(out);
    nframes = dur * srate;
    if(nframes <= 0)  {	/* dur was not specified */
	nframes = MAX(0, MAX(srate*t1+sndGetFrames(in1), 
			     srate*t2+sndGetFrames(in2)));
    }
    sndSetFrames(out, nframes);
    /* setup for correct output format */
    ipsffmt = SFGetFormat(in1->file, NULL);
    if(sfofmtname) {
      if (!sndSetSFformatByName(out, sfofmtname)) {
	fprintf(stderr, "sndfofmt: '%s' is not recognized.  Try:\n", 
		sfofmtname);
	SFListFormats(stderr);
	fprintf(stderr, "\n");
	exit(-1);
      }
    } else {
      sndSetSFformat(out, ipsffmt);
    }
    /* Open the output */
    if( (out = sndOpen(out, outPath, "w")) == NULL )  {
	fprintf(stderr, "%s: cannot open '%s' as output soundfile\n",
		programName, outPath);
	usage();
    }
    DanMix(in1, f1, t1, in2, f2, t2, out);
    sndClose(out);
    sndClose(in1);
    sndClose(in2);
}

void DanMix(SOUND *in1, double scale1, double time1, 
	    SOUND *in2, double scale2, double time2, SOUND *out)
{   /* read files in1 and in2; add them together and write out to out;
       Continue until sndGetFrames(out) frames are written. 
       Each sound is scaled by <scaleN> and prepended by <timeN>'s worth 
       of zeros.  If <timeN> is less than zero, some of start is skipped. */
    long in1len, in2len, outlen;
    long got, thistime, zerothistime, todo, done, blocksize = 10000;
    int chans;
    float *buf, *buf2;
    long silence1 = 0, silence2 = 0;
    double srate = sndGetSrate(out);

    in1len = sndGetFrames(in1);
    in2len = sndGetFrames(in2);
    todo = outlen = sndGetFrames(out);
    done = 0;
    chans  = sndGetChans(out);
    buf = TMMALLOC(float, 2 * chans * blocksize, "DanMix:buf");
    buf2 = buf + chans*blocksize;
    /* seek on sounds if positive skip times */
    if(time1 > 0)  {	/* skip some of sound 1 */
	sndFrameSeek(in1, (long)(time1*srate), SEEK_SET);
    } else {
	silence1 = (long)(-time1*srate);
    }
    if(time2 > 0)  {	/* skip some of sound 2 */
	sndFrameSeek(in2, (long)(time2*srate), SEEK_SET);
    } else {
	silence2 = (long)(-time2*srate);
    }
    while(todo) {
	thistime = MIN(todo, blocksize);
	if(done < silence1)  {
	    /* still getting silence from sound 1 */
	    got = MIN(thistime, silence1-done);
	    VZero(buf, 1, chans*got);
	    if(thistime > got)
		got += sndReadFrames(in1, buf+chans*got, thistime-got);
	} else {
	    got = sndReadFrames(in1, buf, thistime);
	}
	if(got < thistime)
	    VZero(buf + chans*got, 1, chans * (thistime - got) );
	if(scale1 != 1.0)
	    VSMul(buf, 1, scale1, chans * got);
	if(done < silence2)  {
	    /* still getting silence from sound 2 */
	    got = MIN(thistime, silence2-done);
	    VZero(buf2, 1, chans*got);
	    if(thistime > got)
		got += sndReadFrames(in2, buf2+chans*got, thistime-got);
	} else {
	    got = sndReadFrames(in2, buf2, thistime);
	}
	if(got < thistime)
	    VZero(buf2 + chans*got, 1, chans * (thistime - got) );
	if(scale2 != 1.0)
	    VSMul(buf2, 1, scale2, chans * got);
	VVAdd(buf2, 1, buf, 1, thistime * chans);
	got = sndWriteFrames(out, buf, thistime);
	todo -= got;
	done += got;
	if(got < thistime)  {
	    fprintf(stderr, "%s: write error with %ld left to go\n", programName, todo);
	    goto endlabel;
	}
    }
 endlabel:
    ;
}


static void usage(void)     /* print syntax & exit */
{
    fprintf(stderr,
	    "usage: %s [-f f1][-g f2] insnd1 insnd2 [outsnd]\n",programName);
    fprintf(stderr,"where (defaults in brackets)\n");
    fprintf(stderr," -f f1    scale factor for first sound (1.0)\n");
    fprintf(stderr," -g f2    scale factor for second sound (1.0)\n");
    fprintf(stderr," -k t1    skip(+)/prepend silence(-) front of sound 1 (0)\n");
    fprintf(stderr," -l t2    skip(+)/prepend slience(-) front of sound 2 (0)\n");
    fprintf(stderr," -d dur   total duration of output (until input done)\n");
    fprintf(stderr," insnd1   first input sound\n");
    fprintf(stderr," insnd2   second input sound\n");
    fprintf(stderr," outsnd   mixed output sound  (inStem+'%s')\n",DFLT_EXTN);
    exit(1);
}
