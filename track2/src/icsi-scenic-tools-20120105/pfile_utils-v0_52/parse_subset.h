/*
    $Header: /u/drspeech/repos/pfile_utils/parse_subset.h,v 1.1.1.1 2002/11/08 19:47:07 bdecker Exp $
  
    Parses a subset spec 
    Jeff Bilmes <bilmes@cs.berkeley.edu>
*/

#ifndef PARSE_SUBSET_H
#define PARSE_SUBSET_H

extern
void
parse_subset(const char *subset_str,
	     size_t* &subset,
	     size_t &subset_size,
	     size_t &subset_alen,
	     const size_t n_feats);
#endif
