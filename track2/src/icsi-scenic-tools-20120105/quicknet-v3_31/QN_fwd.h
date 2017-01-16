// $Header: /u/drspeech/repos/quicknet2/QN_fwd.h,v 1.5 2004/07/24 00:10:59 davidj Exp $

#ifndef QN_fwd_h_INCLUDED
#define QN_fwd_h_INCLUDED


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_MLP.h"


// Perform a forward pass with handling of hard labels.  A stream of features
// is read from "inp_str" and passed through the MLP "mlp" in chunks of
// "bunch_size" frames.  If "out_str" is non-NULL, the output units form the
// net are sent to it.  If "inlab_str" is non-NULL, the labels from this
// stream are compared with the highest output unit to produce accuracy
// statistics.  If "outlab_str" is non-NULL, the index of the highest output
// unit is sent to this stream.  If "verbose" is non-zero, more status
// messages are produced.

void QN_hardForward(int debug, const char* dbgname, int verbose, QN_MLP* mlp,
		    QN_InFtrStream* inp_str, QN_InLabStream* inplab_str,
		    QN_OutFtrStream* out_str, QN_OutLabStream* outlab_str,
		    size_t bunch_size, int lastlab_reject = 0);


void
QN_enumForward(int debug, const char* dbgname, int verbose, QN_MLP* mlp,
	       QN_InFtrStream* inp_str, QN_InLabStream* inlab_str,
	       QN_OutFtrStream* out_str, size_t unary_size);

#endif
