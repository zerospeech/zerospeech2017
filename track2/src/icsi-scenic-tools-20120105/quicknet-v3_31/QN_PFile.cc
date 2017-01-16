const char* QN_PFile_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_PFile.cc,v 1.42 2010/10/29 02:20:37 davidj Exp $";

// This file contains some miscellaneous routines for handling PFiles

#include <QN_config.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef QN_HAVE_ERRNO_H
#include <errno.h>
#endif
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"
#include "QN_PFile.h"
#include "QN_Logger.h"

// Abbreviations for the sscanf/printf strings for signed and unsigned longs
// or long-longs, compiler-dependent.
// Note that the short versions of these are done here rather than in the
// header as they are not namespace clean.
#define LLU QN_PF_LLU
#define LLD QN_PF_LLD

// This is the verision string that appears in the PFile header
static const char* pfile_version0_string = 
    "-pfile_header version 0 size 32768";

// This is the size of the PFile header

#define PFILE_HEADER_SIZE (32768)

// This routine is used to get one unsigned integer argument from a pfile
// header stored in a buffer.  It returns 0 on success, else -1.

static int
get_uint(const char* hdr, const char* argname, unsigned int* val)
{
    const char* p;		// Pointer to argument
    int count = 0;		// Number of characters scanned

    // Find argument in header
    p = strstr(hdr, argname);
    if (p==NULL)
	return -1;
    // Go past argument name
    p += strlen(argname);
    // Get value from stfing
    sscanf(p, " %u%n", val, &count);

    // We expect to pass one space, so need >1 characters for success.
    if (count > 1)
	return 0;
    else
	return -1;
}


// This routine is used to find string-valued data in the PFile header.
// Returns pointer to string or null.
static const char*
get_str(const char* hdr, const char* argname)
{
    const char* p;		// Pointer to argument.

    // Find argument in header.
    p = strstr(hdr, argname);
    if (p==NULL)
	return NULL;
    // Go past argument name.
    p += strlen(argname);
    // Pass over spaces.
    while (isspace(*p))
	p++;
    return p;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_PFile
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrLabStream_PFile::QN_InFtrLabStream_PFile(int a_debug,
						 const char* a_dbgname,
						 FILE* a_file,
						 int a_indexed)
  : log(a_debug, "QN_InFtrLabStream_PFile", a_dbgname),
    file(a_file),
    indexed(a_indexed),
    buffer(NULL),
    sentind(NULL)
{
    if (qn_fseek(file, (qn_off_t) 0, SEEK_SET))
    {
	log.error("Failed to seek to start of pfile '%s' header - "
		 "cannot read PFiles from streams.",
		 QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Started using PFile '%s'.", QN_FILE2NAME(file));
    read_header();
    log.log(QN_LOG_PER_RUN, "num_sents=%u num_frames=%u.",
	    total_sents, total_frames);
    log.log(QN_LOG_PER_RUN, "Using features from cols %u to %u.",
	    first_ftr_col, first_ftr_col + num_ftr_cols - 1);
    log.log(QN_LOG_PER_RUN, "Using labels from cols %u to %u.",
	    first_lab_col, first_lab_col + num_lab_cols - 1);

    // Allocate frame buffer.
    buffer = new QN_PFile_Val[num_cols];
    // Remember the width, in bytes, of one column
    bytes_in_row = num_cols * sizeof(QN_PFile_Val);

    if (indexed)
    {
	// Allocate space for sentence index
	sentind = new QNUInt32[total_sents + 1];
	if (sentind_offset!=0)
	    build_index_from_sentind_sect();
	else
	    build_index_from_data_sect();
    }
    // Move to the start of the PFile
    rewind();
}

QN_InFtrLabStream_PFile::~QN_InFtrLabStream_PFile()
{
    if (indexed)
	delete[] sentind;
    delete[] buffer;
}


void
QN_InFtrLabStream_PFile::read_header()
{
    char* header;
    unsigned int ndim;		// Number of dimensions of data section
    qn_ulonglong_t size;	// Size of data section
    unsigned int rows;		// Number of rows in section
    unsigned int cols;		// Number of columns in section
    qn_longlong_t offset;	// offset in data section
    char* p;			// Temporary pointer
    int ec;			// Error code

// Allocate space for header - do not use stack, as putting big things
// on the stack is dangerous on SPERT.

    header = new char[PFILE_HEADER_SIZE]; // Store header here

// Read in pfile header

    if (fread(header, PFILE_HEADER_SIZE, 1, file)!=1)
	log.error("Failed to read pfile '%s' header.",
		 QN_FILE2NAME(file));

// Check pfile header

    if (strstr(header, pfile_version0_string)==NULL)
	log.error("Bad PFile header version in '%s'.",
		 QN_FILE2NAME(file));

    p = strstr(header, "-data");
    if (p==NULL)
	log.error("Cannot find pfile -data parameter in header of"
		 " '%s'.", QN_FILE2NAME(file));

    sscanf(p, "-data size " LLU " offset " LLD " ndim %u nrow %u ncol %u",
	   &size, &offset, &ndim, &rows, &cols);
    if (offset!=0 || ndim!=2
	|| ((qn_ulonglong_t)rows * (qn_ulonglong_t)cols) != size)
	log.error("Bad or unrecognized pfile header -data args in"
		 " '%s'.", QN_FILE2NAME(file));

// Find feature, label and target details and check okay

    ec = get_uint(header, "-first_feature_column",
			   &first_ftr_col);
    ec |= get_uint(header, "-num_features",
			    &num_ftr_cols);

    first_lab_col = 0;      // Initialize to zero.
    if (get_uint(header, "-num_labels", &num_lab_cols))
        num_lab_cols = 0;       // Set to zero if not found.
    if (num_lab_cols > 0)
    {
        ec |= get_uint(header, "-first_label_column", &first_lab_col);
    }

// A few other bits of information
    ec |= get_uint(header, "-num_frames", &total_frames);
    if (total_frames!=rows)
	log.error("Inconsistent number of frames in header for"
		 " pfile '%s'.", QN_FILE2NAME(file));
    ec |= get_uint(header, "-num_sentences", &total_sents);
    if (ec)
	log.error("Problems reading pfile '%s' header.",
		 QN_FILE2NAME(file));

// Check the "format" field.
// The format value is a string containing 'f's, 'd's or prefixed repeat
// counts.
// e.g. ddffffffd or 2d24f

	long i;		// Local counter.

// First unpack the string.
	char* unpacked_format = new char[cols];
	memset(unpacked_format, '\0', cols); // Set to null.
	const char* hdr_format = get_str(header, "-format");
	if (hdr_format==NULL)
	{
	    log.error("Could not find '-format' field in PFile '%d'.",
		      QN_FILE2NAME(file));
	}
	char* upk_ptr = unpacked_format;
	unsigned upk_cnt = 0;
	char c = *hdr_format++;
	while (c=='f' || c=='d' || isdigit(c))
	{
	    if (c=='f' || c=='d')
	    {
		if (upk_cnt>cols)
		{
		    log.error("Format too long in PFile '%s' header.",
			      QN_FILE2NAME(file));
		}
		*upk_ptr++ = c;
		upk_cnt++;
		c = *hdr_format++;
	    }
	    else
	    {
		long rpt;	// No of times to repeat formatting character.
		char *eptr;
		rpt = strtol((hdr_format-1), &eptr, 10);
		/* (can't pass &hdr_format to strtol because of const) */
		hdr_format = eptr;
		upk_cnt += rpt;
		if (upk_cnt > cols)
		{
		    log.error("Format too long in PFile '%s' header.",
			      QN_FILE2NAME(file));
		}
		c = *hdr_format++;
		if (c!='f' && c!='d')
		{
		    log.error("Format field corrupted in PFile '%s' header.",
			      QN_FILE2NAME(file));
		}
		for (i=0; i<rpt; i++)
		{
		    *upk_ptr++ = c;
		}
		c = *hdr_format++;
	    }
	}

// Now check the string.
	// Two 'd's first for sentence and column.
	if (unpacked_format[0]!='d' || unpacked_format[1]!='d')
	{
	    log.error("Format field corrupted in PFile '%s' header.",
		      QN_FILE2NAME(file));
	}
	size_t col;
	for (col=first_ftr_col; col<(first_ftr_col+num_ftr_cols); col++)
	{
	    if (unpacked_format[col]!='f')
	    {
		log.error("Format field corrupted in PFile '%s' header - "
			  "not all feature columns have format 'f'.",
			  QN_FILE2NAME(file));
	    }
	}
	for (col=first_lab_col; col<(first_lab_col+num_lab_cols); col++)
	{
	    if (unpacked_format[col]!='d')
	    {
		log.error("Format field corrupted in PFile '%s' header - "
			  "not all label columns have format 'd'.",
			  QN_FILE2NAME(file));
	    }
	}
	delete[] unpacked_format;
    

// Put some stuff we stored locally in the main structure
    data_offset = (offset*sizeof(QN_PFile_Val) + PFILE_HEADER_SIZE);
    num_cols = cols;

// Get the sentence index information
    sentind_offset = 0;
    
    p = strstr(header, "-sent_table_data");
    if (p!=NULL)
    {
	sscanf(p, "-sent_table_data size " LLU " offset " LLD " ndim %u",
	       &size, &offset, &ndim);
	if (size!=total_sents+1 || ndim!=1)
	{
	    log.error("Bad or unrecognized header"
		     " -sent_table_data args in PFile '%s'.", 
		     QN_FILE2NAME(file));
	}
	sentind_offset = (offset*sizeof(QN_PFile_Val) + PFILE_HEADER_SIZE);
    }

// Some simple consistency checks on header information
    if (first_ftr_col >= num_cols
	|| (first_ftr_col + num_ftr_cols) > num_cols
	|| first_lab_col >= num_cols
	|| (first_lab_col + num_lab_cols) > num_cols
	|| ( (first_lab_col>=first_ftr_col) // Check for overlapped labs & ftr
	     && (first_lab_col<(first_ftr_col+num_ftr_cols)) )
	|| total_sents > total_frames
	)
    {
	log.error("Inconsistent pfile header values in '%s' "
		 "- probably corrupted PFile.", QN_FILE2NAME(file));
    }
    delete[] header;
}

// Build a sentence index using the sentence index section in the PFile

void
QN_InFtrLabStream_PFile::build_index_from_sentind_sect()
{
    if (qn_fseek(file, (qn_off_t) sentind_offset, SEEK_SET)!=0)
    {
        log.error("Failed to move to start of sentence index data, "
		  "sentind_offset=" LLD " - probably a corrupted PFile.",
		  sentind_offset);
    }
    long size = sizeof(QN_PFile_Val) * (total_sents + 1);
    if (fread((char *) sentind, size, 1, file)!=1)
    {
	log.error("Failed to read sent_table_data section in '%s'"
		 " - probably a corrupted PFile.", QN_FILE2NAME(file));
    }
    // The index is in big-endian format on file - convert it to the host
    // endianness
    qn_btoh_vi32_vi32(total_sents+1, (QNInt32*) sentind,
		   (QNInt32*) sentind);

    // Check that the index of the frame after the last one is the same
    // as the number of frames in the PFile
    if (sentind[total_sents] != total_frames)
    {
	log.error("Last sentence index (%lu) does not correspond"
		 " with number of frames (%i) in PFile '%s' - probably a"
		 " corrupted PFile.", (unsigned long) sentind[total_sents],
		 total_frames, QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Read sent_table_data for %lu sentences from '%s'.",
	    (unsigned long) total_sents, QN_FILE2NAME(file));
}

// Build a sentence index using the data section in the PFile
// This entails scanning the whole PFile

void
QN_InFtrLabStream_PFile::build_index_from_data_sect()
{
    long last_sentno = -1;	// Number of the last sentence.
    long next_frameno = -1;	// Number of the next frame within sentence.
    long abs_frameno = 0;	// Frame number from beginning of data.

    if (qn_fseek(file, (qn_off_t) data_offset, SEEK_SET)!=0)
    {
        log.error("Failed to move to start of data in '%s',"
		  " data_offset=" LLD " - probably a corrupted PFile.",
		  QN_FILE2NAME(file), data_offset);
    }

    do
    {
	if (fread((char *) buffer, sizeof(QN_PFile_Val)*num_cols,
		  1, file) != 1)
	{
	    if (feof(file))
	    {
		last_sentno++;
		break;
	    }
	    else
	    {
		log.error("Failed to read pfile record from '%s',"
			 "last_sentno=%li next_frameno=%li abs_frameno=%li "
			 "filepos=" LLD " - probably a corrupted PFile.",
			 QN_FILE2NAME(file),
			 last_sentno, next_frameno, abs_frameno,
			  (qn_longlong_t ) qn_ftell(file));
	    }
	}
	// Convert from big endian to native
        long sentno = qn_btoh_i32_i32(buffer[0].l);
        if (sentno != last_sentno)
        {
            last_sentno++;

	    if (last_sentno == (long) total_sents)
	    {
		break;
	    }
            else if (sentno != last_sentno)
            {
		log.error("Non-sequential sentence numbers in "
			 "pfile '%s', "
			 "sentno=%li next_sentno=%li, abs_frameno=%li - "
			 "probably a corrupted PFile.",
			 QN_FILE2NAME(file),
			 sentno, last_sentno, abs_frameno);
            }
	    else
	    {
		sentind[sentno] = (QNUInt32) abs_frameno;
		next_frameno = -1;
	    }
        }
	long frameno = qn_btoh_i32_i32(buffer[1].l);
        next_frameno++;
        if (frameno != next_frameno)
        {
	    log.error("Incorrect frame number in pfile '%s', "
		     "sentno=%li frameno=%li next_frameno=%li abs_frameno=%li"
		     " - probably a corrupted PFile.", QN_FILE2NAME(file),
		     sentno, frameno, next_frameno, abs_frameno);
	}
	abs_frameno++;
    } while(1);

    if (last_sentno!=(long) total_sents)
    {
	log.error("Not enough sentences in pfile '%s', "
		 "header says %lu, file has %li - probably a corrupted "
		 "PFile.", QN_FILE2NAME(file), total_sents, last_sentno);
    }
    // Need to add one extra index so we can calculate the length of the last
    // sentence.
    sentind[total_sents] = (QNUInt32) abs_frameno;

    // Check that the index of the frame after the last one is the same
    // as the number of frames in the PFile
    if ((unsigned long) abs_frameno != (unsigned long) total_frames)
    {
	log.error("Last sentence index (%lu) does not correspond"
		 " with number of frames (%lu) in PFile '%s' - probably a"
		 " corrupted PFile.", (unsigned long) abs_frameno,
		 (unsigned long) total_frames, QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Indexed %lu sentences.", (unsigned long) total_sents);
}

// Read one frame from the PFile, setting "buffer", "pfile_sent" and
// "pfile_frame"

inline void
QN_InFtrLabStream_PFile::read_frame()
{
    int ec;			// Return code

    ec = fread((char *) buffer, bytes_in_row, 1, file);
    if (ec!=1)
    {
	if (feof(file))
	{ 
	    pfile_sent = SENT_EOF;
	    pfile_frame = 0;
	}
	else
	{
	    log.error("Failed to read frame from PFile"
		     " '%s', sent=%li frame=%li row=%li file_offset=" LLD ".",
		     QN_FILE2NAME(file), current_sent, current_frame,
		     current_row, (qn_longlong_t) qn_ftell(file));
	}
    }
    else
    {
	// Convert the PFile data from big endian to host native
	pfile_sent = qn_btoh_i32_i32((QNInt32) buffer[0].l);
	pfile_frame = qn_btoh_i32_i32((QNInt32) buffer[1].l);
    }
}

// Move to the start of the PFile
int
QN_InFtrLabStream_PFile::rewind()
{
    // Move to the start of the data and initialise our own file offset.
    if (qn_fseek(file, (qn_off_t) data_offset, SEEK_SET)!=0)
    {
	log.error("Rewind failed to move to start of data in "
		 "'%s', data_offset=" LLD " - probably corrupted PFile.",
		 QN_FILE2NAME(file), data_offset);
    }
    current_sent = -1;
    current_frame = 0;
    current_row = 0;
    // Read the first frame from the PFile
    read_frame();
    log.log(QN_LOG_PER_EPOCH, "At start of PFile.");
    return 0;			// Should return senence ID
}

size_t
QN_InFtrLabStream_PFile::read_ftrslabs(size_t frames, float* ftrs,
				       QNUInt32* labs)
{
    size_t count;		// Count of number of frames

    long first_frame = current_frame;
    for (count = 0; count<frames; count++)
    {
	if (pfile_sent==current_sent)
	{
	    if (pfile_frame==current_frame)
	    {
		// PFiles are big endian - need to convert
		if (ftrs!=NULL)
		{
		    qn_btoh_vf_vf(num_ftr_cols, &(buffer[first_ftr_col].f), ftrs);
		    ftrs += num_ftr_cols;
		}
		if (labs!=NULL)
		{
		    qn_btoh_vi32_vi32(num_lab_cols,
			     (const QNInt32 *) &(buffer[first_lab_col].l),
			     (QNInt32 *) labs);
		    labs += num_lab_cols;
		}
		read_frame();
		current_frame++;
		current_row++;
	    }
	    else
	    {
		log.error("Inconsistent frame number in PFile '%s',"
			 " sentence=%li frame=%li - probably corrupted PFile.",
			 QN_FILE2NAME(file), current_sent, current_frame);
	    }
	}
	else
	{
	    // Different sentence number - simply return number of frames
	    // so far
	    break;
	}
    }
    log.log(QN_LOG_PER_BUNCH, "Read sentence %lu, first_frame=%lu, "
	    "num_frames=%lu.", (unsigned long) current_sent,
	    (unsigned long) first_frame, (unsigned long) count);
    return count;
}

QN_SegID
QN_InFtrLabStream_PFile::nextseg()
{
    int ret;			// Return value

    // Skip over existing frames in sentence
    size_t skip_count = 0;	// Number of frames we have skipped
    while (current_sent==pfile_sent)
    {
	    if (pfile_frame==current_frame)
	    {
		current_frame++;
		current_row++;
		skip_count++;
		read_frame();
	    }
	    else
	    {
		log.error("Inconsistent frame number in PFile '%s',"
			 " sentence=%li frame=%li - probably corrupted PFile.",
			 QN_FILE2NAME(file), current_sent, current_frame);
	    }
    }
    if (skip_count!=0)
    {
	log.log(QN_LOG_PER_SENT, "Skipped %lu frames in sentence %li.",
		skip_count, current_sent);
    }

    // If possible, check that the end of sentence ties up with the index
    if (indexed)
    {
	if (current_row != (long) sentind[current_sent+1])
	    log.error("Sentence data inconsistent with index in"
		     " PFile '%s' - sentence %li should end at row %li  "
		     "but instead ends at row %li", QN_FILE2NAME(file),
		     current_sent,
		     (long) sentind[current_sent+1],
		     current_row);
	ret = QN_SEGID_BAD;
    }
    // If we are at the end of file, exit gracefully
    if (current_sent==(long) (total_sents-1))
    {
	log.log(QN_LOG_PER_SENT, "At end of PFile.");
	ret = QN_SEGID_BAD;
    }
    else
    {
	current_sent++;
	if (current_sent!=pfile_sent)
	{
	    log.error("Sentence %li in PFile '%s' has sentence "
		     "number %li - probably corrupted PFile.",
		     current_sent, QN_FILE2NAME(file), pfile_sent);
	}
	current_frame = 0;
	if (current_frame!=pfile_frame)
	{
	    log.error("Sentence %li in PFile '%s' has first frame "
		     "number %li, should be 0 - probably corrupted PFile.",
		     current_sent, QN_FILE2NAME(file), pfile_frame);
	}
	log.log(QN_LOG_PER_SENT, "At start of sentence %lu.",
		(unsigned long) current_sent);
	ret = 0;		// FIXME - should return proper segment ID
    }
    return ret;
}

QN_SegID
QN_InFtrLabStream_PFile::set_pos(size_t segno, size_t frameno)
{
    int ret;			// Return value

    assert(segno < total_sents); // Check we do not seek past end of file
    if (indexed)
    {
	long next_sent_row;	// The row at the start of next sent.
	long row;		// The number of the row we require
	qn_longlong_t offset;	// The position as a file offset

	row = sentind[segno] + frameno;
	next_sent_row = sentind[segno+1];
	if (row > next_sent_row)
	{
	    log.error("Seek beyond end of sentence %li.",
		     (unsigned long) segno);
	}
	offset = (qn_longlong_t) bytes_in_row * (qn_longlong_t) row
		+ data_offset;
	
	if (qn_fseek(file, (qn_off_t) offset, SEEK_SET)!=0)
	{
	    log.error("Seek failed in PFile "
		     "'%s', offset=" LLD " - file problem?",
		     QN_FILE2NAME(file), offset);
	}
	current_sent = segno;
	current_frame = frameno;
	current_row = row;

	// Read the frame from the PFile
	read_frame();
	log.log(QN_LOG_PER_SENT, "Seek to sentence %lu, frame %lu.",
		(unsigned long) segno, (unsigned long) frameno);
	ret = 0;		// FIXME - should return sentence ID
    }
    else
    {
	// Tried to seek when not indexed
	ret = QN_SEGID_BAD;
    }
    return ret;
}


int
QN_InFtrLabStream_PFile::get_pos(size_t* segnop, size_t* framenop)
{
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (current_sent==-1)
    {
	segno = QN_SIZET_BAD;
	frameno = QN_SIZET_BAD;
    }
    else
    {
	segno = (size_t) current_sent;
	frameno = (size_t) current_frame;
	assert(current_frame>=0);
    }
    if (segnop!=NULL)
	*segnop = segno;
    if (framenop!=NULL)
	*framenop = frameno;
    log.log(QN_LOG_PER_SENT, "Currently at sentence %lu, frame %lu.",
	    (unsigned long) segno, (unsigned long) frameno);
    return QN_OK;
}


size_t
QN_InFtrLabStream_PFile::num_frames(size_t segno)
{
    size_t num_frames;		// Number of frames returned

    assert(segno<total_sents || segno==QN_ALL);

    if (segno==QN_ALL)
    {
	num_frames = total_frames;
	log.log(QN_LOG_PER_EPOCH, "%lu frames in PFile.",
		(unsigned long) num_frames);
    }
    else
    {
	if (indexed)
	{
	    num_frames = sentind[segno+1] - sentind[segno];
	    log.log(QN_LOG_PER_SENT, "%lu frames in sentence %lu.",
		    (unsigned long) num_frames, (unsigned long) segno);
	}
	else
	    num_frames = QN_SIZET_BAD;
    }
    
    return num_frames;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_PFile
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_OutFtrLabStream_PFile::QN_OutFtrLabStream_PFile(int a_debug,
						   const char* a_dbgname,
						   FILE* a_file,
						   size_t a_ftrs,
						   size_t a_labs,
						   int a_indexed)
  : clog(a_debug, "QN_OutFtrLabStream_PFile", a_dbgname),
    file(a_file),
    indexed(a_indexed),
    num_ftr_cols(a_ftrs),
    num_lab_cols(a_labs),
    current_sent(0),
    current_frame(0),
    current_row(0),
    index(NULL),
    index_len(0)
{
    int ec;			// Error code.

    // Move to the start of the PFile data section
    ec = qn_fseek(file, (qn_off_t) PFILE_HEADER_SIZE, SEEK_SET);
    if (ec!=0)
    {
	clog.error("Failed to seek to data section in output PFile "
		   "'%s' - cannot write PFiles to streams.",
		   QN_FILE2NAME(file) );
    }
    clog.log(1, "Creating PFile '%s'.",  QN_FILE2NAME(file) );

    // Allocate a buffer for one frame.
    buffer = new QN_PFile_Val[num_ftr_cols + num_lab_cols + 2];
    // On SPERT, it is safer to malloc big things early.
    // We could do this later, but do not want to crash with a memory
    // error after writing a big PFile.
    header = new char[PFILE_HEADER_SIZE];
    if (indexed!=0)
    {
	index = new QNUInt32[DEFAULT_INDEX_SIZE];
	index_len = DEFAULT_INDEX_SIZE;
	index[0] = 0;
    }
}

QN_OutFtrLabStream_PFile::~QN_OutFtrLabStream_PFile()
{
    // doneseg() should be called before closing pfile - not doing this
    // is an error.
    if (current_frame!=0)
    {
	clog.error("PFile '%s' closed mid sentence.",
		  QN_FILE2NAME(file));
    }
    if (indexed)
    {
	write_index();
    }
    write_header();
    clog.log(1, "Finished creating PFile '%s'.", 
	     QN_FILE2NAME(file));
    if (index!=NULL)
	delete[] index;
    delete[] header;
    delete[] buffer;
}

void
QN_OutFtrLabStream_PFile::write_ftrslabs(size_t frames, const float* ftrs,
				      const QNUInt32* labs)
{
    size_t i;			// Local counter.
    const size_t num_ftrs = num_ftr_cols; // Local version of value in object.
    const size_t num_labs = num_lab_cols; // Local version of value in object.
    const size_t cols = num_ftrs + num_labs + 2;
    const size_t bytes_in_frame = sizeof(QN_PFile_Val) * cols;

    // Note we convert all data to big endian when we write it.
    for (i=0; i<frames; i++)
    {
	int ec;			// Return code.

	buffer[0].l = qn_htob_i32_i32(current_sent);
	buffer[1].l = qn_htob_i32_i32(current_frame);
	if (num_ftrs!=0)
	{
	    if (ftrs!=NULL)
	    {
		qn_htob_vf_vf(num_ftrs, ftrs, &(buffer[2].f));
		ftrs += num_ftrs;
	    }
	    else
		qn_copy_f_vf(num_ftrs, 0.0f, &(buffer[2].f));
	}
	if (num_labs!=0)
	{
	    if (labs!=NULL)
	    {
		qn_htob_vi32_vi32(num_labs, (const QNInt32*) labs,
			       &(buffer[2+num_ftrs].l));
	    	labs += num_labs;
	    }
	    else
		qn_copy_i32_vi32(num_labs, 0, &(buffer[2+num_ftrs].l));
	}
	ec = fwrite((char*) buffer, bytes_in_frame, 1, file);
	if (ec!=1)
	{
	    clog.error("Failed to write frame to PFile '%s' - %s.",
		       QN_FILE2NAME(file), strerror(errno)); 
	}
	current_frame++;
	current_row++;
    }
}

void
QN_OutFtrLabStream_PFile::doneseg(QN_SegID)
{
    if (current_frame==0)
	clog.error("wrote zero length sentence.");
    clog.log(4, "Finished sentence %lu.",
	     (unsigned long) current_sent); 
    current_sent++;
    current_frame = 0;
    // Update the index if necessary.
    if (indexed)
    {
	// If the index is not large enough, make it bigger.
	if ((size_t) current_sent>=index_len)
	{
	    size_t new_index_len = index_len * 2;
	    QNUInt32* new_index = new QNUInt32[new_index_len];
	    qn_copy_vi32_vi32(index_len, (const QNInt32*) index, 
			   (QNInt32*) new_index);
	    delete[] index;
	    index_len = new_index_len;
	    index = new_index;
	}
	// Update the index.
	index[current_sent] = current_row;
    }
}

void
QN_OutFtrLabStream_PFile::write_index()
{
    // The index is in big-endian format on file - convert it from the host
    // endianness
    qn_htob_vi32_vi32(current_sent+1, (QNInt32*) index,
		   (QNInt32*) index);
    fwrite(index, (current_sent+1) * sizeof(QNUInt32), 1, file);
    clog.log(1, "Wrote sentence index for %lu sentences (%lu entries).",
	     (unsigned long) current_sent, (unsigned long) current_sent+1);
    // Swap it back just in case we want to use it again
    qn_btoh_vi32_vi32(current_sent+1, (QNInt32*) index,
		   (QNInt32*) index);
}

void
QN_OutFtrLabStream_PFile::write_header()
{
    int chars = 0;		// Number of characters added to header this
				// printf
    int count = 0;		// Total number of characters in header so far
    char* ptr = NULL;		// Point into header array
    size_t i;			// Local counter
    int ec;			// Error code

    // Unused sections of the header should be filled with \0.
    memset(header, '\0', PFILE_HEADER_SIZE);
    ptr = header;

    // Note - some sprintfs are broken - cannot use return value.

    // The version string.
    sprintf(ptr, "%s\n", pfile_version0_string);
    chars = strlen(ptr);
    count += chars; ptr += chars;

    // "Vertical" information.
    // -num_sentences
    sprintf(ptr, "-num_sentences %lu\n", (unsigned long) current_sent);
    chars = strlen(ptr);
    count += chars; ptr += chars;
    // -num_frames
    sprintf(ptr, "-num_frames %lu\n", (unsigned long) current_row);
    chars = strlen(ptr);
    count += chars; ptr += chars;

    // Feature information.
    sprintf(ptr, "-first_feature_column %lu\n", (unsigned long) 2);
    chars = strlen(ptr);
    count += chars; ptr += chars;
    sprintf(ptr, "-num_features %lu\n", (unsigned long) num_ftr_cols);
    chars = strlen(ptr);
    count += chars; ptr += chars;

    // Label information.
    sprintf(ptr, "-first_label_column %lu\n",
	    (unsigned long)  2+num_ftr_cols);
    chars = strlen(ptr);
    count += chars; ptr += chars;
    sprintf(ptr, "-num_labels %lu\n", (unsigned long) num_lab_cols);
    chars = strlen(ptr);
    count += chars; ptr += chars;

    // The format string.
    sprintf(ptr, "-format dd");
    chars = strlen(ptr);
    count += chars; ptr += chars;
    for (i=0; i<num_ftr_cols; i++)
	*ptr++ = 'f';
    for (i=0; i<num_lab_cols; i++)
	*ptr++ = 'd';
    count += (num_ftr_cols + num_lab_cols);
    *ptr++ = '\n';
    count++;

    // The details of the data sections.
    size_t cols = num_ftr_cols + num_lab_cols + 2;
    qn_ulonglong_t data_size = (qn_ulonglong_t) cols
	    * (qn_ulonglong_t) current_row;
    sprintf(ptr, "-data size " LLU " offset %lu ndim %lu nrow %lu ncol %lu\n",
	    data_size, (unsigned long) 0, (unsigned long) 2,
	    (unsigned long) current_row, (unsigned long) cols);
    chars = strlen(ptr);
    count += chars; ptr += chars;

    // If necessary, details of the sentence index.
    if (indexed)
    {
	size_t sentind_size = current_sent + 1;
	sprintf(ptr, "-sent_table_data size %lu offset " LLU " ndim 1\n",
		(unsigned long) sentind_size, data_size);
	chars = strlen(ptr);
	count += chars; ptr += chars;
    }

    // The end of the header.
    sprintf(ptr, "-end\n");
    chars = strlen(ptr);
    count += chars; ptr += chars;

    assert((unsigned long) count<=PFILE_HEADER_SIZE);

    // Seek to start of file to write header.
    ec = fseek(file, 0L, SEEK_SET);
    if (ec!=0)
    {
	clog.error("Failed to seek to start of PFile '%s' - %s.",
		   QN_FILE2NAME(file), strerror(errno));
    }
    ec = fwrite(header, PFILE_HEADER_SIZE, 1, file);
    if (ec!=1)
    {
	clog.error("Failed to write header in PFile '%s' - %s.",
		   QN_FILE2NAME(file), strerror(errno));
    }
}

