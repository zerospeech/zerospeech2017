// $Header: /u/drspeech/repos/quicknet2/testsuite/tanh_test.cc,v 1.4 2007/06/08 19:00:36 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Test of "mat" matlab format routines.

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "QN_types.h"
#include "QN_fltvec.h"

#include "rtst.h"


void
tanh_test()
{
    rtst_start("tanh_test");
    int test;
    double err, max_err, percent;

    max_err = 0.0f;

    for (test = 0; test<rtst_numtests; test++)
    {
	float *in, *out1, *out2;
	size_t i;

	size_t size = rtst_urand_i32i32_i32(1, rtst_sizetests);
	in = rtst_padvec_new_vf(size);
	out1 = rtst_padvec_new_vf(size);
	out2 = rtst_padvec_new_vf(size);
	rtst_urand_ff_vf(size, -0.1, 0.1, in);
 	qn_nv_tanh_vf_vf(size, in, out1);
 	qn_fe_tanh_vf_vf(size, in, out2);
// 	qn_nv_sigmoid_vf_vf(size, in, out1);
// 	qn_fe_sigmoid_vf_vf(size, in, out2);
	for (i = 0; i<size; i++)
	{
	    err = fabs(out1[i] - out2[i]);
	    percent = err/out1[i] * 100.0;
	    rtst_log("in=%f nv=%f fe=%f err=%f pc=%f\n", in[i], out1[i], out2[i], err, percent);
	    if (err>max_err)
		max_err = err;
	}
	rtst_padvec_del_vf(out2);
	rtst_padvec_del_vf(out1);
	rtst_padvec_del_vf(in);

    }
    rtst_log("max_err=%f\n", max_err);
    rtst_passed();
}

void
steps()
{
    QN_EXPQ_WORKSPACE;
    
    rtst_start("tanh_step_test");
    
    float in = -0.000179;
    float t = -in * 2.0f;

    rtst_log("in=%f t=%f\n", in, t);

#ifdef QN_HAVE_EXPF
    float e1 = expf(t);
#else
    float e1 = exp(t);
#endif
    float e2 = QN_EXPQ(t);
    rtst_log("e1=%f e2=%f\n", e1, e2);
    

    rtst_passed();
}


int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);

//    assert(arg == argc);
//    tanh_test();
    steps();
    rtst_exit();
}

