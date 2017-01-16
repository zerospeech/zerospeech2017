/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_patchlabels.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $

    Replacement for patchlabels's call to pfile to patch labels.
    Jeff Bilmes
    <bilmes@cs.berkeley.edu>
    Some code taken from Krste Asanovic.

*/

#include "pfile_utils.h"

#include "error.h"

#include "QN_PFile.h"
#include "QN_camfiles.h"
#include "QN_ILab.h"

/* Patch broken header files. */

#ifndef SEEK_SET
#define SEEK_SET (0)
#endif

#ifndef SEEK_END
#define SEEK_END (2)
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (1)
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
"-help        print this message.\n"
"-i  <fname>  name of input feature file.\n"
"-if <fmt>    format of input feature file (<pfile>/lna/pre).\n"
"-iw <nftrs>  width of input ftrs (for pre or lna).\n"
"-o  <fname>  name of resulting output ftr+lab file.\n"
"-of <fmt>    format of output feature+lab file (<pfile>/lna/pre).\n"
"-l  <fname>  name of file holding frame labels to use.\n"
"-lf <fmt>    format of input label file (<ascii>/pfile/lna/pre/ilab).\n"
"-lw <nftrs>  width of ftrs in label input file (for pre or lna).\n"
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
        error("Error: EOF in labels file, or I/O error. nread=%ld",
	      nread);
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
/////////////////////////////////////////////////////////////////////////////

class LabelValsStream
{
    // base class for objects returning sequences of labels
public:
    LabelValsStream() {num_labels_read = 0;}
    virtual ~LabelValsStream() {}

    virtual const int next()=0;       // Returns value of next label, or
                                         // -1 if at end of file or error.
    virtual int eos() { return 1;}	// call when EOS expected
    virtual int num_labs() { return 1;}
    size_t labels_read() { return num_labels_read; }

protected:
    size_t num_labels_read;
};

class LabelValsStream_File : public LabelValsStream
{

public:
    LabelValsStream_File(FILE *fp);
    virtual ~LabelValsStream_File();

    const int next();       // Returns value of next label, or
                               // -1 if at end of file or error.
private:
    FILE *fp;
    size_t buf_size;
    char *buf;
};

LabelValsStream_File::LabelValsStream_File(FILE *fp_arg)
    : fp(fp_arg)
{
    num_labels_read = 0;
    buf_size = 128;
    buf = new char[buf_size];
}

LabelValsStream_File::~LabelValsStream_File()
{
    delete buf;
}

const int
LabelValsStream_File::next()
{
    // TODO: Doesn't distinguish file I/O errors from end of file condition.

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
    return -1;
}

class LabelValsStream_InStr : public LabelValsStream
{
    // Get labels via a QuickNet InLabStream
public:
    LabelValsStream_InStr(QN_InLabStream* a_instr) { 
	instr = a_instr;
	instr->nextseg();
    }
    virtual ~LabelValsStream_InStr() {};

    virtual const int next();
    virtual int eos();

    virtual int num_labs() { return instr->num_labs(); }

protected:
    QN_InLabStream* instr;
};

const int LabelValsStream_InStr::next() {
    QNUInt32 lab;
    int cnt = instr->read_labs(1, &lab);
    if (cnt == 1) {
	num_labels_read++;
	return lab;
    } else {
	return -1;
    }
}

int LabelValsStream_InStr::eos() {
    if (instr->nextseg() == QN_SEGID_BAD) {
	return 0;
    } else {
	return 1;
    }
}

class LabelValsStream_PFile : public LabelValsStream_InStr {
public:
    LabelValsStream_PFile(FILE *fp) :
	LabelValsStream_InStr(new QN_InFtrLabStream_PFile(0 /*debug_level*/, 
							 "plabels:labf", 
							 fp, 0)) { };
    ~LabelValsStream_PFile() { delete instr; }
	
};

class LabelValsStream_ilab : public LabelValsStream_InStr {
public:
    LabelValsStream_ilab(FILE *fp) :
	LabelValsStream_InStr(new QN_InLabStream_ILab(0 /*debug_level*/, 
						      "plabels:labf", 
						      fp, 0)) { };
    ~LabelValsStream_ilab() { delete instr; }
};

class LabelValsStream_pre : public LabelValsStream_InStr {
public:
    LabelValsStream_pre(FILE *fp, int width) :
	LabelValsStream_InStr(new QN_InFtrLabStream_PreFile(0 /*debug_level*/, 
							 "plabels:labf", 
							 fp, width, 0)) { };
    ~LabelValsStream_pre() { delete instr; }
};

class LabelValsStream_lna : public LabelValsStream_InStr {
public:
    LabelValsStream_lna(FILE *fp, int width) :
	LabelValsStream_InStr(new QN_InFtrLabStream_LNA8(0 /*debug_level*/, 
							 "plabels:labf", 
							 fp, width, 0)) { };
    ~LabelValsStream_lna() { delete instr; }
};

/////////////////////////////////////////////////////////////////////////////
// Routine that actually does most of the work.
/////////////////////////////////////////////////////////////////////////////

static void
plabels(LabelValsStream& label_vals_stream_p,
	QN_InFtrLabStream& in_stream,
	QN_OutFtrLabStream& out_stream)
{
    size_t buf_size = 300;      // Start with storage for 300 frames.
    size_t cur_sent = 0;

    if (in_stream.num_labs() > 1) {
      error("Number of labels per frame of input pfile must be <= one.");
    } 
    const size_t n_labs = 1; // set to one regardless
    const size_t n_ftrs = in_stream.num_ftrs();


    float *ftr_buf = new float[buf_size * n_ftrs];
    QNUInt32* lab_buf = new QNUInt32[buf_size * n_labs];

    size_t end_sentence = in_stream.num_segs();
    for (cur_sent = 0; cur_sent < end_sentence ; cur_sent++) {

        const size_t n_frames = in_stream.num_frames(cur_sent);
        if (n_frames == QN_SIZET_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't find number of frames "
                   "at sentence %lu in input pfile.\n",
                   program_name, (unsigned long) cur_sent);
            error("Aborting.");
        }


        // Increase size of buffers if needed.
        if (n_frames > buf_size)
        {
            // Free old buffers.
            delete ftr_buf;
            delete lab_buf;

            // Make twice as big to cut down on future reallocs.
            buf_size = n_frames * 2;

            // Allocate new larger buffers.
            ftr_buf = new float[buf_size * n_ftrs];
            lab_buf = new QNUInt32[buf_size * n_labs];
        }

        const QN_SegID seg_id = in_stream.set_pos(cur_sent, 0);
        if (seg_id == QN_SEGID_BAD)
        {
            fprintf(stderr,
                   "%s: Couldn't seek to start of sentence %lu "
                   "in input pfile.",
                   program_name, (unsigned long) cur_sent);
            error("Aborting.");
        }

        const size_t n_read =
            in_stream.read_ftrslabs(n_frames, ftr_buf, lab_buf);
        if (n_read != n_frames)
        {
            fprintf(stderr, "%s: At sentence %lu in input pfile, "
                   "only read %lu frames when should have read %lu.\n",
                   program_name, (unsigned long) cur_sent,
                   (unsigned long) n_read, (unsigned long) n_frames);
            error("Aborting.");
        }

	// Read the appropriate number of labels from the label stream.
	for (size_t i=0; i<n_frames; i++) {
	    int lab;
	    lab = label_vals_stream_p.next();
	    if (lab < 0) {
		fprintf(stderr, "%s: At sentence %lu in input pfile, "
			"error after reading %lu of %lu labels.\n",
			program_name, (unsigned long) cur_sent,
			(unsigned long) i, (unsigned long) n_frames);
		error("Aborting.");
	    }
	    lab_buf[i] = lab;
	}
	// Advance labels to next utt (should check for errors?)
	label_vals_stream_p.eos();
	

        // Write output.
        out_stream.write_ftrslabs(n_frames, ftr_buf, lab_buf);
        out_stream.doneseg(seg_id);
    }
}

main(int argc, const char *argv[])
{
    const char *ipfile_fname = 0;   // pfile name
    const char *opfile_fname = 0;   // pfile name
    const char *label_vals_fname = 0;   // Label values filename.
    const char *ipfile_fmt = 0;   // ftr file type
    const char *opfile_fmt = 0;   // output file type
    const char *label_fmt = 0;   // Labels file type

    int ip_width = 0;
    int lab_width = 0;
    int debug_level = 0;

    program_name = *argv++;
    argc--;

    while (argc--)
    {
        const char *argp = *argv++;

        if (strcmp(argp, "-help")==0)
        {
            usage();
        }
        else if (strcmp(argp, "-i")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is sentences ids file name.
                ipfile_fname = *argv++;
                argc--;
            }
            else
                usage("No input pfile filename given.");
        }
        else if (strcmp(argp, "-o")==0)
        {
            // Input file name.
            if (argc>0)
            {
                // Next argument is sentences ids file name.
	        opfile_fname = *argv++;
                argc--;
            }
            else
                usage("No output pfile filename given.");
        }
        else if (strcmp(argp, "-l")==0)
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
        else if (strcmp(argp, "-if")==0)
        {
            // Input file type
            if (argc>0)
            {
                // Next argument is type of input ftr file
                ipfile_fmt = *argv++;
                argc--;
            }
            else
                usage("No input file format given.");
        }
        else if (strcmp(argp, "-of")==0)
        {
            // Output file type
            if (argc>0)
            {
                // Next argument is type of output file
                opfile_fmt = *argv++;
                argc--;
            }
            else
                usage("No output file format given.");
        }
        else if (strcmp(argp, "-lf")==0)
        {
            // Label file type
            if (argc>0)
            {
                // Next argument is type of input ftr file
                label_fmt = *argv++;
                argc--;
            }
            else
                usage("No label file format given.");
        }
        else if (strcmp(argp, "-iw")==0)
        {
            if (argc>0)
            {
                // Next argument is input width
                ip_width = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No input width given.");
        }
        else if (strcmp(argp, "-lw")==0)
        {
            if (argc>0)
            {
                // Next argument is input labels width
                lab_width = (int) parse_long(*argv++);
                argc--;
            }
            else
                usage("No input label width given.");
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

    if (ipfile_fname==0)
        usage("No input ftr file name supplied.");
    FILE *ipfile_fp = fopen(ipfile_fname, "r");
    if (ipfile_fp==NULL)
        error("Couldn't open ftr file %s for reading.",ipfile_fname);


    if (opfile_fname==0)
        usage("No output file name supplied.");
    FILE *opfile_fp = fopen(opfile_fname, "wb");
    if (opfile_fp==NULL)
        error("Couldn't open file %s for writing.",opfile_fname);


    if (label_vals_fname==0)
        usage("No label values file name supplied.");
    FILE *label_vals_fp = fopen(label_vals_fname, "r");
    if (label_vals_fp==NULL)
        error("Couldn't open label vals file for reading.");

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    LabelValsStream* label_vals_stream_p;
    if (label_fmt == 0 || strcmp(label_fmt, "ascii") == 0) {
	label_vals_stream_p = 
	    new LabelValsStream_File(label_vals_fp);
    } else if (strcmp(label_fmt, "pfile") == 0) {
	label_vals_stream_p = 
	    new LabelValsStream_PFile(label_vals_fp);
    } else if (strcmp(label_fmt, "ilab") == 0) {
	label_vals_stream_p = 
	    new LabelValsStream_ilab(label_vals_fp);
    } else if (strcmp(label_fmt, "pre") == 0) {
	if (lab_width == 0) {
	    error("Must specify width (-lw) for pre labels input.");
	}
	label_vals_stream_p = 
	    new LabelValsStream_pre(label_vals_fp, lab_width);
    } else if (strcmp(label_fmt, "lna") == 0) {
	if (lab_width == 0) {
	    error("Must specify width (-lw) for lna labels input.");
	}
	label_vals_stream_p = 
	    new LabelValsStream_lna(label_vals_fp, lab_width);
    } else {
	error("Input label format %s is not ascii/pfile/ilab/pre/lna.", label_fmt);
    }

    int n_op_labs = label_vals_stream_p->num_labs();
    
    QN_InFtrLabStream* in_stream_p;
    if (ipfile_fmt == 0 || strcmp(ipfile_fmt, "pfile") == 0) {
	in_stream_p = new QN_InFtrLabStream_PFile(debug_level, "plabels:ipf", 
					  ipfile_fp, 1);
    } else if (strcmp(ipfile_fmt, "pre") == 0) {
	if (ip_width == 0) {
	    error("Must specify width (-iw) for pre file input.");
	}
	in_stream_p = new QN_InFtrLabStream_PreFile(debug_level, "plabels:ipf",
					    ipfile_fp, ip_width, 1);
    } else if (strncmp(ipfile_fmt, "lna", 3) == 0) {
	if (ip_width == 0) {
	    error("Must specify width (-iw) for lna file input.");
	}
	in_stream_p = new QN_InFtrLabStream_LNA8(debug_level, "plabels:ipf", 
					 ipfile_fp, ip_width, 1);
    } else { /* could have more, but what for? */
	error("Input format %s is not pfile/pre/lna.", ipfile_fmt);
    }

    /* default output format follows input */
    if (opfile_fmt == 0) 	opfile_fmt=ipfile_fmt;

    QN_OutFtrLabStream* out_stream_p;
    if (opfile_fmt == 0 || strcmp(opfile_fmt, "pfile") == 0) {
	out_stream_p
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "plabels:opf",
                                       opfile_fp,
                                       in_stream_p->num_ftrs(),
                                       n_op_labs,
                                       0); // TODO: Should write indexed.
    } else if (strcmp(opfile_fmt, "pre") == 0) {
	if (n_op_labs != 1) error("Pre files have exactly 1 label only.");
	out_stream_p
        = new QN_OutFtrLabStream_PreFile(debug_level,
				       "plabels:opf",
                                       opfile_fp,
                                       in_stream_p->num_ftrs());
    } else if (strncmp(opfile_fmt, "lna", 3) == 0) {
	if (n_op_labs != 1) error("LNA files have exactly 1 label only.");
	out_stream_p
        = new QN_OutFtrLabStream_LNA8(debug_level,
				       "plabels:opf",
                                       opfile_fp,
                                       in_stream_p->num_ftrs());
    } else { /* could have more, but what for? */
	error("Output format %s is not pfile/pre/lna.", opfile_fmt);
    }

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    plabels(*label_vals_stream_p,
	    *in_stream_p,
	    *out_stream_p);

    fprintf(stderr,"Processed %zu labels total.\n",
	    label_vals_stream_p->labels_read());

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////


    delete label_vals_stream_p;
    if (fclose(label_vals_fp))
        error("Couldn't close labels file.");
    delete in_stream_p;
    if (fclose(ipfile_fp))
        error("Couldn't close sentence ids file.");
    delete out_stream_p;
    if (fclose(opfile_fp))
        error("Couldn't close output pfile.");
    return EXIT_SUCCESS;
}
