/***************************************************************\
*   sndffname.c			MACINTOSH			*
*   functions to keep a table of filenames by filehandles	*
*   THIS FILE IS CONDITIONALLY INCLUDED IN sndfmac.c etc	*
*   12jun91 dpwe						*
\***************************************************************/

#ifndef THINK_C
typedef char Str255[256];
#endif /* !THINK_C */

#ifndef SNDF_FTYPE_DEFINED
#define SNDF_FTYPE void*
#define SNDF_FTYPE_DEFINED
#endif /* SNDF_FTYPE */

/* private type declarations */
typedef struct fnameinfo {
    struct fnameinfo *next;
    FILE   *fhan;
    Str255 fname;
    long   sndstart;	/* absolute seek points of start and end of sound */
    long   sndend;	/*   .. to allow faking of eof in AIFFs */
    int	   bytemode;	/* since it isn't in the header.. ugh! (for SPHERE) */
    int    miscflags;	/* bitmap of general purpose stuff */
    SNDF_FTYPE *sfftype;	/* filetype definition to use (1997jan29) */
    long   filelen;	/* total bytes in file inc. all headers etc */
} FNAMEINFO;	/* used to build a map between FILE * & filenames */

/* values for miscflags */
#define SFFLAG_ABBOT 1	/* abbot-style output - clip to 0x8001, write 0x8000
			   at end */

/* static variables */
static FNAMEINFO   *fniroot = NULL;	/* root of fnameinfo list */

/* ----------------------- implementations ------------------------ */

static FNAMEINFO *find_node(FILE *key, int shouldcreate)
{   /* find the info node, or create it if permitted */
    FNAMEINFO *node;
    FNAMEINFO **prunner = &fniroot;

    while((*prunner) != NULL && (*prunner)->fhan != key)
	prunner = &(*prunner)->next;
    if((*prunner) != NULL) {	/* found : return val */
	return *prunner;
    } else {	/* else not found */
	if (!shouldcreate) {
	    return NULL;
	} else {	/* We *should* create, so do it */
	    node = (FNAMEINFO *)malloc(sizeof(FNAMEINFO));
	    node->fhan = key;
	    node->next = fniroot;
	    node->fname[0] = '\0';
	    node->sndstart = 0L;
	    node->sndend   = 0L;
	    node->bytemode = -1;	/* invalid byte mode */
	    node->miscflags = 0;	/* no flags set */
	    node->filelen = SF_UNK_LEN;
	    node->sfftype = NULL;
	    fniroot = node;	/* add to top of list, faster to find */
	    return node;
	}
    }
}

static int remove_node(FILE *key)
{   /* remove the node corresponding to this key.  Return 0 if not found */
    FNAMEINFO *node;
    FNAMEINFO **prunner = &fniroot;

    while((*prunner) != NULL && (*prunner)->fhan != key)
	prunner = &(*prunner)->next;
    if((*prunner) != NULL) {
	/* record doomed node */
	node = *prunner;
	/* close up gap */
	*prunner = node->next;
	/* delete the node */
	free(node);
	return 1;
    } else {	/* else not found */
	return 0;
    }
}

static void AddFname(char *fname, FILE *file)
{   /* associates name 'fname' with file pointer 'file' in our lookup table */
    /* find existing node, or create new one */
    FNAMEINFO *node = find_node(file, 1);
    /* either way, store the new name */
    strcpy((char *)node->fname, fname);
}

static void RmvFname(FILE *file)
{   /* removes any fnameinfo record associated with 'file' */
    int rc = remove_node(file);
    DBGFPRINTF((stderr, "RmvFname(0x%lx)=%d\n", file, rc));
}

static char *FindFname(FILE *file)
{   /* looks up file ptr, returns mapped name if any */
    FNAMEINFO *node = find_node(file, 0);

    if(node != NULL)
	return( (char *)node->fname );
    else
	return( NULL );     /* fileptr not in our list */
}

static void SetSeekEnds(FILE *file, long start, long end)
{   /* register start and end indexes (create new record if needed) */
    FNAMEINFO *node = find_node(file, 1);

    node->sndstart = start;
    node->sndend   = end;
}
    
static void GetSeekEnds(FILE *file, long *pstart, long *pend)
{   /* retrieve start and end indexes */
    FNAMEINFO *node = find_node(file, 0);

    if(node != NULL) {	/* found : set vals */
	*pstart = node->sndstart;
	*pend   = node->sndend;
    } else {	/* else not found - error */
	fprintf(stderr, "sndffname:GetSeekEnds: cannot file file %ld\n",
		(long)file);
	abort();
    }
}

static void SetBytemode(FILE *file, int mode)
{   /* register mode spec for this file */
    FNAMEINFO *node = find_node(file, 1);

    node->bytemode = mode;
    DBGFPRINTF((stderr, "SetBytemode FILE=0x%lx, mode=%d\n", file, mode));
}
    
static int GetBytemode(FILE *file)
{   /* find the bytemode associated with this file */
    FNAMEINFO *node = find_node(file, 0);
    int mode;

    if(node != NULL) {	/* found : return val */
	mode = node->bytemode;
    } else {
	mode = -1;
    }
    DBGFPRINTF((stderr, "GetBytemode FILE=0x%lx, mode=%d\n", file, mode));
    return mode;
}

static void SetFlags(FILE *file, int flags)
{   /* register flags for this file */
    FNAMEINFO *node = find_node(file, 1);

    node->miscflags = flags;
    DBGFPRINTF((stderr, "SetFlags FILE=0x%lx, flags=%d\n", file, flags));
}
    
static int GetFlags(FILE *file)
{   /* find the flags associated with this file */
    FNAMEINFO *node = find_node(file, 0);
    int flags;

    if(node != NULL) {	/* found : return val */
	flags = node->miscflags;
    } else {
	flags = 0;
    }
    DBGFPRINTF((stderr, "GetFlags FILE=0x%lx, flags=%d\n", file, flags));
    return flags;
}

static void SetSFFType(FILE *file, SNDF_FTYPE* sfft)
{
    FNAMEINFO *node = find_node(file, 1);
    node->sfftype = sfft;
    DBGFPRINTF((stderr, "SetSFFType FILE=0x%lx, type=%s\n", file, sfft->name));
}

static SNDF_FTYPE *GetSFFType(FILE *file)
{
    SNDF_FTYPE *type = NULL;

    FNAMEINFO *node = find_node(file, 0);
    if(node != NULL) {
	type = node->sfftype;
    }
    DBGFPRINTF((stderr, "GetSFFType FILE=0x%lx, type=%s\n", file, type?type->name:"(null)"));
    return type;
}

static void SetFileLen(FILE *file, long len)
{
    FNAMEINFO *node = find_node(file, 1);
    node->filelen = len;
    DBGFPRINTF((stderr, "SetFileLen FILE=0x%lx, len=%d\n", file, len));
}

static long GetFileLen(FILE *file)
{
    long len = SF_UNK_LEN;

    FNAMEINFO *node = find_node(file, 0);
    if(node != NULL) {
	len = node->filelen;
    }
    DBGFPRINTF((stderr, "GetFileLen FILE=0x%lx, len=%d\n", file, len));
    return len;
}

