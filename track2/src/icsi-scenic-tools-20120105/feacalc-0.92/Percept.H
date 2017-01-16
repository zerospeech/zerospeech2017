//
// Percept.H
//
// C++ encapsulation of rasta processing chain (interface to old C code)
// Part of feacalc project.
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/Percept.H,v 1.1 2002/03/18 21:13:16 dpwe Exp $

#ifndef _PERCEPT_H
#define _PERCEPT_H

#include "FtrCalc.H"

// Instance that calculates Percept features
class FtrCalc_Percept {
    // encapsulation of full processing chain 
private:
  int coeff;	      // Number of coefficients if any
  int smallmask;
  float nyqhz;
  float sampfreq;

  int fourier;	/* calculate the fourier coefficients*/
  int extended;
  int pbz;

public:
    FtrCalc_Percept(CL_VALS* clvals);
    ~FtrCalc_Percept(void);

    floatRef Process(/* const */ floatRef* samples);
    void Reset(CL_VALS* clvals);

  float get_loudness(fvec *fptr);
  float get_zeroCrossing(fvec *fptr);
  float get_bandwidth(fvec *fptr, float brigthness, float power_spectrum);
  float get_brigthness(fvec *fptr, float power_spectrum);
  float get_spectrum(fvec *fptr);
  struct floatVec get_subband(fvec *fptr);  
  struct floatVec Process(floatRef* samples, int *isZero);
  float get_correlation(int lag,  float *data, int n);
  void ApplyHammingDistance(fvec * fptr_data, fvec * fptr_hamming_data);
  float get_pitch(fvec *data_fptr);
  float find_Maximum(float *correlation, int n);
  struct StaticVec computeMeanAndVariance(floatVec frame, int nframes); 
  int get_num_points_FFT(int samplesNumber);
};

#endif /* _PERCEPT_H */
