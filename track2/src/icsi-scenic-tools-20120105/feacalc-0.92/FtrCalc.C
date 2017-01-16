//
// FtrCalc.C
//
// Base class for feature calculation object
//
// 2001-12-28 dpwe@ee.columbia.edu
// $Header: /u/drspeech/repos/feacalc/FtrCalc.C,v 1.1 2002/03/18 21:11:25 dpwe Exp $

#include "FtrCalc.H"

FtrCalc::FtrCalc(CL_VALS* clvals) {
    // Initialize feature processing base class
    // set up our public fields
  nfeats = 0;
  framelen = 0;
  steplen = 0;
  padlen = 0;
  dozpad = 0;
}

FtrCalc::~FtrCalc(void) {
}
