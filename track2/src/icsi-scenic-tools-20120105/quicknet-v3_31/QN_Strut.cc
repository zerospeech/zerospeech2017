const char* QN_Strut_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_Strut.cc,v 1.5 2010/10/29 02:20:37 davidj Exp $";

// This file contains some miscellaneous routines for handling Struts

#include <QN_config.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"
#include "QN_Strut.h"
#include "QN_Logger.h"

// This is the verision string that appears in the Strut header

static const char* strut_hdr_string = "STRUT_1A";


//////////////////////////////////////////////////
// QN_InFtrStream_STRUT - Read STRUT files (some of them, anyway)
// 2001-04-11 dpwe@ee.columbia.edu
//////////////////////////////////////////////////

// bits from dpwelib/sndfnist.c

static void chop(char *s)
{  /* if last char of s is a newline, overwrite it with a zero */
   int l = strlen(s);
   if(l>0 && (s[l-1]=='\r' || s[l-1]=='\n')) {
       s[l-1] = '\0';
   }
}

static int getNextTok(char **ps, char *dest)
{   /* ps is the address of a char ptr that we modify.
       dest is filled with the next token, null terminated.
       return length of acquired token. */
    int  l1, l2;

    /* skip leading white space */
    l1 = strspn(*ps, " \t\r\n");
    /* copy token */
    l2 = strcspn(*ps+l1, " \t\r\n\0");
    strncpy(dest, *ps+l1, l2);
    dest[l2] = '\0';
    *ps += l1+l2;

    return l2;
}

void 
QN_InFtrStream_Strut::read_header(void) {
  /* Read the NIST-like header of a STRUT file.
     This version works only for STRUT 'probabilities' files. */
#define LBLEN 256
    char lbuf[LBLEN];
    int nhdread = 0;
    int hdtotlen = 0;
    char info_buf[1024];
    char *ib_ptr;
    int done = 0;
    int bigendian = 1;
    
    sentind_offset = 0;
    data_offset = 0;
    num_cols = 0;
    total_sents = 0;
    total_frames = 0;

    nhdread = fread(lbuf, 1, strlen(strut_hdr_string)+1, file);
    lbuf[strlen(strut_hdr_string)+1] = '\0'; chop(lbuf);
    if(strcmp(lbuf, strut_hdr_string)!=0) {
	fprintf(stderr, "STRUT SFRdHdr: id of '%s' is not '%s' - lose\n", 
		lbuf, strut_hdr_string);
	return;
    }
    /* read header values */
    /* first line is number of bytes in header */
    fgets(lbuf, LBLEN, file);    nhdread += strlen(lbuf);    chop(lbuf);
    hdtotlen = atoi(lbuf);
    /* rest are token/value pairs */
    /* reset the info_buf used to store unknown fields */
    info_buf[0] = '\0';
    ib_ptr = info_buf;

    while(!done && !feof(file)) {
	char tok[LBLEN], *t;
	int tklen;

	t = lbuf;

	fgets(lbuf, LBLEN, file);    nhdread += strlen(lbuf);    chop(lbuf);

/* fprintf(stderr, "HDR: got '%s'\n", lbuf); */

	/* we need to find:
	   - the start of the actual data
	   - dimensions of the actual data */

	/* check for:
	   file_type -s13 probabilities
	   data_format -s9 BigEndian
	   utterance_count -i 200
	   utterance_start_offset -i 8052 ?? 0x1f74 start of utlen table
	   data_offset -i 8856 ?? 0x2298           start of actual data
	   data_size -i 3268836 ??
	   frame_count -i 30267
	   feature_dimension -i 4
	   probability_count -i 27
	*/

	tklen = getNextTok(&t, tok);
	if(strcmp(tok, "end_head")==0)  {
	    done = 1;
	} else if(strcmp(tok, "file_type")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -sN */
	    tklen = getNextTok(&t, tok);		/* actual type */
	    if (strcmp(tok, "probabilities")!=0) {
	      log.error("Type of STRUT file %s is '%s' not 'probabilities' - lose.", QN_FILE2NAME(file), tok);
	    }
	} else if(strcmp(tok, "data_format")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -sN */
	    tklen = getNextTok(&t, tok);		/* actual format */
	    if(strcmp(tok, "BigEndian")==0) {
	      bigendian = 1;
	    } else if(strcmp(tok, "LittleEndian")==0) {
	      bigendian = 0;
	    } else {
	      log.error("Format of STRUT file %s is '%s' not 'BigEndian' or 'LittleEndian - lose.", QN_FILE2NAME(file), tok);
	    }
	} else if(strcmp(tok, "utterance_count")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    total_sents = atol(tok);
	} else if(strcmp(tok, "frame_count")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    total_frames = atol(tok);
	} else if(strcmp(tok, "utterance_start_offset")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual offset */
	    sentind_offset = atol(tok);
	} else if(strcmp(tok, "data_offset")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual offset */
	    data_offset = atoi(tok);
	} else if(strcmp(tok, "probability_count")==0) {
	    tklen = getNextTok(&t, tok);
	    tklen = getNextTok(&t, tok);
	    num_cols = atoi(tok);
	} else {
  	    /* not an anticipated field - ignore, I guess */
	    if(strlen(lbuf)) {
	      //sprintf(ib_ptr, "%s\n", lbuf);
	      //ib_ptr += strlen(ib_ptr);
	    }
	}
    }
    if(!done) {
	/* hit eof before completing */
      log.error("Hit end of STRUT file %s before end_head.",
		QN_FILE2NAME(file));
    }
    if (sentind_offset == 0 \
	|| data_offset == 0 \
	|| num_cols == 0 \
	|| total_sents == 0 \
	|| total_frames == 0) {
      log.error("STRUT file %s did not include all required header fields.",
		QN_FILE2NAME(file));
    }
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_Strut
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrStream_Strut::QN_InFtrStream_Strut(int a_debug,
					   const char* a_dbgname,
					   FILE* a_file)
  : log(a_debug, "QN_InFtrStream_Strut", a_dbgname),
    file(a_file),
    buffer(NULL),
    sentind(NULL)
{
    if (fseek(file, 0, SEEK_SET)) {
	log.error("Failed to seek to start of strut '%s' header - "
		 "cannot read Struts from streams.",
		 QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Started using Strut '%s'.", QN_FILE2NAME(file));
    read_header();
    bytes_in_row = num_cols * sizeof(float);
    log.log(QN_LOG_PER_RUN, "num_sents=%u num_frames=%u num_ftrs=%u.",
	    total_sents, total_frames, num_cols);

    // Allocate frame buffer.
    buffer = new float[num_cols];

    // Allocate space for sentence index
    sentind = new QNUInt32[total_sents + 1];
    read_index();

    // Move to the start of the Strut
    rewind();
}

QN_InFtrStream_Strut::~QN_InFtrStream_Strut()
{
  delete[] sentind;
  delete[] buffer;
}

// Read the sentence length index in the strut header
// Happily, it's exactly the same format as a Pfile index:
// nsents+1 byte offsets to the start of each sentence from the 
// start of the data segment!
void
QN_InFtrStream_Strut::read_index()
{
    if (fseek(file, sentind_offset, SEEK_SET)!=0)
    {
        log.error("Failed to move to start of sentence index data, "
		  "sentind_offset=%li - probably a corrupted Strut header.",
		  sentind_offset);
    }
    long size = sizeof(QNUInt32) * (total_sents + 1);
    if (fread((char *) sentind, size, 1, file)!=1)
    {
	log.error("Failed to read sent_table_data section in '%s'"
		 " - probably a corrupted Strut header.", QN_FILE2NAME(file));
    }
    // The index is in big-endian format on file - convert it to the host
    // endianness
    qn_btoh_vi32_vi32(total_sents+1, (QNInt32*) sentind,
		   (QNInt32*) sentind);

    // Scale down from byte counts to row counts
    size_t i;
    QNUInt32 row;
    for (i = 0; i < total_sents + 1; ++i) {
      row = sentind[i] / (num_cols*sizeof(float));
      if (sentind[i] != row * num_cols*sizeof(float)) {
	log.error("In strut file %s index for sent %lu is not an exact "
		  "multiple of row bytes %d.", QN_FILE2NAME(file),
		  static_cast<unsigned long> (i), num_cols*sizeof(float));
      }
      sentind[i] = row;
    }
    // Check that the index of the frame after the last one is the same
    // as the number of frames in the Strut
    if (sentind[total_sents] != total_frames)
    {
	log.error("Last sentence index (%lu) does not correspond"
		 " with number of rows (%i) in file '%s' - probably a"
		 " corrupted Strut header.", (unsigned long) sentind[total_sents],
		 total_frames, QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Read sent_table_data for %lu sentences from '%s'.",
	    (unsigned long) total_sents, QN_FILE2NAME(file));
}

inline void
QN_InFtrStream_Strut::read_frame()
{
    int ec;			// Return code

    ec = fread((char *) buffer, num_cols*sizeof(float), 1, file);
    if (ec!=1) {
	if (feof(file))	{ 
	  // hit EOF
	} else{
	    log.error("Failed to read frame from Strut file "
		     " '%s', sent=%li frame=%li row=%li file_offset=%li.",
		     QN_FILE2NAME(file), current_sent, current_frame,
		     current_row, ftell(file));
	}
    }
}

// Move to the start of the Strut
int
QN_InFtrStream_Strut::rewind()
{
    // Move to the start of the data and initialise our own file offset.
    if (fseek(file, data_offset, SEEK_SET)!=0)
    {
	log.error("Rewind failed to move to start of data in "
		 "'%s', data_offset=%li - probably corrupted Strut file.",
		 QN_FILE2NAME(file), data_offset);
    }
    current_sent = -1;
    current_frame = 0;
    current_row = 0;
    // Read the first frame from the Strut
    read_frame();
    log.log(QN_LOG_PER_EPOCH, "At start of Strut.");
    return 0;			// Should return senence ID
}

size_t
QN_InFtrStream_Strut::read_ftrs(size_t frames, float* ftrs)
{
    size_t count;		// Count of number of frames

    long first_frame = current_frame;
    QNUInt32 end_row = sentind[current_sent+1];

    for (count = 0; count<frames && current_row < end_row; count++) {
      // Struts are big endian - need to convert
      if (ftrs!=NULL) {
	qn_btoh_vf_vf(num_cols, buffer, ftrs);
	ftrs += num_cols;
      }
      read_frame();
      ++current_frame;
      ++current_row;
    }
    log.log(QN_LOG_PER_BUNCH, "Read sentence %lu, first_frame=%lu, "
	    "num_frames=%lu.", (unsigned long) current_sent,
	    (unsigned long) first_frame, (unsigned long) count);
    return count;
}

QN_SegID
QN_InFtrStream_Strut::nextseg()
{
    int ret;			// Return value

    // Skip over existing frames in sentence
    size_t skip_count = 0;	// Number of frames we have skipped
    while (current_row < sentind[current_sent+1]) {
      current_frame++;
      current_row++;
      skip_count++;
      read_frame();
    }
    if (skip_count!=0) {
	log.log(QN_LOG_PER_SENT, "Skipped %lu frames in sentence %li.",
		skip_count, current_sent);
    }

    // If we are at the end of file, exit gracefully
    if (current_sent==(long) (total_sents-1))    {
	log.log(QN_LOG_PER_SENT, "At end of Strut file.");
	ret = QN_SEGID_BAD;
    } else {
	current_sent++;
	current_frame = 0;
	log.log(QN_LOG_PER_SENT, "At start of sentence %lu.",
		(unsigned long) current_sent);
	ret = 0;		// FIXME - should return proper segment ID
    }
    return ret;
}

QN_SegID
QN_InFtrStream_Strut::set_pos(size_t segno, size_t frameno)
{
    int ret;			// Return value

    assert(segno < total_sents); // Check we do not seek past end of file

    long this_sent_row;	// The number of the row at the start of sent.
    long next_sent_row;	// The row at the start of next sent.
    long row;		// The number of the row we require
    long offset;		// The position as a file offset

    this_sent_row = sentind[segno];
    row = sentind[segno] + frameno;
    next_sent_row = sentind[segno+1];
    if (row > next_sent_row) {
      log.error("Seek beyond end of sentence %li.",
		(unsigned long) segno);
    }
    offset = bytes_in_row * row + data_offset;
	
    if (fseek(file, offset, SEEK_SET)!=0) {
      log.error("Seek failed in Strut file "
		"'%s', offset=%li - file problem?",
		QN_FILE2NAME(file), offset);
    }
    current_sent = segno;
    current_frame = frameno;
    current_row = row;

    // Read the frame from the Strut
    read_frame();
    log.log(QN_LOG_PER_SENT, "Seek to sentence %lu, frame %lu.",
	    (unsigned long) segno, (unsigned long) frameno);
    ret = 0;		// FIXME - should return sentence ID

    return ret;
}


int
QN_InFtrStream_Strut::get_pos(size_t* segnop, size_t* framenop)
{
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (current_sent==-1) {
	segno = QN_SIZET_BAD;
	frameno = QN_SIZET_BAD;
    } else {
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
QN_InFtrStream_Strut::num_frames(size_t segno)
{
    size_t num_frames;		// Number of frames returned

    assert(segno<total_sents || segno==QN_ALL);

    if (segno==QN_ALL) {
	num_frames = total_frames;
	log.log(QN_LOG_PER_EPOCH, "%lu frames in Strut file.",
		(unsigned long) num_frames);
    } else {
      num_frames = sentind[segno+1] - sentind[segno];
      log.log(QN_LOG_PER_SENT, "%lu frames in sentence %lu.",
	      (unsigned long) num_frames, (unsigned long) segno);
    }
    
    return num_frames;
}

