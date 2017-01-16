//
// Rasta.H
//
// C++ encapsulation of rasta processing chain (interface to old C code)
// Part of feacalc project.
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/Rasta.H,v 1.7 2002/03/18 21:10:38 dpwe Exp $

#ifndef _RASTA_H
#define _RASTA_H

#include "FtrCalc.H"

extern "C" {
// Get the rasta definitions
#ifdef FALSE
/* These get defined in rasta.h, so avoid the error */
#undef TRUE
#undef FALSE
#endif /* FALSE */
#include <rasta.h>
#ifndef TRUE
/* Make sure they got defined */
#error "rasta.h did not redefine TRUE!"
#endif /* TRUE */
}

// Instance that calculates Rasta features
class FtrCalc_Rasta : public FtrCalc {
    // encapsulation of full processing chain 
private:
    struct param params;
    struct fhistory history;
    struct map_param *pmapping_param;	// jah-rasta mapping matrix
    struct fvec *goodchans;		// for channel output
    DeltaCalc *dc;
    int rasta;		// the value of command-line-vals.rasta
    int cmpwt;		// the value of command-line-vale.cmpwt
    int plp;		// the value of command-line-vals.plp
    int domain;		// the value of command-line-vals.domain
    int slimspect;	// clv val: remove 'junk' spectral channels
public:
    FtrCalc_Rasta(CL_VALS* clvals);
    ~FtrCalc_Rasta(void);

    floatRef Process(/* const */ floatRef* samples);
    void Reset(CL_VALS* clvals);

    floatRef Powspec(floatRef* samps);
    // Just call powspec.  First part of process

};

#endif /* _RASTA_H */
