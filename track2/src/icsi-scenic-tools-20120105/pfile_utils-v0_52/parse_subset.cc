/*
    $Header: /u/drspeech/repos/pfile_utils/parse_subset.cc,v 1.1.1.1 2002/11/08 19:47:07 bdecker Exp $
  
    Parses a subset spec 
    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <values.h>
#include <math.h>
#include <assert.h>

#include "error.h"
#include "parse_subset.h"


void
check_subset(const char* subset_str,
	     const size_t *subset,
	     const size_t subset_size,
	     const size_t n_feats)
{
  for (size_t i=0;i<subset_size;i++) {
    if (subset[i] >= n_feats) {
      error("ERROR: The specified argument (%s) contains features out of the range [0:%d].\n",
	    subset_str,n_feats-1);
    }
  }
}


/*
 * Parse a subset specification contained in subset_str and
 * put the resulting subset, as an array of integers in the
 * range [0:n_feats-1] inclusive, into subset. 'subset_size' gets
 * set to the number of features selected (i.e., the subset size),
 * 'subset_alen' gets set to the size of the array (which might
 * end up being bigger than subset_size). n_feats is the number of
 * features in the corresponding file and is used to check that
 * the resulting subset only specifies features in the 
 * range [0:n_feats-1].
 *
 * A subset spec grammer is:
 *  subset_spec -> token | list
 *  token -> 'nil' | 'none' | 'all' | 'full'
 *  list -> range | [ list ',' range ]
 *  range -> num | [num1'-'num2]
 *  num -> [0:n_feats-1]
 *
 *  Also
 *   num1 < num2
 *   'nil' - select no featuers
 *   'none' - select no featuers
 *   'all' - select all featuers in [0:n_feats-1]
 *   'full' - select all featuers in [0:n_feats-1]
 *   
 * Examples:
 *   0,4-7,14,30-35 
 * corresponds to:
 *   0,4,5,6,7,14,30,31,32,32,34,35
 */
void
parse_subset(const char *subset_str,
	     size_t* &subset,
	     size_t &subset_size,
	     size_t &subset_alen,
	     const size_t n_feats)
{
  size_t i;
  const char *p = subset_str;

  if (p == NULL)
    error("Error: No subset string argument given.\n");

  if (!strcmp(p,"nil") || !strcmp(p,"none")) {
    // empty subset, select no features.
    subset = 0;
    subset_size = 0;
    return;
  } else if (!strcmp(p,"all") || !strcmp(p,"full")) {
    // complete subset, select all features.
    subset = 0;
    subset_size = n_feats;
    return;
  }


  // initial sizes.
  subset_size = 0;
  subset_alen = 20;
  subset = new size_t[subset_alen];

  while (*p) {
    char*next;
    size_t l,u,val;

    l = (size_t) strtol(p, &next, 0);
    if (next == p) {
      error("Error: Invalid subset string argument given at (%s).\n",p);
    }

    if (*next == '-') {
      p = next+1;
      u = (size_t) strtol(p, &next, 0);
      if (next == p) {
	error("Error: Invalid subset string argument given at (%s).\n",p);
      }
    } else
      u = l;

    if (l > u)
      error("Error: Invalid subset string argument given at (%s).\n",p);

    for (val=l;val<=u;val++) {
      if (subset_size+1 == subset_alen) {
	subset_alen *= 2;
	size_t *tmp = new size_t[subset_alen];
	for (i=0;i<subset_size;i++)
	  tmp[i] = subset[i];
	delete subset;
	subset = tmp;
      }
      subset[subset_size++] = val;
    }
    
    p = next;
    if (*p == ',')
      p++;
  }

  // check that things are in range.
  check_subset(subset_str,subset,subset_size,
	       n_feats);

}






