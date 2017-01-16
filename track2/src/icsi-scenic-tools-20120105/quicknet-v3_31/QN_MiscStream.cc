// $Header: /u/drspeech/repos/quicknet2/QN_MiscStream.cc,v 1.13 2010/10/29 02:20:37 davidj Exp $

// QN_MiscStream.cc
// Functions for reading/writing various miscellaneous data file formats
// 2000-03-14 dpwe@icsi.berkeley.edu after QN_HTKstream.cc

// -*- C++ -*-

#include <QN_config.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "QN_types.h"
#include "QN_libc.h"
#include "QN_intvec.h"
#include "QN_fltvec.h"
#include "QN_MiscStream.h"
#include "QN_Logger.h"

////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_UNIIO - DaimlerChrysler UNIIO format input
////////////////////////////////////////////////////////////////

/* from tmp/daimlerchrysler/uni_io.c: */

/*---------------------------------------------------------------------------*/
/*   header information (non-VMS):                                           */
/*    0               4       8           20               84                */
/*  |---|---|---|---|--...--|---|...|---|---|---|...|---|-----------         */
/*  |    lTestID    |   0   | lReserve  |      cInfo    |file data           */
/*  |---|---|---|---|--...--|---|...|---|---|---|...|---|-----------         */
/*                                                                           */
/*                                                                           */
/*  lTestID:            test sequence to check files byte order              */
/*  lReserve:           reserved for later use                               */
/*  cInfo:              users file description                               */
/*---------------------------------------------------------------------------*/

/* .. then each frame of <lCount> features occurs as (floats):		     */
/*   |              0       3 4                                            | */
/*   |              ---------------------...-------                        | */
/*   |              | lCount |   R*4 array        |                        | */
/*   |              ---------------------...-------                        | */

/* - no mention of frame count in header
   - no way to have more than one utterance in a file
     EXCEPT by concatenating, and guessing that unexpected frame count as 
     header magic ID is next file...
 */

/* From a later email...

Date:    Mon, 5 Jun 2000 17:08:43 +0200
To:      <morris@idiap.ch>, <dpwe@ICSI.Berkeley.EDU>
From:    Joan Mari <joan.mari@daimlerchrysler.com>
Subject: The RESPITE cepstral/spectral interface

Hi Andrew and Dan,
    in the following e-mail, I'll give you a short description of the format in
which the feature vector must be stored for the cepstral/spectral interface.
Note that the UNI_IO software is needed, so if any of you don't have it, please
let me know it.

    For cepstral, J-RASTA, PLP feature vectors and the like

1st position Segmentation flag  ==>  You oughtn't write your own segmentation
flag there. Put just a dummy value in that position
  |
  |   2nd postion Frame normalised energy ==> In case your features use this
  |   |                  value, put it in that position, otherwise ignore it.
  |   |
  |   |   From the 3rd position on put  your feature vector.
  |   |   |<----          ------>
_ | _ | __|_____           ________
| - | - | - | - | - ............ - | - | - |


IMPORTANT 1:  WRITE IN THE OUPUT FILE JUST THE FRAMES THAT WILL BE USED
              DURING TRAINING AND RECOGNITION, that is, write just
              the feature vectors that contain speech.

IMPORTANT 2: IN CASE YOU WRITE MULTI-BAND FEATURES, THAT IS, CEPSTRAL, PLP
             OR RASTA FEATURES COMPUTED ON EACH SUB-BAND, WRITE THEM IN THE 
             SAME VECTOR, AND THEN JUST TELL ME HOW MANY COEFFICIENTS HAS 
             EACH SUB-BAND.
     For just spectral feature vectors:
     From the 1st position on put the spectral feature vector.
     |<-------               -------->
     |__________________________
     | - | - | - | - ................. - | - | - | - |

    The general procedure to input/output from/to those files is:
      // Open output/input feature file
      openfile(...)  // UNI_IO  function
      // Write/Read the length of the stored vector, that is, if the length
      // of your feature vector is 12, then you should write 12 + 2 = 14 in
      // the cepstral case, and just 12 in the cepstral case
      write_i4(...) / read_i4(...)    // UNI_IO functions 
      while (not end of file)
         // Write/read the present frame
	 write_r4_array(...) / read_r4_array(...)    // UNI_IO functions
      endwhile

I hope this will serve to start with the exchange of features, although
any suggestions you give will be welcomed. Concerning the way we should
exchange those files, I rather prefer ftp with password than a CDROM, but
that's up to you. By the end of the week I send you a document in which the
STATE LIKELIHOODS interface is explained.
     Best wishes
      JOAN
*/

/* From a still later email:

Date:    Mon, 28 Aug 2000 18:15:27 +0200
To:      <dpwe@ICSI.Berkeley.EDU>, <morris@idiap.ch>, <hagen@idiap.ch>
cc:      Fritz Class <fritz.class@daimlerchrysler.com>,
	 Udo Haiber <udo.haiber@daimlerchrysler.com>
From:    Joan Mari <joan.mari@daimlerchrysler.com>
Subject: CEPSTRAL interface and energy nomalization

[...]
I've noted that there are a few necessary little changes to be made to Dan's
"feacat" program, if we want to ease those exchanges. So Dan, I think this is
for you:
    the first suggested change concerns the positioning of the frame energy
value in the vector which is written in the UNIIO output file. Due to the fact
that our recognizer treats this value in a special way, it should be positioned
in the 2nd position of the written vector. From the 3rd position on should be
positioned the rest of the feature vector.
Note that I've made a distinction between the "feature vector" and the "written
vector", and that's  because of the fact that in the 1st position of the
"written vector" there's the "segmentation flag" for that frame. In that
position, your "feacat" program should write a "4" for the first frame in the
file and a "6" for the rest of the frames of the file.
So the converted file should look like:

       ____ _______ _______ _______       ________
       |4 |fr. energ. |c1 |c2 |  ................. |cN |
       ____ _______ _______ _______       ________
       |6 | | | | ................. ||
       ____ _______ _______ _______       ________
       |6 | | | | ................. ||
       ____ _______ _______ _______       ________
       |6 | | | | ................. ||
       ____ _______ _______ _______       ________
       |6 | | | | ................. ||
       :    :  :
       :    :  :
       :    :  :
       :    :  :

       I suggest you to introduce an on-line option in your feacat program, for
example an "-e" flag followed by the position of the "frame energy" in the
feature vector, ie a call to your modified program would look like:
       $ feacat -e 1 ......

 */

/* dpwe writes:
   So, the cepAnz 'flag' is actually a preview of the number of 
   cepstral components i.e. the feature width, although the actual 
   with of the records written to file should be n+2, since the 
   first two values of each frame are essentially dummies that 
   will be hidden by this layer. 
   2000-06-28: It turns out that cepAnz is simply a preview of the 
   vector sizes, so it too should be n_ftrs+2.
*/

QN_InFtrStream_UNIIO::QN_InFtrStream_UNIIO(int a_debug, 
			      const char* a_dbgname, FILE* a_file,
			      int a_indexed, int a_n_dummy) 
    : log(a_debug, "QN_InFtrStream_UNIIO", a_dbgname), 
      file(a_file), 
      current_seg(-1), 
      current_frame(0), 
      n_ftrs(QN_SIZET_BAD), 
      n_segs(QN_SIZET_BAD), 
      n_rows(QN_SIZET_BAD), 
      byteswap(0),
      cepAnz(0),
      n_dummy(a_n_dummy), 
      indexed(a_indexed), 
      segind(NULL),
      uniio_version(0)
{
    log.log(QN_LOG_PER_RUN, "Creating InFtrStream_UNIIO from file '%s'.",
	    QN_FILE2NAME(a_file));
    // Seems like these flags need to be cleared??
    header_peeked = 0;
    magic_peeked = 0;

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

    if (n_ftrs == QN_SIZET_BAD) {
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
}

QN_InFtrStream_UNIIO::~QN_InFtrStream_UNIIO() {
    if (segind) {
	delete [] segind;
	segind = NULL;
    }
}

int QN_InFtrStream_UNIIO::peek_magic(int state, QNUInt32* pval) {
    // Next four bytes in stream will tell us what's going on:
    //  - if it's a valid header, this is the start of a file/new segment
    //     so return 1
    //  - if it's the byteswap of a valid header, ditto, but set byteswap
    //  - if we're at EOF, it's all over - return 0
    //  - if it's the frame count, we have another frame, return 2
    //  - if framecount is unknown, and this looks plausible, 
    //    set the framecount and return 2
    //  - else it's nonsense; return -1
    int ec;
    QNUInt32 test;

    if (magic_peeked != 0) {
	// We've already read this; return whatever we said last time
	return magic_peeked;
    }

    ec = fread((char *)&test, sizeof(QNUInt32), 1, file);
    if (ec < 1) {
	// Hit EOF
	magic_peeked = 0;
	return 0;
    }

    // Maybe check for header?
    if (state & UNIO_ALLOW_HDR) {
	if (test == UNIO_TESTSEQUENCE || test == UNIO_TESTSEQUENCE2) {
	    byteswap = 0;
	    magic_peeked = 1;
	    goto return_peeked;
	}
	if (test == UNIO_REVTESTSEQUENCE || test == UNIO_REVTESTSEQUENCE2) {
	    byteswap = 1;
	    magic_peeked = 1;
	    goto return_peeked;
	}
    }
    // Fixup per byteswap
    if (byteswap) {
	qn_swapb_vi32_vi32(1, (QNInt32*)&test, (QNInt32*)&test);
    }

    if (test == UNIO_TESTSEQUENCE2) {
        uniio_version = 2;
    }

    // Maybe check for framecount?
    if (state & UNIO_ALLOW_COUNT) {
	if (test > 0 && n_ftrs == QN_SIZET_BAD) {
	    n_ftrs = test - n_dummy;
	    // Sanity checks
	    if (n_ftrs <= 0) {
		log.error("Ftrcount %d is too small in UNIIO file %s (n_dummy=%d).",
			  n_ftrs, QN_FILE2NAME(file), n_dummy);
	    } else if (n_ftrs > 1024) {
		log.error("Framewidth %d is too big in UNIIO file %s.",
			  n_ftrs+n_dummy, QN_FILE2NAME(file));
	    } else if (n_ftrs > 256) {
		log.warning("Suspicious framewidth %d in UNIIO file %s.",
			    n_ftrs+n_dummy, QN_FILE2NAME(file));
	    }
	}
	if (test == n_ftrs + n_dummy) {
	    magic_peeked = 2;	// flag that saw n_ftrs OK
	    goto return_peeked;
	} else {
	    log.error("Inconsistent ftrcount in UNIIO file %s - %d not %d.", 
		      QN_FILE2NAME(file), test, n_ftrs + n_dummy);
	}
    }
    // Nothing worked
    magic_peeked = -1;

 return_peeked:
    // Maybe pass back the magic number
    if (pval)	*pval = test;

    return magic_peeked;
}

int QN_InFtrStream_UNIIO::SkipRemainingExtensions(void)
{
    QNUInt32 lExt;
    int res, i;
    while (1) {
        res = fread(&lExt, sizeof(QNUInt32), 1, file);
        if (res != 1)  return -1;
	if (byteswap) {
	    qn_swapb_vi32_vi32(1, (QNInt32*)&lExt, (QNInt32*)&lExt);
	}
        if (lExt == 0) break;
	for (i = 0; i < lExt; ++i) {
	    fread(&lExt, sizeof(QNUInt32), 1, file);
	}
    }
    return 0;
}

int QN_InFtrStream_UNIIO::read_header(void) {
    // UNIIO header is 20 bytes + 64 char comment + opt extensions, 
    //    endianness adaptive
    // return is 1 on success, -1 at EOF
    QNUInt32 hdr[5];
    char info[UNIO_FILEHEADINFOLEN];
    int ec;

    if (header_peeked) {
	header_peeked = 0;
    } else {

	ec = peek_magic(UNIO_ALLOW_HDR, &hdr[0]);
	if (ec == 0) {
	    // EOF
	    return -1;
	}
	if (ec == -1) {
	    log.error("Magic # in UNIIO file %s is not recognized (0x%08lx).", 
			QN_FILE2NAME(file), hdr[0]);
	}

	// Clear the peeked flag, since we'll read the rest
	magic_peeked = 0;

	// Read remainder of header longs
	ec = fread((char *)&hdr[1], sizeof(QNUInt32), 4, file);
	if (ec < 4) {
	    return -1;	// flag that EOF reached
	}
	if (byteswap) {
	    qn_swapb_vi32_vi32(5, (QNInt32*)hdr, (QNInt32*)hdr);
	}

	ec = fread((char *)info, sizeof(char), UNIO_FILEHEADINFOLEN, file);
	if (ec < UNIO_FILEHEADINFOLEN) {
	    log.error("Short header in UNIIO file %s.", 
			QN_FILE2NAME(file));
	    return -1;
	}

	if ( uniio_version >= 2 ) {
	    if(SkipRemainingExtensions()) {
		log.error("Problem skipping header extensions in UNIIO file %s.", 
			  QN_FILE2NAME(file));
		return -1;
	    }
	}

	// First data element in a DC UNIIO file is the cepAnz flag
	ec = fread((char *)&cepAnz, sizeof(QNUInt32), 1, file);
	if (byteswap) {
	    qn_swapb_vi32_vi32(1, (QNInt32*)&cepAnz, (QNInt32*)&cepAnz);
	}
	// It seems that it should be the number of cepstra
	if (cepAnz > 0) {
	    n_ftrs = cepAnz - n_dummy;
	}

	// Now read again to see what we have next
	if (peek_magic() != 2) {
	    log.error("Error peeking magic in UNIIO file %s.", 
			QN_FILE2NAME(file));
	}

    }

    // We got a header, so OK to advance seg
    ++current_seg;
    current_frame = 0;

    return 1;		// successful read
}

size_t QN_InFtrStream_UNIIO::read_ftrs(size_t count, float* ftrs) {
    int ec;
    size_t got = 0;
    
    if (current_seg < 0) {
	// Haven't performed our first next_seg()
	log.error("read_ftrs called before next_seg on file %s.", 
		  QN_FILE2NAME(file));
    }

    // We're going to read the dummy values into the vector, so 
    // just confirm that they won't overflow
    assert(n_ftrs >= n_dummy);

    while (got < count) {

	// Check that element count is OK, not looking at next start
	ec = peek_magic();
	if (ec == 0 || ec == 1) {
	    // hit eof (0) or saw start of next segment (1)
	    goto at_EOF;
	}
	if (ec < 0) {
	    // unrecognized
	    log.error("Error reading header of seg:frame %d:%d in UNIO file %s.", 
		      current_seg, current_frame, QN_FILE2NAME(file));
	}
	// Clear the peeked flag, since we're actually reading it now
	magic_peeked = 0;

	// read the dummy values to discard them (overflow checked above)
	if (n_dummy > 0) {
	    ec = fread((char *)ftrs, n_dummy*sizeof(float), 1, file);
	}

	if (( ec = fread((char *)ftrs, n_ftrs*sizeof(float), 1, file)) < 1) {
	    log.error("EOF reading seg:frame %d:%d of UNIO file %s.", 
		      current_seg, current_frame, QN_FILE2NAME(file));
	}
	if (byteswap)
	    qn_swapb_vi32_vi32(n_ftrs, (QNInt32*)ftrs, (QNInt32*)ftrs);
	ftrs += n_ftrs;
	++current_frame;
	++got;
    }

 at_EOF:
    return got;
}

int QN_InFtrStream_UNIIO::rewind(void) {
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

size_t QN_InFtrStream_UNIIO::read_to_eos() {
    // skip forward to the end of this frame by reading
#define BUFFRAMES 64
    float *tmpbuf = new float [BUFFRAMES*n_ftrs];
    size_t frames_this_seg = 0, got;

    while( (got = read_ftrs(BUFFRAMES, tmpbuf)) > 0) {
	frames_this_seg += got;
    }

    delete [] tmpbuf;

    return frames_this_seg;
}

QN_SegID QN_InFtrStream_UNIIO::nextseg() {
    QN_SegID rc = QN_SEGID_BAD;	// default return indicates EOF

    // Scan forward to end-of-segment
    if (current_seg != -1) {
	read_to_eos();
    }

    // Read the next header (else EOF)
    if(read_header() != -1) {
	// read_header also increments the current_seg
	rc = QN_SEGID_UNKNOWN;
    }
    
    return rc;
}

size_t QN_InFtrStream_UNIIO::num_frames(size_t segno /* = QN_ALL */) {
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

int QN_InFtrStream_UNIIO::get_pos(size_t* segnop, size_t* framenop) {
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

QN_SegID QN_InFtrStream_UNIIO::set_pos(size_t segno, size_t frameno) {
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
	size_t frames_this_seg = segind[segno+1] - this_seg_row;
	if (frameno > frames_this_seg)	{
	    log.error("Seek beyond end of segment %li.",
		     (unsigned long) segno);
	}

	int frame_bytes = (n_ftrs+1)*sizeof(float);

	if (uniio_version >= 2) {
	    log.warning("Seeking on UNIIO version2 files is just a guess");
	}

	// basic header, plus optional 4 byte null extension, plus cepAnz
	int header_bytes = UNIO_HDR_BYTES + ((uniio_version>=2)?4:0) + 4;

	offset = frame_bytes * row + (segno+1)*header_bytes;
	
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
QN_InFtrStream_UNIIO::build_index()
{
    size_t frame = 0;	// Current frame number.

    // Assume we're rewound
    assert(current_seg == -1);

    // .. but have our own count
    size_t seg = 0;
    segind[seg] = (QNUInt32) frame;

    while(read_header() != -1) {
	// Read through the segment
	size_t frames_this_seg = read_to_eos();

	// Record the segment length
	frame += frames_this_seg;
	++seg;

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
// QN_OutFtrStream_UNIIO - output stream for UNIIO format ftrs
////////////////////////////////////////////////////////////////


QN_OutFtrStream_UNIIO::QN_OutFtrStream_UNIIO(int a_debug, 
					     const char* a_dbgname,
					     FILE* a_file, 
					     size_t a_n_ftrs, 
					     size_t a_n_dummy) 
    : clog(a_debug, "QN_OutFtrStream_UNIIO", a_dbgname), 
      file(a_file), 
      n_ftrs(a_n_ftrs), 
      current_seg(0), 
      bos(1),
      byteswap(0),
      cepAnz(0),
      n_dummy(a_n_dummy), 
      uniio_version(0)
{
    assert(n_ftrs!=0);
    clog.log(QN_LOG_PER_RUN, "Creating UNIIO file '%s'.",  QN_FILE2NAME(file) );
    // Seemingly, cepAnz should be the number of features... ???
    cepAnz = n_ftrs + n_dummy;
}

QN_OutFtrStream_UNIIO::~QN_OutFtrStream_UNIIO() {
    if (!bos) {
	clog.error("tried to close file without finishing segment.");
    }
    clog.log(QN_LOG_PER_RUN, "Finished creating UNIIO file '%s'.", 
	     QN_FILE2NAME(file));
}

void QN_OutFtrStream_UNIIO::write_ftrs(size_t cnt, const float* ftrs) {
    // Dummy flag is 6 unless we are at BOS, when it is 4
    int dummyval = UNIO_DUMMY_NORMAL;
    // write a header if we are at the beginning of a segment    
    if (bos) {
	QNUInt32 hdr[5];

	dummyval = UNIO_DUMMY_BOS;

	if (uniio_version >= 2) {
	    hdr[0] = UNIO_TESTSEQUENCE2;
	} else {
	    hdr[0] = UNIO_TESTSEQUENCE;
	}
	hdr[1] = 0;	hdr[2] = 0;
	hdr[3] = 0;	hdr[4] = 0;
	if (byteswap) {
	    qn_swapb_vi32_vi32(5, (QNInt32*)hdr, (QNInt32*)hdr);
	}
	fwrite((char *)hdr, sizeof(QNUInt32), 5, file);

	// Write the info string
	char key_word[UNIO_FILEHEADINFOLEN];
	const char *info_str   = "  UNIIO File cep";
	strncpy(key_word, info_str, UNIO_FILEHEADINFOLEN);
	fwrite((char *)key_word, sizeof(char), UNIO_FILEHEADINFOLEN, file);

	// Write null extensions (for version >= 2)
	if (uniio_version >= 2) {
	    hdr[0] = 0;
	    // no need to byteswap 0...
	    fwrite((char *)hdr, sizeof(QNUInt32), 1, file);
	}

	// Write the DC flag
	hdr[0] = cepAnz;
	if (byteswap) {
	    qn_swapb_vi32_vi32(1, (QNInt32*)hdr, (QNInt32*)hdr);
	}
	fwrite((char *)hdr, sizeof(QNUInt32), 1, file);

	bos = 0;
    }
    // write frame hdr (len)
    QNUInt32 mylen = n_ftrs + n_dummy;
    if (byteswap) qn_swapb_vi32_vi32(1, (QNInt32*)&mylen, (QNInt32*)&mylen);
    // Can't byteswap in place because we were passed a pointer to const, 
    // so make a tmp buffer
    size_t done = 0;
    const float *myftrs = ftrs;
    float *ftrbuf = new float[n_ftrs + n_dummy];
    int i;
    // first dummy field (if any) takes on dummy val - integer, as a float
    if (n_dummy > 0)	*ftrbuf = (float)dummyval;
    // preset any further dummy fields to zero
    for (i = 1; i < n_dummy; ++i) ftrbuf[i] = 0.0;
    // maybe we should byteswap them?
    if (byteswap)
	qn_swapb_vi32_vi32(n_dummy, (const QNInt32*)ftrbuf, (QNInt32*)ftrbuf);
    // Write out all the frames
    while(done < cnt) {
	// byteswapped vector length
	fwrite((char *)&mylen, sizeof(QNUInt32), 1, file);
	// set up one frame worth of data to write
	if (byteswap) {
	    qn_swapb_vi32_vi32(n_ftrs, (const QNInt32*)myftrs, (QNInt32*)(ftrbuf+n_dummy));
	} else {
	    memcpy((void*)(ftrbuf+n_dummy), (const void*)myftrs, n_ftrs*sizeof(float));
	}
	fwrite((char *)ftrbuf, (n_ftrs+n_dummy)*sizeof(float), 1, file);
	myftrs += n_ftrs;
	++done;

	// Reset the dummyflag if it's at BOS val
	if (dummyval == UNIO_DUMMY_BOS) {
	    dummyval = UNIO_DUMMY_NORMAL;
	    if (n_dummy > 0) {
		*ftrbuf = (float)dummyval;
		if (byteswap)
		    qn_swapb_vi32_vi32(1, (const QNInt32*)ftrbuf, (QNInt32*)ftrbuf);
	    }
	}
    }
    delete [] ftrbuf;

    frames_this_seg += cnt;
}

void QN_OutFtrStream_UNIIO::doneseg(QN_SegID segid) {
    // Nothing more to write - just reset flags

    // next write will need a new header
    bos = 1;
    ++current_seg;
    frames_this_seg = 0;

    clog.log(QN_LOG_PER_SENT, "Finished segment.");
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
// QN_InFtrLabStream_raw - raw format input
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////


QN_InFtrStream_raw::QN_InFtrStream_raw(int a_debug, 
		       const char* a_dbgname, FILE* a_file,
		       int a_format, int a_byteorder, 
		       size_t a_width, int a_indexed)
    : log(a_debug, "QN_InFtrStream_raw", a_dbgname), 
      file(a_file), 
      current_seg(-1), 
      current_frame(0), 
      format(a_format),
      byteorder(a_byteorder),
      n_ftrs(QN_SIZET_BAD), 
      n_segs(QN_SIZET_BAD), 
      n_rows(QN_SIZET_BAD), 
      frame_bytes(QN_SIZET_BAD), 
      indexed(a_indexed)
{
    log.log(QN_LOG_PER_RUN, "Creating InFtrStream_raw from file '%s'.",
	    QN_FILE2NAME(a_file));
    // If required, build an index of the file.
    if (a_indexed) {
	// At the moment, raw files can't be read as indexed
	log.error("You cannot access raw file %s in indexed mode.",
		   QN_FILE2NAME(file));
    }
    // We assume samples are floats, but check for a type we know about
    if (! (format == QNRAW_FLOAT32 && (byteorder == QNRAW_BIGENDIAN 
				       || byteorder == QNRAW_LITTLEENDIAN ))) {
	log.error("Format/byteorder in raw file %s is unknown (%d/%d).", 
		    QN_FILE2NAME(file), format, byteorder);
    }
    // features per frame has to be specified
    n_ftrs = a_width;
    int bytes_per_sample = sizeof(float);
    frame_bytes = bytes_per_sample*n_ftrs;
    frame_buf = new float[n_ftrs];
    // at end of seg -1
    eos = 1;
}

QN_InFtrStream_raw::~QN_InFtrStream_raw() {
    delete [] frame_buf;
}

void
QN_InFtrStream_raw::next_frame()
{
    int ec;			// Return code
    
    ec = fread((char *) frame_buf, n_ftrs*sizeof(float), 1, file);
    if (ec==0) {
	if (ferror(file)) {
	    log.error("Error reading frame from file '%s' - %s.",
		      QN_FILE2NAME(file), strerror(errno));
	} else {
	    // else plain end-of-segment
	    eos = 1;
	}
    } else {
	current_frame++;
    }
}

size_t QN_InFtrStream_raw::read_ftrs(size_t count, float* ftrs) {

    if (current_seg < 0) {
	// Haven't performed our first next_seg()
	log.error("read_ftrs called before next_seg on file %s.", 
		  QN_FILE2NAME(file));
    }

    int frame = 0;
    while(frame < count && !eos) {
	if (byteorder == QNRAW_BIGENDIAN) {
	    qn_btoh_vf_vf(n_ftrs, frame_buf, ftrs);
	} else {
	    /* ltoh_vf_vf(n_ftrs, ftrs, ftrs); */
	    qn_ltoh_vi32_vi32(n_ftrs, (QNInt32*)frame_buf, (QNInt32*)ftrs);
	}
	frame++;
	next_frame();
    }
    current_frame += frame;
    return frame;
}

int QN_InFtrStream_raw::rewind(void) {
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
	eos = 1;
	log.log(QN_LOG_PER_EPOCH, "File %s rewound.", 
		QN_FILE2NAME(file));
	ret = QN_OK;
    }
    return ret;    
}

QN_SegID QN_InFtrStream_raw::nextseg() {
    QN_SegID rc = QN_SEGID_BAD;	// default return indicates EOF

    // Only valid time to call is at the beginning
    if (current_seg < 0) {
	current_seg = 0;
	eos = 0;
	current_frame = 0;
	rc = QN_SEGID_UNKNOWN;
	next_frame();
    }

    return rc;
}

size_t QN_InFtrStream_raw::num_frames(size_t segno /* = QN_ALL */) {
    size_t ret = QN_SIZET_BAD;			// Return value.

    if (!indexed) {		// If not indexed, do not know no. of frames.
	ret = QN_SIZET_BAD;
    } else {
	// it can't be indexed
	log.error("Raw stream is indexed, which is impossible");
    }

    return ret;
}

int QN_InFtrStream_raw::get_pos(size_t* segnop, size_t* framenop) {
    int ret = QN_BAD;		// Return value

    if (!indexed) {
	ret = QN_BAD;
    } else {
	log.error("Raw stream is indexed, which is impossible");
    }
    return ret;
}

QN_SegID QN_InFtrStream_raw::set_pos(size_t segno, size_t frameno) {
    int ret = QN_SEGID_BAD;			// Return value

    assert(segno < n_segs); // Check we do not seek past end of file
    if (!indexed) {
	ret = QN_SEGID_BAD;
    } else {
	log.error("Raw stream is indexed, which is impossible");
    }
    return ret;
}

////////////////////////////////////////////////////////////////
// QN_OutFtrStream_raw - write a raw binary file
////////////////////////////////////////////////////////////////

QN_OutFtrStream_raw::QN_OutFtrStream_raw(int a_debug, 
					 const char* a_dbgname,
					 FILE* a_file, 
					 int a_format, 
					 int a_byteorder, 
					 size_t a_num_ftrs, 
					 int a_indexed) 
    : log(a_debug, "QN_OutFtrStream_raw", a_dbgname), 
      file(a_file), 
      indexed(a_indexed), 
      format(a_format),
      byteorder(a_byteorder),
      num_ftr_cols(a_num_ftrs), 
      current_seg(0), 
      current_frame(0)
{
    log.log(QN_LOG_PER_RUN, "Started writing raw file '%s'.", 
	    QN_FILE2NAME(file));
    if (indexed) {
	log.error("Cannot use raw streams ('%s') in indexed mode.", 
	    QN_FILE2NAME(file));
    }
    // We assume samples are floats, but check for a type we know about
    if (! (format == QNRAW_FLOAT32 && (byteorder == QNRAW_BIGENDIAN 
				       || byteorder == QNRAW_LITTLEENDIAN ))) {
	log.error("Format/byteorder for raw file %s is unknown (%d/%d).", 
		    QN_FILE2NAME(file), format, byteorder);
    }
    frame_buf = new float[num_ftr_cols];
}

QN_OutFtrStream_raw::~QN_OutFtrStream_raw() {
    delete[] frame_buf;
}

void QN_OutFtrStream_raw::write_ftrs(size_t cnt, const float* ftrs) {
    // Write some frames. ftrs can be null, meaning fill with zeros
    int ec;

    while (cnt-- > 0) {
	// Write features
	if (byteorder == QNRAW_BIGENDIAN) {
	    qn_htob_vf_vf(num_ftr_cols, ftrs, frame_buf);
	} else {
	    /* htol_vf_vf(num_ftr_cols, ftrs, frame_buf); */
	    qn_htol_vi32_vi32(num_ftr_cols, (QNInt32*)ftrs,
			      (QNInt32*)frame_buf);
	}
	ec = fwrite((char *) frame_buf, num_ftr_cols*sizeof(float), 1, file);
	if (ec==0) {
	    log.error("Error writing to raw file '%s' - %s.",
		      QN_FILE2NAME(file), strerror(errno));
	} else {
	    current_frame++;
	}
    }
}

void QN_OutFtrStream_raw::doneseg(QN_SegID /* segid */) {
    ++current_seg;
    current_frame = 0;
}

