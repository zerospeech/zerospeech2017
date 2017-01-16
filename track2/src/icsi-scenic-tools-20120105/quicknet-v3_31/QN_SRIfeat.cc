const char* QN_SRIfeat_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_SRIfeat.cc,v 1.12 2010/10/29 02:43:30 davidj Exp $";

// This file contains routines for handling SRI feature files. It is based
// on QN_Strut.cc.

#include <QN_config.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_fltvec.h"
#include "QN_SRIfeat.h"
#include "QN_Logger.h"

// This is the verision string that appears in the SRI (NIST) header
const static char *sri_hdr_string = "NIST_1A";
// This is the default length of NIST headers (used by the output stream)
const static unsigned sri_hdr_len = 1024;
// The two types of feature-like files
const static char *sri_cepfile_type = "cepfile";
const static char *sri_featurefile_type = "featurefile";


//////////////////////////////////////////////////
// QN_InFtrStream_SRI - Read SRI feature files, which are basically
// NIST sphere-headered files.
// 2001-06-26 wooters@ICSI.Berkeley.Edu
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
QN_InFtrStream_SRI::read_header(void) {
  /* Read the NIST-like header. */
#define LBLEN 256
    char lbuf[LBLEN];
    int nhdread = 0;
    int done = 0;
    
    data_offset = 0;
    num_cols = 0;
    total_frames = 0;

    nhdread = fread(lbuf, 1, strlen(sri_hdr_string)+1, file);
    lbuf[strlen(sri_hdr_string)+1] = '\0'; chop(lbuf);
    if(strcmp(lbuf, sri_hdr_string)!=0) {
	fprintf(stderr, "SRI Feat File Hdr: id of '%s' is not '%s' - lose\n", 
		lbuf, sri_hdr_string);
	return;
    }
    /* read header values */
    /* first line is number of bytes in header */
    /* this also indicates offset to data */
    fgets(lbuf, LBLEN, file);    nhdread += strlen(lbuf);    chop(lbuf);
    data_offset = static_cast<unsigned int> (atoi(lbuf));

    /* rest are token/value pairs */

    while(!done && !feof(file)) {
	char tok[LBLEN], *t;
	int tklen;

	t = lbuf;

	fgets(lbuf, LBLEN, file);    nhdread += strlen(lbuf);    chop(lbuf);

	/* we need to find:
	   - the number of frames of data
	   - the dimension of each frame */

	/* check for:
	   file_type -s7 cepfile
	   num_elements -i x
	   num_frames -i y
	*/

	tklen = getNextTok(&t, tok);
	if(strcmp(tok, "end_head")==0)  {
	    done = 1;
	} else if(strcmp(tok, "file_type")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -sN */
	    tklen = getNextTok(&t, tok);		/* actual type */
	    // We don't care whether it is a featurefile or cepfile.
 	    if (strcmp(tok, sri_cepfile_type)!=0 &&
 	        strcmp(tok, sri_featurefile_type)!=0) {
 	      log.error("Type of SRI file %s is '%s' not '%s' or '%s' - lose.", 
 			QN_FILE2NAME(file), tok,
 			sri_cepfile_type, sri_featurefile_type);
	    }
	} else if(strcmp(tok, "num_elements")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    num_cols = static_cast<unsigned int>(atol(tok));
	} else if(strcmp(tok, "num_frames")==0) {
	    tklen = getNextTok(&t, tok);		/* format spec - -i */
	    tklen = getNextTok(&t, tok);		/* actual count */
	    total_frames = static_cast<unsigned int>(atol(tok));
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
      log.error("Hit end of SRI feat file %s before end_head.",
		QN_FILE2NAME(file));
    }
    if (data_offset == 0 || num_cols == 0 || total_frames == 0) {
      log.error("SRI feat file %s did not include all required header fields.",
		QN_FILE2NAME(file));
    }
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_SRI
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrStream_SRI::QN_InFtrStream_SRI(int a_debug,
				       const char* a_dbgname,
				       FILE* a_file)
  : log(a_debug, "QN_InFtrStream_SRI", a_dbgname),
    file(a_file),
    buffer(NULL)
{
    if (fseek(file, 0, SEEK_SET)) {
	log.error("Failed to seek to start of SRI feat file '%s' header - "
		 "cannot read features from this stream.",
		 QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Started using file '%s'.", QN_FILE2NAME(file));
    read_header();
    bytes_in_row = num_cols * sizeof(float);
    log.log(QN_LOG_PER_RUN, "num_sents=%u num_frames=%u num_ftrs=%u.",
	    total_sents, total_frames, num_cols);

    // Allocate frame buffer.
    buffer = new float[num_cols];

    // Move to the start of the features
    rewind();
}

QN_InFtrStream_SRI::~QN_InFtrStream_SRI()
{
  delete[] buffer;
}


inline void
QN_InFtrStream_SRI::read_frame()
{
    int ec;			// Return code

    ec = fread(reinterpret_cast<char *>(buffer), 
	       num_cols*sizeof(float), 1, file);
    if (ec!=1) {
	if (feof(file))	{ 
	  // hit EOF
	} else{
	    log.error("Failed to read frame of data from SRI file "
		     " '%s', frame=%lu file_offset=%lu.",
		     QN_FILE2NAME(file), current_frame, ftell(file));
	}
    }
}

// Move to the start of the features
int
QN_InFtrStream_SRI::rewind()
{
    // Move to the start of the data and initialise our own file offset.
    if (fseek(file, data_offset, SEEK_SET)!=0)
    {
	log.error("Rewind failed to move to start of data in "
		 "'%s', data_offset=%u - probably corrupted file.",
		 QN_FILE2NAME(file), data_offset);
    }
    current_sent = -1;
    current_frame = 0;
    // Read the first frame from the file
    read_frame();
    log.log(QN_LOG_PER_EPOCH, "At start of SRI feat file.");
    return 0;			// Should return senence ID
}

size_t
QN_InFtrStream_SRI::read_ftrs(size_t frames, float* ftrs)
{
    size_t count;		// Count of number of frames

    long first_frame = current_frame;

    for (count = 0; count<frames && current_frame < total_frames; count++) {
      // files are big endian - need to convert
      if (ftrs!=NULL) {
	qn_btoh_vf_vf(num_cols, buffer, ftrs);
	ftrs += num_cols;
      }
      read_frame();
      ++current_frame;
    }
    log.log(QN_LOG_PER_BUNCH, "Read frames: first_frame=%lu, "
	    "num_frames=%lu.",
	    (unsigned long) first_frame, (unsigned long) count);
    return count;
}

QN_SegID
QN_InFtrStream_SRI::nextseg()
{
  // current_sent will be set to -1 by a call to rewind(), which can happen
  // in the constructor, or elsewhere.
  if(current_sent >= 0) {	// can't go to next segment, no more segs
    // exit gracefully
    log.log(QN_LOG_PER_SENT, 
	    "Can't go to next segment: Only one segment per SRI feat file.");
    return QN_SEGID_BAD;
  }
  // haven't begun reading first segment (utterance) yet, so we're ok
  current_sent++;
  log.log(QN_LOG_PER_SENT, "At start of utterance %lu.",
	  static_cast<unsigned long>(current_sent));
  return 0;

}

QN_SegID
QN_InFtrStream_SRI::set_pos(size_t segno, size_t frameno)
{
    long offset;		// The position as a file offset

    if (segno >= total_sents) {
      log.error("Tried to seek to utterance %lu. Only one utterance in file.",
		static_cast<unsigned long> (segno));
    }

    if (frameno >= total_frames) {
      log.error("Tried to seek to frame %lu, which is past last frame.",
		static_cast<unsigned long> (frameno));
    }

    // calculate seek offset
    offset = static_cast<long>(bytes_in_row * frameno + data_offset);
	
    if (fseek(file, offset, SEEK_SET)!=0) {
      log.error("Seek failed in SRI feature file "
		"'%s', offset=%lu - file problem?",
		QN_FILE2NAME(file), offset);
    }
    current_sent = static_cast<long>(segno);
    current_frame = static_cast<long>(frameno);

    // Read the frame from the file
    read_frame();

    log.log(QN_LOG_PER_SENT, "Seek to sentence %lu, frame %lu.",
	    static_cast<unsigned long> (segno), 
	    static_cast<unsigned long> (frameno));

    return static_cast<QN_SegID>(0);		// return sentence ID
}


int
QN_InFtrStream_SRI::get_pos(size_t* segnop, size_t* framenop)
{
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (current_sent==-1) {
	segno = QN_SIZET_BAD;
	frameno = QN_SIZET_BAD;
    } else {
	segno = static_cast<size_t> (current_sent);
	frameno = static_cast<size_t> (current_frame);
	assert(current_frame>=0);
    }
    if (segnop!=NULL)
	*segnop = segno;
    if (framenop!=NULL)
	*framenop = frameno;
    log.log(QN_LOG_PER_SENT, "Currently at sentence %lu, frame %lu.",
	    static_cast<unsigned long> (segno), 
	    static_cast<unsigned long> (frameno));
    return QN_OK;
}

size_t
QN_InFtrStream_SRI::num_frames(size_t segno)
{
    assert(segno<total_sents || segno==QN_ALL);

    log.log(QN_LOG_PER_EPOCH, "%lu frames in file.",
		static_cast<unsigned long> (total_frames));
    
    return static_cast<size_t> (total_frames);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_SRI - output stream for SRI format ftrs
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// Copied from QN_OutFtrStream_HTK

QN_OutFtrStream_SRI::QN_OutFtrStream_SRI(int a_debug, 
                                         const char* a_dbgname,
                                         FILE* a_file, 
                                         size_t a_n_ftrs,
					 const char* a_ftr_name) 
    : clog(a_debug, "QN_OutFtrStream_SRI", a_dbgname), 
      file(a_file),
      buffer(NULL),
      n_ftrs(a_n_ftrs), 
      current_seg(0), 
      frames_this_seg(0), 
      bytes_per_frame(sizeof(float)*n_ftrs), 
      bos(1),
      header_pos(-1)
{
    assert(n_ftrs!=0);
    // Non-null means "featurefile" rather than "cepfile"
    if (a_ftr_name)
    {
	ftr_name = new char[strlen(a_ftr_name) + 1];
	strcpy(ftr_name, a_ftr_name);
    }
    else
	ftr_name = NULL;
    buffer = new float[n_ftrs];
    clog.log(QN_LOG_PER_RUN, "Creating SRI file '%s'.",  QN_FILE2NAME(file) );
}

QN_OutFtrStream_SRI::~QN_OutFtrStream_SRI() {
  if (!bos) {
    clog.error("tried to close file without finishing segment.");
  }
  clog.log(QN_LOG_PER_RUN, "Finished creating SRI file '%s'.", 
	   QN_FILE2NAME(file));
  delete[] buffer;
}

// Write a NIST Sphere header
void QN_OutFtrStream_SRI::write_hdr(char* xtra_info) {
  /* From the Sphere 2.6 documentation:
     SPHERE files contain a strictly defined header portion followed by
     the file body (waveform).  Any waveform encoding may be used, but the
     encoding must be sufficiently described in the header.

     The header is an object-oriented, 1024-byte blocked, ASCII structure
     which is prepended to the waveform data.  The header is composed of a
     fixed-format portion followed by an object-oriented variable portion.
     The fixed portion is as follows:

     NIST_1A<new-line>
        1024<new-line>

     The first line specifies the header type and the second line specifies the
     header length.  Each of these lines are 8 bytes long (including new-line)
     and are structured to identify the header as well as allow those who do 
     not wish to read the subsequent header information to programmatically 
     skip over it. 
  */
  char *hdr_buff = new char[sri_hdr_len]; // this is the whole header
  for(unsigned int i=0;i<sri_hdr_len;i++) // fill header with spaces
    hdr_buff[i] = ' ';
  // the string 'sri_hdr_string' is defined above as "NIST_1A"
  sprintf(hdr_buff,"%7s",sri_hdr_string);
  // the value of 'sri_hdr_len' is defined above as 1024
  sprintf(hdr_buff,"%s\n%7u",hdr_buff,sri_hdr_len);
  if (ftr_name!=NULL)  {
      sprintf(hdr_buff,"%s\nfile_type -s%lu %s",hdr_buff,
	      (unsigned long) strlen(sri_featurefile_type),
	      sri_featurefile_type);
      sprintf(hdr_buff,"%s\nfeature_names -s%lu %s",hdr_buff,
	      (unsigned long) strlen(ftr_name),ftr_name);
  }
  else {
      sprintf(hdr_buff,"%s\nfile_type -s%lu %s",hdr_buff,
	      strlen(sri_cepfile_type),sri_cepfile_type);
  }
  sprintf(hdr_buff,"%s\nsample_coding -s7 feature",hdr_buff);
  sprintf(hdr_buff,"%s\nsample_byte_format -s2 10",hdr_buff);
  sprintf(hdr_buff,"%s\nsample_count -i %lu",hdr_buff,(unsigned long)n_ftrs*frames_this_seg);
  sprintf(hdr_buff,"%s\nsample_n_bytes -i 2",hdr_buff);
  sprintf(hdr_buff,"%s\nchannel_count -i 1",hdr_buff);
  sprintf(hdr_buff,"%s\nnum_elements -i %lu",hdr_buff,(unsigned long) n_ftrs);
  sprintf(hdr_buff,"%s\nnum_frames -i %lu",hdr_buff,(unsigned long) frames_this_seg);
  if(xtra_info != NULL) {
    chop(xtra_info);		// remove final new line, if there
    if(strlen(xtra_info) > 0) {
      // make sure there is enough room in the header for xtra_info
      size_t len = strlen(xtra_info);
      size_t curbuflen = strlen(hdr_buff) + 10; // add 10 for "\nend_head\n"
      assert(sri_hdr_len > curbuflen); // just to be sure...
      if(len < (sri_hdr_len - curbuflen))
	sprintf(hdr_buff,"%s\n%s",hdr_buff,xtra_info);
      else { 
	// xtra_info was too large to fit in remaining space in the header
	// so create a smaller version of xtra_info and put it into
	// the header as a comment
	size_t xtra_space = sri_hdr_len-curbuflen-2;
	if(xtra_space > 1){
	  char *tmpbuf = new char[xtra_space];
	  strncpy(tmpbuf,xtra_info,xtra_space-1);
	  tmpbuf[xtra_space-1] = '\0'; // add terminating null
	  sprintf(hdr_buff,"%s\n;%s",hdr_buff,tmpbuf);
	  delete [] tmpbuf;
	}
      }
    }
  }
  strcat(hdr_buff,"\nend_head\n");

  header_pos = ftell(file);
  if(fwrite(hdr_buff,sri_hdr_len,1,file) != 1) {
        clog.error("Header write failed for segment %d of file "
                   "%s.", current_seg, QN_FILE2NAME(file));
  }
  delete[] hdr_buff;
}

void QN_OutFtrStream_SRI::write_ftrs(size_t cnt, const float* ftrs)
{
    int ec;
    size_t i;

    // write a header if we are at the beginning of a segment
    if (bos) {
	write_hdr();
	bos = 0;
    }
    for (i=0; i<cnt; i++)
    {
	qn_htob_vf_vf(n_ftrs,ftrs, buffer);
	ftrs += n_ftrs;
	ec = fwrite(buffer, sizeof(float), n_ftrs, file);
	if (ec!=n_ftrs)
	{
	    clog.error("Failed to write frame to SRI file '%s' - %s.",
		       QN_FILE2NAME(file), strerror(errno)); 
	}
    }
    frames_this_seg += cnt;
    clog.log(QN_LOG_PER_BUNCH, "Wrote frames: num_frames=%lu.",
	    (unsigned long) cnt);
}

void QN_OutFtrStream_SRI::doneseg(QN_SegID segid) {
    // rewrite the number of frames in this segment with the final value
    if (bos) {
        clog.error("doneseg called before any frames written in seg %d "
                   "of file %s.", current_seg, QN_FILE2NAME(file));
    }

    if (fseek(file, header_pos, SEEK_SET)!=0) {
        clog.error("Seek for header rewrite failed in file "
                   "'%s', offset=%li - file problem?",
                   QN_FILE2NAME(file), header_pos);
    }
    write_hdr();

    // Here is a note from QN_HTKstream.cc:
    //
    // I'm getting bugs where the rewritten size isn't getting 
    // written out to disk (1999dec03)
    //        fprintf(stderr, "HTK::doneseg: rewrote framelen %d at pos %d\n", 
    //              frames_this_seg, ftell(file));
    // Saw it again on FUDGE, 2000-06-26.  Symptom is that test 
    // fails when writing HTK files across NFS, but writing them 
    // to the local disk works OK!!!

    // 2000-06-26 - think I fixed it.  Problem was that files were being
    // opened in mode "w" by feacat, which is necessary in general if 
    // we want to write .gz files through popen, but no good for 
    // pfiles, htk files and ilab files that want to seek back and 
    // rewrite their headers.  So I changed feacat to open in mode 
    // "w+" for these file types (only), and the bug went away
    // (although it was always pretty elusive, so I may be fooling 
    // myself).

    // Go back to the end to resume writing
    if (fseek(file, 0, SEEK_END)!=0) {
        clog.error("Reseek to EOF after header rewrite failed in file "
                   "'%s', segment %d.",
                   QN_FILE2NAME(file), current_seg);
    }

    // next write will need a new header
    bos = 1;
    ++current_seg;
    frames_this_seg = 0;

    clog.log(QN_LOG_PER_SENT, "Finished segment.");
}
