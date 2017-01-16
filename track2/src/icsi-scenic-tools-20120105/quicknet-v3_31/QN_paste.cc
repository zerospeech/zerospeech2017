const char* QN_paste_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_paste.cc,v 1.3 2004/04/08 02:57:44 davidj Exp $";

// This file contains the paste class, for glueing streams end-to-end

#include <QN_config.h>
#include "QN_paste.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////
QN_InFtrStream_Paste::QN_InFtrStream_Paste(int a_debug,
			 const char* a_dbgname,
			 QN_InFtrStream& a_str1,
			 QN_InFtrStream& a_str2) 
  : clog(a_debug, "QN_InFtrStream_Paste", a_dbgname), 
    str1(a_str1),
    str2(a_str2),
    str1_n_segs(a_str1.num_segs()),
    str2_n_segs(a_str2.num_segs())
{
    n_ftrs = str1.num_ftrs();
    if (str2.num_ftrs() != n_ftrs) {
	clog.error("QN_InFtrStream_Paste: pasted ftr streams have differing num_ftrs");
    }
    clog.log(QN_LOG_PER_RUN, "Pasting streams of %lu and %lu segments.", 
	     (unsigned long) str1_n_segs, (unsigned long) str2_n_segs);
    current_seg = QN_SIZET_BAD;
    cur_str = &str1;
    seg_base = 0;
}

QN_InFtrStream_Paste::~QN_InFtrStream_Paste()
{
}

QN_SegID QN_InFtrStream_Paste::nextseg() {
    QN_SegID rslt = QN_SEGID_BAD;
    if (current_seg == QN_SIZET_BAD || current_seg < str1_n_segs - 1) {
	// at beginning of stream, or still in first half
	rslt = str1.nextseg();
	cur_str = &str1;
	seg_base = 0;
    } else {
	// time to move in second stream
	rslt = str2.nextseg();
	cur_str = &str2;
	seg_base = str1_n_segs;
    }
    if (current_seg == QN_SIZET_BAD) {
	current_seg = 0;
    } else {
	++current_seg;
    }
    //    clog.log(QN_LOG_PER_EPOCH, "QN_Paste: nextseg to seg %lu.", 
    //	     (unsigned long) current_seg);
    return rslt;
}

size_t QN_InFtrStream_Paste::read_ftrs(size_t cnt, float* ftrs, size_t stride){
    return cur_str->read_ftrs(cnt, ftrs, stride);
}

int QN_InFtrStream_Paste::rewind() {
    int rc;
    rc = str1.rewind();
    rc += str2.rewind();
    current_seg = QN_SIZET_BAD;
    cur_str = &str1;
    seg_base = 0;
    //    clog.log(QN_LOG_PER_EPOCH, "QN_Paste: rewind.");
    return rc;
}
    
size_t QN_InFtrStream_Paste::num_frames(size_t segno /*= QN_ALL*/) {
    size_t rslt;
    if (segno == QN_ALL) {
	rslt = str1.num_frames(QN_ALL) + str2.num_frames(QN_ALL);
    } else {
	if (segno < str1_n_segs) {
	    rslt = str1.num_frames(segno);
	} else {
	    rslt = str2.num_frames(segno - str1_n_segs);
	}
    }
    return rslt;
}
  
int QN_InFtrStream_Paste::get_pos(size_t* segno, size_t* frameno) {
    size_t seg = QN_SIZET_BAD, fr = QN_SIZET_BAD;
    int rc;
    rc = cur_str->get_pos(&seg, &fr);
    if (rc != QN_BAD) {
	seg += seg_base;
    }
    if (segno) *segno = seg;
    if (frameno) *frameno = fr;
    return rc;
}

QN_SegID QN_InFtrStream_Paste::set_pos(size_t segno, size_t frameno) {
    QN_SegID rslt;
    if (segno < str1_n_segs) {
	cur_str = &str1;
	seg_base = 0;
	// Also, rewind the subsequent files so following nextsegs()
	// do the intended thing (was a bug)
	str2.rewind();
   } else {
	cur_str = &str2;
	seg_base = str1_n_segs;
    }
    rslt = cur_str->set_pos(segno - seg_base, frameno);
    current_seg = segno;
    return rslt;
}

