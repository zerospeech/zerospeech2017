// $Header: /u/drspeech/repos/quicknet2/testsuite/convol_test.cc,v 1.1 2007/02/19 09:04:50 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Test of convol routines

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "QN_types.h"
#include "QN_fltvec.h"

#include "rtst.h"

// Pulled in from QN_fir.cc
size_t
do_convolution(int filt_len, int ilen, float *filt,
               float *ip,
               float *op)
{
    int irem = ilen;
    int j;
    int ndone = 0;
    while (irem >= filt_len) {
    	double a = 0.0;
    	for (j = 0; j < filt_len; ++j) {
    	    a += filt[filt_len - 1 - j] * *(ip + j);
    	}
    	*op = a;
    	op ++;
    	ip ++;
    	--irem;
    	++ndone;
    }
    return ndone;
}

void
convol_test()
{
    int test;
  
    for (test = 0; test<rtst_numtests; test++)
    {
        size_t hsize, xsize, ysize;
	float *h, *x, *y1, *y2;

	hsize = rtst_urand_i32i32_i32(1, rtst_sizetests);
	xsize = rtst_urand_i32i32_i32(0, rtst_sizetests) + hsize;
	ysize = xsize - hsize + 1;
	rtst_log("hsize=%d xsize=%d ysize=%d\n", hsize, xsize, ysize);
	h = rtst_padvec_new_vf(hsize);
	x = rtst_padvec_new_vf(xsize);
	y1 = rtst_padvec_new_vf(ysize);
	y2 = rtst_padvec_new_vf(ysize);
	rtst_urand_ff_vf(hsize, -1.0, 1.0, h);
	rtst_urand_ff_vf(xsize, -1.0, 1.0, x);
	qn_convol_vfvf_vf(hsize, xsize, h, x, y1);
	do_convolution(hsize, xsize, h, x, y2);
	//	y1[0] += 0.0000000001;
        rtst_checknear_fvfvf(ysize, 1e-4, y1, y2);

	rtst_padvec_del_vf(y2);
	rtst_padvec_del_vf(y1);
	rtst_padvec_del_vf(x);
	rtst_padvec_del_vf(h);


    }
}



int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);

    assert(arg == argc);
    qn_math = 0;
    rtst_start("convol_test (nv)");
    convol_test();
    rtst_passed();
    // pp versin doesn't work - davidj - 2007/02/18
    //    qn_math |= QN_MATH_PP;
    //    rtst_start("convol_test (pp)");
    //    convol_test();
    //    rtst_passed();
    rtst_exit();
}


