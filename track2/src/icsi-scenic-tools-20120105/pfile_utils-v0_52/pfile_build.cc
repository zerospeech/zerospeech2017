/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_build.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $

    This program builds a pfile containing raw features
    features given a list of sentence ids to include. It also uses
    labels from the label file given as an argument.

    Assumes that features are stored in <sentence-id>.<suffix> file in
    current directory.
  
    Code taken from code originally 
    written by Krste Asanovic <krste@cs.berkeley.edu>.
    Modified to work generally by
    Jeff Bilmes
    <bilmes@cs.berkeley.edu>

*/

#include "pfile_utils.h"
#include "QN_PFile.h"
#include "error.h"

#ifndef HAVE_BOOL
enum bool { false = 0, true = 1 };
#endif

/* Patch broken header files. */

#ifndef SEEK_SET
#define SEEK_SET (0)
#endif

#ifndef SEEK_END
#define SEEK_END (2)
#endif

#define N_FEATURES (2)
#define N_LABELS (1)

static const char* program_name;


static void
usage(const char* message = 0)
{
    if (message)
        fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
"Where <options> include:\n"
"-help           print this message.\n"
"-sents <fname>  Name of file holding sentence ids to include.\n"
"-suffix suf     suffix of the files. Names are <name>.<suf>, forall <name> in file <fname>.\n"
"-nfeats <n>     number of features per frame in each speech utternace.\n"
"-labels <fname> name of file holding frame labels to use.\n"
"-o <fname>      name of output pfile.\n"
"-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );

    exit(EXIT_FAILURE);
}

static long
parse_long(const char*const s)
{
    size_t len = strlen(s);
    char *ptr;
    long val;

    val = strtol(s, &ptr, 0);

    if (ptr != (s+len))
        error("Error: (%s) is not an integer argument.",s);

    return val;
}

/////////////////////////////////////////////////////////////////////////////
// SentIdStream_File returns a stream of sentence id strings from a file.
// TOOD: Make separate abstract base class.
/////////////////////////////////////////////////////////////////////////////

class SentIdStream_File
{
public:
    SentIdStream_File(FILE *fp);
    ~SentIdStream_File();

    const char *next();         // Returns pointer to next sentence id, or
                                // 0 if at end of file or error.
    size_t number_read() { return nread; }
private:
    FILE *fp;
    size_t buf_size;
    char *buf;
    size_t nread;
};

SentIdStream_File::SentIdStream_File(FILE *fp_arg)
    : fp(fp_arg)
{
    buf_size = 128;
    buf = new char[buf_size];
}

SentIdStream_File::~SentIdStream_File()
{
    delete buf;
}

const char*
SentIdStream_File::next()
{
    // TODO: Doesn't distinguish file I/O errors from end of file condition.

    if (fgets(buf, buf_size, fp)==NULL)
    {
        return 0;
    }
    else
    {
        char *p = strchr(buf,'\n'); // Get pos. of newline character.
        if (p==NULL)
        {
            error("Sentence id too long.");
        }
        else
        {
            *p = '\0';          // Trim off newline character.
        }   
	nread++;
        return buf;
    }
}


/////////////////////////////////////////////////////////////////////////////
// LabelValsStream_File returns a stream of label values strings from a file.
// TOOD: Make separate abstract base class.
/////////////////////////////////////////////////////////////////////////////

class LabelValsStream_File
{

public:
  LabelValsStream_File(FILE *fp);
  ~LabelValsStream_File();

  const size_t next();       // Returns value of next label, or
                             // -1 if at end of file or error.
  size_t labels_read() { return num_labels_read; }

  bool nullFile() { return (fp == NULL); }

private:
  FILE *fp;
  size_t num_labels_read;
  size_t buf_size;
  char *buf;

};

LabelValsStream_File::LabelValsStream_File(FILE *fp_arg)
    : fp(fp_arg)
{
  if (fp == NULL)
    return;
  num_labels_read = 0;
  buf_size = 128;
  buf = new char[buf_size];
}

LabelValsStream_File::~LabelValsStream_File()
{
  if (fp == NULL)
    return;
  delete buf;
}

const size_t
LabelValsStream_File::next()
{
    // TODO: Doesn't distinguish file I/O errors from end of file condition.

  if (fp == NULL) {
    // a null file produces an infinite stream of zeros
    return 0;
  }

  if (fgets(buf, buf_size, fp)==NULL)
    {
      error("Error reading labels. fgets returned NULL, label num %d\n",
	    num_labels_read+1);
    }
  else
    {
      char *p = strchr(buf,'\n');
      if (p == NULL) {
	error("Label much too long. label num %d", num_labels_read+1);
      } else {
        char *ptr;
        int i = (int) strtol(buf,&ptr,0);
	if (ptr != p) {
	  error("Malformed label (%s), label num %d",buf, num_labels_read+1);
	}
	num_labels_read++;
	return i;
      }
    }
  // Shouldn't happen, but leave in to reduce warning messages.
  return ~0;
}


/////////////////////////////////////////////////////////////////////////////
// Routine that actually does most of the work.
/////////////////////////////////////////////////////////////////////////////

#define ATTACH_BUFSIZE (1024)

static const char*
attach_suffix(const char*front, const char*back)
{
    // Returns pointer to statically allocated buffer containing second
    // string appended to first string.

    static char buf[ATTACH_BUFSIZE];

    const size_t front_len = strlen(front);
    const size_t back_len = strlen(back);

    if (front_len + back_len >= ATTACH_BUFSIZE)
        error("Sentence id plus suffix too long.");

    strcpy(buf, front);
    strcpy(buf+front_len, back);

    return buf;
}

static void
pfile_build(SentIdStream_File& sent_id_stream,
		LabelValsStream_File& label_vals_stream_p,
		QN_OutFtrLabStream& output_stream,
		int nfeats,
		char *suffix,
		bool quiet)
{
    // TODO: Should have some real sentence id information.
    long seg_id = 0;
    const char * sent_id;

    while ((sent_id = sent_id_stream.next()))
    {
      if (!quiet)
        printf("Processing sentence %s (#%zu).\n", 
		sent_id,sent_id_stream.number_read());

        // Get FEAT for this sentence.
        FILE *feat_fp = fopen(attach_suffix(sent_id, suffix), "r");
        if (feat_fp==NULL)
            error("Couldn't open file %s%s",sent_id,suffix);
        if (fseek(feat_fp, 0L, SEEK_END))
            error("Couldn't fseek() to end of %s%s file.",sent_id,suffix);
	long feat_len = ftell(feat_fp);
	if (feat_len < 0L)
            error("Couldn't ftell() on %s%s file.",sent_id,suffix);
	if (feat_len % (sizeof(float)*nfeats))
	  error("Length of %s%s is not k*sizeof(float)*nfeats(=%d).",
		sent_id,suffix,nfeats);
        const size_t n_frames = feat_len / (nfeats*sizeof(float));
        if (fseek(feat_fp, 0L, SEEK_SET))
            error("Couldn't fseek() to start of %s%s file.",sent_id,suffix);

        // TODO: Should optimize new/deletes out of inner loop.
        float *feature = new float[n_frames * nfeats];
        QNUInt32* label = new QNUInt32[n_frames];

	// Read the featurs from the .feat file.
	if (fread(feature, sizeof(float), n_frames*nfeats, feat_fp) 
	    != n_frames*nfeats)
	  error("Couldn't read features from %s%s file.",sent_id,suffix);

	// And read the appropriate number of labels from the label stream.
	if (label_vals_stream_p.nullFile()) {
	  // Write output.
	  output_stream.write_ftrs(n_frames, feature);
	} else {
	  for (size_t i=0; i<n_frames; i++) {
	    label[i] = label_vals_stream_p.next();
	  }
	  // Write output.
	  output_stream.write_ftrslabs(n_frames, feature, label);
	}


        output_stream.doneseg((QN_SegID) seg_id++);

        delete feature;
        delete label;

        if (fclose(feat_fp))
            error("Couldn't close %s.ae file.",sent_id);
    }
}

main(int argc, const char *argv[])
{
    const char *sent_ids_fname = 0;   // Sentence ids filename.
    const char *label_vals_fname = 0;   // Label values filename.
    const char *output_fname = 0;  // Output pfile name.
    int debug_level = 0;
    size_t nfeats = 0;
    char *suffix=0;
    bool quiet = false;

    program_name = *argv++;
    argc--;

    while (argc--)
    {
        const char *argp = *argv++;

        if (strcmp(argp, "-help")==0)
        {
            usage();
        }
        if (strcmp(argp, "-q")==0)
        {
            quiet = true;
        }
        else if (strcmp(argp, "-sents")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is sentences ids file name.
                sent_ids_fname = *argv++;
                argc--;
            }
            else
                usage("No sentence ids filename given.");
        }
        else if (strcmp(argp, "-suffix")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is suffix name.
	      suffix = new char[strlen(*argv)+1];
	      *suffix = '.';
	      strcpy(suffix+1,*argv);
	      argv++;
	      argc--;
            }
            else
                usage("No sentence ids filename given.");
        }
        else if (strcmp(argp, "-o")==0)
        {
            // Output file name.
            if (argc>0)
            {
                // Next argument is output file name.
                output_fname = *argv++;
                argc--;
            }
            else
                usage("No output filename given.");
        }
        else if (strcmp(argp, "-labels")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is label values file name.
                label_vals_fname = *argv++;
                argc--;
            }
            else
                usage("No label values filename given.");
        }
        else if (strcmp(argp, "-nfeats")==0)
        {
            if (argc>0)
            {
                // Next argument is nfeats
                nfeats = parse_long(*argv++);
                argc--;
            }
            else
                usage("No debug level given.");
        }
        else if (strcmp(argp, "-debug")==0)
        {
            if (argc>0)
            {
                // Next argument is debug level.
                debug_level = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No debug level given.");
        }
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    if (sent_ids_fname==0)
        usage("No sentence ids file name supplied.");
    FILE *sent_ids_fp = fopen(sent_ids_fname, "r");
    if (sent_ids_fp==NULL)
        error("Couldn't open sentence ids file for reading.");

    FILE *label_vals_fp;
    if (label_vals_fname==0) {
      label_vals_fp = NULL;
    } else {
      label_vals_fp = fopen(label_vals_fname, "r");
      if (label_vals_fp==NULL)
        error("Couldn't open label vals file for reading.");
    }

    if (output_fname==0)
        usage("No output pfile name supplied.");
    FILE *output_fp = fopen(output_fname, "wb");
    if (output_fp==NULL)
        error("Couldn't open output pfile for writing.");

    if (suffix == NULL)
        error("Must supply a suffix.");

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    SentIdStream_File* sent_ids_stream_p = new SentIdStream_File(sent_ids_fp);
    LabelValsStream_File* label_vals_stream_p = 
      new LabelValsStream_File(label_vals_fp);

    QN_OutFtrLabStream* output_stream_p
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "pfile_build",
                                       output_fp,
                                       nfeats,   //  values
                                       (label_vals_fp == NULL ? 0 : N_LABELS), // label
                                       1);

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_build(*sent_ids_stream_p,
		*label_vals_stream_p,
		*output_stream_p,
		nfeats,
		suffix,
		quiet);

    if (!quiet)
      printf("Processed %zu labels total.\n",
	     label_vals_stream_p->labels_read());

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////


    delete sent_ids_stream_p;
    delete label_vals_stream_p;
    if (fclose(sent_ids_fp))
        error("Couldn't close sentence ids file.");
    delete output_stream_p;
    if (fclose(output_fp))
        error("Couldn't close output pfile.");
    if (label_vals_fp != NULL) {
      if (fclose(label_vals_fp))
        error("Couldn't close labels file.");
    }

    return EXIT_SUCCESS;
}
