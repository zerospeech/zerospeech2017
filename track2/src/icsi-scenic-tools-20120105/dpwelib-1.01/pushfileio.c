/*
 * pushfileio.c
 * 
 * Layer on top of fopen(3S) etc
 * to permit - pushing of arbitrary-sized blocks back onto input files
 *           - 'virtual' file repositioning (for multiple files on 
 *             on a single stream)
 *
 * necessitated to support Abbot soundfile streams for sndf(3).
 * 
 * 1997sep10 dpwe@icsi.berkeley.edu
 * $Header: /u/drspeech/repos/dpwelib/pushfileio.c,v 1.10 2011/03/09 01:35:02 davidj Exp $
 */

#include "dpwelib.h"
#include "pushfileio.h"


/* ------- my_finfo block ---------------- 1997sep01 dpwe@icsi.berkeley.edu */
/* Covers for fread &c to simulate multiple files in a stream */
typedef struct _myfinfo {
    struct _myfinfo* next;	/* pointer to next in linked list */
    FILE *file;			/* the file we relate to */
    char *pushbuf;		/* buffer of data pushed back (or NULL) */
    int  pushlen;		/* total size of data pushed back */
    int  pushpos;		/* how much of that pushed back data is read */
    int  seekable;		/* is this file seekable? */
    int  pos;			/* where do we think we are? */
    int  posbase;		/* finfo->pos = ftell() - finfo->posbase */
    int  len;			/* where do we think the end is? */
    int  bypass;		/* hand off routines to plain file if poss */
} FINFO;

static FINFO *finfoRoot = NULL;

static void finfoPrint(FINFO *finfo, FILE *stream, char *tag)
{   /* detail finfo fields */
    if (tag==NULL)	tag = "?";
    fprintf(stream, "finfo '%s' @0x%lx:\n", tag, (unsigned long)finfo);
    fprintf(stream, "  pos=%d len=%d seekable=%d\n", finfo->pos, finfo->len, finfo->seekable);
    fprintf(stream, "  pushbuf=0x%lx pushlen=%d pushpos=%d\n", (unsigned long)finfo->pushbuf, finfo->pushlen, finfo->pushpos);
    fprintf(stream, "  posbase=%d bypass=%d\n", finfo->posbase, finfo->bypass);
}

static void finfoPrintAll(FILE *stream)
{
    FINFO *finfo = finfoRoot;
    fprintf(stream, "----finfoPrintAll:\n");
    while (finfo) {
	finfoPrint(finfo, stream, NULL);
	finfo = finfo->next;
    }
    fprintf(stream, "----done\n");
}

static void finfoSetFile(FINFO *finfo, FILE *file)
{   /* setup the seek/tell structures for the named file */
    finfo->file = file;
    finfo->posbase = 0;
    if (file == NULL) {
	finfo->seekable = 0;
	finfo->pos = 0;
	finfo->len = 0;
    } else {
	int rc = -999;
	finfo->pos = ftell(file);
	if (finfo->pos < 0 || (rc = fseek(file, 0, SEEK_END)) != 0) {
	    if (finfo->pos < 0) 	finfo->pos = 0;
	    finfo->seekable = 0;
	    finfo->len = -1;		/* we don't know */
	} else {
	    finfo->seekable = 1;
	    finfo->len = ftell(file);
	    /* move back to where we were */
	    rc = fseek(file, finfo->pos, SEEK_SET);
	}
	DBGFPRINTF((stderr, "finfoSetFile(0x%lx,0x%lx) ftell=%d fseek=%d len=%d sk=%d\n", finfo, file, finfo->pos, rc, finfo->len, finfo->seekable));
    }
}

static int finfoSetPos(FINFO *finfo, int pos)
{   /* force the current file pos to be regarded as 'pos'.
       Return the accumulated skew between pos and ftell(). */
    /* .. which means the skew between true and reported must shift.. */
    int step = finfo->pos - pos;
    finfo->posbase += step;
    /* and just modify our internal record */
    finfo->pos = pos;
    /* if we knew how long this file was, change the answer */
    if (finfo->len != -1) {
	finfo->len -= step;
    }
    /* setting the pos means we should definitely not bypass */
    finfo->bypass = 0;
    /* return the total skew */
    return finfo->posbase;
}

static FINFO *finfoAlloc(FILE *file)
{   /* allocate a new finfo block for the specified file */
    FINFO *finfo = (FINFO *)malloc(sizeof(FINFO));
    /* init fields */
    finfo->pushbuf = NULL;
    finfo->pushlen = 0;
    finfo->pushpos = 0;
    /* setup stuff related to the actual file */
    finfoSetFile(finfo, file);

    /* stitch into list */
    finfo->next = finfoRoot;
    finfoRoot = finfo;

    /* start off in bypass mode? */
    finfo->bypass = 1;

    DBGFPRINTF((stderr, "finfoAlloc@0x%lx\n", finfo));
    return finfo;
}

static FINFO *finfoFind(FILE *file, int alloc)
{   /* find the finfo for the specified file. If not found & alloc, make it */
    FINFO **runner = &finfoRoot;

    while ( (*runner) ) {
	if ( (*runner)->file == file) {
	    return *runner;
	}
	runner = &((*runner)->next);
    }
    /* didn't find it, pointing to final null */
    if (alloc) {
	return(finfoAlloc(file));
    } else {
	return NULL;
    }
}

static void finfoFreePush(FINFO *finfo, int warn)
{   /* release any existing pushbuffer in the finfo.
       Print a warning message if one exists & warn!=0 */
    if (finfo->pushbuf != NULL) {
	if (warn  && finfo->pushpos != finfo->pushlen) {
	    fprintf(stderr, "finfoFreePush: discarding %d of %d pushbuf\n", 
		    finfo->pushlen - finfo->pushpos, finfo->pushlen);
	}
	free(finfo->pushbuf);
	finfo->pushbuf = NULL;
    }
    finfo->pushlen = 0;
    finfo->pushpos = 0;
}

static void finfoFree(FINFO *finfo)
{   /* free the struct & remove from list */
    FINFO **runner = &finfoRoot;

    DBGFPRINTF((stderr, "finfoFree@0x%lx\n", finfo));
    while ( (*runner) && (*runner) != finfo) {
	runner = &((*runner)->next);
    }
    if ( !(*runner) ) {
	/* got to end of list without finding it */
	fprintf(stderr, "finfoFree: 0x%lx not in list???\n", (unsigned long)finfo);
	/* but continue.. */
    } else {
	/* close up list */
	*runner = finfo->next;
    }
    finfoFreePush(finfo, 0);
    free(finfo);
}

int my_fread(void *buf, size_t size, size_t nitems, FILE *file)
{   /* cover for fread uses the FINFO structure to do push back */
    FINFO *finfo = finfoFind(file, 1);
    int reqbytes = size*nitems;
    int gotbytes = 0;

    DBGFPRINTF((stderr, "my_fread(%dx%d@%d)@0x%lx (%d@0x%lx)\n", size, nitems, 
		finfo->pos, finfo, ftell(file), file));

    if (finfo->bypass) {
	int gotitems = fread(buf, size, nitems, file);
	finfo->pos += gotitems * size;
	return gotitems;
    }

    if (finfo->pushbuf) {
	int tocopy = MIN(finfo->pushlen - finfo->pushpos, reqbytes);
	memcpy(buf, finfo->pushbuf + finfo->pushpos, tocopy);
	finfo->pushpos += tocopy;
	if (finfo->pushpos == finfo->pushlen) {
	    /* used up all the push buffer - clear it */
	    finfoFreePush(finfo, 0);
	}
	gotbytes = tocopy;
    }
    /* use fread for the remainder */
    /* do it multiple times to absorb short reads across network */
  /* gotbytes += fread(((char *)buf)+gotbytes, 1, reqbytes-gotbytes, file); */
    {
	int want = reqbytes - gotbytes;
	int got  = 0;
	int thistime = 0;

        do { 
	    thistime = fread(((char *)buf)+gotbytes+got, 1, 
		             want - got, file);
	    want -= thistime;
	    got += thistime;
	    /*	    if (want > 0) {fprintf(stderr, "my_fread: got %d leaving %d bytes\n", 
				   got, want); } */
	    /* Don't report this error, since we always try to read off the 
	       end of soundfiles, in case there are multiple soundfiles 
	       in a single stream (to generalize abbot audio streams) */
	} while (want > 0 && thistime > 0);

	gotbytes += got;
    }
    finfo->pos += gotbytes;

    DBGFPRINTF((stderr, "my_fread()@0x%lx=(%d/%d)\n", finfo, gotbytes, size));
    return(gotbytes/size);
}

int my_fwrite(void *buf, size_t size, size_t nitems, FILE *file)
{   /* just plain write */
    FINFO *finfo = finfoFind(file, 1);
    int ngot = fwrite(buf, size, nitems, file);

    DBGFPRINTF((stderr, "my_fwrite(%dx%d@%d)@0x%lx\n", size, nitems, 
		finfo->pos, finfo));
    finfo->pos += ngot * size;
    if (finfo->pos > finfo->len)	finfo->len = finfo->pos;
    return ngot;
}

int my_fpush(void *buf, size_t size, size_t nitems, FILE *file)
{   /* push some data back onto a file so it will be read next;
       return the new pos of the file. */
    FINFO *finfo = finfoFind(file, 1);
    int totbytes = size*nitems;

    DBGFPRINTF((stderr, "my_fpush(%dx%d@%d)@0x%lx\n", size, nitems, 
		finfo->pos, finfo));
    /* If we're currently already reading from pushed data, 
       attempt to write this push back into where we're reading 
       from. */
    if (finfo->pushbuf) {
	if (finfo->pushpos >= totbytes) {
	    if (memcmp(finfo->pushbuf + finfo->pushpos - totbytes, buf, 
		       totbytes) != 0) {
		fprintf(stderr, "my_fpush: re-pushing different %d bytes at %d in %d\n", totbytes, finfo->pushpos, finfo->pushlen);
		memcpy(finfo->pushbuf + finfo->pushpos - totbytes, buf, 
		       totbytes);
	    }
	    DBGFPRINTF((stderr, "my_fpush: restored %d bytes at %d into push of %d\n", totbytes, finfo->pushpos, finfo->pushlen));
	    /* wind back again */
	    finfo->pushpos -= totbytes;
	} else {
	    /* pushing extra data back */
	    int extra = finfo->pushlen - finfo->pushpos;
	    char *newbuf = malloc(totbytes + extra);
	    memcpy(newbuf, buf, totbytes);
	    memcpy(newbuf+totbytes, finfo->pushbuf+finfo->pushpos, extra);
	    finfoFreePush(finfo, 0);
	    finfo->pushbuf = buf;
	    DBGFPRINTF((stderr, "my_fpush: extended push of %d to %d from %d\n", finfo->pushlen, totbytes+extra, finfo->pushpos));
	    finfo->pushlen = totbytes + extra;
	    finfo->pushpos = 0;
	}
    } else {
	/* dispose any existing pushbuf (but there can't be!) */
	finfoFreePush(finfo, 1);
	/* build a new one */
	finfo->pushbuf = malloc(totbytes);
	memcpy(finfo->pushbuf, buf, totbytes);
	finfo->pushlen = totbytes;
	finfo->pushpos = 0;
	DBGFPRINTF((stderr, "my_fpush: new push of %d\n", finfo->pushlen));
    }
    /* note that the effective pos has moved backwards */
    finfo->pos -= totbytes;

    /* disable bypass after a push */
    finfo->bypass = 0;

    return finfo->pos;
}

void my_fsetpos(FILE *file, long pos)
{   /* force the pos to be read as stated */
    FINFO *finfo = finfoFind(file, 1);    
    DBGFPRINTF((stderr, "my_fsetpos(%d->%d)@0x%lx\n", finfo->pos, pos, finfo));
    finfoSetPos(finfo, pos);
}

FILE *my_fopen(char *path, char *mode)
{
    FILE *rslt = fopen(path, mode);
    if (rslt) {
	/* make sure the finfo is created */
	FINFO *finfo = finfoFind(rslt, 1);
	DBGFPRINTF((stderr, "my_fopen(%s,%s)@0x%lx\n", path, mode, finfo));
    }
    return rslt;
}

#ifdef HAVE_POPEN
FILE *my_popen(const char *path, const char *mode)
{
    FILE *rslt = popen(path, mode);
    if (rslt) {
	/* make sure the finfo is created */
	FINFO *finfo = finfoFind(rslt, 1);
	DBGFPRINTF((stderr, "my_popen(%s,%s)@0x%lx\n", path, mode, finfo));
    }
    return rslt;
}

int my_pclose(FILE *file)
{
    FINFO *finfo = finfoFind(file, 0);
    if (!finfo) {
	fprintf(stderr, "my_pclose: no finfo found\n");
    } else {
	finfoFree(finfo);
    }
    DBGFPRINTF((stderr, "my_pclose(0x%lx)@0x%lx\n", file, finfo));
    return pclose(file);
}

#endif /* HAVE_POPEN */

void my_fclose(FILE *file)
{
    FINFO *finfo = finfoFind(file, 0);
    if (!finfo) {
	fprintf(stderr, "my_fclose: no finfo found\n");
    } else {
	finfoFree(finfo);
    }
    DBGFPRINTF((stderr, "my_fclose(0x%lx)@0x%lx\n", file, finfo));
    fclose(file);
}

int my_fseek(FILE *file, long pos, int mode)
{   /* do fseek, return 0 if OK or -1 if can't seek */
    FINFO *finfo = finfoFind(file, 1);
    int relpos;
    int handled = 0;

    DBGFPRINTF((stderr, "my_fseek(%ld,%d(%d))@0x%lx\n", pos, mode, finfo->bypass, finfo));

    if (!finfo->seekable) {finfo->bypass = 0;}

    if (finfo->bypass) {
	int rc = fseek(file, pos, mode);
	DBGFPRINTF((stderr, "my_fseek_bypass() fseek(x,%d,%d)\n", pos,mode));
	if (rc >= 0) {
	    finfo->pos = ftell(file);
	}
	/* maybe clear seekable on seek error? but could be bad args */
	return rc;
    }

    if (mode == SEEK_CUR) {
	pos = finfo->pos + pos;
    } else if (mode == SEEK_END) {
	if (finfo->len < 0) {   /* not a seekable stream, so lose */
	    return -1;
	}
	pos = finfo->len + pos;
    }
    /* pos is the sought absolute pos; how does that relate to where 
       we are? */
    relpos = pos - finfo->pos;

    if(relpos == 0)	return 0;

    /* now, does this seek relate to pushed data ? */
    if (finfo->pushbuf) {
	int pushrel = finfo->pushpos + relpos;
	if (pushrel >= 0 && pushrel < finfo->pushlen) {
	    /* seek to within the pushed stuff */
	    finfo->pushpos += relpos;
	    handled = 1;
	} else {
	    /* seek to outside of pushed area.  Absorb what we 
	       can, adjust the params, and continue */
	    /* without the push, we'd be at the *end* of the push, so 
	       pretend to move there */
	    int rempushpts = finfo->pushlen - finfo->pushpos;
	    finfo->pos += rempushpts;
	    relpos -= rempushpts;
	    /* discard the push since we've left it */
	    finfoFreePush(finfo, 0);
	    /* will continue to try to resolve since handled == 0 */
	}
    }
    if (!handled) {     /* still have to do something */
	if (!finfo->seekable) {
	    if (relpos < 0) {
		/* can't move backwards on non-seekable stream */
		return -1;
	    } else {
		seek_by_read(file, relpos);
		/* seek_by_read adjusts finfo->pos, but we'll do it anyway */
	    }
	} else {
	    /* fully seekable stream */
	    assert(fseek(file, finfo->posbase+pos, SEEK_SET) == 0);
	    DBGFPRINTF((stderr, "my_fseek_skbl() fseek(x,%d,%d) (pos=%d)\n", 
			finfo->posbase+pos, SEEK_SET, ftell(file)));
	}
    }
    finfo->pos = pos;
    /* indicate seek was OK */
    return 0;
}

long my_ftell(FILE *file)
{
    FINFO *finfo = finfoFind(file, 0);
    long pos;
    DBGFPRINTF((stderr, "my_ftell()@0x%lx=%d (really %d)\n", finfo, finfo?finfo->pos:-999, -999));  /* ftell(file) - makes a diff'ce to linux read streams ??? */
    if (!finfo || finfo->bypass) {
	pos = ftell(file);
	if(finfo) {
	    if (pos >= 0) {
		finfo->pos = pos;
	    } else {
		/* got -1 from ftell - stream not seekable */
		finfo->seekable = 0;
	    }
	}
    } else {
	/* not in bypass - just give our pos */
	pos = finfo->pos;
    }
    return pos;
}

int my_feof(FILE *file)
{   /* it's not EOF if we still have stuff in the pushbuf */
  int rc;
  FINFO *finfo = finfoFind(file,0);
  if (finfo && finfo->bypass==0 && finfo->pushbuf) {
    /* existing pushbuf so we can't be at EOF */
    rc = 0;
  } else {
    rc = feof(file);
  }
  /*  fprintf(stderr, "my_feof(0x%lx): fi=0x%lx pb=0x%lx l=%d p=%d rc=%d\n",
	  file, finfo, finfo?finfo->pushbuf:0, finfo->pushlen, finfo->pushpos, rc); */
  return rc;
}
    

int my_fputs(char *s, FILE *file)
{   /* write a regular string to the stream */
    FINFO *finfo = finfoFind(file, 1);
    int len = fputs(s, file);
    finfo->pos += len;
    if (finfo->pos > finfo->len)	finfo->len = finfo->pos;
    return len;
}

char *my_fgets(char *s, int n, FILE *stream)
{   /* read a string from the stream */
    FINFO *finfo = finfoFind(stream, 1);
    char *r = fgets(s, n, stream);
    if(r)
	finfo->pos += strlen(r);
    return r;
}

/* ----- END of my_finfo pushable file block ---------- */

#define TBSIZE 256	/* temp buffer size for seek_by_read */

long seek_by_read(FILE *file, long bytes)
{   /* skip over bytes for files not supporting seek by repeated read */
  char tmpbuf[TBSIZE];
  long thistime, rem, got, tot = 0;

  DBGFPRINTF((stderr, "seek_by_read: %ld bytes from %ld..", bytes, my_ftell(file)));
  if(bytes < 0)  {
    fprintf(stderr, "seek_by_read: cannot go back %ld bytes\n", -bytes);
  }
  rem = bytes;
  got = -1;
  while(rem && got != 0) {
    thistime = MIN(rem, TBSIZE);
    got = my_fread(tmpbuf, 1, thistime, file);
    DBGFPRINTF((stderr, " %ld", got));
    tot += got;
    rem -= thistime;
  }
  DBGFPRINTF((stderr, " to %ld", my_ftell(file)));

  return tot;
}

long GetFileSize(FILE *file)
{    /* return number of bytes in a file, or -1 if couldn't tell */
    long  fpos, flen = -1;
    FINFO *finfo = finfoFind(file, 0);

    if (finfo) {
	flen = finfo->len;
    } else {
	fpos = ftell(file);
	DBGFPRINTF((stderr, "GeFileSize(no finfo) fseek(x,%d,%d)\n", 
		    0, SEEK_END));
	if(fseek(file, 0L, SEEK_END)!= -1 && (flen = ftell(file))>0 ) {
	    fseek(file, fpos, SEEK_SET);	/* back where we started */
	    DBGFPRINTF((stderr, "GeFileSize(no finfo) fseek back(x,%d,%d)\n", 
			fpos, SEEK_SET));
	} else {
	    flen = -1;		/* couldn't establish length */
	}
    }

    DBGFPRINTF((stderr, "GetFileSize(0x%lx,finf=0x%lx)=%d\n", file, finfo, flen));

    return flen;
    }

