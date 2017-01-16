// $Header: /u/drspeech/repos/quicknet2/testsuite/RateSchedule_test.cc,v 1.4 1996/07/03 22:21:29 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Testfile for learning rate schedules

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "rtst.h"
#include "QN_RateSchedule.h"
#include "QN_Logger_Simple.h"

QN_Logger* QN_logger;

void
test_enum()
{
    const float rates[] = { 0.01, 0.005, 0.003 };
    const size_t NUMRATES = 3;

    rtst_start("RateSchedule_List");
    QN_RateSchedule_List rs(rates, NUMRATES);
    rtst_assert(rs.get_rate() == 0.01f);
    rtst_assert(rs.next_rate(100.0) == 0.005f);
    rtst_assert(rs.get_rate() == 0.005f);
    rtst_assert(rs.get_rate() == 0.005f);
    rtst_assert(rs.next_rate(0.0) == 0.003f);
    rtst_assert(rs.next_rate(1.0) == 0.0f);
    rtst_assert(rs.get_rate() == 0.0f);
    rtst_assert(rs.next_rate(0.5) == 0.0f);
    rtst_passed();
}

void
test_newbob()
{

    rtst_start("RateSchedule_NewBoB");

    QN_RateSchedule_NewBoB rs(0.01, 0.5, 1.0, 0.5);
    rtst_assert(rs.get_rate() == 0.01f);
    rtst_assert(rs.next_rate(29.0) == 0.01f);
    rtst_assert(rs.get_rate() == 0.01f);
    rtst_assert(rs.next_rate(28.1) == 0.005f);
    rtst_assert(rs.get_rate() == 0.005f);
    rtst_assert(rs.next_rate(27.4) == 0.0025f);
    rtst_assert(rs.get_rate() == 0.0025f);
    // Error increasing - stop
    rtst_assert(rs.next_rate(28.0) == 0.0f);
    rtst_assert(rs.next_rate(77.0) == 0.0f);
    
    QN_RateSchedule_NewBoB rs2(0.01, 0.5, 1.0, 0.5, 100.0, 3);
    rtst_assert(rs2.get_rate() == 0.01f);
    rtst_assert(rs2.next_rate(29.0) == 0.01f);
    rtst_assert(rs2.next_rate(28.1) == 0.005f);
    // Here we stop at epoch 3, despite lowering of error.
    rtst_assert(rs2.next_rate(26) == 0.0f);
    
    // Test 0 epochs limit.
    QN_RateSchedule_NewBoB rs3(0.01, 0.5, 1.0, 0.5, 100.0, 0);
    rtst_assert(rs3.get_rate() == 0.0f);
    rtst_assert(rs3.next_rate(29.0) == 0.0f);
    rtst_passed();
}

int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);
    assert(arg-argc==0);

    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "RateSchedule_test");
    test_enum();
    test_newbob();
}
