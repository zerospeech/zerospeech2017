/***************************************************************\
*   sndfstrut.c				Switchboard STRUT fmt	*
*   Extra functions for sndf.c					*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   1996may23 modeled on sndfaif.c				*
\***************************************************************/

/*
 * Changes 
 *
 */

#include <byteswap.h>
#include <assert.h>

/* exported file type indicator */
char *sndfExtn = ".str";		/* file extension, if used a*/

/* #define DEBUG 1  /* */

/* billg's cool types etc */
/* #include "sndffname.c"	/* define AddFname, FindFname etc */

/* subsidiary extern func protos */

/* private type declarations */

/* static variables */

/* static function prototypes ---------------------------------------------- */

/* ----------------------- implementations ------------------------ */

#define TBSIZE 256
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* !MIN */

/* place to put info temporarily */
/* 
  static char *info_buf=NULL;
  static int ib_len = 0;
 */
/* already declared in sndfnist.c */

int SFReadHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
/* utility to sample the header of an opened file, at most infobmax info byts */
    {
#define LBLEN 256
    char lbuf[LBLEN];
    int nhdread = 0;
    int hdtotlen = 0;
    int done = 0;
    int  schan = 0;
    long samps = 0;
    int  srate = 0;
    int  snbyt = 0;
    int  sbits = 0;
    int  basefmt = _LINEAR;
    long ft;
    char *ib_ptr;
    int bytemode = BM_INORDER;
    int seekable = TRUE;
    int skipmagic = 0;

    ft = ftell(file);
    if( ft < 0 ) {			/* don't try seek on stdin */
	/* pipe - skip magic if already ID'd */
	if (GetSFFType(file) != NULL) {
#ifdef HAVE_FPUSH
	    /* If we have fpush, we'll have pushed back to zero anyway */
	    skipmagic = 0;
#else /* !HAVE_FPUSH */
	    /* we'll guess that the type came from SFIdentify, although 
	       it *could* have come from SFSetFileType, in which case 
	       we'll fail */
	    skipmagic = sizeof(INT32);
#endif /* HAVE_FPUSH */
	}
	seekable = FALSE;
    } else if (ft == sizeof(INT32)) {
	/* assume we already read the magic & fake it up */
	skipmagic = ft;
    } else {
	if(fseek(file,(long)0,SEEK_SET) != 0 ) /* move to start of file */
	    return(SFerror = SFE_RDERR);
    }
    if(skipmagic)		strncpy(lbuf, "STRU", skipmagic);

/*    fgets(lbuf+skipmagic, LBLEN-skipmagic, file);    
      nhdread += strlen(lbuf);    chop(lbuf); */
    /* Use fread rather than fgets for this first read because the 
       4 bytes 'pushed back' by SFIdentify are only restored in 
       my_fread - not in my_fgets */
    nhdread = skipmagic;
    nhdread += fread(lbuf+skipmagic, 1, strlen("STRUT_1A/") - skipmagic, file);
    /* if we're at EOF, it's not a sound file (lame attempt to readon strem) */
    if (feof(file)) {
	return (SFerror = SFE_EOF);
    }
    lbuf[strlen("STRUT_1A/")] = '\0'; chop(lbuf);
    if(strcmp(lbuf, "STRUT_1A")!=0) {
	fprintf(stderr, "STRUT SFRdHdr: id of '%s' is not 'STRUT_1A' - lose\n", 
		lbuf);
	return SFE_NSF; /* Not a Sound File */
    }
    /* read header values */
    /* first line is number of bytes in header */
    fgets(lbuf, LBLEN, file);    nhdread += strlen(lbuf);    chop(lbuf);
    hdtotlen = atoi(lbuf);
    /* rest are token/value pairs */
    /* reset the info_buf used to store unknown fields */
    /* 2002-10-08: allocate if not yet, or too small */
    if (ib_len < hdtotlen) {
	if (info_buf)	free(info_buf);
	info_buf = NULL;
    }
    if (info_buf == NULL) {
	ib_len = hdtotlen;
	info_buf = malloc(ib_len);
    }
    /* reset the info_buf used to store unknown fields */
    info_buf[0] = '\0';
    ib_ptr = info_buf;

    while(!done && !feof(file)) {
	char tok[LBLEN], *t;
	int tklen;

	t = lbuf;

	fgets(lbuf, LBLEN, file);    nhdread += strlen(lbuf);    chop(lbuf);

/* fprintf(stderr, "HDR: got '%s'\n", lbuf); */

	tklen = getNextTok(&t, tok);
	if(strcmp(tok, "end_head")==0)  {
	    done = 1;
	} else if(strcmp(tok, "channel_count")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    schan = atoi(tok);
	} else if(strcmp(tok, "sample_coding")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -sN */
	    tklen = getNextTok(&t, tok);		/* actual coding */
	    if(strcmp(tok, "pcm")==0) {
		basefmt |= _LINEAR;
	    } else if(strncmp(tok, "pcm,",4)==0) {
		basefmt |= _LINEAR;
		/* check for shorten (any version 2.xx) */
		if (strncmp(tok+4, "embedded-shorten-v2.", 20)==0) {
		    /* this is embedded shorten - hope we are decoding it */
		} else {
		    fprintf(stderr, "STRUT SFRdHdr: Extra in '%s' ignored\n", 
			    tok);
		}
	    } else if(strcmp(tok, "alaw")==0) {
		basefmt |= _ALAW;
	    } else if(strcmp(tok, "ulaw")==0) {
		basefmt |= _ULAW;
	    } else if(strncmp(tok, "ulaw",4)==0) {
		basefmt |= _ULAW;
		fprintf(stderr, "STRUT SFRdHdr: Extra in '%s' ignored\n", tok);
	    } else if(strcmp(tok, "mu-law")==0) {
		basefmt |= _ULAW;
	    } else {
		fprintf(stderr, "STRUT SFRdHdr: %s not pcm, alaw, ulaw or mu-law - ignore\n", 
			tok);
	    }
	} else if(strcmp(tok, "sample_count")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    samps = atol(tok);
	} else if(strcmp(tok, "sample_rate")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    srate = atol(tok);
	} else if(strcmp(tok, "sample_n_bytes")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    snbyt = atoi(tok);
	} else if(strcmp(tok, "sample_byte_format")==0) {
	    tklen = getNextTok(&t, tok);
	    tklen = getNextTok(&t, tok);
	    if(strcmp(tok, "01")==0) {
		bytemode = BM_BYTEREV;
		/* fprintf(stderr,"this data is byteswapped, but I can't handle it\n"); */
	    } else if(strcmp(tok, "10")==0) {
		bytemode = BM_INORDER;
	    } else if(strcmp(tok, "1")==0) {	/* mu-law reported like this? */
		bytemode = BM_INORDER;
	    } else if(strcmp(tok, "mu-law")==0) {
		bytemode = BM_INORDER;
		fprintf(stderr, "STRUT SFRdHdr: funky mu-law sample_byte_format\n");
	    } else {
		fprintf(stderr, "STRUT SFRdHdr: sample_byte_format '%s' is not mu-law, 10 or 01 (ignore)\n", tok);
	    }
	} else if(strcmp(tok, "sample_sig_bits")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    sbits = atoi(tok);
	} else if(strcmp(tok, "sample_checksum")==0) {
	    long cksum;
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    cksum = atoi(tok);		/* just consume & ignore */
	} else {
	    /* not an anticipated field - write into the info_buf */
	    if(strlen(lbuf)) {
		sprintf(ib_ptr, "%s\n", lbuf);
		ib_ptr += strlen(ib_ptr);
		/* hope info_buf never fills up! */
	    }
	}
    }
    if(!done) {
	/* hit eof before completing */
	fprintf(stderr, "STRUT SFRdHdr: looked good, but no end_head\n");
	return SFE_NSF;
    }
    if(sbits != 8 * snbyt) {
      fprintf(stderr, "STRUT SFRdHdr: %d bits a surprise for %d bytes/samp\n", 
	      sbits, snbyt);
    }
    /* handle whatever happened to bytemode */
    if(snbyt>1 && (bytemode & BM_WORDMASK) != (HostByteMode() & BM_WORDMASK)) {
	SetBytemode(file, BM_BYTEREV);
    } else {	/* set the flag that no byteswapping is needed */
	SetBytemode(file, BM_INORDER);
    }
    if(seekable) {
	fseek(file, hdtotlen, SEEK_SET);
    } else {
	seek_by_read(file, hdtotlen-nhdread);
    }
    /* now at start of data */

    /* set up fields */
    snd->magic = SFMAGIC;
    snd->headBsize = sizeof(SFSTRUCT);
    assert(snbyt==2 || snbyt==1);	/* only ulw, lin8 or lin16 for now */
    snd->format = basefmt | ((snbyt==1)?SFMT_CHAR:SFMT_SHORT);
    snd->channels = schan;
    snd->samplingRate = (float)srate;
    snd->dataBsize = snbyt*samps*schan; /* samps is frames now 1999sep28 */
    {   /* handle whatever we put into info_buf */
	int ibSize = ib_ptr - info_buf;
	int lastwd = ibSize % 4;

	if(infoBmax <= 0)	infoBmax = SFDFLTBYTS;

	snd->info[0] = '\0';
        if(lastwd) {
	    /* pad last word to 4-byte boundary with zeros */
	    int xtra = 4-lastwd;
	    long l = 0;
	    memcpy(info_buf+ibSize, &l, xtra);
	    ibSize += xtra;
	}
	memcpy(snd->info, info_buf, MIN(infoBmax, ibSize));
	snd->headBsize += MAX(0, ibSize-SFDFLTBYTS);
    }

    /* record the indicated seek points for start and end of sound data */
    SetSeekEnds(file, hdtotlen, hdtotlen+snd->dataBsize);

    return SFE_OK;
}

int SFRdXInfo(FILE *file, SFSTRUCT *snd, int done)
/* fetch any extra info bytes needed in view of read header
    Passes open file handle, existing sound struct
    already read 'done' bytes; rest inferred from structure */
{
    int infoBsize = snd->headBsize - sizeof(SFSTRUCT) + SFDFLTBYTS;

    if(done < SFDFLTBYTS)
	done = SFDFLTBYTS;
    if(infoBsize > done) {
	memcpy(snd->info+done, info_buf+done, infoBsize-done);
    }
    return done;
}

static int struthostbytemode = -1;

int SFWriteHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
/*  Utility to write out header to open file
    infoBmax optionally limits the number of info chars to write.
    infoBmax = 0 => write out all bytes implied by .headBsize field
    infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */
    {
    int err = 0;
    int ft;
#define LINELEN 256
    char line[LINELEN], *s, *t;
    int hedlen = 0;
    int bytemode = -1;

    if(snd == NULL) {	/* rewrite but can't seek */
	return SFE_OK;
    }

    if(struthostbytemode < 0)	struthostbytemode = HostByteMode();

    if(snd->magic != SFMAGIC)
	return(SFerror = SFE_NSF);

    ft = ftell(file);
    if( ft > 0 ) {			/* don't try seek on pipe */
	if(fseek(file, (long)0, SEEK_SET) != 0 )   /* move to start of file */
	return(SFerror = SFE_WRERR);
    }
    s = "STRUT_1A\n   1024\n";
    fputs(s, file);	hedlen += strlen(s);
    /* coversation_id, database_id, switch_channel_num, switch_start_sample, 
       sample_checksum
       all lost */

    sprintf(line, "channel_count -i %d\n", snd->channels);
    s = line;	fputs(s, file);	hedlen += strlen(s);

    if(snd->format & _ULAW) {
	t = "ulaw";
    } else if(snd->format & _ALAW) {
	t = "alaw";
    } else if(snd->format & _FLOAT) {
	t = "float";
    } else {
	t = "pcm";
    }
    sprintf(line, "sample_coding -s%ld %s\n", (long) strlen(t), t);
    s = line;	fputs(s, file);	hedlen += strlen(s);

    sprintf(line, "sample_count -i %ld\n", (snd->dataBsize == SF_UNK_LEN)?
	    SF_UNK_LEN : (snd->dataBsize/SFSampleFrameSize(snd)));
    s = line;	fputs(s, file);	hedlen += strlen(s);

    sprintf(line, "sample_rate -i %d\n", (int)snd->samplingRate);
    s = line;	fputs(s, file);	hedlen += strlen(s);

    sprintf(line, "sample_n_bytes -i %d\n", SFSampleSizeOf(snd->format));
    s = line;	fputs(s, file);	hedlen += strlen(s);

    sprintf(line, "sample_sig_bits -i %d\n", 8*SFSampleSizeOf(snd->format));
    s = line;	fputs(s, file);	hedlen += strlen(s);

    /* Guessing the format of the sample_byte_format flag */
    if(snd->format != SFMT_ULAW && snd->format != SFMT_ALAW) {
	/* don't report sample_byte_format for mulaw/alaw data */
	/* See if we have a bytemode defined; otherwise, use natural machine 
	   order */
        char *ename = "Big";
	if((bytemode = GetBytemode(file)) == -1) {
	    /* attempt to write out INORDER, if have a choice.
	       Thus, bytemode copies what host did (to reverse it) */
	    bytemode = struthostbytemode;
	    SetBytemode(file, bytemode);
	}
	if((struthostbytemode == BM_INORDER && bytemode == BM_INORDER) ||
	    (struthostbytemode != BM_INORDER && bytemode != BM_INORDER)) {
	    t = "10";
	} else {
	    /* either this machine is byteswapped and the swapmode is none
	       or this machine is not byteswapped, but we are swapping */
	    t = "01";
	    ename = "Little";
	}
	sprintf(line, "sample_byte_format -s%ld %s\n", (long) strlen(t), t);
	s = line;	fputs(s, file);	hedlen += strlen(s);
	sprintf(line, "data_format -s%ld %sEndian\n", (long) 6+strlen(ename), ename);
	s = line;	fputs(s, file);	hedlen += strlen(s);
	sprintf(line, "record_freq -r %.6f\n", snd->samplingRate);
	s = line;	fputs(s, file);	hedlen += strlen(s);
	sprintf(line, "start_time -r %.6f\n", (float)0.0);
	s = line;	fputs(s, file);	hedlen += strlen(s);

    }
    
    {   /* copy contents of info[] as extra ascii lines */
	int infoBsize = snd->headBsize - sizeof(SFSTRUCT) + SFDFLTBYTS;
        if(infoBmax >= SFDFLTBYTS) { 
	    /* limit to the number of info bytes to write out is ip arg */
	    infoBsize = infoBmax;
	}
	if(infoBsize > SFDFLTBYTS) {
	    if(snd->info[infoBsize-1]=='\0') {
		fputs(snd->info, file);
		hedlen += strlen(snd->info);
	    } else {
		fwrite(snd->info, 1, infoBsize, file);
		hedlen += infoBsize;
	    }
	} else {
	  /* no extra info - fake up some STRUT fields */
	  sprintf(line, "file_type -s7 samples\n");
	  s = line;	fputs(s, file);	hedlen += strlen(s);
	  sprintf(line, "data_offset -i 1024\n");
	  s = line;	fputs(s, file);	hedlen += strlen(s);
	  sprintf(line, "data_size -i %ld\n", (snd->dataBsize == SF_UNK_LEN)?
	    SF_UNK_LEN : snd->dataBsize);
	  s = line;	fputs(s, file);	hedlen += strlen(s);
	}
    }

    s = "end_head\n";
    fputs(s, file);	hedlen += strlen(s);

    {	/* pad out to 1024 ascii chrs */
	int remain = 1024;
	int thistime = -1;

	memset((void*)line, ' ', LINELEN);
	remain -= hedlen;
	while(thistime && remain > 0) {
	    thistime = MIN(LINELEN, remain);
	    thistime = fwrite((void*)line, 1, thistime, file);
	    remain -= thistime;
	    hedlen += thistime;
	}
    }

    /* record the indicated seek points for start and end of sound data */
    SetSeekEnds(file, hedlen, hedlen+snd->dataBsize);

    if(err)
	return(SFerror = SFE_WRERR);
    else
	return(SFerror = SFE_OK);
    }

static long SFHeDaLen(file) /* find header length for a file */
    FILE *file;
    {
    /* this number is subtracted from the ftell on header rewrite to
       figure the data byte size. */
/*    return(sizeof(AIFFHdr));	    /* billg makes it sooo easy */
    long stt, end;

    GetSeekEnds(file, &stt, &end);
    return stt;
    }

static long SFTrailerLen(FILE *file)
{   /* return data space after sound data in file (for multiple files) */
    return(0);
}

static long SFLastSampTell(file)	/* return last valid ftell for smps */
    FILE	*file;
    {
    long stt, end;

    GetSeekEnds(file, &stt, &end);
    return end;
    }

static long FixupSamples(file, ptr, format, nitems, flag)
    FILE  *file;
    void  *ptr;
    int   format;
    long  nitems;
    int   flag;		/* READ_FLAG or WRITE_FLAG indicates direction */
    {
    int bytemode;
    int wordsz;
    long bytes;

    wordsz = SFSampleSizeOf(format);
    bytes  = nitems * (long)wordsz;
/*  bytemode = HostByteMode();	/* relative to 68000 mode, which we want */
    bytemode = GetBytemode(file);
    if(bytemode != BM_INORDER) {	/* already OK? */
	/* otherwise need to fixup buffer .. */
	ConvertBufferBytes(ptr, bytes, wordsz, bytemode);
    }
    return nitems;
}
