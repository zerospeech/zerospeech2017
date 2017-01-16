/***************************************************************\
*   sndfesps.c				ESPS waves+ *.sd format *
*   Extra functions for sndf.c					*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   1997may14 modeled on sndfnist.c				*
\***************************************************************/

/*
 * Changes 
 *
 */

#include <byteswap.h>
#include <assert.h>

/* exported file type indicator */
char *sndfExtn = ".sd";		/* file extension, if used a*/

/* ----------------------- implementations ------------------------ */

/* header string sizes */
#define DATESIZE 	26
#define VERSIONSIZE	8
#define PROGSIZE	16
#define USERSIZ		8
#define NSPARES		10
/* magic numbers */
#define HD_CHECK_VAL	0x6A1A

typedef struct _espshdr {
    /* first 16 bytes are ?? */
    INT32	unk0;			/* = 0x00000004 */
    INT32	unk1;			/* = 3000 decimal */
    INT32	header_bsize;		/* pos of first sample data */
    INT32	unk3;			/* = 0x00000002 */
    /* then a favorite check feild */
    INT32	check1;			/* = 0x00006a1a, HD_CHECK_VAL */
    INT32	dummy0;			/* = 0 */
    INT32	dummy1;			/* = 0 */
    INT32	dummy2;			/* = 0 */
    /* Now, at byte 32, starts to look like esps (5) struct header */
    short 	type;			/* = 13 */
    short	pad1;			/* gap */
    INT32	check;			/* HD_CHECK_VAL: nb: not wd-algnd */
    char	date[DATESIZE];
    char	hdvers[VERSIONSIZE];
    char	prog[PROGSIZE];
    char	vers[VERSIONSIZE];
    char	progdate[DATESIZE];
    INT32	ndrec;			/* number of data records (=0 !) */
    short	tag;			/* boolean: has tag? (=0) */
    short 	pad2;
    /* counts of different datum types */
    INT32	ndouble;
    INT32	nfloat;
    INT32	nlong;
    INT32	nshort;
    INT32	nchar;
    
    INT32	fixsiz;			/* 'fixed hdr size' = 40 (INT32s) */
    INT32	hsize;			/* total hdr size in INT32s (=187) */
    char	user[USERSIZ];		/* username of creator */
    short	edr;			/* boolean for 'EDR'/Native fmt (=0) */
    short	machine_code;		/* this machine (=0) */
    short	spares[NSPARES];
} ESPSHDR;

/* key for byteswapping */
char *espshdrfmt = "4L 4L 2W L 84C L2W 5L2L 8C 2W 10W";

#define FREAD_LONG(plong, file, bytemode) 	\
     (_tmp = fread(plong, 1, sizeof(INT32), file), *plong = lshuffle(*plong, bytemode), _tmp)
#define FREAD_WORD(pword, file, bytemode) 	\
     (_tmp = fread(pword, 1, sizeof(short), file), *pword = wshuffle(*pword, bytemode), _tmp)
#define FREAD_BYTE(pbyte, file, bytemode) 	\
     fread(pbyte, 1, sizeof(char), file)

static int fread_ESPSHDR(ESPSHDR *hdr, FILE *file, int bytemode, int skip)
{   /* Read the fields of an ESPSHDR one by one, byteswapping as we go */
    int i,_tmp, red = 0;

    if(skip == 0) {
	red += FREAD_LONG(&hdr->unk0, file, bytemode);
    } else {
	red = skip;
    }
    assert(red == sizeof(INT32));

    red += FREAD_LONG(&hdr->unk1, file, bytemode);
    red += FREAD_LONG(&hdr->header_bsize, file, bytemode);
    red += FREAD_LONG(&hdr->unk3, file, bytemode);

    red += FREAD_LONG(&hdr->check1, file, bytemode);
    red += FREAD_LONG(&hdr->dummy0, file, bytemode);
    red += FREAD_LONG(&hdr->dummy1, file, bytemode);
    red += FREAD_LONG(&hdr->dummy2, file, bytemode);

    red += FREAD_WORD(&hdr->type, file, bytemode);
    red += FREAD_WORD(&hdr->pad1, file, bytemode);
    red += FREAD_LONG(&hdr->check, file, bytemode);
    red += fread(hdr->date, 1, DATESIZE, file);
    red += fread(hdr->hdvers, 1, VERSIONSIZE, file);
    red += fread(hdr->prog, 1, PROGSIZE, file);
    red += fread(hdr->vers, 1, VERSIONSIZE, file);
    red += fread(hdr->progdate, 1, DATESIZE, file);
    red += FREAD_LONG(&hdr->ndrec, file, bytemode);
    red += FREAD_WORD(&hdr->tag, file, bytemode);
    red += FREAD_WORD(&hdr->pad2, file, bytemode);

    red += FREAD_LONG(&hdr->ndouble, file, bytemode);
    red += FREAD_LONG(&hdr->nfloat, file, bytemode);
    red += FREAD_LONG(&hdr->nlong, file, bytemode);
    red += FREAD_LONG(&hdr->nshort, file, bytemode);
    red += FREAD_LONG(&hdr->nchar, file, bytemode);

    red += FREAD_LONG(&hdr->fixsiz, file, bytemode);
    red += FREAD_LONG(&hdr->hsize, file, bytemode);
    red += fread(hdr->user, 1, USERSIZ, file);
    red += FREAD_WORD(&hdr->edr, file, bytemode);
    red += FREAD_WORD(&hdr->machine_code, file, bytemode);
    for(i = 0; i < NSPARES; ++i) {
	red += FREAD_WORD(&hdr->spares[i], file, bytemode);
    }

    return red;
}

typedef struct _espsparam_1 {
    short	type;			/* 0x0D = named numeric param */
    short	namelen;		/* 4-byte words in name field */
    char	name[4];		/* extensible name area */
} ESPSPARAM_1;
char  *espsp1fmt = "WW 4C";

/* .. and after the end of the name */
typedef struct _espsparam_2 {
    INT32	count;			/* how many numbers */
    short	type;			/* encoding; 0x0001 = double */
    char	data[4];		/* the actual data */
} ESPSPARAM_2;
char *espsp2fmt = "LW 4C";

#define MAXPNAMLEN 1024
#define MAXDATALEN 4096

/* tag types */
#define TAG_NUM_PARAM	0x0d
#define TAG_COMMENT	0x0b
#define TAG_END		0x00
#define TAG_DIR		0x0f
#define TAG_FNAME	0x01
#define TAG_BINARY	0x04
/* data types */
#define DAT_DOUBLE	0x01
#define DAT_FLOAT      	0x02
#define DAT_LONG	0x03
#define DAT_SHORT	0x04
#define DAT_CODED	0x07

typedef struct _code_table {
    struct _code_table *next;
    int val;
    char *tag;
} CODE_TABLE;

static int ParseCodedData(FILE *file, int count, int bytemode)
{   /* read a block of indexed string definitions, then a sequence 
       of data indexing into it.  Return the number of bytes read 
       if OK, -1 on error. */
    CODE_TABLE *ent, *root = NULL;
    CODE_TABLE **next = &root;
    int done = 0;
    int bytesread = 0;
    int i, j;
    short len, val;
    int entcount = 0;
    int _tmp;	/* for FREAD_LONG/WORD macros */

    while (!done) {
	bytesread += FREAD_WORD(&len, file, bytemode);
	if (len == 0) {
	    /* marks end of code definitions */
	    done = 1;
	} else {
	    if (!(len > 0 && len < 64)) {
		fprintf(stderr, "ParseCodedData: string len of %d (0x%lx) looks bad\n", 
			len, (unsigned long)len);
		abort();
	    }
	    ent = (CODE_TABLE *)malloc(sizeof(CODE_TABLE)+len);
	    /* space for string is tacked on to the end of this block */
	    ent->tag = (char *)(ent + 1);
	    for (i=0; i < len; ++i) {
		bytesread += FREAD_BYTE(&(ent->tag[i]), file, bytemode);
	    }
	    /* actually includes the terminator - no need to add */
	    ent->val = entcount++;
	    ent->next = NULL;
	    DBGFPRINTF((stderr, "ESPS PCoded: %d = '%s'\n", ent->val, ent->tag));
	    /* move on to next */
	    *next = ent;
	    next = &(ent->next);
	}
    }
    /* now read the coded data */
    for (i = 0; i < count; ++i) {
	bytesread += FREAD_WORD(&val, file, bytemode);
	if (val >= entcount) {
	    fprintf(stderr, "ParseCodedData: datum %d is outside tablesize %d\n", val, entcount);
	    abort();
	}
	ent = root;
	for (j = 0; j < val; ++j) {
	    ent = ent->next;
	}
	DBGFPRINTF((stderr, "%d=%s ", val, ent->tag));
	/* nothing to actually *do* with this data... */
    }
    DBGFPRINTF((stderr, "\n"));
    /* free the table we read */
    while (root != NULL) {
	ent = root->next;
	free(root);
	root = ent;
    }
    return bytesread;
}

static int ParseParamBlock(FILE *file, float *psrate, int bytemode) 
{   /* read through a block of ESPS params.  Return count of bytes. */
    ESPSPARAM_1	p1;
    ESPSPARAM_2 p2;
    char name[MAXPNAMLEN];
    /* char data[MAXDATALEN]; */
    /* allocate the data as doubles to ensure alignment (crashed on IRIX6.2) */
    double ddata[MAXDATALEN/sizeof(double)];
    char *data = (char *)ddata;
    int i, j, t, datasize;
    int done = 0;
    int count = 0;
    int bytesread = 0;
    int _tmp;	/* for FREAD_LONG/WORD macros */

    while (!done) {
	bytesread += FREAD_WORD(&p1.type, file, bytemode);
	bytesread += FREAD_WORD(&p1.namelen, file, bytemode);
	switch (p1.type) {
	case TAG_BINARY:
	    { 
		/* just consume it */
		int got = -1, thistime;
		/* Somewhere there's an example where p1.namelen is in 
		   bytes, not words, but I can't find it now.  For 
		   beep.sd (standard Rasta test file), it has to be 
		   in words. (That example was the one that made me 
		   define DAT_SHORT and increase MAXNAMELEN and MAXDATALEN) */
		int toget = p1.namelen * sizeof(INT32);
		/* int toget = p1.namelen; */
	    DBGFPRINTF((stderr, "ESPS PPB: TAG_BINARY (%d bytes)\n", toget));
		while (got && toget > 0) {
		    thistime = MIN(MAXDATALEN, toget);
		    got = fread(data, sizeof(char), thistime, file);
		    bytesread += got;
		    toget -= got;
		}
		assert(toget == 0); 	/* otherwise hit EOF */
	    }
	    break;
	case TAG_COMMENT:
	case TAG_NUM_PARAM:
	case TAG_DIR:
	case TAG_FNAME:
	    bytesread += fread(name, sizeof(char), 4*p1.namelen, file);
	    name[4*p1.namelen] = '\0';	/* terminator */
	    if (p1.type == TAG_COMMENT) {
		DBGFPRINTF((stderr, "ESPS PPB: TAG_COMMENT:\n%s\n", name));
	    } else if (p1.type == TAG_FNAME) {
		DBGFPRINTF((stderr, "ESPS PPB: TAG_FNAME: %s\n", name));
	    } else if (p1.type == TAG_DIR) {
		DBGFPRINTF((stderr, "ESPS PPB: TAG_DIR: %s\n", name));
	    } else {	/* TAG_NUM_PARAM - read the param value too */
		bytesread += FREAD_LONG(&p2.count, file, bytemode);
		bytesread += FREAD_WORD(&p2.type, file, bytemode);
		DBGFPRINTF((stderr, "ESPS PPB: TAG_NUM_PARAM '%s' (%d x type %d):\n", 
			name, p2.count, p2.type));
		switch (p2.type) {
		case DAT_DOUBLE:
		    datasize = 8;
		    break;
		case DAT_FLOAT:
		    datasize = 4;
		    break;
		case DAT_LONG:
		    datasize = 4;
		    break;
		case DAT_SHORT:
		    datasize = 2;
		    break;
		case DAT_CODED:
		    datasize = 0;	/* inhibit */
		    break;
		default:
		    fprintf(stderr, "ESPS ParseParamBlock: data type of %d is unknown\n", p2.type);
		    abort();
		}
		if(datasize*p2.count >= MAXDATALEN) {
		    fprintf(stderr, "sndfesps.c:ParseParamBlock: datasize=%d count=%d but MAXDATA=%d\n", datasize, p2.count, MAXDATALEN);
		}
		assert(datasize*p2.count < MAXDATALEN);
		bytesread += fread(data,sizeof(char), datasize*p2.count, file);
		switch (p2.type) {
		case DAT_DOUBLE:
		    for (i=0; i<p2.count; ++i) {
			/* byteswapping for double data? */
			if(bytemode != BM_INORDER) {
			    for(j = 0; j < datasize/2; ++j) {
				t = *(data+datasize*i+j);
				*(data+datasize*i+j) = *(data+datasize*(i+1)-j-1);
				*(data+datasize*(i+1)-j-1) = t;
			    }
			}
			DBGFPRINTF((stderr, "%g ", *(double *)(data+datasize*i)));
		    }
		    DBGFPRINTF((stderr, "\n"));
		    /* pick out the sample rate field */
		    if (strcmp(name, "record_freq") == 0 && psrate) {
			*psrate = *(double *)data;
		    }
		    break;
		case DAT_FLOAT:
		    for (i=0; i<p2.count; ++i) {
			/* byteswapping for float data? */
			if(bytemode != BM_INORDER) {
			    for(j = 0; j < datasize/2; ++j) {
				t = *(data+datasize*i+j);
				*(data+datasize*i+j) = *(data+datasize*(i+1)-j-1);
				*(data+datasize*(i+1)-j-1) = t;
			    }
			}
			DBGFPRINTF((stderr, "%f ", *(float *)(data+datasize*i)));
		    }
		    DBGFPRINTF((stderr, "\n"));
		    break;
		case DAT_LONG:
		    for (i=0; i<p2.count; ++i) {
			/* byteswapping for short */
			if(bytemode != BM_INORDER) {
			    for(j = 0; j < datasize/2; ++j) {
				t = *(data+datasize*i+j);
				*(data+datasize*i+j) = *(data+datasize*(i+1)-j-1);
				*(data+datasize*(i+1)-j-1) = t;
			    }
			}
			DBGFPRINTF((stderr, "%d ", *(long *)(data+datasize*i)));
		    }
		    DBGFPRINTF((stderr, "\n"));
		    break;
		case DAT_SHORT:
		    for (i=0; i<p2.count; ++i) {
			/* byteswapping for short */
			if(bytemode != BM_INORDER) {
			    for(j = 0; j < datasize/2; ++j) {
				t = *(data+datasize*i+j);
				*(data+datasize*i+j) = *(data+datasize*(i+1)-j-1);
				*(data+datasize*(i+1)-j-1) = t;
			    }
			}
			DBGFPRINTF((stderr, "%d ", *(short *)(data+datasize*i)));
		    }
		    DBGFPRINTF((stderr, "\n"));
		    break;
		case DAT_CODED:
		    /* first parse the encoding table, then read the data */
		    bytesread = ParseCodedData(file, p2.count, bytemode);
		    break;
		}
	    }
	    done = 0;
	    break;
	case TAG_END:
	    done = 1;
	    break;
	default: 
	    fprintf(stderr, "ESPS ParseParamBlock: unknown tag: %d\n", p1.type);
	    abort();
	}
	++count;
    }
    return bytesread;
}

/* This was definitely a cute idea, but it just doesn't work 
   because of the portability issues with struct alignment */
#ifdef NOT_USED
static void fixStructBytemode(void *data, char *format, int bytemode) 
{   /* go through a big structure modifying the bytemode */
    /* format = e.g. "LLW10B4W" for 2x longs, 1 16-bit word, 10 bytes, 4xwds */
    char *pos = data;
    char c;
    int i, count = 1;
    if (bytemode == BM_INORDER) {
	/* nothing to do */
	return;
    }
    while( c = *format++ != '\0') {
	if (c >= '0' && c <= '9') {
	    /* embedded count */
	    count = atoi(format - 1);
	    /* skip over it */
	    format += strspn(format, "0123456789");
	} else {
	    if ( c == 'L' || c == 'l') {
		for(i = 0; i < count; ++i) {
		    *((INT32 *)pos) = lshuffle(*((INT32 *)pos), bytemode);
		    pos += sizeof(INT32);
		}
	    } else if ( c == 'W' || c == 'w' || c == 'S' || c == 's') {
		for(i = 0; i < count; ++i) {
		    *((short *)pos) = lshuffle(*((short *)pos), bytemode);
		    pos += sizeof(short);
		}
	    } else if ( c == 'B' || c == 'b' || c == 'C' || c == 'c' ) {
		pos += count;
	    } else if ( c != ' ') { /* just ignore spaces, error on rest */
		fprintf(stderr, "fixStructBM: format '%s' unrecognized\n", 
			format-1);
		abort();
	    }
	    /* reset the count for next time */
	    count = 1;
	}
    }
}
#endif /* NOT_USED */

int SFReadHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
{   /*  sample the header of an opened file, at most infobmax info byts */
    int nhdread = 0;
    int  chans = 0;
    long samps = 0;
    float srate = 0;
    int	 format = -1;
    int  fcount = 0;
    long ft;
    int bytemode = GetBytemode(file);
    int seekable = TRUE;
    int skipmagic = 0;
    char str[32];
    long fsize = SF_UNK_LEN, nsamps;
    
    ESPSHDR hdr;

    if(bytemode == -1)	SetBytemode(file, bytemode = HostByteMode());
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
    nhdread = skipmagic;
    if(skipmagic) {
	/* insert a fake header */
	hdr.unk0 = lshuffle(4, bytemode);
    }
    /*nhdread += fread(((char* )&hdr)+skipmagic, 1, 
		     sizeof(ESPSHDR)-skipmagic, file); */
    /* fixup the bytemode */
    /*fixStructBytemode(&hdr, espshdrfmt, bytemode); */

    /* Do the structure alignment & byteswapping in a big routine */
    nhdread = fread_ESPSHDR(&hdr, file, bytemode, skipmagic);

    /* check the fields */
    if(hdr.check1 != HD_CHECK_VAL) {
	DBGFPRINTF((stderr, "ESPS RdHdr: check1=0x%lx, not 0x%lx\n", hdr.check1, HD_CHECK_VAL));
	return SFE_NSF;
    }
    if(hdr.unk0 != 0x0004) DBGFPRINTF((stderr, "unk0 is not 4 but %d\n", hdr.unk0));
    if(hdr.unk1 != 3000) DBGFPRINTF((stderr, "unk1 is not 3000 but %d\n",hdr.unk1));
    if(hdr.unk3 != 0x0002) DBGFPRINTF((stderr, "unk3 is not 2 but %d\n", hdr.unk3));
    if(hdr.type != 13) DBGFPRINTF((stderr, "type is not 13 but %d\n", hdr.type));
    
    if(hdr.ndouble) {
	format = SFMT_DOUBLE;	fcount = hdr.ndouble;
    }
    if(hdr.nfloat) {
	if(format != -1) {DBGFPRINTF((stderr, "ESPS: nfloat %d but format %d\n", hdr.nfloat, format));}
	format = SFMT_FLOAT; fcount = hdr.nfloat;
    }
    if(hdr.nlong) {
	if(format != -1) {DBGFPRINTF((stderr, "ESPS: nlong %d but format %d\n", hdr.nlong, format));}
	format = SFMT_LONG; fcount = hdr.nlong;
    }
    if(hdr.nshort) {
	if(format != -1) {DBGFPRINTF((stderr, "ESPS: nshort %d but format %d\n", hdr.nshort, format));}
	format = SFMT_SHORT; fcount = hdr.nshort;
    }
    if(hdr.nchar) {
	if(format != -1) {DBGFPRINTF((stderr, "ESPS: nchar %d but format %d\n", hdr.nchar, format));}
	format = SFMT_CHAR; fcount = hdr.nchar;
    }
    if(fcount != 1) {
	DBGFPRINTF((stderr, "ESPS: typecount not 1 but %d\n", fcount));
    }
    /* take the count as chans? */
    chans = fcount;
    DBGFPRINTF((stderr, "ESPS: at end of known bit, nhdread=%d, tell=%d\n", nhdread, ftell(file)));
    /* skip the next bit */
    nhdread += seek_by_read(file, 128);
    /* now should be in ascii bit */
    nhdread += fread(str, 1, 13, file);
    if(strcmp(str, "samples") != 0) {
	/* don't understand where we are */
	DBGFPRINTF((stderr, "ESPS: @0x140 (0x%lx) '%s' not 'samples'\n", ftell(file), str));
	return SFE_NSF;
    }
    /* otherwise, can parse the header block (& find the srate) */
    nhdread += ParseParamBlock(file, &srate, bytemode);

    /* line up to end of header */
    if(seekable) {
	fseek(file, hdr.header_bsize, SEEK_SET);
    } else {
	seek_by_read(file, hdr.header_bsize - nhdread);
    }
    /* now at start of data */

    /* set up fields */
    snd->magic = SFMAGIC;
    snd->headBsize = sizeof(SFSTRUCT);
    snd->format = format;
    snd->channels = chans;
    snd->samplingRate = srate;
    snd->dataBsize = SF_UNK_LEN;
    if(seekable) {
	fsize = GetFileSize(file);
	if(fsize > 0) {
	    nsamps = (fsize - hdr.header_bsize) 
		/ (chans * SFSampleSizeOf(format));		
	    snd->dataBsize = nsamps * chans * SFSampleSizeOf(format);
	}
    }
    snd->info[0] = '\0';
    /* record the indicated seek points for start and end of sound data */
    SetSeekEnds(file, hdr.header_bsize, fsize);

    return SFE_OK;
}

int SFRdXInfo(FILE *file, SFSTRUCT *snd, int done)
/* fetch any extra info bytes needed in view of read header
    Passes open file handle, existing sound struct
    already read 'done' bytes; rest inferred from structure */
{
    DBGFPRINTF((stderr, "No SFRdXInfo for ESPS (done %d)\n", done));
    return 0;
}

int SFWriteHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
/*  Utility to write out header to open file
    infoBmax optionally limits the number of info chars to write.
    infoBmax = 0 => write out all bytes implied by .headBsize field
    infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */
{
    fprintf(stderr, "ESPS SFWrHdr: no write for ESPS fmt (yet?) Sorry!\n");
    return(SFerror = SFE_WRERR);
}

static long SFHeDaLen(FILE *file) /* find header length for a file */
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

static long SFLastSampTell(FILE *file)	/* return last valid ftell for smps */
{
    long stt, end;

    GetSeekEnds(file, &stt, &end);
    return end;
}

static long FixupSamples(
    FILE  *file,
    void  *ptr,
    int   format,
    long  nitems,
    int   flag) 	/* READ_FLAG or WRITE_FLAG indicates direction */
{
    int bytemode;
    int wordsz;
    long bytes;

    wordsz = SFSampleSizeOf(format);
    bytes  = nitems * (long)wordsz;
/*  bytemode = HostByteMode();	/* relative to 68000 mode, which we want */
    bytemode = GetBytemode(file);
    if(bytemode != BM_INORDER) {    /* already OK? */
	/* otherwise need to fixup buffer .. */
	ConvertBufferBytes(ptr, bytes, wordsz, bytemode);
    }
    return nitems;
}

#undef FREAD_LONG
#undef FREAD_WORD
#undef FREAD_BYTE
