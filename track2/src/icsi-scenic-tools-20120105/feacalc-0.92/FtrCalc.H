//
// FtrCalc.H
//
// Base class for feature calculation object
//
// 2001-12-28 dpwe@ee.columbia.edu
// $Header: /u/drspeech/repos/feacalc/FtrCalc.H,v 1.1 2002/03/18 21:11:21 dpwe Exp $

#ifndef _FTRCALC_H
#define _FTRCALC_H

//#include <floatArray/floatArray.H>
#include "floatVec.H"
#include "CmdLineStuff.H"	// to define CL_VALS

// Abstract base class for feature calculation
class FtrCalc {
    // encapsulation of full processing chain 
private:
public:
    // Read-only slots (really)
    int nfeats;		// width of feature rows written to output
    int framelen;	// samples in analysis window
    int steplen;	// samples between successive an.win starts
    int padlen;		// samples of padding to add at start and end
    int dozpad;		// pad with zeros? (else attempt to reflect sig)

    FtrCalc(CL_VALS* clvals);
    ~FtrCalc(void);

    virtual floatRef Process(/* const */ floatRef* samples) = 0;
    virtual void Reset(CL_VALS *clvals) = 0;
};

#endif /* _FTRCALC_H */
