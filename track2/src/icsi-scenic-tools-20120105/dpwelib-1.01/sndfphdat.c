/***************************************************************\
*   sndfphdat.c				VerbMobil PHONDAT fmt   *
*   Extra functions for sndf.c					*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndf.c		*
*   2000-02-21 based on sndfesps.c				*
\***************************************************************/

/*
 * Changes 
 *
 */

#include <byteswap.h>
#include <assert.h>

/* exported file type indicator */
char *sndfExtn = ".pdt";		/* file extension, if used */

/* ----------------------- implementations ------------------------ */

typedef struct {
    INT32   not_used1[5],
	    nspbk,          /* # of data blocks (512 bytes) */
	    anz_header,     /* # of header blocks (512 bytes) */
	    not_used2[5];
    char    sprk[2];        /* speaker id                   */
    INT16   swdh;           /* session repetition           */
    INT32   ifld1[3],       /* ILS                          */
	    not_used3[6];
    char    kenn1[2];       /* ILS text characters 1 - 8    */
    INT16   not_used4;
    char    kenn2[2];
    INT16   not_used5;
    char    kenn3[2];
    INT16   not_used6;
    char    kenn4[2];
    INT16   not_used7;
    INT32   not_used8[35],
	    isf,            /* sampling rate in Hz          */
	    flagtype,       /* ILS: -32000 if sampling file
			       -29000 if param. file   */
	    flaginit;       /* ILS: 32149 if init           */
    char    ifl[32],        /* filename (terminated by /0)  */
	    day,            /* # of day                     */
	    month;          /* # of month                   */
    INT16   year;           /* # of year                    */
    char    sex,            /* sex of speaker               */
	    version;        /* header version:
			       0 = extended ILS
			       1 = phondat version 1
			       2 = phondat version 2        */
    INT16   adc_bits,       /* resolution of adc            */
	    words;          /* # of words in text           */
    INT32   not_useda[50];
    INT16   wdh,            /* # of repetition              */
	    abs_ampl;       /* maximum of amplitude         */
    char    not_usedb[10];
} PHDATHDR;

#define FREAD_LONG(plong, file, bytemode) 	\
     (_tmp = fread(plong, 1, sizeof(INT32), file), *plong = lshuffle(*plong, bytemode), _tmp)
#define FREAD_WORD(pword, file, bytemode) 	\
     (_tmp = fread(pword, 1, sizeof(short), file), *pword = wshuffle(*pword, bytemode), _tmp)
#define FREAD_BYTE(pbyte, file, bytemode) 	\
     fread(pbyte, 1, sizeof(char), file)

static int fread_PHDATHDR(PHDATHDR *hdr, FILE *file, int bytemode, int skip)
{   /* Read the fields of an ESPSHDR one by one, byteswapping as we go */
    int i,_tmp, red = 0;

    if(skip == 0) {
	red += FREAD_LONG(&hdr->not_used1[0], file, bytemode);
	if (red == 0) {
	    /* no more data (re-read on end of file?) */
	    return 0;
	}
    } else {
	red = skip;
    }
    assert(red == sizeof(INT32));

    for (i = 1; i < 5; ++i) {
	red += FREAD_LONG(&hdr->not_used1[i], file, bytemode);
    }
    red += FREAD_LONG(&hdr->nspbk, file, bytemode);
    red += FREAD_LONG(&hdr->anz_header, file, bytemode);
    for (i = 0; i < 5; ++i) {
	red += FREAD_LONG(&hdr->not_used2[i], file, bytemode);
    }
    red += FREAD_BYTE(&hdr->sprk[0], file, bytemode);
    red += FREAD_BYTE(&hdr->sprk[1], file, bytemode);
    red += FREAD_WORD(&hdr->swdh, file, bytemode);
    for (i = 0; i < 3; ++i) {
	red += FREAD_LONG(&hdr->ifld1[i], file, bytemode);
    }
    for (i = 0; i < 6; ++i) {
	red += FREAD_LONG(&hdr->not_used3[i], file, bytemode);
    }

    red += FREAD_BYTE(&hdr->kenn1[0], file, bytemode);
    red += FREAD_BYTE(&hdr->kenn1[1], file, bytemode);
    red += FREAD_WORD(&hdr->not_used4, file, bytemode);
    red += FREAD_BYTE(&hdr->kenn2[0], file, bytemode);
    red += FREAD_BYTE(&hdr->kenn2[1], file, bytemode);
    red += FREAD_WORD(&hdr->not_used5, file, bytemode);
    red += FREAD_BYTE(&hdr->kenn3[0], file, bytemode);
    red += FREAD_BYTE(&hdr->kenn3[1], file, bytemode);
    red += FREAD_WORD(&hdr->not_used6, file, bytemode);
    red += FREAD_BYTE(&hdr->kenn4[0], file, bytemode);
    red += FREAD_BYTE(&hdr->kenn4[1], file, bytemode);
    red += FREAD_WORD(&hdr->not_used7, file, bytemode);

    for (i = 0; i < 35; ++i) {
	red += FREAD_LONG(&hdr->not_used8[i], file, bytemode);
    }
    red += FREAD_LONG(&hdr->isf, file, bytemode);
    red += FREAD_LONG(&hdr->flagtype, file, bytemode);
    red += FREAD_LONG(&hdr->flaginit, file, bytemode);

    red += fread(&hdr->ifl[0], sizeof(char), 32, file);

    red += FREAD_BYTE(&hdr->day, file, bytemode);
    red += FREAD_BYTE(&hdr->month, file, bytemode);
    red += FREAD_WORD(&hdr->year, file, bytemode);
    red += FREAD_BYTE(&hdr->sex, file, bytemode);
    red += FREAD_BYTE(&hdr->version, file, bytemode);

    red += FREAD_WORD(&hdr->adc_bits, file, bytemode);
    red += FREAD_WORD(&hdr->words, file, bytemode);

    for (i = 0; i < 50; ++i) {
	red += FREAD_LONG(&hdr->not_useda[i], file, bytemode);
    }
    red += FREAD_WORD(&hdr->wdh, file, bytemode);
    red += FREAD_WORD(&hdr->abs_ampl, file, bytemode);

    red += fread(&hdr->not_usedb[0], sizeof(char), 10, file);

    return red;
}

/* size of blocks */
#define PHDAT_BLKSZ (512)

/* flag values */
#define ILS_FLAG_SAMPS	(-32000)
#define ILS_FLAG_PARAM	(-29000)
#define ILS_FLAG_INIT	(32149)

#define ILS_HVSN_EXT	(0)
#define ILS_HVSN_PHDAT1	(1)
#define ILS_HVSN_PHDAT2	(2)

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
    
    PHDATHDR hdr;

    if(bytemode == -1)	{
	/* files are always little-endian */
	if (HostByteMode() == BM_INORDER) {
	    bytemode = BM_BYWDREV;
	} else {
	    /* little-endian machine, no need to swap */
	    bytemode = BM_INORDER;
	}
	SetBytemode(file, bytemode);
    }

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

    /* Do the structure alignment & byteswapping in a big routine */
    nhdread = fread_PHDATHDR(&hdr, file, bytemode, skipmagic);
    if (nhdread == 0) {
	return(SFerror = SFE_NSF);
    }

    /* check the fields */
#ifdef DEBUG
    fprintf(stderr, "PDAT: read %d header bytes\n", nhdread);
    fprintf(stderr, "PDAT:       nspk=%d\n", hdr.nspbk);
    fprintf(stderr, "PDAT: anz_header=%d\n", hdr.anz_header);
    fprintf(stderr, "PDAT:       sprk=%c%c\n", hdr.sprk[0], hdr.sprk[1]);
    fprintf(stderr, "PDAT:       swdh=%d\n", hdr.swdh);
    fprintf(stderr, "PDAT:      ifld1=0x%lx 0x%lx 0x%lx\n", hdr.ifld1[0], hdr.ifld1[1], hdr.ifld1[2]);
    fprintf(stderr, "PDAT:       kenn=%02x %02x %02x %02x %02x %02x %02x %02x \n", hdr.kenn1[0], hdr.kenn1[1], hdr.kenn2[0], hdr.kenn2[1], hdr.kenn3[0], hdr.kenn3[1], hdr.kenn4[0], hdr.kenn4[1]);
    fprintf(stderr, "PDAT:        isf=%d\n", hdr.isf);
    fprintf(stderr, "PDAT:   flagtype=%d\n", hdr.flagtype);
    fprintf(stderr, "PDAT:   flaginit=%d\n", hdr.flaginit);
    fprintf(stderr, "PDAT:        ifl='%s'\n", hdr.ifl);
    fprintf(stderr, "PDAT:       date=%d-%02d-%02d\n", hdr.year, hdr.month, hdr.day);
    fprintf(stderr, "PDAT:        sex=%c\n", hdr.sex);
    fprintf(stderr, "PDAT:    version=%d\n", hdr.version);
    fprintf(stderr, "PDAT:   adc_bits=%d\n", hdr.adc_bits);
    fprintf(stderr, "PDAT:      words=%d\n", hdr.words);
    fprintf(stderr, "PDAT:        wdh=%d\n", hdr.wdh);
    fprintf(stderr, "PDAT:   abs_ampl=%d\n", hdr.abs_ampl);
#endif /* DEBUG */

    /* Check that this really looks like the header we're expecting */
    if (hdr.flagtype != ILS_FLAG_SAMPS) {
	fprintf(stderr, "sndfphdat: RdHdr: flagtype is %d not %d\n", 
		hdr.flagtype, ILS_FLAG_SAMPS);
	return (SFerror = SFE_NSF);
    }
    if (hdr.version != ILS_HVSN_PHDAT1) {
	fprintf(stderr, "sndfphdat: RdHdr: version is %d not %d\n", 
		hdr.version, ILS_HVSN_PHDAT1);
	return (SFerror = SFE_NSF);
    }

    format = SFMT_SHORT;
    chans  = 1;
    /* now at start of data */

    /* set up fields */
    snd->magic = SFMAGIC;
    snd->headBsize = sizeof(SFSTRUCT);
    snd->format = format;
    snd->channels = chans;
    snd->samplingRate = hdr.isf;
    snd->dataBsize = hdr.nspbk*PHDAT_BLKSZ;

    if(seekable) {
	fsize = GetFileSize(file);
	if(fsize > 0) {
	    long dataBsize;
	    nsamps = (fsize - hdr.anz_header*PHDAT_BLKSZ) 
		/ (chans * SFSampleSizeOf(format));		
	    dataBsize = nsamps * chans * SFSampleSizeOf(format);
	    if (dataBsize != snd->dataBsize) {
		fprintf(stderr, "sndfphdat: RdHdr: Warning: dataBsize is %ld in header, %ld from file size\n", snd->dataBsize, dataBsize);
		/* shorter dominates, unless header says zero */
		if (snd->dataBsize == 0 || dataBsize < snd->dataBsize) {
		    snd->dataBsize = dataBsize;
		}
	    }
	}
    }
    snd->info[0] = '\0';
    /* record the indicated seek points for start and end of sound data */
    SetSeekEnds(file, hdr.anz_header*PHDAT_BLKSZ, fsize);

    return SFE_OK;
}

int SFRdXInfo(FILE *file, SFSTRUCT *snd, int done)
/* fetch any extra info bytes needed in view of read header
    Passes open file handle, existing sound struct
    already read 'done' bytes; rest inferred from structure */
{
    DBGFPRINTF((stderr, "No SFRdXInfo for PHDAT (done %d)\n", done));
    return 0;
}

int SFWriteHdr(FILE *file, SFSTRUCT *snd, int infoBmax)
/*  Utility to write out header to open file
    infoBmax optionally limits the number of info chars to write.
    infoBmax = 0 => write out all bytes implied by .headBsize field
    infoBmax >= SFDFLTBYS => write out only infoBmax infobytes */
{
    fprintf(stderr, "PHDAT SFWrHdr: no write for PHDAT fmt (yet?) Sorry!\n");
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
