const char* QN_ILab_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_ILab.cc,v 1.8 2010/10/29 02:20:37 davidj Exp $";

// This file contains routines to access the new ICSI compressed 
// binary label file format, "ILab".  This stores the same information 
// as 

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
#include "QN_intvec.h"
#include "QN_ILab.h"
#include "QN_Logger.h"

// The structure of an ILabfile header

#define QN_ILAB_VERSION 19990304
static const char *qn_ilab_magic = "ILAB";

typedef struct {
    QNInt32	magic;		// "ILAB" to mark file type
    QNInt32	version;	// version specifier - 19990304 for now
    QNInt32	dataOffset;	// i.e. number of bytes in the header
    QNInt32	indexOffset;	// where to seek to to find the index.
    QNInt32	bitsPerLabel;	// bits per label determines how many bytes 
    QNInt32	numSegments;	// total number of segments in this file
    QNInt32	numFrames;	// sum of all the frames in all the segments
} QN_ILab_header;

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InLabStream_ILab
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_InLabStream_ILab::QN_InLabStream_ILab(int a_debug,
					 const char* a_dbgname,
					 FILE* a_file,
					 int a_indexed)
  : log(a_debug, "QN_InLabStream_ILab", a_dbgname),
    file(a_file),
    indexed(a_indexed),
    segpos(NULL), 
    seglens(NULL)
{
    if (fseek(file, 0, SEEK_SET)) {
	log.error("Failed to seek to start of ilabfile '%s' header - "
		 "cannot read ilabfiles from streams.",
		 QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Started using ilabfile '%s'.", QN_FILE2NAME(file));
    read_header();
    log.log(QN_LOG_PER_RUN, "num_sents=%u num_frames=%u.",
	    total_segs, total_frames);

    if (indexed) {
	// Allocate space for sentence index
	segpos = new QNUInt32[total_segs];
	seglens = new QNUInt32[total_segs];
	if (sentind_offset != 0)
	    build_index_from_sentind_sect();
	else
	    build_index_from_data_sect();
    }
    // Move to the start of the ilabfile
    rewind();
}

QN_InLabStream_ILab::~QN_InLabStream_ILab()
{
    if (indexed) {
	delete[] segpos;
	delete[] seglens;
    }
}


void
QN_InLabStream_ILab::read_header()
{
    QN_ILab_header header;
    
// Read in ilabfile header

    if (fread(&header, sizeof(header), 1, file)!=1)
	log.error("Failed to read ilabfile '%s' header.",
		 QN_FILE2NAME(file));

// Check ilabfile header

    if (strncmp((char *)&header.magic, qn_ilab_magic, sizeof(QNInt32)) != 0)
	log.error("Bad ilab header in '%s'.",
		 QN_FILE2NAME(file));
    // Convert to host order (stored bigendian)
    qn_btoh_vi32_vi32(sizeof(header)/sizeof(QNInt32), 
		   (QNInt32*) &header, (QNInt32*) &header);
    if (header.version != QN_ILAB_VERSION) {
	log.error("Unknown ilab version (%d is not expected %d) in '%s'.",
		  header.version, QN_ILAB_VERSION, QN_FILE2NAME(file));
    }

// Put some stuff we stored locally in the main structure
    bytes_per_label = header.bitsPerLabel / QN_BITS_PER_BYTE;
    total_segs = header.numSegments;
    total_frames = header.numFrames;

    data_offset = header.dataOffset;
    sentind_offset = header.indexOffset;
}

// Build a sentence index using the sentence index section in the ILabfile

void
QN_InLabStream_ILab::build_index_from_sentind_sect()
{
    if (fseek(file, sentind_offset, SEEK_SET)!=0)
    {
        log.error("Failed to move to start of sentence index data, "
		  "sentind_offset=%li - probably a corrupted ilabfile.",
		  sentind_offset);
    }

    // First read the segpos array - seek positions of each frame

    long size = sizeof(QNInt32) * total_segs;
    if (fread((char *) segpos, size, 1, file)!=1) {
	log.error("Failed to read segpos section in '%s'"
		 " - probably a corrupted ILabfile.", QN_FILE2NAME(file));
    }
    // The index is in big-endian format on file - convert it to the host
    // endianness
    qn_btoh_vi32_vi32(total_segs, (QNInt32*) segpos,
		   (QNInt32*) segpos);

    // Now read the frames-per-segment index

    size = sizeof(QNInt32) * total_segs;
    if (fread((char *) seglens, size, 1, file)!=1)
    {
	log.error("Failed to read seglens section in '%s'"
		 " - probably a corrupted ILabfile.", QN_FILE2NAME(file));
    }
    // The index is in big-endian format on file - convert it to the host
    // endianness
    qn_btoh_vi32_vi32(total_segs, (QNInt32*) seglens,
		   (QNInt32*) seglens);

    // Check that the sum of the lengths of the segments matches the 
    // total number of frames indicated to be in the file by the header
    QNUInt32 lensum = 0;
    int i;
    for (i=0; i<(int)total_segs; ++i) {
	lensum += seglens[i];
    }
    if (lensum != total_frames) {
	log.error("Sum of segment lens (%lu) does not correspond"
		 " with total frame count (%i) in ILabfile '%s' - probably a"
		 " corrupted ILabfile.", (unsigned long) lensum, 
		 total_frames, QN_FILE2NAME(file));
    }

    log.log(QN_LOG_PER_RUN, "Read sent_table_data for %lu sentences from '%s'.",
	    (unsigned long) total_segs, QN_FILE2NAME(file));
}

int QN_OutLabStream_ILab::write_next_block(QNUInt32 count, QNUInt32 label) {
    // Write out a block of labels to the current stream
    // return number of bytes written
    int i, nbytes, nwritten = 0;
    unsigned char c, topbytemask;
    unsigned char buf[4];
    QNUInt32 mycount = count;
    // How many bytes of count to write
    if (count < 128) {
	nbytes = 1;
	topbytemask = 0;
    } else if (count < 16384) {
	nbytes = 2;
	topbytemask = 0x80;
    } else {
	nbytes = 4;
	topbytemask = 0xC0;
    }
    // Write the count
    for ( i = 0; i < nbytes; ++i ) {
	c = (0xFF & mycount);
	mycount >>= 8;
	if (i == nbytes-1) {
	    c |= topbytemask;
	}
	buf[nbytes - 1 - i] = c;
    }
    if (fwrite(buf, sizeof(char), nbytes, file) != (size_t)nbytes) {
	// Write error
	return 0;
    }
    // Track number of bytes written
    nwritten += nbytes;

    // If count > 0, write the label (count == 0 is the EOS marker)
    if (count > 0) {
	nbytes = bytes_per_label;
	// Write the count
	for ( i = 0; i < nbytes; ++i ) {
	    c = (0xFF & label);
	    label >>= 8;
	    buf[nbytes - 1 - i] = c;
	}
	if (fwrite(buf, sizeof(char), nbytes, file) != (size_t)nbytes) {
	    // Write error
	    return 0;
	}
	// Track number of bytes written
	nwritten += nbytes;
    }
    return nwritten;
}

int QN_InLabStream_ILab::read_next_block(void) {
    // Within a segment, read a compressed block
    // return count of bytes consumed
    QNUInt32 count = 0, label = 0;
    unsigned char c;
    int i, nbytes = 1;
    int nread = 0;

    if (fread(&c, sizeof(char), 1, file) != 1) {
	// EOF
	return 0;
    }
    ++nread;
    if (c < 128) {
	// 0x0??? ???? -> 1-byte length code
	count = c;
    } else {
	if (c < 192) {
	    // 0x10?? ???? -> 2-byte length code
	    nbytes = 2;
	} else {
	    // 0x11?? ???? -> 4-byte length code
	    nbytes = 4;
	}
	count = (0x3F & (int)c);
	// read next bytes, shifting up
	for (i = 0; i < nbytes-1; ++i) {
	    if (fread(&c, sizeof(char), 1, file) != 1) {
		// EOF
		return 0;
	    }
	    ++nread;
	    count = c + (256*count);
	}
    }
    // count = 0 implies end of seg.  Otherwise, read the label
    if (count > 0) {
	// Read the label value
	// read next bytes, shifting up
	nbytes = bytes_per_label;
	for (i=0; i<nbytes; ++i) {
	    if (fread(&c, sizeof(char), 1, file) != 1) {
		// EOF
		return 0;
	    }
	    ++nread;
	    label = c + (256*label);
	}
    }

    // Store in object slots
    block_start_frame += block_frame_count;
    block_frame_count = count;
    block_label = label;

    return nread;
}


	    
// Build a sentence index using the data section in the ILabfile
// This entails scanning the whole ILabfile

void
QN_InLabStream_ILab::build_index_from_data_sect()
{
    long bytes_this_seg = 0;
    long frames_this_seg = 0;
    long abs_frame_no = 0;
    long last_segpos = data_offset;
    int nbytes;

    rewind();

    while (!feof(file)) {
	if (bytes_this_seg == 0) {
	    if (nextseg() != QN_SEGID_BAD) {
		// Be sure to count the segment number header we consumed
		bytes_this_seg = 4;
	    }
	} else {
	    // Read the next block in this segment
	    nbytes = read_next_block();
	    if (nbytes > 0) {
		// not EOF
		bytes_this_seg += nbytes;
		frames_this_seg += block_frame_count;
		if (block_frame_count == 0) {
		    // End of block - record and move on
		    segpos[current_seg] = last_segpos;
		    last_segpos += bytes_this_seg;
		    bytes_this_seg = 0;
		    seglens[current_seg] = frames_this_seg;
		    abs_frame_no += frames_this_seg;
		    frames_this_seg = 0;
		    // current_seg will be incremented by nextseg()
		}
	    }
	}
    }
    // Assume we have hit EOF.  But last marker should have been EOS
    if (bytes_this_seg != 0) {
	log.error("Unexpected end-of-file in segment %i"
		  " in ILabfile '%s' - probably a"
		  " corrupted ILabfile.", 
		  current_seg, QN_FILE2NAME(file));
    }
    // Also, we should have the number of segments we expected
    if ((unsigned long) current_seg != (unsigned long) total_segs) {
	log.error("Last segment index (%lu) does not correspond"
		 " with number of segs (%lu) in ILabfile '%s' - probably a"
		 " corrupted ILabfile.", (unsigned long) current_seg,
		 (unsigned long) total_segs, QN_FILE2NAME(file));
    }

    // Check that the index of the frame after the last one is the same
    // as the number of frames in the PFile
    if ((unsigned long) abs_frame_no != (unsigned long) total_frames) {
	log.error("Total frame count (%lu) does not correspond"
		 " with number of frames (%lu) in ILabfile '%s' - probably a"
		 " corrupted ILabfile.", (unsigned long) abs_frame_no,
		 (unsigned long) total_frames, QN_FILE2NAME(file));
    }
    log.log(QN_LOG_PER_RUN, "Indexed %lu sentences.", (unsigned long) total_segs);
}

// Move to the start of the ILabfile
int
QN_InLabStream_ILab::rewind()
{
    // Move to the start of the data and initialise our own file offset.
    if (fseek(file, data_offset, SEEK_SET)!=0)
    {
	log.error("Rewind failed to move to start of data in "
		 "'%s', data_offset=%li - probably corrupted ILabfile.",
		 QN_FILE2NAME(file), data_offset);
    }
    current_seg = -1;
    current_frame = 0;
    block_start_frame = -1;
    block_frame_count = 0;
    block_label = -1;
    log.log(QN_LOG_PER_EPOCH, "At start of ILabfile.");
    return 0;			// Should return sentence ID
}

size_t
QN_InLabStream_ILab::read_labs(size_t frames, QNUInt32* labs)
{
    size_t count = 0;		// Count of number of frames
    size_t first_frame = current_frame;
    size_t rem_frames = frames;
    int atEOS = 0;
    int i, todo;
    long block_end_frame;
    
    if (block_start_frame == -1) {
	log.error("Somehow managed to call read_labs before nextseg "
		 "on ILabfile '%s'.", QN_FILE2NAME(file));
    }

    if (block_start_frame != 0 && block_frame_count == 0) {
	atEOS = 1;
    }
    while (rem_frames > 0 && !atEOS) {
	// Take from any current buffer first
	block_end_frame = block_start_frame + block_frame_count;
	if (current_frame < block_end_frame) {
	    todo =block_end_frame - current_frame;
	    if (rem_frames < (size_t)todo) todo = rem_frames;
	} else {
	    // current_frame is beyond end of current block
	    todo = 0;
	    read_next_block();
	    if (block_frame_count == 0) {
		atEOS = 1;
	    }
	}
	for (i = 0; i < todo; ++i) {
	    *labs++ = block_label;
	    ++current_frame;
	    ++count;
	    --rem_frames;
	}
    }
    if (rem_frames > 0) {
	//	log.error("Hit end-of-segment at frame %li in seg %li"
	//	  " with %li frames unread"
	//	  " in file '%s' - probably corrupted ILabfile.",
	//	  current_frame, current_seg, rem_frames, QN_FILE2NAME(file));
	// No, it's OK - just return the number of frames read so far.
    }
    log.log(QN_LOG_PER_BUNCH, "Read sentence %lu, first_frame=%lu, "
	    "num_frames=%lu.", (unsigned long) current_seg,
	    (unsigned long) first_frame, (unsigned long) count);
    return count;
}

QN_SegID
QN_InLabStream_ILab::nextseg()
{
    int ret;			// Return value
    int nbytes;
    int skip_count = 0;
    QNUInt32 segno;
    long block_end_frame;

    // DON'T try to read to end of seg if we just did a seek
    if (block_start_frame != -1) {
	// Preset skip count to remaining frames in current block
	block_end_frame = block_start_frame + block_frame_count;
	skip_count = block_end_frame - current_frame;
	current_frame = block_end_frame;

	// Skip over any remaining blocks in current segment - 
	// unless we're already waiting at the start (indicated by 
	// both b_s_f and b_f_c == 0)
	if (block_start_frame != 0 || block_frame_count != 0) {
	    while (block_frame_count != 0) {
		read_next_block();
		skip_count += block_frame_count;
		current_frame += block_frame_count;
	    }
	}
	// If possible, check that the end of sentence ties up with the index
	if (indexed) {
	    if (current_seg >= 0 \
		&& current_frame != (long) seglens[current_seg])
		log.error("Sentence data inconsistent with index in"
			 " PFile '%s' - sentence %li should have %li frames "
			 "but instead ends at frame %li", QN_FILE2NAME(file),
			 current_seg,
			 (long) seglens[current_seg],
			 current_frame);
	    ret = QN_SEGID_BAD;
	}
    }
    // If we are at the end of file, exit gracefully
    current_seg++;
    if (current_seg ==(long)total_segs) {
	log.log(QN_LOG_PER_SENT, "At end of PFile.");
	ret = QN_SEGID_BAD;
    } else {
	// First 4 bytes of frame should be segment number
	if ( (nbytes = fread(&segno, 1, sizeof(QNUInt32), file)) > 0) {
	    // Check that it's the frame we expect
	    qn_btoh_vi32_vi32(1, (QNInt32*) &segno,
			   (QNInt32*) &segno);
	    if (segno != (QNUInt32)current_seg) {
		log.error("Segment number %li had unexpected number %li"
			  " in ILabfile '%s' - probably a"
			  " corrupted ILabfile.", 
			  current_seg, segno, QN_FILE2NAME(file));
	    }
	}
	if (skip_count!=0) {
	    log.log(QN_LOG_PER_SENT, "Skipped %lu frames in sentence %li.",
		    skip_count, current_seg);
	}
	current_frame = 0;
	// clear the block end as well
	block_start_frame = 0;
	// other values should already be zero, but make sure
	block_frame_count = 0;

	if (nbytes != sizeof(QNUInt32)) {
	    log.error("EOF reading start of sentence %li in ILabfile '%s'"
		     " - probably corrupted ILabfile.",
		     current_seg, QN_FILE2NAME(file));
	}
	if (current_seg!=(int)segno) {
	    log.error("Sentence %li in ILabfile '%s' has sentence "
		     "number %li - probably corrupted ILabfile.",
		     current_seg, QN_FILE2NAME(file), segno);
	}
	log.log(QN_LOG_PER_SENT, "At start of sentence %lu.",
		(unsigned long) current_seg);

	// Preload the first block (can't be an EOS mark)
	//	read_next_block();
	//if (block_frame_count == 0) {
	//    log.error("Sentence %li in ILabfile '%s' has zero length "
	//	     "- probably corrupted ILabfile.",
	//	     current_seg, QN_FILE2NAME(file));
	//}

	ret = 0;		// FIXME - should return proper segment ID
    }
    return ret;
}

QN_SegID
QN_InLabStream_ILab::set_pos(size_t segno, size_t frameno)
{
    int ret;			// Return value

    if (segno > total_segs) {
	log.error("Seek to seg %li is beyond last segment %li.",
		  (unsigned long) segno, (unsigned long) total_segs);
    }
    if (indexed) {
	if (frameno > seglens[segno]) {
	    log.error("Seek to frame %li is beyond end of sentence %li"
		      " at %li.",
		      (unsigned long) frameno, (unsigned long) segno, 
		      (unsigned long) seglens[segno]);
	}

	// Seek to start of frame, then have to read through until 
	// reach desired block
	long offset = segpos[segno];
	if (fseek(file, offset, SEEK_SET)!=0) {
	    log.error("Seek failed in ILabfile "
		     "'%s', offset=%li - file problem?",
		     QN_FILE2NAME(file), offset);
	}
	// Set up for nextseg()
	current_seg = segno - 1;
	block_start_frame = -1;
	block_frame_count = 0;
	nextseg();
	// current_frame is now 0 - step forward till we get block
	// containing requested frame
	while (block_start_frame + block_frame_count <= (int)frameno) {
	    read_next_block();
	    if (block_frame_count == 0) {
		// QN_cut often set_pos's to the very end of a segment 
		// if that's where we had got to before the last cut swap.
		// Thus, tolerate being seeked to the end of a segment.
		if (block_start_frame == (int)frameno) {
		    break;
		} else {
		    log.error("set_pos(%d,%d) failed - end of segment %li"
			      " at %li in ILabfile '%s', file problem?",
			      segno, frameno, 
			      current_seg, block_start_frame, 
			      QN_FILE2NAME(file));
		}
	    }
	}
	// OK, we've found a block containing the desired frame no: be there
	current_frame = frameno;

	log.log(QN_LOG_PER_SENT, "Seek to sentence %lu, frame %lu.",
		(unsigned long) segno, (unsigned long) frameno);
	ret = 0;		// FIXME - should return sentence ID
    } else {
	// Tried to seek when not indexed
	ret = QN_SEGID_BAD;
    }
    return ret;
}

int
QN_InLabStream_ILab::get_pos(size_t* segnop, size_t* framenop)
{
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (current_seg==-1) {
	segno = QN_SIZET_BAD;
	frameno = QN_SIZET_BAD;
    } else {
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
    return QN_OK;
}


size_t
QN_InLabStream_ILab::num_frames(size_t segno)
{
    size_t num_frames;		// Number of frames returned

    assert(segno<total_segs || segno==QN_ALL);

    if (segno==QN_ALL) {
	num_frames = total_frames;
	log.log(QN_LOG_PER_EPOCH, "%lu frames in ILabfile.",
		(unsigned long) num_frames);
    } else {
	if (indexed) {
	    num_frames = seglens[segno];
	    log.log(QN_LOG_PER_SENT, "%lu frames in sentence %lu.",
		    (unsigned long) num_frames, (unsigned long) segno);
	} else {
	    num_frames = QN_SIZET_BAD;
	}
    }
    
    return num_frames;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrLabStream_ILabfile
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

QN_OutLabStream_ILab::QN_OutLabStream_ILab(int a_debug,
					   const char* a_dbgname,
					   FILE* a_file,
					   int maxlabel, 
					   int a_indexed)
  : clog(a_debug, "QN_OutFtrLabStream_ILab", a_dbgname),
    file(a_file),
    indexed(a_indexed),
    current_seg(0),
    current_frame(0),
    current_pos(0),
    total_frames(0),
    block_start_frame(0),
    block_frame_count(0),
    segpos(NULL),
    seglens(NULL),
    index_len(0)
{
    // figure out bytes per label to contain maxlabel
    unsigned long maxlabelp1 = 256;
    bytes_per_label = 1;
    while( !(maxlabelp1 > (unsigned long)maxlabel) ) {
	++bytes_per_label;
	maxlabelp1 <<= 8;
    }

    // Write a header out just to get us started
    write_header();
    current_pos = sizeof(QN_ILab_header);
    clog.log(1, "Creating ILabfile '%s'.",  QN_FILE2NAME(file) );

    if (indexed!=0) {
	segpos = new QNUInt32[DEFAULT_INDEX_SIZE];
	seglens = new QNUInt32[DEFAULT_INDEX_SIZE];
	index_len = DEFAULT_INDEX_SIZE;
	segpos[0] = current_pos;
    }
}

QN_OutLabStream_ILab::~QN_OutLabStream_ILab()
{
    // doneseg() should be called before closing ILabfile - not doing this
    // is an error.
    if (current_frame!=0) {
	clog.error("ILabfile '%s' closed mid sentence.",
		  QN_FILE2NAME(file));
    }
    if (indexed) {
	write_index();
    }
    write_header();
    clog.log(1, "Finished creating ILabfile '%s'.", 
	     QN_FILE2NAME(file));
    if (segpos!=NULL)
	delete[] segpos;
    if (seglens!=NULL)
	delete[] seglens;
}



void
QN_OutLabStream_ILab::write_labs(size_t frames, const QNUInt32* labs)
{
    long rem_frames = frames;
    QNUInt32 lab;
    int ec = 0;

    // Make sure we write out a header for this segment if it's the 
    // first time we write to it
    if (current_frame == 0 && frames > 0) {
	QNUInt32 segno = current_seg;
	qn_htob_vi32_vi32(1, (QNInt32*) &segno,
		       (QNInt32*) &segno);
	ec = fwrite(&segno, sizeof(QNUInt32), 1, file);
	if (ec != 1) {
	    clog.error("Failed to write segment header ILabfile '%s' - %s.",
		       QN_FILE2NAME(file), strerror(errno)); 
	}
	current_pos += sizeof(QNUInt32);
    }

    // We don't write out a block until we've seen how many frames 
    // in a row have the same label, or until EOS

    while(rem_frames) {
	lab = *labs++;
	if (lab != (QNUInt32)block_label) {
	    if (block_frame_count > 0) {
		// Not the first block, so write out what we've got
		current_pos += write_next_block(block_frame_count, 
						block_label);
	    }
	    block_label = lab;
	    block_frame_count = 0;
	    block_start_frame = current_frame;
	}
	++block_frame_count;
	++current_frame;
	++total_frames;
	--rem_frames;
    }
}

void
QN_OutLabStream_ILab::doneseg(QN_SegID) {
    if (current_frame==0)
	clog.error("wrote zero length sentence.");

    // Write out the final block (should always be one)
    if (block_frame_count > 0) {
	current_pos += write_next_block(block_frame_count, block_label);
	block_frame_count = 0;
    }
    // Write out the EOS marker (a zero-len block)
    current_pos += write_next_block(block_frame_count, block_label);

    clog.log(4, "Finished sentence %lu.",
	     (unsigned long) current_seg); 

    // Update the frame-length index.  The pos index is always one
    // ahead, so leave realloc for that
    if (indexed) {
	seglens[current_seg] = current_frame;
    }
    current_seg++;
    current_frame = 0;

    // Update the file pos index if necessary.
    if (indexed) {
	// If the index is not large enough, make it bigger.
	// segpos actually has total_segs+1 entries
	if ((size_t) current_seg>=index_len) {
	    size_t new_index_len = index_len * 2;
	    QNUInt32* new_segpos = new QNUInt32[new_index_len];
	    qn_copy_vi32_vi32(index_len, (QNInt32*) segpos, 
			      (QNInt32*) new_segpos);
	    delete[] segpos;
	    segpos = new_segpos;
	    QNUInt32* new_seglens = new QNUInt32[new_index_len];
	    qn_copy_vi32_vi32(index_len, (QNInt32*) seglens, 
			      (QNInt32*) new_seglens);
	    delete[] seglens;
	    seglens = new_seglens;
	    index_len = new_index_len;
	}
	// Update the index.
	segpos[current_seg] = current_pos;
    }
}

void
QN_OutLabStream_ILab::write_index()
{
    // The index is in big-endian format on file - convert it from the host
    // endianness
    qn_htob_vi32_vi32(current_seg, (QNInt32*) segpos,
		   (QNInt32*) segpos);
    fwrite(segpos, current_seg * sizeof(QNUInt32), 1, file);
    qn_htob_vi32_vi32(current_seg, (QNInt32*) seglens,
		   (QNInt32*) seglens);
    fwrite(seglens, current_seg * sizeof(QNUInt32), 1, file);
    clog.log(1, "Wrote sentence index for %lu sentences.",
	     (unsigned long) current_seg);
    // Swap it back just in case we want to use it again
    qn_btoh_vi32_vi32(current_seg, (QNInt32*) segpos,
		   (QNInt32*) segpos);
    qn_btoh_vi32_vi32(current_seg, (QNInt32*) seglens,
		   (QNInt32*) seglens);
}

void
QN_OutLabStream_ILab::write_header() {
    QN_ILab_header header;
    int i;

    header.version = QN_ILAB_VERSION;
    header.dataOffset = sizeof(header);
    if (segpos == NULL) {
	header.indexOffset = 0;	// No index for now
    } else {
	header.indexOffset = segpos[current_seg];
    }
    header.bitsPerLabel = QN_BITS_PER_BYTE * bytes_per_label;
    header.numSegments = current_seg;
    header.numFrames = total_frames;

    // Seek to start of file to write header.
    int ec = fseek(file, 0L, SEEK_SET);
    if (ec!=0) {
	clog.error("Failed to seek to start of ILabfile '%s' - %s.",
		   QN_FILE2NAME(file), strerror(errno));
    }
    // Convert to host order (stored bigendian)
    qn_htob_vi32_vi32(sizeof(header)/sizeof(QNInt32), 
		   (QNInt32*) &header, (QNInt32*) &header);
    // Magic is written directly
    char *s = (char *)&header.magic;
    for(i = 0; i < (int)sizeof(QNInt32); ++i) {
	*s++ = qn_ilab_magic[i];
    }
    ec = fwrite(&header, sizeof(header), 1, file);
    // Swap back
    qn_btoh_vi32_vi32(sizeof(header)/sizeof(QNInt32), 
		   (QNInt32*) &header, (QNInt32*) &header);
    if (ec != 1) {
	clog.error("Failed to write header in ILabfile '%s' - %s.",
		   QN_FILE2NAME(file), strerror(errno));
    }
    //return ec;
}

