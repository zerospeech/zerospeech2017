// $Header: /u/drspeech/repos/quicknet2/QN_HTKstream.cc,v 1.11 2004/04/08 02:57:40 davidj Exp $

// QN_HTKstream.cc
// Functions for reading/writing HTK-format data files
// 1999jun03 dpwe@icsi.berkeley.edu

// -*- C++ -*-

// HTK format files have a 12-byte header:
//  typedef struct {              /* HTK File Header */
//    long nSamples;
//    long sampPeriod;	/* in 100ns units */
//    short sampSize;
//    short sampKind;
//  } HTKhdr;
// and each file contains just one utterance.  However, since 
// FtrStreams are supposed to hold any number of utterances, the 
// files read and writen by these classes are actually a slight 
// generalization, in that any number of HTK files can be 
// concatenated together to form a single ftr archive (this 
// works because the header specifies the length, so the start 
// of the next file can be found).  Of course, an archive containing 
// just one file is a normal HTK feature file.

// Another class that would be interesting to make would be 
// an HTKList, which used HTK-style filename-list-files to 
// define an 'archive' of features as a collection of individual 
// per-utterance files.  This could almost be done in the 
// standard stream format for input: the open file handle 
// passed to the constructor would be the file list, and the 
// class would open the individual feature files internally. 
// However, the reason we pass these classes file handles is 
// (I assume) because we mess with the buffering etc. when 
// running on the SPERT, something we don't want to have to 
// put inside every class.  There's no big concern about the 
// buffering of the HTK file list file, so it would make 
// more sense to have the constructors take the file name of 
// the list file (this would also work for output), and 
// then, I guess, pass an optional function pointer for a 
// replacement to fopen() that the application can set up 
// to do whatever it needs on each file.


#include <QN_config.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"
#include "QN_HTKstream.h"
#include "QN_Logger.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_HTK - HTK format input
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


QN_InFtrStream_HTK::QN_InFtrStream_HTK(int a_debug, 
			      const char* a_dbgname, FILE* a_file,
			      int a_indexed) 
    : log(a_debug, "QN_InFtrStream_HTK", a_dbgname), 
      file(a_file), 
      current_seg(-1), 
      current_frame(0), 
      n_ftrs(QN_SIZET_BAD), 
      n_segs(QN_SIZET_BAD), 
      n_rows(QN_SIZET_BAD), 
      frame_bytes(QN_SIZET_BAD), 
      type_code(-1), 
      period(-1), 
      indexed(a_indexed), 
      segind(NULL),
      compressed(0),
      compScales(NULL),
      compBiases(NULL)
{
    log.log(QN_LOG_PER_RUN, "Creating InFtrStream_HTK from file '%s'.",
	    QN_FILE2NAME(a_file));
    // If required, build an index of the file.
    if (a_indexed) {
	// Allocate and build buffer
	segind_len = DEFAULT_INDEX_LEN;
	segind = new QNUInt32[segind_len];
	build_index();
	log.log(QN_LOG_PER_RUN, "Indexed stream, n_segs=%lu n_rows=%lu.",
		n_segs, n_rows);
	if (rewind()==QN_BAD) {
	    log.error("Failed to seek to start of %s after "
		     " building index.", QN_FILE2NAME(file));
	}
    }
    if (frame_bytes == QN_SIZET_BAD) {
	// Need to peek at the first header to get the number of features
	if (read_header() == -1) {
	    log.error("Failed to read first header of %s.", 
		      QN_FILE2NAME(file));
	}
	// Set a flag that we've pre-read the header, so first 
	// next_seg is a NOP
	header_peeked = 1;
	// set the current_seg back to what it should be
	--current_seg;
    }
    // We assume samples are floats, but check for a type we know about
    int base = (type_code & HTK_BASE_MASK);
    if (! (base == HTK_BASE_LPC \
	   || base == HTK_BASE_LPREFC \
	   || base == HTK_BASE_LPCEPSTRA \
	   || base == HTK_BASE_LPDELCEP \
	   || base == HTK_BASE_MFCC \
	   || base == HTK_BASE_FBANK \
	   || base == HTK_BASE_MELSPEC \
	   || base == HTK_BASE_USER) ) {
	log.warning("Sample format in HTK file %s is unknown (%d).", 
		    QN_FILE2NAME(file), base);
    }
    int bytes_per_sample = compressed?sizeof(short):sizeof(float);
    n_ftrs = frame_bytes/bytes_per_sample;
}

int QN_InFtrStream_HTK::read_header(void) {
    // HTK header is 12 bytes, big-endian
    // return is number of frames in this segment, or -1 for EOF
    QNUInt32 hdr[3];
    int ec;
    int frames, pd, bytes_per_frame, type;

    ec = fread((char *)hdr, sizeof(QNUInt32), 3, file);
    if (ec < 3) {
	return -1;	// flag that EOF reached
    }
    qn_btoh_vi32_vi32(3, (QNInt32*)hdr, (QNInt32*)hdr);
    frames = hdr[0];
    pd = hdr[1];
    bytes_per_frame = ( 0xFFFF0000 & hdr[2] ) >> 16;
    type  = ( 0x0000FFFF & hdr[2] );

    // We got a header, so OK to advance seg
    ++current_seg;
    current_frame = 0;

    // Sanity checks compared to class slots
    if (frame_bytes == QN_SIZET_BAD) {
	frame_bytes = bytes_per_frame;
    } else if ((int)frame_bytes !=bytes_per_frame) {
	log.error("Frame bytes switched from %d to %d at seg %d in HTK "
		  "file %s.", frame_bytes, bytes_per_frame, 
		  current_seg, QN_FILE2NAME(file));
    }
    if (type_code == -1) {
	type_code = type;
    } else if (type_code != type) {
	log.error("Sample type switched from %d to %d at seg %d in HTK "
		  "file %s.", type_code, type, 
		  current_seg, QN_FILE2NAME(file));
    }
    if (period == -1) {
	period = pd;
    } else if (period != pd) {
	log.error("Period switched from %d to %d at seg %d in HTK "
		  "file %s.", period, pd, 
		  current_seg, QN_FILE2NAME(file));
    }

    // Store the number of frames in this seg
    frames_this_seg = frames;

    // If the data is compressed, set the flag and read the constants
    compressed = 0;
    if (type_code & HTK_QUAL_C) {
	compressed = 1;
	// Compression means that the number of frames is overestimated 
	// by 4 ?????? to compensate for the compression factor table?
	frames_this_seg -= 4;
	//
	int n_ftrs = bytes_per_frame / sizeof(short);
	// Maybe allocate the vectors
	if (compScales == NULL) {
	    compScales = new float[n_ftrs];
	    compBiases = new float[n_ftrs];
	}
	// Read scales
	ec = fread((char *)compScales, sizeof(float), n_ftrs, file);
	if (ec < n_ftrs) {
	    log.error("HTK header ended while reading compression scales "
		      "in seg %d of file %s.", 
		      current_seg, QN_FILE2NAME(file));
	}
	qn_btoh_vf_vf(n_ftrs, compScales, compScales);
	// Invert
	qn_div_fvf_vf(n_ftrs, 1.0, compScales, compScales);
	// Read biases
	ec = fread((char *)compBiases, sizeof(float), n_ftrs, file);
	if (ec < n_ftrs) {
	    log.error("HTK header ended while reading compression biases "
		      "in seg %d of file %s.", 
		      current_seg, QN_FILE2NAME(file));
	}
	qn_btoh_vf_vf(n_ftrs, compBiases, compBiases);
    }

    return 1;		// successful read
}

QN_InFtrStream_HTK::~QN_InFtrStream_HTK() {
    if (segind) {
	delete [] segind;
	segind = NULL;
    }
    if (compScales) {
	delete [] compScales;
	delete [] compBiases;
    }
}

size_t QN_InFtrStream_HTK::read_ftrs(size_t count, float* ftrs) {
    if (current_seg < 0 || header_peeked == 1) {
	// Haven't performed our first next_seg()
	log.error("read_ftrs called before next_seg on file %s.", 
		  QN_FILE2NAME(file));
    }
    if (count > frames_this_seg - current_frame) {
	count = frames_this_seg - current_frame;
    }
    if (compressed) {
	if ( fread((char *)ftrs, n_ftrs*sizeof(short), count, file) < count) {
	    log.error("EOF reading %d frms at frame %d in seg %d of file %s.", 
		      count, current_frame, current_seg, QN_FILE2NAME(file));
	}
	// 16 bit words still need byteswapping!!! (2000-05-31)
	qn_btoh_vi16_vi16(n_ftrs*count, (short *)ftrs, (short *)ftrs);
	// Undo compression
	size_t i,j;
	int d;
	// Start at end, since shorts take up less space than floats
	short *ps = (((short *)ftrs)+count*n_ftrs);
	float *pf = ftrs+count*n_ftrs;
	for (i = 0; i < count; ++i) {
	    for (j = 0; j < n_ftrs; ++j) {
		d = *--ps;
		*--pf = (((float)d)+compBiases[n_ftrs - 1 - j])
		              * compScales[n_ftrs - 1 - j];
	    }
	}
    } else {
	if ( fread((char *)ftrs, n_ftrs*sizeof(float), count, file) < count) {
	    log.error("EOF reading %d frms at frame %d in seg %d of file %s.", 
		      count, current_frame, current_seg, QN_FILE2NAME(file));
	}
	qn_btoh_vf_vf(n_ftrs*count, ftrs, ftrs);
    }
    current_frame += count;
    return count;
}

int QN_InFtrStream_HTK::rewind(void) {
    int ec;
    int ret;			// Return code

    ec = fseek(file, 0L, SEEK_SET);
    // Sometimes it is valed for rewind to fail (e.g. on pipes),
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

QN_SegID QN_InFtrStream_HTK::nextseg() {
    QN_SegID rc = QN_SEGID_BAD;	// default return indicates EOF

    // Special case: if we've peeked this segment, don't read it again
    if (header_peeked) {

	header_peeked = 0;	// OK, we've trapped the peek now.
	++current_seg;
	rc = QN_SEGID_UNKNOWN;

    } else {

	// Scan forward to end-of-segment
	if (current_frame < (long)frames_this_seg) {
	    float *tmp = new float [n_ftrs];
	    while (current_frame < (long)frames_this_seg) {
		read_ftrs(1, tmp);
	    }
	    delete [] tmp;
	}

	// Read the next header (else EOF)
	if(read_header() != -1) {
	    // read_header also increments the current_seg
	    rc = QN_SEGID_UNKNOWN;
	}
    }
    
    return rc;
}

size_t QN_InFtrStream_HTK::num_frames(size_t segno /* = QN_ALL */) {
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

int QN_InFtrStream_HTK::get_pos(size_t* segnop, size_t* framenop) {
    int ret;			// Return value
    size_t segno;		// The segno value returned.
    size_t frameno;		// The frameno value returned.

    if (!indexed) {
	ret = QN_BAD;
    } else {
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
	ret = QN_OK;
    }
    return ret;
}

QN_SegID QN_InFtrStream_HTK::set_pos(size_t segno, size_t frameno) {
    int ret;			// Return value

    assert(segno < n_segs); // Check we do not seek past end of file
    if (!indexed) {
	ret = QN_SEGID_BAD;
    } else {
	long this_seg_row;	// The number of the row at the start of seg.
	long row;		// The number of the row we require
	long offset;		// The position as a file offset

	this_seg_row = segind[segno];
	row = segind[segno] + frameno;
	frames_this_seg = segind[segno+1] - this_seg_row;
	if (frameno > frames_this_seg)	{
	    log.error("Seek beyond end of segment %li.",
		     (unsigned long) segno);
	}

	int header_size = 3 * sizeof(QNUInt32) 
	                     + ( compressed ? (n_ftrs*2*sizeof(float)) : 0 ) ;
	offset = frame_bytes * row + (segno+1)*header_size;
	
	if (fseek(file, offset, SEEK_SET)!=0) {
	    log.error("Seek failed in file "
		     "'%s', offset=%li - file problem?",
		     QN_FILE2NAME(file), offset);
	}

	current_frame = frameno;
	current_seg = segno;
	log.log(QN_LOG_PER_SENT, "Seek to segment %lu, frame %lu.",
		(unsigned long) segno, (unsigned long) frameno);
	ret = QN_SEGID_UNKNOWN;		// FIXME - should return sentence ID.
    }
    return ret;
}


void
QN_InFtrStream_HTK::build_index()
{
    size_t frame = 0;	// Current frame number.

    // Assume we're rewound
    assert(current_seg == -1);

    // .. but have our own count
    size_t seg = 0;
    segind[seg] = (QNUInt32) frame;

    while(read_header() != -1) {
	// Record the segment length
	frame += frames_this_seg;
	++seg;
	// Seek over body of utt
	// (don't forget to skip CRC bytes if they're there)
	int ec = fseek(file, frame_bytes * frames_this_seg 
		               + ((type_code & HTK_QUAL_K)?sizeof(short):0), 
		       SEEK_CUR);
	if (ec != 0) {
	    log.error("Seek failed building index to file "
		     "'%s', seg=%d offset=%li - file problem?",
		     QN_FILE2NAME(file), seg, frame_bytes*frame);
	}
	// Here we increase the space allocated for the index if necessary
	if (seg>=segind_len) {
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
	segind[seg] = (QNUInt32) frame;
    }
    n_rows = frame;
    n_segs = seg;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_OutFtrStream_HTK - output stream for HTK format ftrs
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


QN_OutFtrStream_HTK::QN_OutFtrStream_HTK(int a_debug, 
					 const char* a_dbgname,
					 FILE* a_file, 
					 size_t a_n_ftrs, 
					 int a_type, 
					 int a_period) 
    : clog(a_debug, "QN_OutFtrStream_HTK", a_dbgname), 
      file(a_file), 
      n_ftrs(a_n_ftrs), 
      type_code(a_type), 
      period(a_period), 
      current_seg(0), 
      frames_this_seg(0), 
      bytes_per_frame(sizeof(float)*n_ftrs), 
      bos(1),
      header_pos(-1)
{
    assert(n_ftrs!=0);
    clog.log(QN_LOG_PER_RUN, "Creating HTK file '%s'.",  QN_FILE2NAME(file) );
}

QN_OutFtrStream_HTK::~QN_OutFtrStream_HTK() {
    if (!bos) {
	clog.error("tried to close file without finishing segment.");
    }
    clog.log(QN_LOG_PER_RUN, "Finished creating HTK file '%s'.", 
	     QN_FILE2NAME(file));
}

void QN_OutFtrStream_HTK::write_ftrs(size_t cnt, const float* ftrs) {
    // write a header if we are at the beginning of a segment
    if (bos) {
	QNUInt32 hdr[3];

	//hdr[0] = frames_this_seg;	// will be zero at this point
	hdr[0] = 0xFFFFFFFF;	// so we can see it hasn't been rewritten
	hdr[1] = period;
	hdr[2] = (bytes_per_frame << 16) | (0x0000FFFF & type_code);
	qn_htob_vi32_vi32(3, (QNInt32*)hdr, (QNInt32*)hdr);
	// Save where the header was written, for rewrite
	header_pos = ftell(file);
	fwrite((char *)hdr, sizeof(QNUInt32), 3, file);
	bos = 0;
    }
    // write data
    // Byte-swap it in place, even though ftrs was passed as 
    // a pointer to const floats
    qn_htob_vf_vf(n_ftrs, ftrs, (float*)ftrs);
    fwrite((char *)ftrs, bytes_per_frame, cnt, file);
    qn_btoh_vf_vf(n_ftrs, ftrs, (float*)ftrs);

    frames_this_seg += cnt;
}

void QN_OutFtrStream_HTK::doneseg(QN_SegID segid) {
    // rewrite the number of frames in this segment with the final value
    if (bos) {
	clog.error("Doneseg called before any frames written in seg %d "
		   "of file %s.", current_seg, QN_FILE2NAME(file));
    }

    if (fseek(file, header_pos, SEEK_SET)!=0) {
	clog.error("Seek for header rewrite failed in file "
		   "'%s', offset=%li - file problem?",
		   QN_FILE2NAME(file), header_pos);
    }
    QNUInt32 nframes = frames_this_seg;
    qn_htob_vi32_vi32(1, (QNInt32*)&nframes, (QNInt32*)&nframes);
    if (fwrite((char *)&nframes, sizeof(QNUInt32), 1, file) != 1) {
	clog.error("Header rewrite failed for segment %d of file "
		   "%s.", current_seg, QN_FILE2NAME(file));
    }    

    // I'm getting bugs where the rewritten size isn't getting 
    // written out to disk (1999dec03)
    //        fprintf(stderr, "HTK::doneseg: rewrote framelen %d at pos %d\n", 
    //    	    frames_this_seg, ftell(file));
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


