// $Header: /u/drspeech/repos/quicknet2/QN_ListStream.cc,v 1.5 2004/09/08 01:28:14 davidj Exp $

// QN_ListStream.cc

// -*- C++ -*-
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "QN_intvec.h"
#include "QN_ListStream.h"
#include "QN_Logger.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_List - base class for a list of files
//   To see an example derived class implementation of this
//   class, take a look at 'QN_listStreamSRI.cc'.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
QN_InFtrStream_List::QN_InFtrStream_List(int a_debug, 
			      const char* a_dbgname, FILE* a_file,
			      int a_indexed) 
    : log(a_debug, "QN_InFtrStream_List", a_dbgname), 
      debug(a_debug),
      dbgname(a_dbgname),
      file(a_file), 
      current_seg(-1), 
      current_frame(0), 
      curr_seg_name(NULL),
      frames_this_seg(0),
      frame_ftrs(QN_SIZET_BAD), 
      frame_bytes(QN_SIZET_BAD), 
      header_peeked(0),
      indexed(a_indexed), 
      n_segs(QN_SIZET_BAD), 
      n_rows(QN_SIZET_BAD), 
      segind(NULL),
      segind_len(0)
{
    log.log(QN_LOG_PER_RUN, "Creating InFtrStream_List from file '%s'.",
	    QN_FILE2NAME(a_file));
    curr_seg_name = new char[BUFSIZ];
}

/*
** This method MUST be called from within the derived class constructor
** in order to initialize all of the base class variables.
*/
void
QN_InFtrStream_List::init() {
    // If required, build an index of the file.
    if (indexed) {
      log.log(QN_LOG_PER_RUN, "Creating index.");
      // Allocate and build buffer
      segind_len = DEFAULT_INDEX_LEN;
      segind = new QNUInt32[segind_len];
      file_pos_tab = new QNUInt32[segind_len];
      build_index();
      log.log(QN_LOG_PER_RUN, "Indexed stream, n_segs=%lu n_rows=%lu.",
	      n_segs, n_rows);
      if (rewind()==QN_BAD) {
	log.error("Failed to seek to start of %s after "
		  " building index.", QN_FILE2NAME(file));
      }
    }
    // We need to know how many bytes per frame there are so we can
    // calculate the number of features per frame
    if (frame_bytes == QN_SIZET_BAD) {
      // Need to peek at the first header to get the number of bytes.
      // nextseg() will call read_header() which will set frame_bytes.
      if(nextseg() == QN_SEGID_BAD) {
	    log.error("Failed to read first header of %s.", 
		      QN_FILE2NAME(file));
      }
      // set this so that the next call to nextseg() will do nothing
      header_peeked = 1;
    }
    int bytes_per_sample = sizeof(float);
    frame_ftrs = frame_bytes/bytes_per_sample;
}

QN_InFtrStream_List::~QN_InFtrStream_List() {
    if (segind) {
	delete [] segind;
	segind = NULL;
    }
    if(file_pos_tab) {
      delete [] file_pos_tab;
      file_pos_tab = NULL;
    }
    delete [] curr_seg_name;
}

/*
** Rewind the list stream.
*/
int QN_InFtrStream_List::rewind(void) {
    int ec;
    int ret;			// Return code

    ec = fseek(file, 0L, SEEK_SET);
    // Sometimes it is valid for rewind to fail (e.g. on pipes),
    // so return an error code rather than crash.
    if (ec!=0) {
	log.log(QN_LOG_PER_EPOCH, "Rewind failed for %s.", 
		QN_FILE2NAME(file));
	ret = QN_BAD;
    } else {
	current_seg = -1;
	current_frame = 0;
	log.log(QN_LOG_PER_EPOCH, "File %s rewound.", 
		QN_FILE2NAME(file));
	ret = QN_OK;
    }
    return ret;    
}

/*
** Increments current_seg and sets current_frame
** Returns QN_SEGID_BAD if it can't move to the next segment (i.e. EOF)
*/
QN_SegID QN_InFtrStream_List::nextseg() {
  QN_SegID rc = QN_SEGID_BAD;	// default return indicates EOF

  if(header_peeked){
    header_peeked=0;
    return QN_SEGID_UNKNOWN;
  }

  current_seg++;

  if(indexed && static_cast<size_t>(current_seg) >= n_segs) // check for EOF
    return QN_SEGID_BAD;

  if(fgets(curr_seg_name,BUFSIZ,file)==NULL) // get next utt name
    return QN_SEGID_BAD;
  // get rid of newline
  curr_seg_name[strlen(curr_seg_name)-1] = static_cast<char>(NULL);

  // Read the next header (else EOF)
  if(read_header() != -1) {
    rc = QN_SEGID_UNKNOWN;
  }
  current_frame = 0;

  return rc;
}

/*
** Return the number of frames in the given segment, or all segments.
*/
size_t QN_InFtrStream_List::num_frames(size_t segno /* = QN_ALL */) {
    size_t ret;			// Return value.

    if (!indexed) {		// If not indexed, do not know no. of frames.
	ret = QN_SIZET_BAD;
    } else {
	if (segno == QN_ALL) {  // QN_ALL means do for whole stream.
	    ret = n_rows;
	} else {
	    // Look up number of frames in segment index.
	    assert (segno<n_segs);
	    ret = segind[segno+1] - segind[segno]; 
	}
    }
    return ret;
}

/*
** Will put the current segment number into segnop and the current
** frame number into framenop. If we are not at a valid position in
** the stream (i.e. current_seg == -1) then segnop and framenop get
** QN_SIZET_BAD.
** Returns QN_OK if indexed, QN_BAD otherwise.
*/
int QN_InFtrStream_List::get_pos(size_t* segnop, size_t* framenop) {
    int ret;			// Return value
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (!indexed) {
	ret = QN_BAD;
    } else {
	if (current_seg==-1) {	// we are not at a valid position yet
	    segno = QN_SIZET_BAD;
	    frameno = QN_SIZET_BAD;
	} else {		// we are at a valid position
	    segno = static_cast<size_t>(current_seg);
	    frameno = static_cast<size_t>(current_frame);
	    assert(current_frame>=0);
	}
	if (segnop!=NULL)
	    *segnop = segno;
	if (framenop!=NULL)
	    *framenop = frameno;
	log.log(QN_LOG_PER_SENT, "Currently at sentence %lu, frame %lu.",
		static_cast<unsigned long>(segno), 
		static_cast<unsigned long>(frameno));
	ret = QN_OK;
    }
    return ret;
}

/*
** This function will position the stream at the beginning of the frame 
** given by frameno in the segment given in segno, such that the next
** call to read_ftrs() will begin with frame number frameno. The value of
** frameno is relative to the segment, not the whole stream, thus frameno
** should never be greater than or equal to the number of frames in segno.
** Assuming that frameno starts from 0.
** Returns QN_SEGID_UNKNOWN if indexed, QN_SEGID_BAD otherwise.
*/
QN_SegID QN_InFtrStream_List::set_pos(size_t segno, size_t frameno) {
    int ret;			// Return value

    assert(segno < n_segs);	// Check we do not seek past end of file
    if (!indexed) {
	ret = QN_SEGID_BAD;
    } else {
	// need to check to see that frameno is < number of frames in segno
	if(frameno >= segind[segno+1] - segind[segno]) {
	    log.error("Seek beyond end of segment %lu.",
		     static_cast<unsigned long>(segno));
	}
	// jump to the correct location in the list of files
	long offset = static_cast<long>(file_pos_tab[segno]);
	if (fseek(file, offset, SEEK_SET)!=0) {
	  log.error("Seek failed in file "
		    "'%s', offset=%li - file problem?",
		    QN_FILE2NAME(file), offset);
	}

	// read the file name for this segment
	if(fgets(curr_seg_name, BUFSIZ, file) == NULL){
	  log.error("Error reading file name from list "
		    "'%s'.",QN_FILE2NAME(file));
	}
	// get rid of newline
	curr_seg_name[strlen(curr_seg_name)-1] = static_cast<char>(NULL);

	// read the header
	read_header();
	// Read in enough features to position the feat file at the 
	// desired location (i.e. the beginning of frame 'frameno').
	// If the frameno is 0, then we don't have to read any frames.
	if(frameno > 0){
	  float *tmpftrs = new float[frame_bytes*frameno];
	  read_ftrs(frameno,tmpftrs);
	  delete [] tmpftrs;
	}
	current_frame = frameno;
	current_seg = segno;
	log.log(QN_LOG_PER_SENT, "Seek to segment %lu, frame %lu.",
		static_cast<unsigned long>(segno), 
		static_cast<unsigned long>(frameno));

	ret = static_cast<QN_SegID>(current_seg);
    }
    return ret;
}


void
QN_InFtrStream_List::build_index()
{
  // The starting frame number.
  size_t frame = 0;	
  // Count of the seg number
  size_t seg = 0;
  // Space for segind allocated in constructor
  segind[seg] = (QNUInt32) frame;
  // Move to beginning of list of files
  if(rewind() != QN_OK){
	log.error("failed to build index for %s.",QN_FILE2NAME(file));
  }
  file_pos_tab[seg] = 0;

  // Get the name of each of the files in the list of files pointed to by
  // FILE* file and count how many frames there are in each one.
  // (space for curr_seg_name of size BUFSIZ allocated in constructor)
  while(fgets(curr_seg_name, BUFSIZ, file) != NULL) {
    // get rid of newline
    curr_seg_name[strlen(curr_seg_name)-1] = static_cast<char>(NULL); 

    // read_header() uses curr_seg_name. It should set the value
    // for 'frames_this_seg'. This is a virtual method and must be 
    // filled in by any subclass.
    read_header();

    // This is the beginning frame number for
    // the next segment.
    frame += frames_this_seg;
    ++seg;
    // Here we increase the space allocated for the index if necessary
    if (seg>=segind_len) {
      QNUInt32* new_segind;	// Temp pointer to the new index
      QNUInt32* new_file_pos_tab; // Temp pointer to new file position table
      size_t new_segind_len;	// Temp store for lenght of new index

      // Create a new index, including a copy of the old index
      new_segind_len = segind_len * 2;
      new_segind = new QNUInt32[new_segind_len];
      new_file_pos_tab = new QNUInt32[new_segind_len];
      qn_copy_vi32_vi32(segind_len, (const QNInt32*) segind,
		     (QNInt32*) new_segind);
      qn_copy_vi32_vi32(segind_len, (const QNInt32*) file_pos_tab,
		     (QNInt32*) new_file_pos_tab);
      delete[] segind;
      delete[] file_pos_tab;
      segind = new_segind;
      file_pos_tab = new_file_pos_tab;
      segind_len = new_segind_len;
    }
    segind[seg] = static_cast<QNUInt32>(frame);
    file_pos_tab[seg] = static_cast<QNUInt32>(ftell(file));
  }
  n_rows = frame;		// total number of frames in all files
  n_segs = seg;			// total number of utterances
}
