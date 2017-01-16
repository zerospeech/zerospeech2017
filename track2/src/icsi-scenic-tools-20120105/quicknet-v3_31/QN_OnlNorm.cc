const char* QN_OnlNorm_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_OnlNorm.cc,v 1.5 2004/09/08 01:36:28 davidj Exp $";

// This file contains various input stream filters.

#include <QN_config.h>
#include "QN_OnlNorm.h"
#include "QN_fltvec.h"
#include <stdlib.h>

#include <math.h>	// for sqrt()

////////////////////////////////////////////////////////////////

QN_InFtrStream_OnlNorm::QN_InFtrStream_OnlNorm(int a_debug,
					 const char* a_dbgname,
					 QN_InFtrStream& a_str,
					 const float* a_bias_vec,
					 const float* a_scale_vec, 
					 double a_alpha_mu, 
					 double a_alpha_var)
    : QN_InFtrStream_ProtoFilter(a_debug, "QN_InFtrStream_OnlNorm",
				 a_dbgname, a_str),
      num_ftrs(a_str.num_ftrs()),
      bias_vec(new float [num_ftrs]),
      scale_vec(new float [num_ftrs]), 
      onl_bias_vec(new float [num_ftrs]),
      onl_scale_vec(new float [num_ftrs]), 
      alpha_v(a_alpha_var),
      alpha_m(a_alpha_mu)
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
    log.log(QN_LOG_PER_RUN, "Created OnlNormalized stream with %lu features.",
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

QN_InFtrStream_OnlNorm::~QN_InFtrStream_OnlNorm()
{
    delete [] scale_vec;
    delete [] bias_vec;
    delete [] onl_scale_vec;
    delete [] onl_bias_vec;
}

size_t
QN_InFtrStream_OnlNorm::read_ftrs(size_t a_frames, float* a_ftrs)
{
    size_t count;		// Number of frames read.
    float* ftrs = a_ftrs;

    count = str.read_ftrs(a_frames, a_ftrs);
    if (a_ftrs!=NULL)
    {
	size_t i, j;

	for (i=0; i<count; i++)
	{
	    // update the means & vars
	    for (j = 0; j < num_ftrs; ++j) {
		double mean = - onl_bias_vec[j];
		double var = 1 / (onl_scale_vec[j] * onl_scale_vec[j]);
		double x = ftrs[j];

		mean = (1 - alpha_m) * mean + alpha_m * x;
		x -= mean;
		var = (1 - alpha_v) * var + alpha_v * x * x;

		onl_bias_vec[j] = - mean;
		onl_scale_vec[j] = 1 / sqrt(var);		
	    }

	    qn_add_vfvf_vf(num_ftrs, ftrs, onl_bias_vec, ftrs);
	    qn_mul_vfvf_vf(num_ftrs, ftrs, onl_scale_vec, ftrs);
	    ftrs += num_ftrs;
	}
    }
    
    return count;
}

void QN_InFtrStream_OnlNorm::reset_onl_scales(void) {
    qn_copy_vf_vf(num_ftrs, bias_vec, onl_bias_vec);
    qn_copy_vf_vf(num_ftrs, scale_vec, onl_scale_vec);    
}

// We only have to specialize the fns that change the seg

QN_SegID QN_InFtrStream_OnlNorm::nextseg(void) {
    QN_SegID segid = QN_InFtrStream_ProtoFilter::nextseg();
    // We just have to reset the mean and var to their initial vals
    reset_onl_scales();
    return segid;
}

#define QN_ONLNORM_BUFSIZE 32
#define QN_MIN(a,b) (((a)<(b))?(a):(b))

static size_t seek_by_read(QN_InFtrStream *str, size_t n_frames) {
    // Effect a seek into a certain point in a segment by 
    // reading (then discarding) frames.  This is the only 
    // way to seek for onl_norm, when we have to reconstruct
    // the appropriate state.
    float *buf;
    size_t n_ftrs = str->num_ftrs();
    size_t bufsize = QN_ONLNORM_BUFSIZE;
    size_t toget = 0, got = 0, totgot = 0, remain = n_frames;

    buf = new float[n_ftrs*bufsize];
    while(remain > 0 && got == toget) {
	toget = QN_MIN(remain, bufsize);
	got = str->read_ftrs(toget, buf);
	if (got == QN_SIZET_BAD) {
	    got = 0;
	}
	remain -= got;
	totgot += got;
    }
    delete [] buf;
    return totgot;
}

QN_SegID QN_InFtrStream_OnlNorm::set_pos(size_t segno, size_t frameno) {
    // Disallow seeks into the middle of a seg
    QN_SegID rc;
    reset_onl_scales();
	rc = QN_InFtrStream_ProtoFilter::set_pos(segno, 0);
    if (frameno != 0) {
	log.warning("slow seek to mid-seg (seg/fr %d/%d) with online norm", 
		    segno, frameno);
	size_t pos;
	if ( (pos = seek_by_read(this, frameno)) < frameno ) {
	    log.error("error read-seeking to seg/fr %lu/%lu at %lu",
		      (unsigned long) segno,
		      (unsigned long) frameno, (unsigned long) pos);
	}
    }
    return rc;
}
