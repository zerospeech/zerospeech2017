const char* QN_filters_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_filters.cc,v 1.28 2004/09/08 01:36:28 davidj Exp $";

// This file contains various input stream filters.

#include <QN_config.h>
#include <stdlib.h>
#include "QN_filters.h"
#include "QN_fltvec.h"

////////////////////////////////////////////////////////////////

QN_InFtrStream_ProtoFilter::QN_InFtrStream_ProtoFilter(int a_debug,
						       const char* a_classname,
						       const char* a_dbgname,
						       QN_InFtrStream& a_str)
    : log(a_debug, a_classname, a_dbgname),
      str(a_str)
{
}

QN_InFtrStream_ProtoFilter::~QN_InFtrStream_ProtoFilter()
{
}


////////////////////////////////////////////////////////////////

QN_InFtrStream_Narrow::QN_InFtrStream_Narrow(int a_debug,
					     const char* a_dbgname,
					     QN_InFtrStream& a_str,
					     size_t a_first_ftr,
					     size_t a_num_ftrs,
					     size_t a_buf_frames)
    : QN_InFtrStream_ProtoFilter(a_debug, "QN_InFtrStream_Narrow",
				 a_dbgname, a_str),
      input_numftrs(a_str.num_ftrs()),
      first_ftr(a_first_ftr),
      output_numftrs((a_num_ftrs==QN_ALL)
		     ? (input_numftrs - first_ftr) : a_num_ftrs),
      buf_frames(a_buf_frames),
      buffer(new float [a_buf_frames * input_numftrs])
{
    // Check that we have enough features
    assert((a_first_ftr+output_numftrs)<=input_numftrs);
    assert(buf_frames>0);
    log.log(QN_LOG_PER_RUN,
	    "Created narrowed stream using features %lu to %lu"
	    " of %lu input features.",
	    (unsigned long) first_ftr,
	    (unsigned long) (first_ftr+output_numftrs-1),
	    input_numftrs);
}

QN_InFtrStream_Narrow::~QN_InFtrStream_Narrow()
{
    delete [] buffer;
}

size_t
QN_InFtrStream_Narrow::read_ftrs(size_t a_frames, float* ftrs)
{
    size_t frames_left = a_frames; // Frames left to read.
    const size_t buffer_stride = input_numftrs; // Stride for reading buffer.
    const size_t output_stride = output_numftrs; // Stride for outputting ftrs.
    // Where we read from in the buffer.
    const float* buffer_out_start = buffer + first_ftr; 
    size_t count;		// Frames this read.
    size_t frames_read = 0;	// Total number of frames read so far.


    // First handle the case when we want more than one buffer full of data.
    while (frames_left >= buf_frames)
    {
	// Increment of output pointer each iteration.
	const size_t ftr_inc = output_stride * buf_frames;

	count = str.read_ftrs(buf_frames, buffer);
	frames_read += count;
	frames_left -= count;
	// If got less frames than requested, then end of segment.
	if (count<buf_frames)
	{
	    frames_left = 0;	// No need to try reading more frames.
	}
	if (ftrs!=NULL)
	{
	    // Actually copy date into destination.
	    qn_copy_smf_mf(count, output_stride, buffer_stride,
			buffer_out_start, ftrs);
	    ftrs += ftr_inc;
	}
    }

    // Here we handle the less than one buffer full case.
    if (frames_left!=0)
    {
	count = str.read_ftrs(frames_left, buffer);
	frames_read += count;
	if (ftrs!=NULL)
	{
	    qn_copy_smf_mf(count, output_stride, buffer_stride,
			buffer_out_start, ftrs);
	}
    }
    return frames_read;
}

////////////////////////////////////////////////////////////////

QN_InFtrStream_Norm::QN_InFtrStream_Norm(int a_debug,
					 const char* a_dbgname,
					 QN_InFtrStream& a_str,
					 const float* a_bias_vec,
					 const float* a_scale_vec)
    : QN_InFtrStream_ProtoFilter(a_debug, "QN_InFtrStream_Norm",
				 a_dbgname, a_str),
      num_ftrs(a_str.num_ftrs()),
      bias_vec(new float [num_ftrs]),
      scale_vec(new float [num_ftrs])
{
    if (a_bias_vec) {
	qn_copy_vf_vf(num_ftrs, a_bias_vec, bias_vec);
    } else {
	qn_copy_f_vf(num_ftrs, 0.0, bias_vec);
    }
    if (a_scale_vec) {
	qn_copy_vf_vf(num_ftrs, a_scale_vec, scale_vec);
    } else {
	qn_copy_f_vf(num_ftrs, 1.0, scale_vec);
    }
    log.log(QN_LOG_PER_RUN, "Created normalized stream with %lu features.",
	    (unsigned long) num_ftrs);

    size_t i;

    log.log(QN_LOG_PER_SUBEPOCH, "Bias values...");
    for (i=0; i<num_ftrs; i++)
    {
	log.log(QN_LOG_PER_SUBEPOCH, "  %f", bias_vec[i]);
    }
    log.log(QN_LOG_PER_SUBEPOCH, "Scale values...");
    for (i=0; i<num_ftrs; i++)
    {
	log.log(QN_LOG_PER_SUBEPOCH, "  %f", scale_vec[i]);
    }
}

QN_InFtrStream_Norm::~QN_InFtrStream_Norm()
{
    delete [] scale_vec;
    delete [] bias_vec;
}

size_t
QN_InFtrStream_Norm::read_ftrs(size_t a_frames, float* a_ftrs)
{
    size_t count;		// Number of frames read.
    float* ftrs = a_ftrs;

    count = str.read_ftrs(a_frames, a_ftrs);
    if (a_ftrs!=NULL)
    {
	size_t i;

	for (i=0; i<count; i++)
	{
	    qn_add_vfvf_vf(num_ftrs, ftrs, bias_vec, ftrs);
	    qn_mul_vfvf_vf(num_ftrs, ftrs, scale_vec, ftrs);
	    ftrs += num_ftrs;
	}
    }
    
    return count;
}

////////////////////////////////////////////////////////////////
QN_InFtrStream_NormUtts::QN_InFtrStream_NormUtts(int a_debug, 
						 const char* a_dbgname,
						 QN_InFtrStream& a_str) 
    : QN_InFtrStream_Norm(a_debug, a_dbgname, a_str, 0, 0) {
	my_seg = QN_SIZET_BAD;
}
QN_InFtrStream_NormUtts::~QN_InFtrStream_NormUtts(void) {
}

    // We only have to specialize the fns that change the seg
QN_SegID QN_InFtrStream_NormUtts::nextseg(void) {
    QN_SegID segid = QN_InFtrStream_Norm::nextseg();
    if (segid == QN_SEGID_BAD) {
	my_seg = QN_SIZET_BAD;
    } else {
	if (my_seg == QN_SIZET_BAD) {
	    my_seg = 0;
	} else {
	    ++my_seg;
	}
	calc_mean_sd();
	// rewind to start of seg
	if (QN_InFtrStream_Norm::set_pos(my_seg, 0) == QN_SEGID_BAD) {
	    log.error("unable to seek utt %d in nextseg", my_seg);
	    segid = QN_SEGID_BAD;
	}
    }
    return segid;
}

int QN_InFtrStream_NormUtts::rewind(void) {
    my_seg = QN_SIZET_BAD;
    return QN_InFtrStream_Norm::rewind();
}

QN_SegID QN_InFtrStream_NormUtts::set_pos(size_t segno, size_t frameno) {
    if (segno != my_seg) {
	my_seg = segno;
	if (QN_InFtrStream_Norm::set_pos(my_seg, 0) == QN_SEGID_BAD) {
	    log.error("unable to seek utt %d in set_pos", my_seg);
	} else {
	    calc_mean_sd();
	}
    }
    return QN_InFtrStream_Norm::set_pos(my_seg, frameno);
}

void QN_InFtrStream_NormUtts::calc_mean_sd(void) {
    // Calculate the mean and sd thru end of seg
    // & write to bias/scale of parent Norm filter

    // We assume that we've been set up at the start of the utterance we 
    // are to normalize; just read ftrs through to EOS and 
    // calc their norms.

    // basic code copied from QN_utils.cc:QN_ftrstats

    // Allocate some workspace 
    int buf_size = 64;	// Read this many frames at once
    float*const ftr_buf = new float [num_ftrs*buf_size]; // Read features here
#ifdef OLD
    double*const ftr_sum = new double [num_ftrs];  // Sum features here
    double*const ftr_sum2 = new double [num_ftrs]; // Sum square of features here
    double*const ftr_tmp = new double [num_ftrs];  // Temporary holder 

    // Initialize accumulators
    qn_copy_d_vd(num_ftrs, 0.0, ftr_sum);
    qn_copy_d_vd(num_ftrs, 0.0, ftr_sum2);
#else /* NEW */
    float*const ftr_sum = new float [num_ftrs];  // Sum features here
    float*const ftr_sum2 = new float [num_ftrs]; // Sum square of features here
    float*const ftr_tmp = new float [num_ftrs];  // Temporary holder

    // Initialize accumulators
    qn_copy_f_vf(num_ftrs, 0.0, ftr_sum);
    qn_copy_f_vf(num_ftrs, 0.0, ftr_sum2);
#endif /* OLD/NEW */

    int i, frames = -1;
    size_t total_frames = 0;

    while (frames != 0) {

	// Read in some feature vectors
	frames = str.read_ftrs(buf_size, ftr_buf);

#ifdef OLD
	for(i = 0; i < frames; ++i) {

	    total_frames ++;

	    // Calculate sum of features.
	    qn_conv_vf_vd(num_ftrs, ftr_buf+i*num_ftrs, ftr_tmp);
	    qn_acc_vd_vd(num_ftrs, ftr_tmp, ftr_sum);
	    
	    // Calculate sum of squares of features.
	    qn_sqr_vd_vd(num_ftrs, ftr_tmp, ftr_tmp);
	    qn_acc_vd_vd(num_ftrs, ftr_tmp, ftr_sum2);
	}
#else /* NEW */
	if (frames) {
	    qn_sumcol_mf_vf(frames, num_ftrs, ftr_buf, ftr_tmp);
	    qn_acc_vf_vf(num_ftrs, ftr_tmp, ftr_sum);
	    qn_sqr_vf_vf(frames*num_ftrs, ftr_buf, ftr_buf);
	    qn_sumcol_mf_vf(frames, num_ftrs, ftr_buf, ftr_tmp);
	    qn_acc_vf_vf(num_ftrs, ftr_tmp, ftr_sum2);
	    total_frames += frames;
	}
#endif /* OLD/NEW */
    }

    // Calculate means
    // Features mean = sum / N
    // Write bias as negative of mean, since we add it
    const double recip_frames = 1.0/(double) total_frames;
    const double neg_recip_frames = -recip_frames;
#ifdef OLD
    qn_mul_vdd_vd(num_ftrs, ftr_sum, neg_recip_frames, ftr_tmp);
    qn_conv_vd_vf(num_ftrs, ftr_tmp, bias_vec);
    // Calculate standard deviations
    // = sqrt( (sum_of_squares/N - (sum/N)^2)
    qn_sqr_vd_vd(num_ftrs, ftr_tmp, ftr_tmp);
    qn_mul_vdd_vd(num_ftrs, ftr_sum2, recip_frames, ftr_sum2);
    qn_sub_vdvd_vd(num_ftrs, ftr_sum2, ftr_tmp, ftr_tmp);
    qn_conv_vd_vf(num_ftrs, ftr_tmp, scale_vec);
#else /* NEW */
    qn_mul_vff_vf(num_ftrs, ftr_sum, neg_recip_frames, bias_vec);
    // Calculate standard deviations
    // = sqrt( (sum_of_squares/N - (sum/N)^2)
    qn_sqr_vf_vf(num_ftrs, bias_vec, ftr_tmp);
    qn_mul_vff_vf(num_ftrs, ftr_sum2, recip_frames, ftr_sum2);
    qn_sub_vfvf_vf(num_ftrs, ftr_sum2, ftr_tmp, scale_vec);
#endif /* OLD/NEW */
    // Check for channels with excessively small variance
    // limit is 1/1,000,000 of mean-square
#define LIMIT_VAR_ON_SUMSQ (0.000001)
    // Unbias it
    double unbias = ((double) total_frames)/((double) total_frames-1);
    qn_mul_vff_vf(num_ftrs, scale_vec, unbias, scale_vec);

    float ddummy, dmin;
    qn_maxmin_vf_ff(num_ftrs, ftr_sum2, &ddummy, &dmin);
    if (dmin == 0) {
      log.warning("within utterance feature identically zero.");
      // This isn't supposed to happen, so it's OK to do it the slow way
      size_t i;
      for (i = 0; i < num_ftrs; ++i) {
	if (ftr_sum2[i] == 0) {
	  ftr_sum2[i] = 1.0;
	}
      }
    }
    qn_div_vfvf_vf(num_ftrs, scale_vec, ftr_sum2, ftr_tmp);
    qn_maxmin_vf_ff(num_ftrs, ftr_tmp, &ddummy, &dmin);
    if (dmin < LIMIT_VAR_ON_SUMSQ) {
      log.warning("excessively small within-utterance variance.");
      // slow math again
      size_t i;
      for (i = 0; i < num_ftrs; ++i) {
	if (ftr_tmp[i] < LIMIT_VAR_ON_SUMSQ) {
	  scale_vec[i] = LIMIT_VAR_ON_SUMSQ * ftr_sum2[i];
	}
      }
    }

    // Convert variance to SD
    qn_sqrt_vf_vf(num_ftrs, scale_vec, scale_vec); 
    // reciprocal since we multiply by it
    qn_recip_vf_vf(num_ftrs, scale_vec, scale_vec);

    // Free used workspace
#ifdef OLD
    delete [] ftr_tmp;
    delete [] ftr_sum2;
    delete [] ftr_sum;
#else /* NEW */
    delete [] ftr_tmp;
    delete [] ftr_sum2;
    delete [] ftr_sum;
#endif /* OLD/NEW */
    delete [] ftr_buf;

    // Record what we did to the log file
    log.log(QN_LOG_PER_SENT, "Bias values for seg %d (%d frames)...", my_seg, total_frames);
    for (i=0; i<(int)num_ftrs; i++) {
	log.log(QN_LOG_PER_SENT, "  %f", bias_vec[i]);
    }
    log.log(QN_LOG_PER_SENT, "Scale values for seg %d...", my_seg);
    for (i=0; i<(int)num_ftrs; i++) {
	log.log(QN_LOG_PER_SENT, "  %f", scale_vec[i]);
    }
}

////////////////////////////////////////////////////////////////

QN_InFtrStream_JoinFtrs::QN_InFtrStream_JoinFtrs(int a_debug,
						 const char* a_dbgname,
						 QN_InFtrStream& a_str1,
						 QN_InFtrStream& a_str2)
 :  clog(a_debug, "QN_InFtrStream_JoinFtrs", a_dbgname),
    str1(a_str1),
    str2(a_str2),
    str1_n_ftrs(a_str1.num_ftrs()),
    str2_n_ftrs(a_str2.num_ftrs())
{
    clog.log(QN_LOG_PER_RUN, "Joining streams with %lu and %lu features per"
	     " frame.", (unsigned long) str1_n_ftrs,
	     (unsigned long) str2_n_ftrs);
}

QN_InFtrStream_JoinFtrs::~QN_InFtrStream_JoinFtrs()
{
}

size_t 
QN_InFtrStream_JoinFtrs::read_ftrs(size_t cnt, float* ftrs, size_t stride)
{
    size_t frames1;		// Frames read from stream 1.
    size_t frames2;		// Frames read from stream 2.
    size_t real_stride;		// Stride we use.

    if (stride==QN_ALL)
	real_stride = str1_n_ftrs + str2_n_ftrs;
    else
	real_stride = stride;
    
    if (cnt == 1) {
	// desperately try to avoid the strided reads
	frames1 = str1.read_ftrs(cnt, ftrs);
	frames2 = str2.read_ftrs(cnt, ftrs+str1_n_ftrs);
    } else {
	frames1 = str1.read_ftrs(cnt, ftrs, real_stride);
	frames2 = str2.read_ftrs(cnt, ftrs+str1_n_ftrs, real_stride);
    }
    if (frames1!=frames2)
    {
	clog.error("number for frames read differs in read_ftrs().");
    }

    return frames1;
}


QN_SegID 
QN_InFtrStream_JoinFtrs::nextseg()
{
    QN_SegID segid1;
    QN_SegID segid2;

    segid1 = str1.nextseg();
    segid2 = str2.nextseg();

    if (segid1!=segid2)
    {
	clog.error("segment ID's differ in nextseg().");
    }
    return segid1;
}


size_t 
QN_InFtrStream_JoinFtrs::num_segs()
{
    size_t segs1;
    size_t segs2;
    
    segs1 = str1.num_segs();
    segs2 = str2.num_segs();

    if (segs1!=segs2)
    {
	clog.error("number of segments differ.");
    }
    return segs1;
}


size_t
QN_InFtrStream_JoinFtrs::num_frames(size_t segno)
{
    size_t frames1;
    size_t frames2;
    
    frames1 = str1.num_frames(segno);
    frames2 = str2.num_frames(segno);

    if (frames1!=frames2)
    {
	if (segno==QN_ALL)
	    clog.error("total number of frames differ.");
	else
 	    clog.error("number of frames in seg %ul differ.",
		       (unsigned long) segno);
   }
    return frames1;
}

int 
QN_InFtrStream_JoinFtrs::get_pos(size_t* segnop, size_t* framenop)
{
    size_t segno1;
    size_t segno2;
    size_t frameno1;
    size_t frameno2;
    int ec1;
    int ec2;

    ec1 = str1.get_pos(&segno1, &frameno1);
    ec2 = str2.get_pos(&segno2, &frameno2);
    if (ec1!=ec2)
    {
	clog.error("return codes differ in get_pos().");
    }
    if (segno1!=segno2)
    {
	clog.error("segment numbers differ in get_pos().");
    }
    if (frameno1!=frameno2)
    {
	clog.error("frame numbers differ in get_pos().");
    }
    if (segnop!=NULL)
	*segnop = segno1;
    if (framenop!=NULL)
	*framenop = frameno1;
    return ec1;
}

QN_SegID
QN_InFtrStream_JoinFtrs::set_pos(size_t segno, size_t frameno)
{
    QN_SegID segid1;
    QN_SegID segid2;

    segid1 = str1.set_pos(segno, frameno);
    segid2 = str2.set_pos(segno, frameno);
    if (segid1!=segid2)
    {
	clog.error("segment ID's differ in set_pos().");
    }
    return segid1;
}

int 
QN_InFtrStream_JoinFtrs::rewind()
{
    int ec1;
    int ec2;

    ec1 = str1.rewind();
    ec2 = str2.rewind();
    if (ec1!=ec2)
    {
	clog.error("return codes differ in rewind().");
    }
    return ec1;
}

////////////////////////////////////////////////////////////////

QN_InFtrStream_OneHot::QN_InFtrStream_OneHot(int a_debug,
					     const char* a_dbgname,
					     QN_InLabStream& a_labstr,
					     size_t a_size,
					     size_t a_bufsize,
					     float a_highval,
					     float a_lowval)
  : clog(a_debug, "QN_InFtrStream_OneHot", a_dbgname),
    labstr(a_labstr),
    size(a_size),
    bufsize(a_bufsize),
    highval(a_highval),
    lowval(a_lowval)
{
    if (labstr.num_labs()!=1)
    {
	clog.error("Input label stream has %lu labels, should only have 1.",
		   (unsigned long) labstr.num_labs());
    }
    clog.log(QN_LOG_PER_RUN, "Created a one-hot feature stream with %lu "
	     " features, high=%f, low=%f, bufsize=%lu frames.",
	     (unsigned long) size, highval, lowval, (unsigned long) bufsize);
    buf = new QNUInt32[bufsize];
}

QN_InFtrStream_OneHot::~QN_InFtrStream_OneHot()
{
    delete[] buf;
}

// This function does the acutal work of the output function.

void
QN_InFtrStream_OneHot::fill_ftrs(size_t frames, float* ftrs, size_t stride)
{
    size_t i;

    for (i=0; i<frames; i++)
    {
	QNUInt32 lab;		// Label for current frame.
	
	lab = buf[i];
	if (lab>=size)
	{
	    clog.error("Label value is %lu, which is greater than ouput "
		       "width %lu.",
		       (unsigned long) lab, (unsigned long) size);
	}
	qn_copy_f_vf(size, lowval, ftrs);
	ftrs[lab] = highval;
	ftrs += stride;
    }
}

size_t
QN_InFtrStream_OneHot::read_ftrs(size_t a_cnt, float* a_ftrs, size_t a_stride)
{
    size_t stride;		// Stride used in writing features.
    size_t frames_left = a_cnt;	// No. of frames still to be read.
    size_t frames_so_far = 0;	// Number of frames read so far.
    size_t frames;		// Frames this read.

    if (a_stride==QN_ALL)
	stride = size;
    else
	stride = a_stride;
    
    // Do whole buffer full chunks first.
    while (frames_left>=bufsize)
    {
	const size_t ftrs_inc = stride * bufsize; // Ftr pointer increment.

	frames = labstr.read_labs(bufsize, buf);
	frames_left -= frames;
	frames_so_far += frames;
	if (frames<bufsize)
	    frames_left = 0;
	if (a_ftrs!=NULL)
	{
	    fill_ftrs(frames, a_ftrs, stride);
	    a_ftrs += ftrs_inc;
	}
    }
    // Now do less than one buffer full.
    frames = labstr.read_labs(frames_left, buf);
    frames_so_far += frames;
    if (a_ftrs!=NULL)
	fill_ftrs(frames, a_ftrs, stride);

    return frames_so_far;
}

////////////////////////////////////////////////////////////////

// A function to return a random number in a given range using our local
// seed.
static size_t
ranged_rand48(unsigned short xsubi[3], size_t min, size_t max)
{
    assert(max>=min);
    if (max==min)
	return max;
    else
    {
	size_t range = max - min;
	return (nrand48(xsubi) % range) + min;
    }
}

QN_InLabStream_FrameInfo::QN_InLabStream_FrameInfo(int a_debug,
						   const char* a_dbgname,
						   QN_InStream& str)
    : clog(a_debug, "QN_InLabStream_FrameInfo", a_dbgname),
      seglens(NULL),
      epoch(0),
      segno(QN_SIZET_BAD),
      frameno(QN_SIZET_BAD)
{
    size_t i;		// Local counter.

    n_segs = str.num_segs();
    seglens = new size_t[n_segs];
    assert(n_segs!=QN_SIZET_BAD && n_segs>0);
    for (i=0; i<n_segs; i++)
    {
	size_t frames;	// No. of frames in the current segment.

	frames = str.num_frames(i);
	assert(frames!=QN_SIZET_BAD && frames>0);
	seglens[i] = frames;
	total_frames += frames;
    }
    clog.log(QN_LOG_PER_RUN, "Created a stream with %lu segments, "
	     "total %lu frames.", (unsigned long) n_segs,
	     (unsigned long) total_frames);
}

QN_InLabStream_FrameInfo::QN_InLabStream_FrameInfo(int a_debug,
						   const char* a_dbgname,
						   size_t a_min_segs,
						   size_t a_max_segs,
						   size_t a_min_frames,
						   size_t a_max_frames,
						   int a_seed)
    : clog(a_debug, "QN_InLabStream_FrameInfo", a_dbgname),
      seglens(NULL),
      epoch(0),
      segno(QN_SIZET_BAD),
      frameno(QN_SIZET_BAD)
{
    size_t i;		// Local counter.
    unsigned short xsubi[3]; // rand48 seed.

    assert(a_min_segs>0 && a_max_segs>=a_min_segs);
    assert(a_min_frames>0 && a_max_frames>=a_min_frames);

    xsubi[0] = a_seed & 0xffff;
    xsubi[1] = 0x337f;
    xsubi[2] = (a_seed >> 16) & 0xffff;
    n_segs = ranged_rand48(xsubi, a_min_segs, a_max_segs);
    seglens = new size_t[n_segs];
    total_frames = 0;
    for (i=0; i<n_segs; i++)
    {
	size_t frames;	// No. of frames in the current segment.

	frames = ranged_rand48(xsubi, a_min_frames, a_max_frames);
	seglens[i] = frames;
	total_frames += frames;
    }
    clog.log(QN_LOG_PER_RUN, "Created a stream with %lu segments with "
	     "between %lu and %lu frames, "
	     "total %lu frames.", (unsigned long) n_segs,
	     (unsigned long) a_min_frames, (unsigned long) a_max_frames,
	     (unsigned long) total_frames);
}


QN_InLabStream_FrameInfo::~QN_InLabStream_FrameInfo()
{
    delete [] seglens;
}

int
QN_InLabStream_FrameInfo::rewind()
{
    epoch++;
    segno = QN_SIZET_BAD;
    frameno = QN_SIZET_BAD;
    return QN_OK;
}

QN_SegID
QN_InLabStream_FrameInfo::nextseg()
{
    QN_SegID segid;		// Returned segment ID.

    if (segno==QN_SIZET_BAD)
	segno = 0;
    else
	segno++;
    if (segno>=n_segs)
	segid = QN_SEGID_BAD;
    else
    {
	frameno = 0;
	segid = QN_SEGID_UNKNOWN;
	clog.log(QN_LOG_PER_SENT, "Moved on to segmemt %lu which has %lu "
		 "frames.", (unsigned long) segno, seglens[segno]);
    }

    return segid;
}

size_t 
QN_InLabStream_FrameInfo::read_labs(size_t a_count, QNUInt32* a_labs)
{
    size_t count = 0;		// Count of frames returned.
    QNUInt32* labs = a_labs;	// Pointer to where we return labels.
    size_t n_frames;		// Number of frames in current seg.
    n_frames = seglens[segno];

    assert(segno!=QN_SIZET_BAD);
    while(count<a_count && frameno<n_frames)
    {
	labs[EPOCH_OFFSET] = (QNUInt32) epoch;
	labs[SEGNO_OFFSET] = (QNUInt32) segno;
	labs[FRAMENO_OFFSET] = (QNUInt32) frameno;
	frameno++;
	count++;
	labs += NUM_LABELS;
    }
    clog.log(QN_LOG_PER_FRAME, "Read %lu frames (of requested %lu) "
	     "starting at frame no. %lu.", (unsigned long) count,
	     (unsigned long) a_count, (unsigned long) (frameno-count));
    return count;
}

size_t 
QN_InLabStream_FrameInfo::num_segs()
{
    return n_segs;
}

size_t
QN_InLabStream_FrameInfo::num_frames(size_t segn)
{
    size_t ret;			// Returned value.

    if (segn==QN_SIZET_BAD)
	ret = total_frames;
    else
    {
	assert(segn<n_segs);
	ret = seglens[segn];
    }
    return ret;
}

int
QN_InLabStream_FrameInfo::get_pos(size_t* a_segnop, size_t* a_framenop)
{
    if (a_segnop!=NULL)
	*a_segnop = segno;
    if (a_framenop!=NULL)
	*a_framenop = frameno;
    return QN_OK;
}

QN_SegID
QN_InLabStream_FrameInfo::set_pos(size_t a_segno, size_t a_frameno)
{
    assert(a_segno<n_segs);
    assert(a_frameno<seglens[a_segno]);

    segno = a_segno;
    frameno = a_frameno;
    return QN_SEGID_UNKNOWN;
}

