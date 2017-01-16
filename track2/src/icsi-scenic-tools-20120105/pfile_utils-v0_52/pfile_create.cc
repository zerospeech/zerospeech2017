/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_create.cc,v 1.4 2012/01/05 20:32:02 dpwe Exp $
  
    This program creats a pfile from raw ASCII or binary input.
    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include "pfile_utils.h"

#include <QN_PFile.h>
#include <QN_intvec.h>
#include <QN_fltvec.h>
#include "parse_subset.h"
#include "error.h"

#define MAXHISTBINS 1000

#ifndef HAVE_BOOL
enum bool { false = 0, true = 1 };
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;


extern size_t bin_search(float *array,
			 size_t length, // the length of the array
			 float val);     // the value to search for.

const size_t MAX_SIZE_T = UINT_MAX;

class FrameStr {
public:

  FrameStr(FILE*,const bool,size_t,size_t);
  ~FrameStr();

  size_t sent_no;
  size_t frame_no;
  bool push_back;

  const size_t n_ftrs;
  const size_t n_labs;
  const bool binary;
  FILE *in_fp;

  float *ftrs;
  QNUInt32 *labs;

  bool nextFrame();
  void pushback();


};

FrameStr::FrameStr(FILE *in_fp_a,
		   const bool binary_a,
		   size_t n_ftrs_a,
		   size_t n_labs_a)
  : n_ftrs(n_ftrs_a),n_labs(n_labs_a),
    binary(binary_a),in_fp(in_fp_a)
{
  ftrs = new float[n_ftrs];
  labs = new QNUInt32[n_labs];
  sent_no = MAX_SIZE_T;
  frame_no = MAX_SIZE_T;
}

FrameStr::~FrameStr()
{
  delete ftrs;
  delete labs;
}

void
FrameStr::pushback()
{
  push_back = true;
}




// return true if next frame is valid,

bool 
FrameStr::nextFrame()
{
  if (push_back == true) {
    push_back = false;
    return true;
  }
  if (feof(in_fp))
    return false;

  if (binary) {
    if (!fread(&sent_no,sizeof(sent_no),1,in_fp)) {
      if (feof(in_fp)) {
	return false; // eof found
      } else {
	error("ERROR: format error input. Expecting sentence number.");
      }
    }
    sent_no = qn_btoh_i32_i32(sent_no);
    if (!fread(&frame_no,sizeof(frame_no),1,in_fp)) {
	error("ERROR: format error input. Expecting frame number.");
    }
    frame_no = qn_btoh_i32_i32(frame_no);
    if (fread(ftrs,sizeof(ftrs[0]),n_ftrs,in_fp) != n_ftrs) {
	error("ERROR: format error input. Expecting %d features.",n_ftrs);
    }
    qn_btoh_vf_vf(n_ftrs,  ftrs, ftrs);
    if (fread(labs,sizeof(labs[0]),n_labs,in_fp) != n_labs) {
      error("ERROR: format error input. Expecting %d labels.",n_labs);
    }
    qn_btoh_vi32_vi32(n_labs, (QNInt32*) labs, (QNInt32*) labs);
  } else {
    if (fscanf(in_fp,"%zu",&sent_no) == EOF) {
      if (feof(in_fp)) {
	return false; // eof found
      } else {
	error("ERROR: format error input. Expecting sentence number.");
      }
    } 
    if (fscanf(in_fp,"%zu",&frame_no) == EOF) {
      error("ERROR: format error input. Expecting frame number.");
    } 
    size_t i;
    for (i=0;i<n_ftrs;i++) {
      if (fscanf(in_fp,"%f",&ftrs[i]) == EOF)
	error("ERROR: format error input. Expecting feature %d a float.",i);
    }
    for (i=0;i<n_labs;i++) {
      if (fscanf(in_fp,"%u",&labs[i]) == EOF)
	error("ERROR: format error input. Expecting feature %d an uint.",i);
    }
  }
  return true;
}


static void
usage(const char* message = 0)
{
    if (message)
        fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help           print this message\n"
	    "-o <file-name>  output pfile \n"
	    "[-i <name>]     input file or '-' for stdin\n"
	    "-f #            number of features in input stream (def=0)\n"
	    "-l #            number of labels in input stream (def=0)\n"
	    "-b              binary rather than ASCII input\n"
	    "-debug <level>  number giving level of debugging output to produce 0=none.\n"
    );
    exit(EXIT_FAILURE);
}


void
make_buffers_safe_to_add_one(size_t& buf_array_length,
			     size_t  buf_size,
			     float*& ftr_buf,
			     const size_t n_ftrs,
			     QNUInt32*& lab_buf,
			     const size_t n_labs)
{
  if (buf_size+1 > buf_array_length) {
    buf_array_length *= 2;
    float* ftr_tmp = new float [buf_array_length * n_ftrs ];
    QNUInt32* lab_tmp = new QNUInt32[buf_array_length * n_labs];
    ::memcpy(ftr_tmp,ftr_buf,sizeof(float)*buf_size*n_ftrs);
    ::memcpy(lab_tmp,lab_buf,sizeof(QNUInt32)*buf_size*n_labs);
    delete ftr_buf;
    delete lab_buf;
    ftr_buf = ftr_tmp;
    lab_buf = lab_tmp;
  }
}


bool
next_input_sentence(FrameStr& framestr,
		    const size_t cur_sent_no,
		    size_t &n_frames,
		    float*& ftr_buf,
		    QNUInt32*& lab_buf,
		    size_t &buf_array_length)
{
  // read the first line to get the sentno, frameno


  n_frames = 0;
  while(framestr.nextFrame()) {
    if (framestr.sent_no == cur_sent_no+1) {
      if (n_frames > 0) {
	framestr.pushback();
	return true;
      } else 
	error("ERROR: Sentence numbers must be in sequence. Current sent = %d, next sent found = %d",cur_sent_no,framestr.sent_no);
    } else if (framestr.sent_no != cur_sent_no) {
      error("ERROR: Sentence numbers must be in sequence. Current sent = %d, next sent found = %d",cur_sent_no,framestr.sent_no);
    }
    if (framestr.frame_no != n_frames) {
      error("ERROR: Frame numbers must be in sequence.");
    }

    make_buffers_safe_to_add_one(buf_array_length,
				 n_frames,
				 ftr_buf,
				 framestr.n_ftrs,
				 lab_buf,
				 framestr.n_labs);
    ::memcpy(&ftr_buf[n_frames*framestr.n_ftrs],
	     framestr.ftrs,
	     framestr.n_ftrs*sizeof(ftr_buf[0]));
    
    ::memcpy(&lab_buf[n_frames*framestr.n_labs],
	     framestr.labs,
	     framestr.n_labs*sizeof(lab_buf[0]));
    n_frames++;
  }

  // Must have reached eof, 
  // return true if any data exists to use.
  return (n_frames > 0);
}


static void
pfile_create(FILE *in_fp,
	     QN_OutFtrLabStream& out_stream,
	     const size_t n_ftrs,
	     const size_t n_labs,
	     const bool binary)
{
    size_t buf_array_length = 300;      // Start with storage for 300 frames.
    float *ftr_buf = new float[buf_array_length * n_ftrs];
    QNUInt32* lab_buf = new QNUInt32[buf_array_length * n_labs];
    size_t n_frames=0;
    size_t sent_no=0;

    size_t total_frames = 0;
    FrameStr framestr(in_fp,binary,n_ftrs,n_labs);

    while (next_input_sentence(framestr,sent_no,
			       n_frames,ftr_buf,lab_buf,buf_array_length)) {
      // Write output.
      out_stream.write_ftrslabs(n_frames, ftr_buf, lab_buf);
      out_stream.doneseg((QN_SegID) sent_no++);
      total_frames += n_frames;
    };
    delete ftr_buf;
    delete lab_buf;
}


static long
parse_long(const char*const s)
{
    size_t len = strlen(s);
    char *ptr;
    long val;

    val = strtol(s, &ptr, 0);

    if (ptr != (s+len))
        error("Not an integer argument.");

    return val;
}

static float
parse_float(const char*const s)
{
    size_t len = strlen(s);
    char *ptr;
    double val;
    val = strtod(s, &ptr);
    if (ptr != (s+len))
        error("Not an floating point argument.");
    return val;
}


main(int argc, const char *argv[])
{
    //////////////////////////////////////////////////////////////////////
    // TODO: Argument parsing should be replaced by something like
    // ProgramInfo one day, with each extract box grabbing the pieces it
    // needs.
    //////////////////////////////////////////////////////////////////////

    const char *input_fname = 0;  // Input file name.
    const char *output_fname = 0; // Output pfile name.
    
    size_t n_ftrs=0; // number of features per frame
    size_t n_labs=0; // number of labels per frame
    bool binary=false; // binary input??

    int debug_level = 0;

    program_name = *argv++;
    argc--;

    while (argc--)
    {
        char buf[BUFSIZ];
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
                input_fname = *argv++;
                argc--;
            }
            else
                usage("No input filename given.");
        }
        else if (strcmp(argp, "-o")==0)
        {
            // Output file name.
            if (argc>0)
            {
                output_fname = *argv++;
                argc--;
            }
            else
                usage("No output filename given.");
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
        else if (strcmp(argp, "-f")==0)
        {
            if (argc>0)
            {
	      n_ftrs = parse_long(*argv++);
	      argc--;
            }
            else
                usage("No num features value given.");
        }
        else if (strcmp(argp, "-l")==0)
        {
            if (argc>0)
            {
	      n_labs = parse_long(*argv++);
	      argc--;
            }
            else
	      usage("No num features value given.");
        }
        else if (strcmp(argp, "-b")==0)
        {
	  binary=true;
        }
        else {
	  sprintf(buf,"Unrecognized argument (%s).",argp);
	  usage(buf);
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////

    FILE *in_fp;
    if (input_fname==0 || !strcmp(input_fname,"-"))
      in_fp = stdin;
    else 
      in_fp = fopen(input_fname, "r");
    if (in_fp==NULL)
        error("Couldn't open input pfile %s for reading.",input_fname);

    FILE *out_fp;
    if (output_fname==0 || !strcmp(output_fname,"-"))
      usage("Need to supply arguments.");
    else {
      if ((out_fp = fopen(output_fname, "w")) == NULL) {
	error("Couldn't open output file for writing.");
      }
    }

    if (n_ftrs == 0 && n_labs == 0)
      error("Can't create a pfile with 0 features and 0 labels per frame.");

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_OutFtrLabStream* out_streamp
        = new QN_OutFtrLabStream_PFile(debug_level,
				       "",
                                       out_fp,
				       n_ftrs,
                                       n_labs,
                                       1);

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    pfile_create(in_fp,
		 *out_streamp,
		 n_ftrs,n_labs,binary);

    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete out_streamp;
    if (fclose(in_fp))
        error("Couldn't close input pfile.");
    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
