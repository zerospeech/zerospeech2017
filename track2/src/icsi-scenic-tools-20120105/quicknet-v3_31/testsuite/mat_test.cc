// $Header: /u/drspeech/repos/quicknet2/testsuite/mat_test.cc,v 1.4 2006/02/16 01:23:06 davidj Exp $
//
// davidj - Thu Mar  2 21:28:36 1995
//
// Test of "mat" matlab format routines.

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "QN_types.h"
#include "QN_Logger_Simple.h"
#include "QN_mat.h"

#include "rtst.h"


void
write_test(char* filename)
{
    FILE* fp; 
    int ec;
    size_t rows, cols;
    size_t size;
    int test;

    rtst_start("write_test");

    fp = fopen(filename, "w");
    assert(fp!=NULL);

    // An interesting buffer size for testing.
    size_t bufsize = rtst_sizetests/3;

    for (test = 0; test<rtst_numtests/10; test++)
    {
	char nambuf[16];
	float* mat;
	size_t i, j;
	
	rows = rtst_urand_i32i32_i32(1, rtst_sizetests);
	cols = rtst_urand_i32i32_i32(1, rtst_sizetests);
	size = rows * cols;

	sprintf(nambuf, "mat%i", test);
	rtst_log("Matrix %s, size %lu x %lu.\n", nambuf,
		 (unsigned long) rows, (unsigned long) cols);
	mat = new float[size];
	for (i=0; i<cols; i++)
	{
	    for (j=0; j<rows; j++)
	    {
		mat[j*cols+i] = ((float) i)/1000.0 + (float) j;
	    }
	}
//	QN_Mat_dump(fp, nambuf, rows, cols, mat, bufsize);
	delete[] mat;
    }
    ec = fclose(fp);
    assert(ec==0);
    rtst_passed();
}


int
main(int argc, char* argv[])
{
    int arg;

    arg = rtst_args(argc, argv);
    assert(arg==argc-1);

    char* file = argv[arg++];
    assert(arg == argc);
    QN_logger = new QN_Logger_Simple(rtst_logfile, stderr,
				     "mat_test");
    write_test(file);
    rtst_exit();
}

