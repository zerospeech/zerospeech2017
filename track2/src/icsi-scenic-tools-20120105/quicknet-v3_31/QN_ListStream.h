// -*- C++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_ListStream.h,v 1.2 2001/07/13 23:58:48 wooters Exp $

// QN_ListStream.h
// This is a virtual base class that can be used for implementing a list
// of files.

#ifndef QN_ListStream_h_INCLUDED
#define QN_ListStream_h_INCLUDED

#include <stdio.h>
#include "QN_streams.h"
#include "QN_Logger.h"


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_List - abstract base class for stream input from
//                       a list of files.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
class QN_InFtrStream_List : public QN_InFtrStream
{
public:
  QN_InFtrStream_List(int a_debug, 
		      const char* a_dbgname, FILE* a_file,
		      int a_indexed);
  virtual ~QN_InFtrStream_List();

  // There is no default implementation for read_ftrs(), so it must
  // be implemented in a derived class.
  virtual size_t read_ftrs(size_t count, float* ftrs)=0;

  // the following functions are implemented in this base class
  size_t num_ftrs();
  int rewind();
  QN_SegID nextseg();

  // The following functions are also implemented in this base class,
  // but may fail due to lack of an index - 
  // if this is happens, they return an error value and DO NOT call QN_ERROR().
  size_t num_segs();
  size_t num_frames(size_t segno = QN_ALL);
  int get_pos(size_t* segno, size_t* frameno);
  QN_SegID set_pos(size_t segno, size_t frameno);

protected:
  enum {
    DEFAULT_INDEX_LEN = 16 	// Default length of the index vector.
				// (short to aid testing).
  };

  QN_ClassLogger log;		// Send debugging output through here.
  int debug;
  const char* dbgname;
  FILE* const file;		// The stream for reading the list.
  long current_seg;		// Current segment number.
  long current_frame;		// Current frame within segment.
  char* curr_seg_name;		// Name of file containing actual data
  size_t frames_this_seg;	// The number of frames in the current seg
  size_t frame_ftrs;		// The number of features per frame
  size_t frame_bytes;		// The number of bytes per frame

  char header_peeked;		// flag to indicate header has been read

  // index related stuff    
  const int indexed;		// 1 if indexed.
  size_t n_segs;		// The number of segments in the file -
				// can only be used if indexed.
  size_t n_rows;		// The number of frames in the file -
				// can only be used if indexed.
  QNUInt32* segind;		// Segment index vector.
  size_t segind_len;		// Current length of the segment index vector.
  QNUInt32* file_pos_tab;	// table of file name positions
  void build_index();		// Build a segment index.

  void init();			// initialization code for the base class
				// this is stuff that would normally be in
				// the base class constructor, but because
				// it calls pure virtual functions, it
				// must be called after the derived class
				// is constructed (e.g. from inside the 
				// derived class's constructor).

  // There is no default implementation for read_header() so
  // this function must be implemented by a derived class and 
  // should return -1 on failure. Also, it should be sure to set
  // the following variables:
  //   frames_this_seg
  //   frame_bytes
  // It can be assumed that the name of the file to read will be stored
  // in (char*)curr_seg_name. Also, it should close any previously open
  // file.
  virtual int read_header()=0;

};

inline size_t
QN_InFtrStream_List::num_segs()
{
    size_t ret;

    if (indexed)
	ret = n_segs;
    else
	ret = QN_SIZET_BAD;
    return ret;
}


inline size_t
QN_InFtrStream_List::num_ftrs()
{
    return frame_ftrs;
}

#endif // #ifdef QN_ListStream_h_INCLUDED

