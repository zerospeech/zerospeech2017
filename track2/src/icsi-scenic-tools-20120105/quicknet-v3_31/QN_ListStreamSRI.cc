// -*- C++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_ListStreamSRI.cc,v 1.4 2004/09/07 23:30:01 davidj Exp $

// QN_ListStreamSRI.cc

#include <stdio.h>
#include "QN_ListStreamSRI.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_ListSRI - class for a list of SRI feat files
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
QN_InFtrStream_ListSRI::QN_InFtrStream_ListSRI(int a_debug, 
					       const char* a_dbgname, 
					       FILE* a_file,
					       int a_indexed)
  : QN_InFtrStream_List(a_debug, a_dbgname, a_file, a_indexed),
    feat_stream(NULL),
    fp(NULL)
{
  init();			// do base class initialization
}

size_t
QN_InFtrStream_ListSRI::read_ftrs(size_t count, float* ftrs)
{
    size_t cnt = 0;

    if(feat_stream != NULL){
	cnt = feat_stream->read_ftrs(count,ftrs);
	current_frame += cnt;
    } else {
	log.error("Attempting to read features before reading "
		  "the header.");
    }
    return cnt;
}

int
QN_InFtrStream_ListSRI::read_header()
{
  if(feat_stream != NULL) {	// remove existing feature stream
    delete feat_stream;
  }
  if(fp != NULL)		// close file pointer
    fclose(fp);

  fp = fopen(curr_seg_name,"r");
  if(fp == NULL){
    log.error("Error opening SRI feature file: '%s'",curr_seg_name);
  }

  feat_stream = new QN_InFtrStream_SRI(debug,dbgname,fp);
  frames_this_seg = feat_stream->num_frames();
  frame_bytes = feat_stream->num_ftrs() * sizeof(float);
  feat_stream->nextseg();	// prepare for reading the file

  return 1;
}

QN_InFtrStream_ListSRI::~QN_InFtrStream_ListSRI() {
  if(feat_stream != NULL) {	// remove existing feature stream
    delete feat_stream;
  }
  if(fp != NULL)		// close file pointer
    fclose(fp);
}
