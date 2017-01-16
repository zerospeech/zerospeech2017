const char* QN_MLP_Online1632Fx3_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_MLP_Online1632Fx3.cc,v 1.4 2011/03/03 00:32:59 davidj Exp $";

#include <QN_config.h>

#ifdef QN_HAVE_LIBFXLIB

#include <assert.h>
#include <stdlib.h>
#include <float.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "fltvec.h"
#include "intvec.h"
#include "QN_libc.h"
#ifdef __cplusplus
}
#endif

#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_MLP_Online1632Fx3.h"


QN_MLP_Online1632Fx3::QN_MLP_Online1632Fx3(int dbg, size_t input,
					   size_t hidden,
					   size_t output,
					   QN_OutputLayerType outtype,
					   int a_in2hid_exp, int a_hid2out_exp)
    : clog(dbg, "QN_MLP_Online1632Fx3", ""),
      n_input(input),
      n_hidden(hidden),
      n_output(output),
      n_in2hid(input * hidden),
      n_hid2out(hidden * output),
      out_layer_type(outtype),
      in2hid_exp(a_in2hid_exp),
      hid2out_exp(a_hid2out_exp),
      feature_sat_warning_given(0),
      weight_sat_warning_given(0),
      bias_sat_warning_given(0),
      lr_sat_warning_given(0)
{
    // Maybe we do not support all output layer types
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	break;
    default:
	clog.error("Unknown output layer non-linearity selected in "
		   "constructor.");
    }

    in_y = malloc_vi16(n_input);
    in2hid_h = malloc_vi16(n_in2hid); qn_copy_i16_vi16(n_in2hid, 0, in2hid_h);
    in2hid_l = malloc_vi16(n_in2hid); qn_copy_i16_vi16(n_in2hid, 0, in2hid_l);
    hid_bias = malloc_vi32(n_hidden); qn_copy_i32_vi32(n_hidden, 0, hid_bias);
    hid_x32 = malloc_vi32(n_hidden);
    hid_x16 = malloc_vi16(n_hidden);
    hid_y = malloc_vi16(n_hidden);
    hid_sy = malloc_vi16(n_hidden);

    hid2out_h = malloc_vi16(n_hid2out); qn_copy_i16_vi16(n_hid2out, 0, hid2out_h);
    hid2out_l = malloc_vi16(n_hid2out); qn_copy_i16_vi16(n_hid2out, 0, hid2out_l);
    out_bias = malloc_vi32(n_output); qn_copy_i32_vi32(n_output, 0, out_bias);
    out_x32 = malloc_vi32(n_output);
    out_x16 = malloc_vi16(n_output);
    out_y = malloc_vi16(n_output);

    target16 = malloc_vi16(n_output);
    // These intermediate values are only needed for sigmoid output layers
    if (out_layer_type==QN_OUTPUT_SIGMOID)
    {
	out_dedy = malloc_vi16(n_output);
	out_dydx = malloc_vi16(n_output);
    }
    out_dedx = malloc_vi16(n_output);
    
    hid_dedy32 = malloc_vi32(n_hidden);
    hid_dedy16 = malloc_vi16(n_hidden);
    hid_dydx = malloc_vi16(n_hidden);
    hid_dedx = malloc_vi16(n_hidden);
    in2hid_sdedx = malloc_vi16(n_hidden);
    clog.log(QN_LOG_PER_RUN, "Created net inputs=%u hidden=%u output=%u",
	     n_input, n_hidden, n_output);
#ifdef USE_IPM
    IPM_timer_clear(&forw);
    IPM_timer_clear(&back);
    IPM_timer_clear(&wup);
    IPM_timer_clear(&misc);
    IPM_timer_clear(&tofix);
    IPM_timer_clear(&fromfix);
    IPM_timer_clear(&total);
#endif
}

QN_MLP_Online1632Fx3::~QN_MLP_Online1632Fx3()
{
    free_vi16(in_y);
    free_vi16(in2hid_h);
    free_vi16(in2hid_l);
    free_vi32(hid_bias);
    free_vi32(hid_x32);
    free_vi16(hid_x16);
    free_vi16(hid_y);
    free_vi16(hid_sy);

    free_vi16(hid2out_h);
    free_vi16(hid2out_l);
    free_vi32(out_bias);
    free_vi32(out_x32);
    free_vi16(out_x16);
    free_vi16(out_y);

    free_vi16(target16);
    // These intermediate values are only created for sigmoid output layers
    if (out_layer_type==QN_OUTPUT_SIGMOID)
    {
	free_vi16(out_dedy);
	free_vi16(out_dydx);
    }
    free_vi16(out_dedx);
    
    free_vi32(hid_dedy32);
    free_vi16(hid_dedy16);
    free_vi16(hid_dydx);
    free_vi16(hid_dedx);
    free_vi16(in2hid_sdedx);
#ifdef USE_IPM
    char report[IPM_TIMER_REPORT_LEN+10];

    IPM_timer_report("forw", &forw, report); fputs(report, stderr);
    IPM_timer_report("back", &back, report); fputs(report, stderr);
    IPM_timer_report("wup", &wup, report); fputs(report, stderr);
    IPM_timer_report("misc", &misc, report); fputs(report, stderr);
    IPM_timer_report("tofix", &tofix, report); fputs(report, stderr);
    IPM_timer_report("fromfix", &fromfix, report); fputs(report, stderr);
    IPM_timer_report("total", &total, report); fputs(report, stderr);
    double total_time = IPM_timer_read(&total);
    double forw_time = IPM_timer_read(&forw);
    double back_time = IPM_timer_read(&back);
    double wup_time = IPM_timer_read(&wup);
    double tofix_time = IPM_timer_read(&tofix);
    double fromfix_time = IPM_timer_read(&fromfix);
    double misc_time = IPM_timer_read(&misc);
    double unacc_time = total_time - forw_time - back_time - wup_time -
	tofix_time - fromfix_time - misc_time;
    fprintf(stderr, "forward=%g%%\n", forw_time*100.0/total_time);
    fprintf(stderr, "error backprop=%g%%\n", back_time*100.0/total_time);
    fprintf(stderr, "weight update=%g%%\n", wup_time*100.0/total_time);
    fprintf(stderr, "conversions=%g%%\n",
	    (tofix_time + fromfix_time)*100.0 / total_time );
    fprintf(stderr, "unaccounted=%g%%\n", unacc_time * 100.0/total_time);
#endif
}


size_t
QN_MLP_Online1632Fx3::size_layer(QN_LayerSelector layer) const
{
    size_t r;

    switch(layer)
    {
    case QN_MLP3_INPUT:
	r = n_input;
	break;
    case QN_MLP3_HIDDEN:
	r = n_hidden;
	break;
    case QN_MLP3_OUTPUT:
	r = n_output;
	break;
    default:
	r = 0;
	break;
    }
    return r;
}

void
QN_MLP_Online1632Fx3::size_section(QN_SectionSelector section, size_t* output_p,
			       size_t* input_p) const
{
    switch(section)
    {
    case QN_MLP3_INPUT2HIDDEN:
	*output_p = n_hidden;
	*input_p = n_input;
	break;
    case QN_MLP3_HIDDENBIAS:
	*output_p = n_hidden;
	*input_p = 1;
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	*output_p = n_output;
	*input_p = n_hidden;
	break;
    case QN_MLP3_OUTPUTBIAS:
	*output_p = n_output;
	*input_p = 1;
	break;
    default:
	clog.error("Tying to find size of unknown section.");
    }
}

size_t
QN_MLP_Online1632Fx3::num_connections() const
{
    // Note how we take account of the biases separately
    return (n_input+n_output) * n_hidden + n_hidden + n_output;
}

void
QN_MLP_Online1632Fx3::forward(size_t n_frames, const float* in, float* out)
{
    size_t i;

#ifdef USE_IPM
    IPM_timer_start(&total);
#endif
    for (i=0; i<n_frames; i++)
    {
	forward1(in, out);
	in += n_input;
	out += n_output;
    }
#ifdef USE_IPM
    IPM_timer_stop(&total);
#endif
}

void
QN_MLP_Online1632Fx3::train(size_t n_frames, const float* in,
			    const float* target, float* out)
{
    size_t i;

#ifdef USE_IPM
    IPM_timer_start(&total);
#endif
    for (i=0; i<n_frames; i++)
    {
	train1(in, target, out);
	in += n_input;
	out += n_output;
	target += n_output;
    }
#ifdef USE_IPM
    IPM_timer_stop(&total);
#endif
}

void
QN_MLP_Online1632Fx3::forward1(const float* in, float* out)
{
    fxbool sat;

    // Convert inputs
#ifdef USE_IPM
    IPM_timer_start(&tofix);
#endif
    sat = conv_vf_vx16s(n_input, in_y_exp, in, in_y);
#ifdef USE_IPM
    IPM_timer_stop(&tofix);
#endif
    if (sat && !feature_sat_warning_given)
    {
	clog.warning("features saturated on input.");
	feature_sat_warning_given = 1;
    }

    // First layer
#ifdef USE_IPM
    IPM_timer_start(&forw);
#endif
    sat = mul_vx16mx16_vx32t(n_input, n_hidden, in_y_exp, in2hid_exp,
			     hid_x32_exp, in_y, in2hid_h, hid_x32);
    if (sat!=FXFALSE)
    {
	clog.error("Intermediate saturation in forward pass input to hidden "
		   "matrix multiply - check normalization and fixed point "
		   "exponent.");
    }
#ifdef USE_IPM
    IPM_timer_stop(&forw);
#endif
    acc_vx32_vx32(n_hidden, hid_bias_exp, hid_x32_exp, hid_bias, hid_x32);
    conv_vx32_vx16(n_hidden, hid_x32_exp, hid_x16_exp, hid_x32, hid_x16);
    sigmoid_vx16_vx16(n_hidden, hid_x16_exp, hid_y_exp, hid_x16, hid_y);

    // Second layer
#ifdef USE_IPM
    IPM_timer_start(&forw);
#endif
    sat = mul_vx16mx16_vx32t(n_hidden, n_output, hid_y_exp, hid2out_exp,
			     out_x32_exp, hid_y, hid2out_h, out_x32);
    if (sat!=FXFALSE)
    {
	clog.error("Intermediate saturation in forward pass hidden to output "
		   "matrix multiply - check normalization and fixed point "
		   "exponent.");
    }
#ifdef USE_IPM
    IPM_timer_stop(&forw);
#endif
    acc_vx32_vx32(n_output, out_bias_exp, out_x32_exp, out_bias, out_x32);
    conv_vx32_vx16(n_output, out_x32_exp, out_x16_exp, out_x32, out_x16);
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
    case QN_OUTPUT_SIGMOID_XENTROPY:
	sigmoid_vx16_vx16(n_output, out_x16_exp, out_y_exp, out_x16, out_y);
	break;
    case QN_OUTPUT_SOFTMAX:
//	softmax_vx16_vx16(n_output, out_x16_exp, out_y_exp, out_x16, out_y);
	softmax_vx32_vx16(n_output, out_x32_exp, out_y_exp, out_x32, out_y);
	break;
    case QN_OUTPUT_LINEAR:
	conv_vx16_vx16(n_output, out_x16_exp, out_y_exp, out_x16, out_y);
	break;
    default:
	assert(0);
    }
#ifdef USE_IPM
    IPM_timer_start(&fromfix);
#endif
    conv_vx16_vf(n_output, out_y_exp, out_y, out);
#ifdef USE_IPM
    IPM_timer_stop(&fromfix);
#endif
}

void
QN_MLP_Online1632Fx3::train1(const float *in, const float* target, float* out)
{
    fxbool sat;

// First move forward
    forward1(in, out);

// Find the error and propogate back through output layer in an appropriate
// manner 
#ifdef USE_IPM
    IPM_timer_start(&tofix);
#endif
    sat = conv_vf_vx16s(n_output, target16_exp, target, target16);
#ifdef USE_IPM
    IPM_timer_stop(&tofix);
#endif
    switch(out_layer_type)
    {
    case QN_OUTPUT_SIGMOID:
	// For a sigmoid layer, de/dx = de/dy . dy/dx
	sub_vx16vx16_vx16(n_output, out_y_exp, target16_exp, out_dedx_exp,
			  out_y, target16, out_dedy);
	dsigmoid_vx16_vx16(n_output, out_y_exp, out_dydx_exp, out_y,
			   out_dydx);
	mul_vx16vx16_vx16(n_output, out_dydx_exp, out_dedy_exp, out_dedx_exp,
			  out_dydx, out_dedy, out_dedx);
	break;
    case QN_OUTPUT_SIGMOID_XENTROPY:
    case QN_OUTPUT_SOFTMAX:
    case QN_OUTPUT_LINEAR:
	// For these layers, de/dx = dy/dx
	sub_vx16vx16_vx16(n_output, out_y_exp, target16_exp, out_dedx_exp,
			  out_y, target16, out_dedx);
	break;
    default:
	assert(0);
    }

// Back propagate error through hidden to output weights
#ifdef USE_IPM
    IPM_timer_start(&back);
#endif
    sat = mul_mx16vx16_vx32t(n_hidden, n_output, hid2out_exp, out_dedx_exp,
			     hid_dedy32_exp, hid2out_h, out_dedx, hid_dedy32);
    if (sat!=FXFALSE)
    {
	clog.error("Intermediate saturation in training hidden to output "
		   "back propagation - check error terms and fixed point "
		   "exponent.");
    }
#ifdef USE_IPM
    IPM_timer_stop(&back);
#endif
    
    conv_vx32_vx16(n_hidden, hid_dedy32_exp, hid_dedy16_exp, hid_dedy32,
		   hid_dedy16);

// Update hid2out weights and biases
    mulacc_vx16x16_vx32(n_output, out_dedx_exp, neg_out_learnrate_exp,
			out_bias_exp, out_dedx, neg_out_learnrate, out_bias);
    fxexp_t hid_sy_exp = hid_y_exp + neg_hid2out_learnrate_exp;
    mul_vx16x16_vx16(n_hidden, hid_y_exp, neg_hid2out_learnrate_exp,
		     hid_sy_exp, hid_y, neg_hid2out_learnrate,
		     hid_sy);
#ifdef USE_IPM
    IPM_timer_start(&wup);
#endif
    mulacc_vx16vx16_mhx32s(n_hidden, n_output, n_output,
			   hid_sy_exp, out_dedx_exp, hid2out_exp,
			   hid_sy, out_dedx, hid2out_h, hid2out_l);
#ifdef USE_IPM
    IPM_timer_stop(&wup);
#endif

// Propagate error back through sigmoid
    dsigmoid_vx16_vx16(n_hidden, hid_y_exp, hid_dydx_exp, hid_y, hid_dydx);
    mul_vx16vx16_vx16(n_hidden, hid_dydx_exp, hid_dedy16_exp, hid_dedx_exp,
		      hid_dydx, hid_dedy16, hid_dedx);

// Update in2hid weights and biases
    mulacc_vx16x16_vx32(n_hidden, hid_dedx_exp, neg_hid_learnrate_exp,
			hid_bias_exp, hid_dedx, neg_hid_learnrate, hid_bias);
    fxexp_t in2hid_sdedx_exp = hid_dedx_exp + neg_in2hid_learnrate_exp;
    mul_vx16x16_vx16(n_hidden, hid_dedx_exp, neg_in2hid_learnrate_exp,
		     in2hid_sdedx_exp, hid_dedx, neg_in2hid_learnrate,
		     in2hid_sdedx);
#ifdef USE_IPM
    IPM_timer_start(&wup);
#endif
    mulacc_vx16vx16_mhx32s(n_input, n_hidden, n_hidden,
			   in_y_exp, in2hid_sdedx_exp, in2hid_exp,
			   in_y, in2hid_sdedx, in2hid_h, in2hid_l);
#ifdef USE_IPM
    IPM_timer_stop(&wup);
#endif

}

void
QN_MLP_Online1632Fx3::set_weights(enum QN_SectionSelector which, 
				  size_t row, size_t col, 
				  size_t n_rows, size_t n_cols,
				  const float* weights)
{
    fxbool weight_sat = FXFALSE;
    fxbool bias_sat = FXFALSE;

    // Note that the fixed point weight matrix is transposed
    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
    {
	const float* src_ptr = weights;
	fxint16* dest_ptr_h = in2hid_h + col*n_hidden + row;
	fxint16* dest_ptr_l = in2hid_l + col*n_hidden + row;
	fxint16* buf = falloc_vi16(n_cols);
	for (size_t i=0; i<n_rows; i++)
	{
	    weight_sat |= conv_vf_vx16s(n_cols, in2hid_exp, src_ptr, buf);
	    copy_vi16_svi16(n_cols, n_hidden, buf, dest_ptr_h);
	    copy_i16_svi16(n_cols, n_hidden, 0, dest_ptr_l);
	    src_ptr += n_cols;
	    dest_ptr_h += 1;
	    dest_ptr_l += 1;
	}
	ffree_vi16(buf);
	break;
    }
    case QN_MLP3_HIDDEN2OUTPUT:
    {
	const float* src_ptr = weights;
	fxint16* dest_ptr_h = hid2out_h + col*n_output + row;
	fxint16* dest_ptr_l = hid2out_l + col*n_output + row;
	fxint16* buf = falloc_vi16(n_cols);
	for (size_t i=0; i<n_rows; i++)
	{
	    weight_sat |= conv_vf_vx16s(n_cols, hid2out_exp, src_ptr, buf);
	    copy_vi16_svi16(n_cols, n_output, buf, dest_ptr_h);
	    copy_i16_svi16(n_cols, n_output, 0, dest_ptr_l);
	    src_ptr += n_cols;
	    dest_ptr_h += 1;
	    dest_ptr_l += 1;
	}
	ffree_vi16(buf);
	break;
    }
    case QN_MLP3_HIDDENBIAS:
	bias_sat |= 
	    conv_vf_vx32s(n_rows, hid_bias_exp, weights, hid_bias + row);
	break;
    case QN_MLP3_OUTPUTBIAS:
	bias_sat |=
	    conv_vf_vx32s(n_rows, out_bias_exp, weights, out_bias + row);
	break;
    default:
	clog.error("trying to set weights in unknown section.");
    }
    // This next bit gives us at most one warning of saturated weights/biases
    if (weight_sat && !weight_sat_warning_given)
    {
	clog.warning("weights saturated on input.");
	weight_sat_warning_given = 1;
    }
    if (bias_sat && !bias_sat_warning_given)
    {
	clog.warning("biases saturated on input.");
	bias_sat_warning_given = 1;
    }
}

void 
QN_MLP_Online1632Fx3::get_weights(enum QN_SectionSelector which,
			      size_t row, size_t col,
			      size_t n_rows, size_t n_cols,
			      float* weights)
{
    // Note that the fixed point weight matrix is transposed
    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
    {
	float* dest_ptr = weights;
	fxint16* src_ptr = in2hid_h + col*n_hidden + row;
	fxint16* buf = falloc_vi16(n_cols);
	for (size_t i=0; i<n_rows; i++)
	{
	    copy_svi16_vi16(n_cols, n_hidden, src_ptr, buf);
	    conv_vx16_vf(n_cols, in2hid_exp, buf, dest_ptr);
	    src_ptr += 1;
	    dest_ptr += n_cols;
	}
	ffree_vi16(buf);
	break;
    }
    case QN_MLP3_HIDDEN2OUTPUT:
    {
	float* dest_ptr = weights;
	fxint16* src_ptr = hid2out_h + col*n_output + row;
	fxint16* buf = falloc_vi16(n_cols);
	for (size_t i=0; i<n_rows; i++)
	{
	    copy_svi16_vi16(n_cols, n_output, src_ptr, buf);
	    conv_vx16_vf(n_cols, hid2out_exp, buf, dest_ptr);
	    src_ptr += 1;
	    dest_ptr += n_cols;
	}
	ffree_vi16(buf);
	break;
    }
    case QN_MLP3_HIDDENBIAS:
	conv_vx32_vf(n_rows, hid_bias_exp, hid_bias + row, weights);
	break;
    case QN_MLP3_OUTPUTBIAS:
	conv_vx32_vf(n_rows, out_bias_exp, out_bias + row, weights);
	break;
    default:
	clog.error("trying to get weights from unknown section.");
    }
}

void
QN_MLP_Online1632Fx3::set_learnrate(enum QN_SectionSelector which, float learnrate)
{
    fxbool sat = FXFALSE;
    enum { MIN_LEARNRATE_EXP = -10 };

    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
	neg_in2hid_learnrate_exp = qn_findexp_f_i(-learnrate);
	if (neg_in2hid_learnrate_exp < MIN_LEARNRATE_EXP)
	    neg_in2hid_learnrate_exp = MIN_LEARNRATE_EXP;
	sat = conv_f_x16s(neg_in2hid_learnrate_exp, -learnrate,
			  &neg_in2hid_learnrate);
	clog.log(QN_LOG_PER_EPOCH,  "Set in2hid learning rate %f (exp %i).",
		   -conv_x16_f(neg_in2hid_learnrate_exp,neg_in2hid_learnrate),
		   neg_in2hid_learnrate_exp);
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	neg_hid2out_learnrate_exp = qn_findexp_f_i(-learnrate);
	if (neg_hid2out_learnrate_exp < MIN_LEARNRATE_EXP)
	    neg_hid2out_learnrate_exp = MIN_LEARNRATE_EXP;
	sat = conv_f_x16s(neg_hid2out_learnrate_exp, -learnrate,
			  &neg_hid2out_learnrate);
  	clog.log(QN_LOG_PER_EPOCH, "Set hid2out learning rate %f (exp %i).",
		 -conv_x16_f(neg_hid2out_learnrate_exp,
			     neg_hid2out_learnrate),
		 neg_hid2out_learnrate_exp);
	break;
    case QN_MLP3_HIDDENBIAS:
	neg_hid_learnrate_exp = qn_findexp_f_i(-learnrate);
	if (neg_hid_learnrate_exp < MIN_LEARNRATE_EXP)
	    neg_hid_learnrate_exp = MIN_LEARNRATE_EXP;
	sat = conv_f_x16s(neg_hid_learnrate_exp, -learnrate,
			  &neg_hid_learnrate);
	clog.log(QN_LOG_PER_EPOCH,"Set hidbias learning rate %f (exp %i).",
		 -conv_x16_f(neg_hid_learnrate_exp, neg_hid_learnrate),
		 neg_hid_learnrate_exp);
	break;
    case QN_MLP3_OUTPUTBIAS:
	neg_out_learnrate_exp = qn_findexp_f_i(-learnrate);
	if (neg_out_learnrate_exp < MIN_LEARNRATE_EXP)
	    neg_out_learnrate_exp = MIN_LEARNRATE_EXP;
	sat = conv_f_x16s(neg_out_learnrate_exp, -learnrate,
			  &neg_out_learnrate);
	clog.log(QN_LOG_PER_EPOCH, "Set outbias learning rate %f (exp %i).",
		 -conv_x16_f(neg_out_learnrate_exp, neg_out_learnrate),
		 neg_out_learnrate_exp);
	break;
    default:
	clog.error("trying to set learning rate in unknown section.");
	break;
    }
    // This next bit gives us at most on warning of saturated learning rate
    // Note - currently this does nothing - but no harm in leaving it
    // in case we change the way learning rates work again - davidj 9/13/95
    if (sat && !lr_sat_warning_given)
    {
	clog.warning("learning rate saturated.");
	lr_sat_warning_given = 1;
    }
}

float 
QN_MLP_Online1632Fx3::get_learnrate(enum QN_SectionSelector which) const
{
    float res;			// Returned learning rate

    switch(which)
    {
    case QN_MLP3_INPUT2HIDDEN:
	res = -conv_x16_f(neg_in2hid_learnrate_exp, neg_in2hid_learnrate);
	break;
    case QN_MLP3_HIDDEN2OUTPUT:
	res = -conv_x16_f(neg_hid2out_learnrate_exp, neg_hid2out_learnrate);
	break;
    case QN_MLP3_HIDDENBIAS:
	res = -conv_x16_f(neg_hid_learnrate_exp, neg_hid_learnrate);
	break;
    case QN_MLP3_OUTPUTBIAS:
	res = -conv_x16_f(neg_out_learnrate_exp, neg_out_learnrate);
	break;
    default:
	res = 0;
	clog.error("trying to get learning rate from unknown section.");
	break;
    }
    return res;
}

#else // not #ifdef QN_HAVE_LIBFXLIB

// Some compilers break if there is nothing to compile, so have a dummy
// function.

#include "QN_MLP_Online1632Fx3.h"

int
QN_MLP_Online1632Fx3::dummy()
{
    return 0;
}
#endif
