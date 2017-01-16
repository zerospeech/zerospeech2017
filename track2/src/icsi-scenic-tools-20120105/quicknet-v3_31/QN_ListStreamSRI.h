// -*- C++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_ListStreamSRI.h,v 1.2 2001/07/13 23:59:01 wooters Exp $

// QN_ListStreamSRI.h 
// This is a class derived from QN_ListStream that can be used for
// implementing a list of SRI feature files.

#ifndef QN_ListStreamSRI_h_INCLUDED
#define QN_ListStreamSRI_h_INCLUDED

#include <stdio.h>
#include "QN_ListStream.h"
#include "QN_SRIfeat.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_ListSRI - base class for stream input from
//                       a list of files.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
class QN_InFtrStream_ListSRI : public QN_InFtrStream_List
{
public:
  QN_InFtrStream_ListSRI(int a_debug, const char* a_dbgname, FILE* a_file,
			 int a_indexed);
  ~QN_InFtrStream_ListSRI();
  size_t read_ftrs(size_t count, float* ftrs);

protected:
  // This function implements the virtual function defined in
  // QN_InFtrStream_List. It returns -1 on failure. Also, it sets the
  // following base-class variables: 
  //   frames_this_seg 
  //   frame_bytes 
  // It assumes that the name of the file to read will is stored in
  // the base-class variable 'curr_seg_name'.  Also, it will
  // close any previously opened stream and file.
  int read_header();

private:
  QN_InFtrStream_SRI *feat_stream; // SRI feature stream
  FILE *fp;			// file pointer for the feat stream

};

#endif // #ifdef QN_ListStreamSRI_h_INCLUDED

