// -*- c++ -*-
// $Header: /u/drspeech/repos/quicknet2/QN_OnlNorm.h,v 1.1 2000/07/03 20:36:07 dpwe Exp $

#ifndef QN_OnlNorm_h_INCLUDED
#define QN_OnlNorm_h_INCLUDED

// This header contains various simple "filters" for the QuickNet
// stream objects.  Filters do things like caclulate deltas, return
// only some subset of the database or normalize features.


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_args.h"

#include "QN_filters.h"

////////////////////////////////////////////////////////////////
// The "OnlNorm" filter runs an online normalization of each 
// feature stream, resetting to initial values passed in 
// at the start of each utterance.  Non-sequential reading 
// of frames within an utterance is not allowed.

class QN_InFtrStream_OnlNorm : public QN_InFtrStream_ProtoFilter
{
public:
    // Create a "OnlNorm" filter.
    // Note that copies of the bias and scale vectors are taken so the
    // original vectors can be deleted.
    // "a_debug" is the level of debugging verbosity
    // "a_dbgname" is the tag used for this instance in debugging output.
    // "a_str" is the stream we are taking a subset of
    // "a_bias_vec" is the values we will add (it must be the same length as
    //              a frame in the stream being OnlNormalized).
    // "a_scale_vec" is the values we will multiply by.
    // "a_alpha_mu"  is the update constant for means
    // "a_alpha_var" is the update constant for variance
    QN_InFtrStream_OnlNorm(int a_debug, const char* a_dbgname,
			QN_InFtrStream& a_str,
			const float* a_bias_vec, const float* a_scale_vec, 
			double a_alpha_mu, double a_alpha_var);
    ~QN_InFtrStream_OnlNorm();
    size_t read_ftrs(size_t a_frames, float* ftrs);

    QN_SegID nextseg(void);
    QN_SegID set_pos(size_t segno, size_t frameno);

 protected:
    void reset_onl_scales(void);	// called to reset onl versions

protected:
    // Most stuff provided by "ProtoFilter" base class.
    const size_t num_ftrs;	// A local version to save having to get if
				// from the input stream every time.

    float* bias_vec;		// Preset values to start each utt
    float* scale_vec;

    float* onl_bias_vec;	// workspace for bias/scale on the fly
    float* onl_scale_vec;

    double alpha_v;
    double alpha_m;

};

#endif /* QN_OnlNorm_h_INCLUDED */
