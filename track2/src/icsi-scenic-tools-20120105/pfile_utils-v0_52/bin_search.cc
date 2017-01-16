//
// 
// $Header: /u/drspeech/repos/pfile_utils/bin_search.cc,v 1.2 2012/01/05 20:32:02 dpwe Exp $
//  Binary search function.
//  Jeff Bilmes <bilmes@cs.berkeley.edu> June 1997
// 
// 

#include "pfile_utils.h"

//
// bin_search assumes the following preconditions hold:
//   1)  array[0] <= val <= array[length-1];
//   2)  length > 0
// If bin_search is called without these being true, 
//     unanticipated errors could occur.
// bin_search returns the index i such that:
//    array[i] <= val < array[i+1].
// If (val == array[length-1]), then length-1 is returned.
// 
size_t
bin_search(float *array,  // the sorted array to search
           size_t length, // the length of the array
           float val)     // the value to search for.
{
  
  size_t l,r;
  l = 0; r = length-1;

  // array[l] <= val <= array[r].
  // or  array[r] <= val < array[r+1]
  while (l < r) {
    // array[l] <= val <= array[r].
    // or  array[r] <= val < array[r+1]
    size_t m = (l+r+1)/2;

    if (val == array[m])
      return m;

    if (val < array[m])
      r = m-1;
    else if (val > array[m])
      l = m;
  }
  return l;
}



#ifdef MAIN

main(int argc, char*argv[])
{

  int i,j;
  int len;

  if (argc < 2) {
    fprintf(stderr,"supply an integer array length argument\n");
    exit(-1);
  }
  len = atoi(argv[1]);
  float array[len];

  for (i=0;i<len;i++) 
    array[i] = (float)i/(float)len;

  for (i=-30;i<(2*len-1)+5;i++) {
    float val = (float)i*1.0/(2.0*len);
    printf("location of %f is %d\n",
	   val,
	   bin_search(array,len,val));
  }


}

#endif
