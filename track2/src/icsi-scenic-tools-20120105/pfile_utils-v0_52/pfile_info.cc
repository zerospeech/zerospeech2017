/*
    $Header: /u/drspeech/repos/pfile_utils/pfile_info.cc,v 1.3 2012/01/05 20:32:02 dpwe Exp $
  
    This program prints information about the pifle
    given as arguments, eg. number of sentences, frames, labels, features
    etc.
*/

#include "pfile_utils.h"

#include "QN_PFile.h"
#include "error.h"

#ifndef HAVE_BOOL
enum bool { false = 0, true = 1 };
#endif

#define MAXHISTBINS 1000


#define MIN(a,b) ((a)<(b)?(a):(b))

static const char* program_name;


extern size_t bin_search(float *array,
			 size_t length, // the length of the array
			 float val);     // the value to search for.



static void
usage(const char* message = 0)
{
    if (message)
        fprintf(stderr, "%s: %s\n", program_name, message);
    fprintf(stderr, "Usage: %s <options>\n", program_name);
    fprintf(stderr,
	    "Where <options> include:\n"
	    "-help                       print this message\n"
	    "-i <fname> [-i <fname] ...  input pfiles\n"
	    "-o <fname>                  output file (default stdout) \n"
	    "-p                          Also print # frames for each sentence.\n"
	    "-q                          Don't print the normal info (i.e., useful with -p option).\n"
            "<fname> [<fname>] ...       input pfiles\n"
	    "-debug <level>              number giving level of debugging output to produce 0=none.\n"
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


void
pfile_info(const char*input_fname,
	   int debug_level,
	   bool print_sent_frames,
	   bool dont_print_info,
	   FILE *out_fp)
{
    if (input_fname==0)
        usage("No input pfile name supplied.");
    FILE *in_fp = fopen(input_fname, "r");
    if (in_fp==NULL) {
      fprintf(stderr,"WARNING: Couldn't open input pfile (%s) for reading.\n",input_fname);
      return;
    }

    //////////////////////////////////////////////////////////////////////
    // Create objects.
    //////////////////////////////////////////////////////////////////////

    QN_InFtrLabStream* in_streamp
        = new QN_InFtrLabStream_PFile(debug_level, "", in_fp, 1);

    if (!dont_print_info) {
      fprintf(out_fp,"%s\n",input_fname);
      fprintf(out_fp,"%zu sentences, %zu frames, %zu label(s), %zu features\n",
	      in_streamp->num_segs(),
	      in_streamp->num_frames(),
	      in_streamp->num_labs(),
	      in_streamp->num_ftrs());
    }

    if (print_sent_frames) {
      for (size_t next_sent = 0; next_sent < in_streamp->num_segs(); 
	   next_sent++ ) {
	const size_t n_frames = in_streamp->num_frames(next_sent);
	fprintf(out_fp,"%zu %lu\n",next_sent,n_frames);
      }
    }

    delete in_streamp;
    if (fclose(in_fp))
        error("Couldn't close input pfile.");

}

void
insert_iname(size_t &inames_num,
	     size_t &inames_asize,
	     const char** &input_fnames,
	     const char* new_name)
{
  if (inames_num + 1 == inames_asize) {
    inames_asize *= 2;
    const char** tmp = new const char*[inames_asize];
    ::memcpy(tmp,input_fnames,inames_num*sizeof(const char*));
    delete input_fnames;
    input_fnames = tmp;
  }
  input_fnames[inames_num++] = new_name;
}


main(int argc, const char *argv[])
{
    //////////////////////////////////////////////////////////////////////
    // TODO: Argument parsing should be replaced by something like
    // ProgramInfo one day, with each extract box grabbing the pieces it
    // needs.
    //////////////////////////////////////////////////////////////////////

    const char** input_fnames = NULL;  // Input pfile names

    const char* output_fname = NULL; // Output pfile name.
    int debug_level = 0;
    bool print_sent_frames = false; // true if print #frames for each sent.
    bool dont_print_info = false; // don't print the info stuff (i..e, used with -p optin).

    size_t inames_num=0;
    size_t inames_asize=20;
    input_fnames = new const char*[inames_asize];

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
        else if (strcmp(argp, "-p")==0)
        {
            print_sent_frames = true;
        }
        else if (strcmp(argp, "-q")==0)
        {
	  dont_print_info = true;
        }
        else if (strcmp(argp, "-i")==0)
        {
            // Input file name.
            if (argc>0)
            {
	      // Next argument is input file name.
	      insert_iname(inames_num,
			   inames_asize,
			   input_fnames,
			   *argv++);
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
                // Next argument is output file name.
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
        else if (*argp != '-') {
	  // assume any other names are more input files.
	  insert_iname(inames_num,
		       inames_asize,
		       input_fnames,
		       argp);
	} else {
	  sprintf(buf,"Unrecognized argument (%s).",argp);
	  usage(buf);
	}
    }

    //////////////////////////////////////////////////////////////////////
    // Check all necessary arguments provided before creating objects.
    //////////////////////////////////////////////////////////////////////


    FILE *out_fp;
    if (output_fname==0 || !strcmp(output_fname,"-"))
      out_fp = stdout;
    else {
      if ((out_fp = fopen(output_fname, "w")) == NULL) {
	error("Couldn't open output file for writing.");
      }
    }

    //////////////////////////////////////////////////////////////////////
    // Do the work.
    //////////////////////////////////////////////////////////////////////

    for (size_t i=0;i<inames_num;i++) 
      pfile_info(input_fnames[i],debug_level,print_sent_frames,dont_print_info,out_fp);


    //////////////////////////////////////////////////////////////////////
    // Clean up and exit.
    //////////////////////////////////////////////////////////////////////

    delete input_fnames;
    if (fclose(out_fp))
        error("Couldn't close output file.");

    return EXIT_SUCCESS;
}
