// $Header: /u/drspeech/repos/quicknet2/testsuite/endtest.cc,v 1.3 2006/02/03 23:55:30 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Various tests for endianness routines.

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "QuickNet.h"

#include "rtst.h"

const float MIN = -1.0;
const float MAX = 1.0;

void
test(const char* datafile, size_t len)
{
    int rc;
    size_t cnt;
    size_t i;
    FILE* f;

    float* innf = rtst_padvec_new_vf(len);
    float* inbf = rtst_padvec_new_vf(len);
    float* inlf = rtst_padvec_new_vf(len);
    double* innd = rtst_padvec_new_vd(len);
    double* inbd = rtst_padvec_new_vd(len);
    double* inld = rtst_padvec_new_vd(len);
    float* outnf = rtst_padvec_new_vf(len);
    float* outbf = rtst_padvec_new_vf(len);
    float* outlf = rtst_padvec_new_vf(len);
    double* outnd = rtst_padvec_new_vd(len);
    double* outbd = rtst_padvec_new_vd(len);
    double* outld = rtst_padvec_new_vd(len);

    rtst_urand_ff_vf(len, MIN, MAX, innf);
    rtst_start("doing in-core conversions");

    // Basic vf_vd conversion
    qn_conv_vf_vd(len, innf, innd);
    qn_conv_vd_vf(len, innd, outnf);
    rtst_checkeq_vfvf(len, innf, outnf);
    qn_copy_f_vf(len, 0.0, outnf);

    // vf endinanness conversions
    qn_htob_vf_vf(len, innf, inbf);
    qn_htol_vf_vf(len, innf, inlf);
    qn_btoh_vf_vf(len, inbf, outnf);
    rtst_checkeq_vfvf(len, innf, outnf);
    qn_copy_f_vf(len, 0.0, outnf);
    qn_ltoh_vf_vf(len, inlf, outnf);
    rtst_checkeq_vfvf(len, innf, outnf);
    qn_copy_f_vf(len, 0.0, outnf);

    // vd endinanness conversions
    qn_htob_vd_vd(len, innd, inbd);
    qn_htol_vd_vd(len, innd, inld);
    qn_btoh_vd_vd(len, inbd, outnd);
    rtst_checkeq_vdvd(len, innd, outnd);
    qn_copy_d_vd(len, 0.0, outnd);
    qn_ltoh_vd_vd(len, inld, outnd);
    rtst_checkeq_vdvd(len, innd, outnd);
    qn_copy_d_vd(len, 0.0, outnd);

    rtst_passed();

    rtst_start("doing IO tests - writing");
    f = fopen(datafile, "w");
    rtst_assert(f!=NULL);

    rtst_assert(fwrite(innf, sizeof(float), len, f)==len);
    rtst_assert(fwrite(inbf, sizeof(float), len, f)==len);
    rtst_assert(fwrite(inlf, sizeof(float), len, f)==len);
    rtst_assert(fwrite(innd, sizeof(double), len, f)==len);
    rtst_assert(fwrite(inbd, sizeof(double), len, f)==len);
    rtst_assert(fwrite(inld, sizeof(double), len, f)==len);

    rc = fclose(f);
    rtst_assert(rc==0);

    rtst_passed();
    rtst_start("doing IO tests - reading");

    f = fopen(datafile, "r");
    rtst_assert(f!=NULL);

    qn_fread_Nf_vf(len, f, outnf);
    qn_fread_Bf_vf(len, f, outbf);
    qn_fread_Lf_vf(len, f, outlf);
    rtst_checkeq_vfvf(len, innf, outnf);
    qn_copy_f_vf(len, 0.0, outnf);
    rtst_checkeq_vfvf(len, innf, outbf);
    qn_copy_f_vf(len, 0.0, outbf);
    rtst_checkeq_vfvf(len, innf, outlf);
    qn_copy_f_vf(len, 0.0, outlf);

    qn_fread_Nd_vd(len, f, outnd);
    qn_fread_Bd_vd(len, f, outbd);
    qn_fread_Ld_vd(len, f, outld);
    rtst_checkeq_vdvd(len, innd, outnd);
    qn_copy_d_vd(len, 0.0, outnd);
    rtst_checkeq_vdvd(len, innd, outbd);
    qn_copy_d_vd(len, 0.0, outbd);
    rtst_checkeq_vdvd(len, innd, outld);
    qn_copy_d_vd(len, 0.0, outld);

    rewind(f);

    qn_fread_Nf_vd(len, f, outnd);
    qn_fread_Bf_vd(len, f, outbd);
    qn_fread_Lf_vd(len, f, outld);
    rtst_checkeq_vdvd(len, innd, outnd);
    qn_copy_d_vd(len, 0.0, outnd);
    rtst_checkeq_vdvd(len, innd, outbd);
    qn_copy_d_vd(len, 0.0, outbd);
    rtst_checkeq_vdvd(len, innd, outld);
    qn_copy_d_vd(len, 0.0, outld);

    qn_fread_Nd_vf(len, f, outnf);
    qn_fread_Bd_vf(len, f, outbf);
    qn_fread_Ld_vf(len, f, outlf);
    rtst_checkeq_vfvf(len, innf, outnf);
    qn_copy_f_vf(len, 0.0, outnf);
    rtst_checkeq_vfvf(len, innf, outbf);
    qn_copy_f_vf(len, 0.0, outbf);
    rtst_checkeq_vfvf(len, innf, outlf);
    qn_copy_f_vf(len, 0.0, outlf);

    rc = fclose(f);
    rtst_assert(rc==0);

    rtst_passed();

    rtst_padvec_del_vf(outld);
    rtst_padvec_del_vf(outbd);
    rtst_padvec_del_vf(outnd);
    rtst_padvec_del_vf(outlf);
    rtst_padvec_del_vf(outbf);
    rtst_padvec_del_vf(outnf);
    rtst_padvec_del_vf(inld);
    rtst_padvec_del_vf(inbd);
    rtst_padvec_del_vf(innd);
    rtst_padvec_del_vf(inlf);
    rtst_padvec_del_vf(inbf);
    rtst_padvec_del_vf(innf);
}

int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);
    rtst_assert(arg==argc-1);

    const char* datafile = argv[arg++];
    rtst_assert(arg == argc);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				      "endtest");
    test(datafile, 123456);
    test(datafile, 3);
    rtst_exit();
}

