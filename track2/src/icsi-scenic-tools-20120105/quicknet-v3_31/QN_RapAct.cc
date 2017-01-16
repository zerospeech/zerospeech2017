const char* QN_OutFtrStream_Rapact_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_RapAct.cc,v 1.22 2004/04/13 22:53:41 davidj Exp $";

#include <QN_config.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "QN_intvec.h"
#include "QN_fltvec.h"
#include "QN_types.h"
#include "QN_RapAct.h"
#include "QN_libc.h"

QN_InFtrStream_Rapact::QN_InFtrStream_Rapact(int a_debug,
					     const char* a_dbgname,
					     FILE* a_file,
					     QN_StreamType a_format,
					     int a_indexed)
    : clog(a_debug, "QN_InFtrStream_Rapact", a_dbgname),
      file(a_file),
      format(a_format),
      indexed(a_indexed)
{
    if (indexed)
    {
	clog.error("indexed reading of RAP activation files not "
		   "yet supported.");
    }
    if (format!=QN_STREAM_RAPACT_BIN)
    {
	clog.error("reading of anything other than binary activation files "
		   "not yet supported.");
    }

    QNUInt32 width_buf;
    int ec;
    ec = fread((void*) &width_buf, sizeof(width_buf), 1, file);
    if (ec!=1)
    {
	clog.error("failed to read initial frame width - %s.",
		   strerror(errno));
    }
    width = qn_btoh_i32_i32(width_buf);
    buf = new float[width];

    current_frameno = -1;
    current_segno = -1;

    // Pretend we're at EOS so first next_seg() thinks it's job is done
    atEOS = 1;
}

QN_InFtrStream_Rapact::~QN_InFtrStream_Rapact()
{
    delete buf;
}

size_t
QN_InFtrStream_Rapact::num_ftrs()
{
    return width;
}

size_t
QN_InFtrStream_Rapact::num_segs()
{
    return QN_SIZET_BAD;
}

size_t 
QN_InFtrStream_Rapact::num_frames(size_t)
{
    return QN_SIZET_BAD;
}

QN_SegID
QN_InFtrStream_Rapact::set_pos(size_t, size_t)
{
    return QN_SEGID_BAD;
}


int 
QN_InFtrStream_Rapact::get_pos(size_t* segnop, size_t* framenop)
{
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (current_segno==-1)
    {
	segno = QN_SIZET_BAD;
	frameno = QN_SIZET_BAD;
    }
    else
    {
	segno = (size_t) current_segno;
	frameno = (size_t) current_frameno;
	assert(current_frameno>=0);
    }

    if (segnop!=NULL)
	*segnop = segno;
    if (framenop!=NULL)
	*framenop = frameno;
    clog.log(QN_LOG_PER_SENT, "Currently at sentence %lu, frame %lu.",
	    (unsigned long) segno, (unsigned long) frameno);
    return QN_OK;
}

int
QN_InFtrStream_Rapact::rewind()
{
    int ret;

    if (fseek(file, 0, SEEK_SET)!=0)
    {
	clog.log(QN_LOG_PER_EPOCH, "Seek failed.");
	ret = QN_BAD;
    }
    else
    {
	clog.log(QN_LOG_PER_EPOCH, "At start of file.");
	current_segno = -1;
	current_frameno = -1;
	ret = QN_OK;
    }
    return ret;
}

int QN_InFtrStream_Rapact::read1frame(void) {
    // Read the next frame into the buf.
    // Return <width> for success, 0 for end of utterance, -1 for end of file
    QNUInt32 frameno_buf;
    size_t ec;
    int frameno;

    atEOF = atEOS = 0;
    
    ec = fread((void*) &frameno_buf, sizeof(frameno_buf), 1, file);
    if (ec!=1) {
	// Presumably EOF
	atEOF = atEOS = 1;
	return -1;
    }
    frameno = qn_btoh_i32_i32(frameno_buf);
    if (frameno == -1) {
	// End of this segment
	atEOS = 1;
	return 0;
    }
    if (frameno != current_frameno + 1) {
	clog.error("rapbin frame claimed to be %d, expecting %d", frameno, current_frameno+1);
    }
    current_frameno = frameno;
    // Read all the frames
    ec = fread((void*)buf, sizeof(float), width, file);
    if (ec != width) {
	clog.error("unexpected EOF in middle of RapBin frame");
	atEOF = atEOS = 1;
	return -1;	// Abrupt EOF
    }
    // Byteswap?
    qn_btoh_vi32_vi32(width, (int *)buf, (int *)buf);

    // All OK
    return width;
}

size_t
QN_InFtrStream_Rapact::read_ftrs(size_t frames, float* ftrs)
{
    int gotframes = 0;
    
    // Just return the previously-read one, but read the next
    while( !atEOF && !atEOS && gotframes < (int)frames) {
	qn_copy_vi32_vi32(width, (const QNInt32 *)buf, (QNInt32 *)ftrs);
	ftrs += width;
	++gotframes;
	read1frame();
    }
    return gotframes;
}
    
QN_SegID
QN_InFtrStream_Rapact::nextseg(void)
{
    // Scan through remaining frames in this utt
    while (!atEOS && !atEOF) {
	read1frame();
    }
    ++current_segno;
    current_frameno = -1;
    // pre-read first frame of next to test for EOF
    read1frame();
    if (atEOF) {
	return QN_SEGID_BAD;
    }
    return QN_SEGID_UNKNOWN;
}

////////////////////////////////////////////////////////////////

QN_OutFtrStream_Rapact::
QN_OutFtrStream_Rapact(int dbg, const char* dbgname, FILE* f,
		       size_t wid, QN_StreamType form, int onl)
  : clog(dbg, "QN_OutFtrStream_Rapact", dbgname),
    file(f),
    width(wid),
    format(form),
    online(onl),
    buf(new float[wid]),
    current_frameno(0)
    
{

    assert(format==QN_STREAM_RAPACT_ASCII
	   || format==QN_STREAM_RAPACT_HEX
	   || format==QN_STREAM_RAPACT_BIN);

    switch(format)
    {
    case QN_STREAM_RAPACT_ASCII:
    case QN_STREAM_RAPACT_HEX:
	fprintf(file, "***BEGIN PIPE OUTPUT:\n");
	fprintf(file, "%d\n", (int) width);
	fflush(file);
	if (ferror(file))
	{
	    clog.error("failed writing at start of RAPACT character "
		       "output stream - %s.", strerror(errno));
	}
	break;
    case QN_STREAM_RAPACT_BIN:
    {
	QNUInt32 n;

	n = qn_htob_i32_i32(width);
	fwrite((char *) &n, sizeof(n), 1, file);
	fflush(file);
	if (ferror(file))
	{
	    clog.error("failed writing at start of RAPACT binary "
		       "output stream - %s .", strerror(errno));
	}
	break;
    }
    default:
	assert(0);
    } // switch(format)

    const char* formatstr = NULL;
    switch(format)
    {
    case QN_STREAM_RAPACT_ASCII:
	formatstr = "ASCII";
	break;
    case QN_STREAM_RAPACT_HEX:
	formatstr = "hex";
	break;
    case QN_STREAM_RAPACT_BIN:
	formatstr = "binary";
	break;
    default:
	assert(0);
    }
    clog.log(1, "Started output stream, format=RAP %s width=%u.",
	     formatstr, (unsigned) width);
}

// Destructor - wind everything up
QN_OutFtrStream_Rapact::~QN_OutFtrStream_Rapact()
{
    int ec;			// Error flag
    if (online)
    {
	ec = fflush(file);
	if (ec!=0)
	{
	    clog.error("failed to flush at end of stream - %s.",
		     strerror(errno));
	}
    }
    delete[] buf;
    // There is no end of file marker in any of the rapact formats.
    if (current_frameno!=0)
    {
	clog.error("failed to write end of segment marker "
		   "in file %s.", QN_FILE2NAME(file));
    }
}

// Return the width of the file
size_t
QN_OutFtrStream_Rapact::num_ftrs()
{
    return width;
}

void
QN_OutFtrStream_Rapact::doneseg(QN_SegID)
{
    int ec;			// Error code

    if (current_frameno==0)
    {
	clog.error("wrote zero length segment.");
    }
    else
    {
	switch(format)
	{
	case QN_STREAM_RAPACT_ASCII:
	case QN_STREAM_RAPACT_HEX:
	    fprintf(file, "-1\n\n");
	    if (ferror(file))
	    {
		clog.error("failed to write end of segment marker"
			 " - %s.", strerror(errno));
	    }
	    break;
	case QN_STREAM_RAPACT_BIN:
	{
	    QNInt32 endmarker = qn_htob_i32_i32(-1);

	    ec = fwrite((char*) &endmarker, sizeof(endmarker), 1, file);
	    if (ec!=1)
	    {
		clog.error("failed to write end of segment marker"
			 " - %s.", strerror(errno));
	    }
	    break;
	}
	default:
	    assert(0);
	}
    }
    if (online)
    {
	ec = fflush(file);
	if (ec!=0)
	{
	    clog.error("failed to flush at end of segment - %s.",
		     strerror(errno));
	}
    }
    current_frameno = 0;
}

					       
// Output a series of frames

void
QN_OutFtrStream_Rapact::write_ftrs(size_t frames, const float* ftrs)
{
    size_t i, j;		// Local counters
    const float* frame_ftr = ftrs; // A pointer to the current frame data to
				// be output
    int ec;			// Error flag
    
    for (i=0; i<frames; i++)
    {
	switch(format)
	{
	case QN_STREAM_RAPACT_ASCII:
	{
	    fprintf(file, "%d\n", (int) current_frameno);
	    for (j=0; j<width; j++)
	    {
		if (j!=0)
		    putc(' ', file);
		fprintf(file, "%g", (double) frame_ftr[j]);
	    }
	    putc('\n', file);
	    break;
	}
	case QN_STREAM_RAPACT_HEX:
	{
	    union { QNUInt32 u; float f; } val;

	    fprintf(file, "%d\n", (int) current_frameno);
	    for (j=0; j<width; j++)
	    {
		val.f = frame_ftr[j];
		if (j!=0)
		    putc(' ', file);
		fprintf(file, "%8.8x", val.u);
	    }
	    putc('\n', file);
	    break;
	}
	case QN_STREAM_RAPACT_BIN:
	{
	    QNInt32 frame;

	    frame = qn_htob_i32_i32(current_frameno);
	    ec = fwrite((char *) &frame, sizeof(frame), 1, file);
	    if (ec!=1)
	    {
		clog.error("activation frameno output failed - %s.",
			 strerror(errno));
	    }
	    qn_htob_vf_vf(width, frame_ftr, buf);
	    ec = fwrite((char *) buf, sizeof(*buf), width, file);
	    if (ec!=(int) width)
	    {
		clog.error("activation feature output failed - %s.",
			 strerror(errno));
	    }
	    break;
	}
	default:
	    assert(0);
	} // switch(format)

	frame_ftr += width;
	current_frameno++;

    } // for (i=0; i<frames; i++)
    if (online)
    {
	ec = fflush(file);
	if (ec!=0)
	{
	    clog.error("failed to flush at end of frames - %s.",
		       strerror(errno));
	}
    }
}


