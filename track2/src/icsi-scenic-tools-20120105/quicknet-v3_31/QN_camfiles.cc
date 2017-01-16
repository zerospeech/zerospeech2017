const char* QN_camfiles_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_camfiles.cc,v 1.27 2004/04/13 22:53:41 davidj Exp $";

// This file contains some miscellaneous routines for handling Cambridge
// speech group file formats.


#include <QN_config.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef QN_HAVE_ERRNO_H
#include <errno.h>
#endif
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"
#include "QN_camfiles.h"
#include "QN_Logger.h"


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_CamFile - generic Cambridge format input class
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrLabStream_CamFile::QN_InFtrLabStream_CamFile(int a_debug,
						     const char* a_classname,
						     const char* a_dbgname,
						     FILE* a_file,
						     size_t a_width,
						     int a_indexed)
    : log(a_debug, a_classname, a_dbgname),
      file(a_file),
      width(a_width),
      current_seg(-1),		// These are correctly initialised below.
      current_frame(-1),
      indexed(a_indexed),
      segind(NULL)
{
    // We allocate a buffer for one frame, such that the second byte in the
    // frame (the start of the first feature) is suitably aligned to be read
    // as a floating point value.
    size_t buf_len_float = (width+2*(sizeof(float)-1))/sizeof(float);
    frame_buf_float = new float[buf_len_float];
    frame_buf = ((QNUInt8*) frame_buf_float) + (sizeof(float) - 1);
    // fixed for the width != i*4 bug 1998sep29 dpwe@icsi.berkeley.edu

    log.log(QN_LOG_PER_RUN, "Creating InFtrLabStream from file '%s'.",
	    QN_FILE2NAME(a_file));
    // If required, build an index of the file.
    if (a_indexed)
    {
	int ec;

	// Allocate and build buffer
	segind_len = DEFAULT_INDEX_LEN;
	segind = new QNUInt32[segind_len];
	build_index();
	ec = fseek(file, 0L, SEEK_SET);
	if (ec!=0)
	{
	    log.error("Failed to seek to start of '%s' after "
		     " building index.", QN_FILE2NAME(file));
	}
	log.log(QN_LOG_PER_RUN, "Indexed stream, n_segs=%lu n_rows=%lu.",
		n_segs, n_rows);
    }
    eos = 1;			// Effectively at end of sentence -1
}

QN_InFtrLabStream_CamFile::~QN_InFtrLabStream_CamFile()
{
    if (segind!=NULL)
	delete[] segind;
    delete[] frame_buf_float;
}

int
QN_InFtrLabStream_CamFile::rewind()
{
    int ec;
    int ret;			// Return code

    ec = fseek(file, 0L, SEEK_SET);
    // Sometimes it is valed for rewind to fail (e.g. on pipes),
    // so return an error code rather than crash.
    if (ec!=0)
    {
	log.log(QN_LOG_PER_EPOCH, "Rewind failed.");
	ret = QN_BAD;
    }
    else
    {
	current_seg = -1;
	current_frame = -1;
	eos = 1;
	log.log(QN_LOG_PER_EPOCH, "Rewound.");
	ret = QN_OK;
    }
    return ret;
}

void
QN_InFtrLabStream_CamFile::next_frame()
{
    int ec;			// Return code
    
    ec = fread((char *) frame_buf, width, 1, file);
    if (ec==0)
    {
	if (ferror(file))
	{
	    log.error("Error reading frame from file '%s' - %s.",
		     QN_FILE2NAME(file), strerror(errno));
	}
	else
	{
	    if (!eos)
	    {
		log.warning("File '%s' ended in middle of segment.",
			    QN_FILE2NAME(file));
		eos = 1;
	    }
	}
    }
    else
    {
	eos = (frame_buf[0] & EOS_MASK) ? 1 : 0;
	current_frame++;
    }
}

QN_SegID
QN_InFtrLabStream_CamFile::nextseg()
{
    QN_SegID segid;		// Returned segment ID.
    int c;			// EOF test character.

    // If we are not at the end of sentence, scan forward until we are.
    while(!eos)
    {
	next_frame();
    }

    // Check for eos.
    c = getc(file);
    if (c==EOF)
    {
	if (ferror(file))
	{
	    log.error("Error checking for EOF in '%s' - %s.",
		      QN_FILE2NAME(file), strerror(errno));
	    segid = QN_SEGID_BAD;
	}
	else
	{
	    segid = QN_SEGID_BAD;
	}
    }
    else
    {
	int ec;

	segid = QN_SEGID_UNKNOWN; // FIXME - should return segid.
	eos = 0;                  // Clear EOS flag.
	current_seg++;
	current_frame = 0;
	ec = ungetc(c, file);	// Put back charater.
	if (ec==EOF)
	{
	    log.error("Error pushing back EOF check character in '%s' - %s.",
		      QN_FILE2NAME(file), strerror(errno));
	}
    }
    return segid;
}

size_t
QN_InFtrLabStream_CamFile::num_frames(size_t segno)
{
    size_t ret;			// Return value.

    if (!indexed)		// If not indexed, do not know no. of frames.
	ret = QN_SIZET_BAD;
    else
    {
	if (segno == QN_ALL) // QN_ALL means do for whole stream.
	    ret = n_rows;
	else
	{
	    // Look up number of frames in segment index.
	    assert (segno<n_segs);
	    ret = segind[segno+1] - segind[segno]; 
	}
    }
    return ret;
}

// This is a dummy read routine.  All the classes that use this base class
// define their own reading routines, but this is here in case it is
// useful for testing.  It simply returns the values of the bytes in each
// frame as floats.

size_t
QN_InFtrLabStream_CamFile::read_ftrslabs(size_t cnt,
					 float* ftrs, QNUInt32* labs)
{
    size_t frame;       // Current frame in this bunch
    size_t n_ftrs = width - 1 ; // Number of features in frame
    QNInt32* buf32;		// Buffer for conversion

    buf32 = new QNInt32[n_ftrs];

    frame = 0;
    while(frame<cnt && !eos)
    {
	QNUInt32 lab;

	next_frame();
	if (ftrs!=NULL)
	{
	    qn_conv_vi8_vi32(n_ftrs, (const QNInt8*) frame_buf, buf32);
	    qn_conv_vi32_vf(n_ftrs, buf32, ftrs);
	    ftrs += n_ftrs;
	}
	if (labs!=NULL)
	{
	    lab = frame_buf[0] & LABEL_MASK;
	    *labs++ = lab;
	}
	frame++;
    }
    delete[] buf32;
    return frame;
}

int
QN_InFtrLabStream_CamFile::get_pos(size_t* segnop, size_t* framenop)
{
    int ret;			// Return value
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.


    if (!indexed)
	ret = QN_BAD;
    else
    {
	if (current_seg==-1)
	{
	    segno = QN_SIZET_BAD;
	    frameno = QN_SIZET_BAD;
	}
	else
	{
	    segno = (size_t) current_seg;
	    frameno = (size_t) current_frame;
	    assert(current_frame>=0);
	}
	if (segnop!=NULL)
	    *segnop = segno;
	if (framenop!=NULL)
	    *framenop = frameno;
	log.log(QN_LOG_PER_SENT, "Currently at sentence %lu, frame %lu.",
		(unsigned long) segno, (unsigned long) frameno);
	ret = QN_OK;
    }
    return ret;
}

QN_SegID
QN_InFtrLabStream_CamFile::set_pos(size_t segno, size_t frameno)
{
    int ret;			// Return value

    assert(segno < n_segs); // Check we do not seek past end of file
    if (!indexed)
	ret = QN_SEGID_BAD;
    else
    {
	long this_seg_row;	// The number of the row at the start of seg.
	long next_seg_row;	// The row at the start of next seg.
	long row;		// The number of the row we require
	long offset;		// The position as a file offset

	this_seg_row = segind[segno];
	row = segind[segno] + frameno;
	next_seg_row = segind[segno+1];
	if (row > next_seg_row)
	{
	    log.error("Seek beyond end of segment %li.",
		     (unsigned long) segno);
	}
	offset = width * row;
	
	if (fseek(file, offset, SEEK_SET)!=0)
	{
	    log.error("Seek failed in file "
		     "'%s', offset=%li - file problem?",
		     QN_FILE2NAME(file), offset);
	}
	current_frame = frameno;
	current_seg = segno;
	log.log(QN_LOG_PER_SENT, "Seek to segment %lu, frame %lu.",
		(unsigned long) segno, (unsigned long) frameno);
	ret = QN_SEGID_UNKNOWN;		// FIXME - should return sentence ID.
	if (row==next_seg_row)
	    eos = 1;
	else
	    eos = 0;
    }
    return ret;
}

// Build an index of frame numbers of the start of each segment

void
QN_InFtrLabStream_CamFile::build_index()
{
    size_t frame = 0;	// Current frame number.
    size_t seg = 0;	// Current segment number.
    size_t frame_in_seg = 0;	// Frame number within current segment.

    segind[seg] = (QNUInt32) frame;

    size_t cnt;		// Number of frames read this read.
    while(1)
    {
	// Read one frame.
	cnt = fread(frame_buf, width, 1, file);
	if (cnt!=1)
	    break;

	// Check for end of seg.
	if (frame_buf[0] & EOS_MASK)
	{
	    seg++;
	    // Here we increase the space allocated for the index if necessary
	    if (seg>=segind_len)
	    {
		QNUInt32* new_segind;    // Temp pointer to the new index
		size_t new_segind_len;	 // Temp store for lenght of new index

		// Create a new index, including a copy of the old index
		new_segind_len = segind_len * 2;
		new_segind = new QNUInt32[new_segind_len];
		qn_copy_vi32_vi32(segind_len, (const QNInt32*) segind,
			       (QNInt32*) new_segind);
		delete[] segind;
		segind = new_segind;
		segind_len = new_segind_len;
	    }
	    segind[seg] = (QNUInt32) (frame + 1);
	    frame_in_seg = 0;
	}
	else
	    frame_in_seg++;
	frame++;
    }
    // If the read failed because of an error, say so.
    if (ferror(file))
    {
	log.error("Error reading file '%s' to build segment index "
		 "- %s.", QN_FILE2NAME(file), strerror(errno));
    }
    else if (frame_in_seg!=0)
    {
	log.warning("File '%s' ended in middle of segment when "
		    "building segment index.", QN_FILE2NAME(file));
    }
    n_rows = frame;
    n_segs = seg;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_PreFile - Input a cambridge "pre" feature file
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

// Note that PreFiles always have an integer number of words in
// each frame, so there can be up to three spare bytes in each frame.
// We pass the whole width of the frame down to the CamFile class, remembering
// the real number of features in the PreFile class.

QN_InFtrLabStream_PreFile::QN_InFtrLabStream_PreFile(int a_debug,
						     const char* a_dbgname,
						     FILE* a_file,
						     size_t a_num_ftrs,
						     int a_indexed)
    : QN_InFtrLabStream_CamFile(a_debug, "QN_InFtrLabStream_PreFile",
				a_dbgname, a_file, ((1+a_num_ftrs+3)/4)*4,
				a_indexed),
      n_pre_ftrs(a_num_ftrs)
{
    // The number of bytes per frame is 1 for the flag byte plus one for 
    // each feature, padded out to an exact number of 4 byte words in total.
//    if (a_num_ftrs==0)
//	log.error("Cannot open a pre file with 0 features.");
    log.log(QN_LOG_PER_RUN, "Creating InFtrLabStream_PreFile, %lu features, "
	    "width %lu, from file '%s'.", n_pre_ftrs, width,
	    QN_FILE2NAME(a_file));
}

size_t
QN_InFtrLabStream_PreFile::read_ftrslabs(size_t cnt,
					 float* ftrs, QNUInt32* labs)
{
    size_t frame;       // Current frame in this bunch
    size_t n_ftrs = n_pre_ftrs; // Number of features in frame
    
    frame = 0;
    while(frame<cnt && !eos)
    {
	QNUInt32 lab;

	next_frame();
	if (ftrs!=NULL)
	{
	    qn_pre8toh_vi8_vf(n_ftrs, (QNInt8*) &frame_buf[1], ftrs);
	    ftrs += n_ftrs;
	}
	if (labs!=NULL)
	{
	    lab = frame_buf[0] & LABEL_MASK;
	    *labs++ = lab;
	}
	frame++;
    }
    return frame;
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_LNA8 - Input a cambridge "LNA" probability stream
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrLabStream_LNA8::QN_InFtrLabStream_LNA8(int a_debug,
					       const char* a_dbgname,
					       FILE* a_file,
					       size_t a_num_ftrs,
					       int a_indexed)
    : QN_InFtrLabStream_CamFile(a_debug, "QN_InFtrLabStream_LNA8", a_dbgname,
				a_file, a_num_ftrs+1, a_indexed)
{
    log.log(QN_LOG_PER_RUN, "Creating InFtrLabStream_LNA8 from file '%s'.",
	    QN_FILE2NAME(a_file));
}

QN_InFtrLabStream_LNA8::~QN_InFtrLabStream_LNA8()
{
}

size_t
QN_InFtrLabStream_LNA8::read_ftrslabs(size_t cnt, float* ftrs, QNUInt32* labs)
{
    size_t frame;       // Current frame in this bunch
    size_t n_ftrs = width - 1; // Number of features in frame
    
    frame = 0;
    while(frame<cnt && !eos)
    {
	QNUInt32 lab;

	next_frame();
	if (ftrs!=NULL)
	{
	    qn_lna8toh_vi8_vf(n_ftrs, (QNInt8*) &frame_buf[1], ftrs);
	    ftrs += n_ftrs;
	}
	if (labs!=NULL)
	{
	    lab = frame_buf[0] & LABEL_MASK;
	    *labs++ = lab;
	}
	frame++;
    }
    return frame;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrStream_OnlFtr - Input an ICSI "Online Feature" stream
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InFtrLabStream_OnlFtr::QN_InFtrLabStream_OnlFtr(int a_debug,
						   const char* a_debug_name,
						   FILE* a_file,
						   size_t a_num_ftrs,
						   int a_indexed)
    : QN_InFtrLabStream_CamFile(a_debug, "QN_InFtrLabStream_OnlFtr",
				a_debug_name,
				a_file, a_num_ftrs*4+1, a_indexed),
      n_ftrs(a_num_ftrs)
{
    log.log(QN_LOG_PER_RUN, "Creating InFtrLabStream_OnlFtr from file '%s'.",
	    QN_FILE2NAME(a_file));
}

QN_InFtrLabStream_OnlFtr::~QN_InFtrLabStream_OnlFtr()
{
}

size_t
QN_InFtrLabStream_OnlFtr::read_ftrs(size_t cnt, float* ftrs)
{
    size_t frame;       // Current frame in this bunch.
    size_t local_n_ftrs = n_ftrs; // Local copy of objects version.
    const size_t first_frame = current_frame;
    
    
    frame = 0;
    while(frame<cnt && !eos)
    {
	next_frame();
	if (ftrs!=NULL)
	{
	    // Online features are big endian IEEE floats
	    qn_btoh_vf_vf(local_n_ftrs, (float*) &frame_buf[1], ftrs);
	    ftrs += local_n_ftrs;
	}
	frame++;
    }
    log.log(QN_LOG_PER_BUNCH, "Read %lu of requested %lu frames from "
	    "segment %lu, frame offset %lu.",
	    (unsigned long) frame, (unsigned long) cnt,
	    (unsigned long) current_seg, (unsigned long) first_frame);
    return frame;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_LNA8 - output Cambridge format LNA data.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_OutFtrLabStream_LNA8::QN_OutFtrLabStream_LNA8(int a_debug,
						 const char* a_dbgname,
						 FILE* a_file, 
						 size_t a_n_ftrs, 
						 int onl /* = 0 */)
 :  clog(a_debug, "QN_OutFtrLabStream_LNA8", a_dbgname),
    file(a_file),
    n_ftrs(a_n_ftrs),
    frame_buf(new QNInt8[n_ftrs+1]),
    buf_full(0), 
    online(onl)
{
    assert(n_ftrs!=0);
    clog.log(QN_LOG_PER_RUN, "Creating LNA8 file '%s'.",  QN_FILE2NAME(file) );
}

QN_OutFtrLabStream_LNA8::~QN_OutFtrLabStream_LNA8()
{
    if (buf_full)
	clog.error("tried to close file without finishing segment.");
    if (online) {
	int ec = fflush(file);
	if (ec != 0) {
	    clog.error("failed to flush at end of stream - %s.",
		       strerror(errno));
	}
    }
    clog.log(QN_LOG_PER_RUN, "Finished creating LNA8 file '%s'.", 
	     QN_FILE2NAME(file));
    delete[] frame_buf;
}

void
QN_OutFtrLabStream_LNA8::write_ftrslabs(size_t cnt, const float* ftrs, 
					const QNUInt32* labs)
{
    if (cnt!=0)
    {
	size_t i;		// Local counter.
	const float* ftr_ptr = ftrs; // Pointer to current feature vector.
	const QNUInt32* lab_ptr = labs; // Pointer to current label value.

	frame_buf[0] &= ~EOS;	// None of these frames are EOS.
	if (buf_full)
	    fwrite(frame_buf, n_ftrs+1, 1, file);
	for (i=0; i<cnt-1; i++)
	{
	    if (lab_ptr!=NULL)
	    {
		*frame_buf = *lab_ptr;
		lab_ptr++;
	    }
	    else
		*frame_buf = 0;
	    if (ftr_ptr!=NULL) {
		qn_htolna8_vf_vi8(n_ftrs, ftr_ptr, frame_buf+1);
		ftr_ptr += n_ftrs;
	    }
	    fwrite(frame_buf, n_ftrs+1, 1, file);
	}
	if (lab_ptr != NULL) {
	    *frame_buf = *lab_ptr;
	} else {
	    *frame_buf = 0;
	}
	if (ftr_ptr != NULL) {
	    qn_htolna8_vf_vi8(n_ftrs, ftr_ptr, frame_buf+1);
	}
	buf_full = 1;

	if (online) {
	    int ec = fflush(file);
	    if (ec != 0) {
		clog.error("failed to flush at end of frames - %s.",
			   strerror(errno));
	    }
	}
    }
}

void
QN_OutFtrLabStream_LNA8::doneseg(QN_SegID /* segid */)
{
    if (buf_full)
    {
	frame_buf[0] |= EOS;
	fwrite(frame_buf, n_ftrs+1, 1, file);
	buf_full = 0;
    }
    else
	clog.error("tried to write zero length segment.");

    if (online) {
	int ec = fflush(file);
	if (ec != 0) {
	    clog.error("failed to flush at end of segment - %s.",
		       strerror(errno));
	}
    }
    clog.log(QN_LOG_PER_SENT, "Finished segment.");
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_OnlFtr - output ICSI online feature format data.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_OutFtrStream_OnlFtr::QN_OutFtrStream_OnlFtr(int a_debug,
					       const char* a_dbgname,
					       FILE* a_file, size_t a_n_ftrs)
 :  clog(a_debug, "QN_OutFtrStream_OnlFtr", a_dbgname),
    file(a_file),
    n_ftrs(a_n_ftrs),
    frame_buf(new float[n_ftrs+1]),
    buf_full(0)
{
    assert(n_ftrs!=0);
    clog.log(QN_LOG_PER_RUN, "Creating OnlFtr file '%s'.", QN_FILE2NAME(file));
}

QN_OutFtrStream_OnlFtr::~QN_OutFtrStream_OnlFtr()
{
    if (buf_full)
	clog.error("tried to close file without finishing segment.");
    clog.log(QN_LOG_PER_RUN, "Finished creating OnlFtr file '%s'.", 
	     QN_FILE2NAME(file));
    delete[] frame_buf;
}

void
QN_OutFtrStream_OnlFtr::write_ftrs(size_t cnt, const float* ftrs)
{
    if (cnt!=0)
    {
	size_t i;		// Local counter.
	const float* ftr_ptr = ftrs; // Pointer to current feature vector.

	if (buf_full)
	{
	    putc(0, file);
	    fwrite(frame_buf, n_ftrs*sizeof(float), 1, file);
	}
	for (i=0; i<cnt-1; i++)
	{
	    qn_htob_vf_vf(n_ftrs, ftr_ptr, frame_buf);
	    putc(0, file);	// Not end of segment.
	    fwrite(frame_buf, n_ftrs*sizeof(float), 1, file);
	    ftr_ptr += n_ftrs;
	}
	qn_htob_vf_vf(n_ftrs, ftr_ptr, frame_buf);
	buf_full = 1;
    }
}

void
QN_OutFtrStream_OnlFtr::doneseg(QN_SegID /* segid */)
{
    if (buf_full)
    {
	putc(EOS, file);
	fwrite(frame_buf, n_ftrs*sizeof(float), 1, file);
	buf_full = 0;
    }
    else
	clog.error("tried to write zero length segment.");
    clog.log(QN_LOG_PER_SENT, "Finished segment.");
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_PreFile - output Cambridge format feature data.
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_OutFtrLabStream_PreFile::QN_OutFtrLabStream_PreFile(int a_debug,
					   const char* a_dbgname,
					   FILE* a_file, size_t a_n_ftrs)
 :  clog(a_debug, "QN_OutFtrLabStream_PreFile", a_dbgname),
    file(a_file),
    n_ftrs(a_n_ftrs),
    width(((1+a_n_ftrs+3)/4)*4),	// Pad so word aligned, including flag
    frame_buf(new QNInt8[n_ftrs+1]),
    buf_full(0)
{
    //    assert(n_ftrs!=0);
    qn_copy_i8_vi8(width, 0, frame_buf); // Make padding bytes 0.
    clog.log(QN_LOG_PER_RUN, "Creating pre file '%s'.",  QN_FILE2NAME(file) );
}

QN_OutFtrLabStream_PreFile::~QN_OutFtrLabStream_PreFile()
{
    if (buf_full)
	clog.error("tried to close file without finishing segment.");
    clog.log(QN_LOG_PER_RUN, "Finished creating pre file '%s'.", 
	     QN_FILE2NAME(file));
    delete[] frame_buf;
}

void
QN_OutFtrLabStream_PreFile::write_ftrslabs(size_t cnt, const float* ftrs,
					   const QNUInt32* labs)
{
    if (cnt!=0)
    {
	size_t i;		// Local counter.
	const float* ftr_ptr = ftrs; // Pointer to current feature vector.
	const QNUInt32* lab_ptr = labs; // Pointer to current label value.

	frame_buf[0] &= ~EOS;	// None of these frames are EOS.
	// Write out preceding frame
	if (buf_full)
	    fwrite(frame_buf, width, 1, file);

	//
	for (i=0; i<cnt-1; i++)
	{
	    if (lab_ptr!=NULL)
	    {
		*frame_buf = *lab_ptr;
		lab_ptr++;
	    }
	    else
		*frame_buf = 0;
	    if (ftr_ptr!=NULL)
	    {
		qn_htopre8_vf_vi8(n_ftrs, ftr_ptr, frame_buf+1);
		ftr_ptr += n_ftrs;
	    }

	    fwrite(frame_buf, width, 1, file);
	}
	if (lab_ptr!=NULL)
	    *frame_buf = *lab_ptr;
	if (ftr_ptr!=NULL)
	    qn_htopre8_vf_vi8(n_ftrs, ftr_ptr, frame_buf+1);
	buf_full = 1;
    }
}

void
QN_OutFtrLabStream_PreFile::doneseg(QN_SegID /* segid */)
{
    if (buf_full)
    {
	frame_buf[0] |= EOS;
	fwrite(frame_buf, width, 1, file);
	buf_full = 0;
    }
    else
	clog.error("tried to write zero length segment.");
    clog.log(QN_LOG_PER_SENT, "Finished segment.");
}

