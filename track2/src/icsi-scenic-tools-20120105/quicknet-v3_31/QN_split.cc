const char* QN_split_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_split.cc,v 1.4 2004/04/08 02:57:45 davidj Exp $";

// Some classes for splitting one stream into two.

#include <QN_config.h>
#include "QN_split.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"

// Some useful inline functions.

static inline size_t
min(size_t a, size_t b)
{
    if (a<b)
	return a;
    else
	return b;
}

////////////////////////////////////////////////////////////////

QN_InFtrStream_SplitFtrLab::
QN_InFtrStream_SplitFtrLab(int a_debug, const char* a_dbgname,
			   QN_InFtrLabStream& a_instr, size_t a_buf_frames)
  : log(a_debug, "QN_InFtrStream_SplitFtrLab", a_dbgname),
    instr(a_instr),
    buf_frames(a_buf_frames),
    n_ftrs(a_instr.num_ftrs()),
    n_labs(a_instr.num_labs()),
    instr_segno(QN_SIZET_BAD),
    instr_frameno(QN_SIZET_BAD),
    ftr_segno(instr_segno),
    ftr_frameno(instr_frameno),
    ftr_buf(NULL),
    ftr_buf_len(0),
    ftr_buf_start(0),
    ftr_buf_end(0),
    ftr_buf_segno(QN_SIZET_BAD),
    ftr_buf_frameno(QN_SIZET_BAD),
    lab_segno(instr_segno),
    lab_frameno(instr_frameno),
    lab_buf(NULL),
    lab_buf_len(0),
    lab_buf_start(0),
    lab_buf_end(0),
    lab_buf_segno(QN_SIZET_BAD),
    lab_buf_frameno(QN_SIZET_BAD)
{
    QN_SegID segid;
    int ec;
    
    segid = instr.set_pos(0,0);
    ec = instr.rewind();
    if ((ec!=QN_OK) || (segid==QN_SEGID_BAD))
	log.error("Cannot split a non-indexed stream.");
}
						 
QN_InFtrStream_SplitFtrLab::
~QN_InFtrStream_SplitFtrLab()
{
    if (lab_buf!=NULL)
	delete[] lab_buf;
    if (ftr_buf!=NULL)
	delete[] ftr_buf;
}

size_t
QN_InFtrStream_SplitFtrLab::num_segs()
{
    return instr.num_segs();
}

size_t
QN_InFtrStream_SplitFtrLab::num_frames(size_t segno)
{
    return instr.num_frames(segno);
}

size_t
QN_InFtrStream_SplitFtrLab::num_ftrs()
{
    return n_ftrs;
}

size_t
QN_InFtrStream_SplitFtrLab::num_labs()
{
    return n_labs;
}

// Seek to a given position.
// Also works when segno==QN_SIZET_BAD - equivalent of rewind.

void
QN_InFtrStream_SplitFtrLab::seek_if_necessary(size_t segno, size_t frameno)
{
    int ec = QN_OK;		// QN_BAD if operation failed.

    if (segno!=instr_segno || frameno!=instr_frameno)
    {
	if (segno==QN_SIZET_BAD)
	{
	    assert(frameno==QN_SIZET_BAD);
	    ec = instr.rewind();
	}
	else
	{
	    instr_segid = instr.set_pos(segno, frameno);
	    if (instr_segid==QN_SEGID_BAD)
		ec = QN_BAD;
	}
    }
    if (ec==QN_OK)
    {
	instr_segno = segno;
	instr_frameno = frameno;
    }
    else
    {
	log.error("seek_if_necessary() failed - for this access pattern, "
		  "input stream must be seekable.");
    }
}

// If possible, make space for "frames" frames in the feature buffer.
int
QN_InFtrStream_SplitFtrLab::ftr_freespace(size_t frames)
{
    int ret;			// Return value.

    // First see if we have not allocated a buffer yet - if so, create one.
    if (ftr_buf_len==0)
    {
	if (frames>buf_frames)
	    ret = QN_BAD;
	else
	{
	    ftr_buf_len = buf_frames;
	    ftr_buf = new float[ftr_buf_len*n_ftrs];
	    ftr_buf_start = ftr_buf_end = 0;
	    log.log(QN_LOG_PER_RUN, "Allocated %lu bytes for a %lu frame "
		    "feature buffer.",
		    (unsigned long) ftr_buf_len*n_ftrs*sizeof(float),
		    (unsigned long) ftr_buf_len);
	    ret = QN_OK;
	}
    }
    else
    {
	// See if we can add the features in the existing allocated
	// space.

	// The free space at the end of the buffer.
	const size_t trailing_space = ftr_buf_len - ftr_buf_end;
	if (trailing_space>=frames)
	{
	    ret = QN_OK;
	}
	else
	{
	    // Here we see if we can move stuff down to make space.

	    // The number if frames in the buffer at the moment.
	    const size_t ftr_buf_diff = ftr_buf_end - ftr_buf_start;
	    // The total free space, assuming we move stuff.
	    const size_t total_space = ftr_buf_len - ftr_buf_diff;

	    if (total_space>=frames)
	    {
		// Move stuff down.
		size_t i;
		float* from = &ftr_buf[ftr_buf_start*n_ftrs];
		float* to = &ftr_buf[0];
		for (i=0; i<ftr_buf_diff; i++)
		{
		    // Note: cannot use matrix or vector copy cos of
		    // potentially overlapping areas.
		    qn_copy_vf_vf(n_ftrs, from, to);
		    from += n_ftrs;
		    to += n_ftrs;
		}
		ftr_buf_start = 0;
		ftr_buf_end = ftr_buf_diff;
		ret = QN_OK;
	    }
	    else
		ret = QN_BAD;
	}
    }
    return ret;
}

// If possible, make space for "frames" frames in the label buffer.
int
QN_InFtrStream_SplitFtrLab::lab_freespace(size_t frames)
{
    int ret;			// Return value.

    // First see if we have not allocated a buffer yet - if so, create one.
    if (lab_buf_len==0)
    {
	if (frames>buf_frames)
	    ret = QN_BAD;
	else
	{
	    lab_buf_len = buf_frames;
	    lab_buf = new QNUInt32[lab_buf_len*n_labs];
	    lab_buf_start = lab_buf_end = 0;
	    log.log(QN_LOG_PER_RUN, "Allocated %lu bytes for a %lu frame "
		    "label buffer.",
		    (unsigned long) lab_buf_len*n_labs*sizeof(QNUInt32),
		    (unsigned long) lab_buf_len);
	    ret = QN_OK;
	}
    }
    else
    {
	// See if we can add the labels in the existing allocated
	// space.

	// The free space at the end of the buffer.
	const size_t trailing_space = lab_buf_len - lab_buf_end;
	if (trailing_space>=frames)
	{
	    ret = QN_OK;
	}
	else
	{
	    // Here we see if we can move stuff down to make space.

	    // The number if frames in the buffer at the moment.
	    const size_t lab_buf_diff = lab_buf_end - lab_buf_start;
	    // The total free space, assuming we move stuff.
	    const size_t total_space = lab_buf_len - lab_buf_diff;

	    if (total_space>=frames)
	    {
		// Move stuff down.
		size_t i;
		QNInt32* from =
		    (QNInt32*) &lab_buf[lab_buf_start*n_labs];
		QNInt32* to = (QNInt32*) &lab_buf[0];
		for (i=0; i<lab_buf_diff; i++)
		{
		    // Note: cannot use matrix or vector copy cos of
		    // potentially overlapping areas.
		    qn_copy_vi32_vi32(n_labs, from, to);
		    from += n_labs;
		    to += n_labs;
		}
		lab_buf_start = 0;
		lab_buf_end = lab_buf_diff;
		ret = QN_OK;
	    }
	    else
		ret = QN_BAD;
	}
    }
    return ret;
}

// Read features.
size_t
QN_InFtrStream_SplitFtrLab::read_ftrs(size_t a_count, float* a_ftrs)
{
    size_t res_count = 0;	// The returned count of frames read.
    const size_t ftr_stride = n_ftrs; // The stride of the ftr output buffer.

    // See if we have any suitable features in the buffer remaining from a
    // previous label read.
    size_t ftr_buf_len = ftr_buf_end - ftr_buf_start;
    if (ftr_segno==ftr_buf_segno && ftr_frameno==ftr_buf_frameno
	&& ftr_buf_len>0)
    {
	// Here we use features from the buffer.
	size_t buf_count = min(a_count, ftr_buf_len);
	if (a_ftrs!=NULL)
	{
	    // Copy out the stuff from the buffer if the data is really
	    // needed.
	    size_t i;
	    const float* ftr_buf_ptr = &ftr_buf[ftr_buf_start * n_ftrs];
	    for (i=0; i<buf_count; i++)
	    {
		qn_copy_vf_vf(n_ftrs, ftr_buf_ptr, a_ftrs);
		ftr_buf_ptr += n_ftrs;
		a_ftrs += ftr_stride;
	    }
	}
	log.log(QN_LOG_PER_BUNCH, "Copied features from buffer, "
		"first frame=%lu, num_frames=%lu.",
		(unsigned long) ftr_buf_frameno, (unsigned long) buf_count);
	// Update pointers etc.
	res_count += buf_count;
	ftr_buf_frameno += buf_count;
	ftr_buf_start += buf_count;
	ftr_frameno += buf_count;
    }

    // If we still need more, read from the real input stream, saving
    // labels if appropriate.
    size_t remaining_count = a_count - res_count;
    if (remaining_count > 0)
    {
	QNUInt32* lab_buf_ptr;

	// Check to see if it is appropriate to save stuff in label buffer
	// because it follows on from stuff already in the buffer.
	if (ftr_segno==lab_buf_segno
	    && ftr_frameno==(lab_buf_frameno+lab_buf_end-lab_buf_start)
	    && lab_freespace(remaining_count)==QN_OK )
	{
	    lab_buf_ptr = &lab_buf[lab_buf_end * n_labs];
	}
	// Check to see if it is appropriate to save stuff in label buffer
	// because the buffer is empty.
	else if (lab_buf_start==lab_buf_end
		 && lab_freespace(remaining_count)==QN_OK )
	{
	    lab_buf_segno = ftr_segno;
	    lab_buf_frameno = ftr_frameno;
	    lab_buf_start = lab_buf_end = 0;
	    lab_buf_ptr = &lab_buf[0];
	}
	else
	    lab_buf_ptr = NULL;

	// Make sure the real input stream point is in the right place.
	seek_if_necessary(ftr_segno, ftr_frameno);
	// Read the features, and maybe the labels.
	size_t read_count;
	read_count = instr.read_ftrslabs(remaining_count, a_ftrs, lab_buf_ptr);
	log.log(QN_LOG_PER_BUNCH, "Read features from input stream, "
		"first frame=%lu, num_frames=%lu.",
		(unsigned long) ftr_frameno, (unsigned long) read_count);
	if (lab_buf_ptr!=NULL)
	{
	    log.log(QN_LOG_PER_BUNCH, "Copied labels into buffer, "
		"first frame=%lu, num_frames=%lu.",
		(unsigned long) ftr_frameno, (unsigned long) read_count);
	}    
	res_count += read_count;
	instr_frameno += read_count;
	ftr_frameno += read_count;
	 // Reset feature buffer.
	ftr_buf_frameno = ftr_frameno;
	ftr_buf_start = ftr_buf_end = 0;
	if (lab_buf_ptr!=NULL)
	    lab_buf_end += read_count;
    }
    return res_count;
}

// Read labels.
size_t
QN_InFtrStream_SplitFtrLab::read_labs(size_t a_count, QNUInt32* a_labs)
{
    size_t res_count = 0;	// The returned count of frames read.
    const size_t lab_stride = n_labs; // The stride of the lab output buffer.

    // See if we have any suitable features in the buffer remaining from a
    // previous label read.
    size_t lab_buf_len = lab_buf_end - lab_buf_start;
    if (lab_segno==lab_buf_segno && lab_frameno==lab_buf_frameno
	&& lab_buf_len>0)
    {
	// Here we use features from the buffer.
	size_t buf_count = min(a_count, lab_buf_len);
	if (a_labs!=NULL)
	{
	    // Copy out the stuff from the buffer if the data is really
	    // needed.
	    size_t i;
	    const QNInt32* lab_buf_ptr =
		(QNInt32*) &lab_buf[lab_buf_start * n_labs];
	    for (i=0; i<buf_count; i++)
	    {
		qn_copy_vi32_vi32(n_labs, lab_buf_ptr, (QNInt32*) a_labs);
		lab_buf_ptr += n_labs;
		a_labs += lab_stride;
	    }
	}
	log.log(QN_LOG_PER_BUNCH, "Copied labels from buffer, "
		"first frame=%lu, num_frames=%lu.",
		lab_buf_frameno,	(unsigned long) buf_count);
	// Update pointers etc.
	res_count += buf_count;
	lab_buf_frameno += buf_count;
	lab_buf_start += buf_count;
	lab_frameno += buf_count;
    }

    // If we still need more, read from the real input stream, saving
    // labels if appropriate.
    size_t remaining_count = a_count - res_count;
    if (remaining_count > 0)
    {
	float* ftr_buf_ptr;

	// Check to see if it is appropriate to save stuff in featue buffer
	// because it follows on from stuff already in the buffer.
	if (lab_segno==ftr_buf_segno
	    && lab_frameno==(ftr_buf_frameno+ftr_buf_end-ftr_buf_start)
	    && ftr_freespace(remaining_count)==QN_OK )
	{
	    ftr_buf_ptr = &ftr_buf[ftr_buf_end * n_ftrs];
	}
	// Check to see if it is appropriate to save stuff in feature buffer
	// because the buffer is empty.
	else if (ftr_buf_start==ftr_buf_end
		 && ftr_freespace(remaining_count)==QN_OK )
	{
	    ftr_buf_segno = lab_segno;
	    ftr_buf_frameno = lab_frameno;
	    ftr_buf_start = ftr_buf_end = 0;
	    ftr_buf_ptr = &ftr_buf[0];
	}
	else
	    ftr_buf_ptr = NULL;

	// Make sure the real input stream point is in the right place.
	seek_if_necessary(lab_segno, lab_frameno);
	// Read the labels, and maybe the features.
	size_t read_count;
	read_count = instr.read_ftrslabs(remaining_count, ftr_buf_ptr, a_labs);
	log.log(QN_LOG_PER_BUNCH, "Read labels from input stream, "
		"first frame=%lu, num_frames=%lu.",
		(unsigned long) lab_frameno, (unsigned long) read_count);
	if (ftr_buf_ptr!=NULL)
	{
	    log.log(QN_LOG_PER_BUNCH, "Copied featues into buffer, "
		"first frame=%lu, num_frames=%lu.",
		(unsigned long) lab_frameno, (unsigned long) read_count);
	}    
	res_count += read_count;
	instr_frameno += read_count;
	lab_frameno += read_count;
	 // Reset label buffer.
	lab_buf_frameno = lab_frameno;
	lab_buf_start = lab_buf_end = 0;
	if (ftr_buf_ptr!=NULL)
	    ftr_buf_end += read_count;
    }
    return res_count;
}

int
QN_InFtrStream_SplitFtrLab::get_pos(size_t* segnop, size_t* framenop)
{
    if (segnop!=NULL)
	*segnop = ftr_segno;
    if (framenop!=NULL)
	*framenop = ftr_frameno;
    return QN_OK;
}

int
QN_InFtrStream_SplitFtrLab::lab_get_pos(size_t* segnop, size_t* framenop)
{
    if (segnop!=NULL)
	*segnop = lab_segno;
    if (framenop!=NULL)
	*framenop = lab_frameno;
    return QN_OK;
}

QN_SegID
QN_InFtrStream_SplitFtrLab::set_pos(size_t segno, size_t frameno)
{
    // Make sure the real input stream point is in the right place.
    assert(segno!=QN_SIZET_BAD && frameno!=QN_SIZET_BAD);
    seek_if_necessary(segno, frameno);

    if (instr_segid!=QN_SEGID_BAD)
    {
	ftr_segno = instr_segno;
	ftr_frameno = instr_frameno;
	ftr_buf_start = ftr_buf_end = 0;
	log.log(QN_LOG_PER_SENT, "Feature stream moved to seg %lu, frame %lu.",
		(unsigned long) ftr_segno, (unsigned long) ftr_frameno);
    }
    return instr_segid;
}

QN_SegID
QN_InFtrStream_SplitFtrLab::lab_set_pos(size_t segno, size_t frameno)
{
    // Make sure the real input stream point is in the right place.
    assert(segno!=QN_SIZET_BAD && frameno!=QN_SIZET_BAD);
    seek_if_necessary(segno, frameno);

    if (instr_segid!=QN_SEGID_BAD)
    {
	lab_segno = instr_segno;
	lab_frameno = instr_frameno;
	lab_buf_start = lab_buf_end = 0;
	log.log(QN_LOG_PER_SENT, "Label stream moved to seg %lu, frame %lu.",
		(unsigned long) lab_segno, (unsigned long) lab_frameno);
    }
    return instr_segid;
}

int
QN_InFtrStream_SplitFtrLab::rewind()
{
    int ret;			// Return value.

    ret = instr.rewind();
    if (ret==QN_OK)
    {
	// Remember where we are.
	instr_segno = ftr_segno = QN_SIZET_BAD;
	instr_frameno = ftr_frameno = QN_SIZET_BAD;
	// Zap the feature buffer.
	ftr_buf_start = ftr_buf_end = 0;
	log.log(QN_LOG_PER_EPOCH, "Feature stream rewound.");
    }
    return ret;
}

int
QN_InFtrStream_SplitFtrLab::lab_rewind()
{
    int ret;			// Return value.

    ret = instr.rewind();
    if (ret==QN_OK)
    {
	// Remember where we are.
	instr_segno = lab_segno = QN_SIZET_BAD;
	instr_frameno = lab_frameno = QN_SIZET_BAD;
	// Zap the feature buffer.
	lab_buf_start = lab_buf_end = 0;
	log.log(QN_LOG_PER_EPOCH, "Label stream rewound.");
    }
    return ret;
}

QN_SegID
QN_InFtrStream_SplitFtrLab::nextseg()
{
    // An optimization in case we are already in the right place.
    if ((instr_segno==ftr_segno+1) && (instr_frameno==0))
    {
	// Do not need to do anything.
    }
    else
    {
	// Make sure the real input stream point is in the right place.
	seek_if_necessary(ftr_segno, ftr_frameno);
	instr_segid = instr.nextseg();
	if (instr_segid!=QN_SEGID_BAD)
	{
	    if (instr_segno==QN_SIZET_BAD)
		instr_segno = 0;
	    else
		instr_segno++;
	    instr_frameno = 0;
	}
    }
    if (instr_segid!=QN_SEGID_BAD)
    {
	if (ftr_segno==QN_SIZET_BAD)
	    ftr_segno = 0;
	else
	    ftr_segno++;
	ftr_frameno = 0;
	log.log(QN_LOG_PER_SENT, "Feature stream moved on to segment %lu.",
		(unsigned long) ftr_segno);
    }
    else
	log.log(QN_LOG_PER_SENT, "Feature stream at end of input stream.");
    // Clear the buffer of remnants from the previous sentence.
    if (ftr_buf_segno!=ftr_segno)
    {
	ftr_buf_start = ftr_buf_end = 0;
    }
    return instr_segid;
}

QN_SegID
QN_InFtrStream_SplitFtrLab::lab_nextseg()
{
    // An optimization in case we are already in the right place.
    if ((instr_segno==lab_segno+1) && (instr_frameno==0))
    {
	// Do not need to do anything.
    }
    else
    {
	// Make sure the real input stream point is in the right place.
	seek_if_necessary(lab_segno, lab_frameno);
	instr_segid = instr.nextseg();
	if (instr_segid!=QN_SEGID_BAD)
	{
	    if (instr_segno==QN_SIZET_BAD)
		instr_segno = 0;
	    else
		instr_segno++;
	    instr_frameno = 0;
	}
    }
    if (instr_segid!=QN_SEGID_BAD)
    {
	if (lab_segno==QN_SIZET_BAD)
	    lab_segno = 0;
	else
	    lab_segno++;
	lab_frameno = 0;
	log.log(QN_LOG_PER_SENT, "Label stream moved on to segment %lu.",
		(unsigned long) lab_segno);
    }
    else
	log.log(QN_LOG_PER_SENT, "Label stream at end of input stream.");
    // Clear the buffer of remnants from the previous sentence.
    if (lab_buf_segno!=lab_segno)
    {
	lab_buf_start = lab_buf_end = 0;
    }
    return instr_segid;
}


