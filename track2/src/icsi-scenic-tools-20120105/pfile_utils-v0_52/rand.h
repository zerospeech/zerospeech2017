// 
// Random number interface.
// 
// Written by: Jeff Bilmes
//             bilmes@icsi.berkeley.edu
//
// $Header: /u/drspeech/repos/pfile_utils/rand.h,v 1.1.1.1 2002/11/08 19:47:08 bdecker Exp $


#ifndef rand_h
#define rand_h

#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <math.h>
#include <sys/types.h>
#include <stdlib.h>
#include <math.h>

#ifndef HAVE_BOOL
#ifndef BOOL_DEFINED
enum bool { false = 0, true = 1 };
#define BOOL_DEFINED
#endif
#endif

extern "C" {
extern double drand48(void);
unsigned short *seed48(unsigned short seed16v[3]);
	   };

class RAND {
 public:

  static unsigned short seedv[3];

  RAND(bool seed = false) { 
    if (seed) {
      seedv[0] = time(0); seedv[1] = time(0); seedv[2] = time(0);
      seed48(seedv); 
    }
  }

  void seed()
    {
      seedv[0] = time(0); seedv[1] = time(0); seedv[2] = time(0);
      seed48(seedv); 
    }

  /* return a random number uniformly in [l,u] inclusive, l < u */
  int uniform(int l, int u) 
    { return (l + (int)((1+u-l)*drand48())); }

  /* return a random number uniformly in [0,u] inclusive */
  int uniform(int u) 
    { return ((int)((u+1)* drand48())); }

  /* return 1 with probability p */
  int coin(float p) 
    { return (int)
	(drand48() < p); }

  // drand48 plus epsilon, i.e., (0,1)
  double drand48pe() {
    double rc;
    do {
      rc = drand48();
    } while (rc == 0.0);
    return rc;
  }

  // choose from [0:(u-1)] according to a length u 'dist', assume sum(dist) = 1.0
  int sample(const int u,const float *const dist);


};

extern double inverse_error_func(double p);
extern double inverse_normal_func(double p);


extern RAND rnd;

#endif
