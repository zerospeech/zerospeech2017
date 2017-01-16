// -*- c++ -*-
//
// Range.H
//
// Another implementation of the range specification
// based on Jeff's and Eric's.
//
// 1998sep29 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacat/Range.H,v 1.15 2010/10/29 03:41:52 davidj Exp $

// Ranges are a way of specifying a range of integers (indexes) 
// with an ascii string.  For instance, you could say:
//   1,2,3,5,6,7,9,10,11
// or
//   1-3,5-7,9-11
// or 
//   1-11/4,8
// where numbers after a slash are omitted.
// You can also repeat terms, and present them in any order:
//   5,5,5,4,4,4,3,3,3
// although the definitions after a slash are repetition- and 
// order-insensitive.

// The full syntax is:
//      RANGESPEC -> @FILENAME|RANGELIST	Top level can be file of ixs
//  	RANGESPEC -> RANGETERM[;RANGESPEC]	Semicolon-separated terms
//	RANGETERM -> SEQUENCE[/SEQUENCE]	Items after slash are excluded
//	SEQUENCE -> RANGE[,SEQUENCE]		Comma-separated terms 
//	RANGE -> [POSITION][[:NUMBER]:[POSITION]] Matlab range specification
//	POSITION -> [^]NUMBER			Jeff's from-the-end format
// "-" is a synonym for ":", SPACE is a synonym for "," and 
// RETURN is a synonym for ";". 

#ifndef RANGE_H
#define RANGE_H

#include <stdio.h>

class RangeNode {
    // One node in a linked-list of matlab sequences
public:
    RangeNode *next;
    int start;
    int step;
    int end;
    RangeNode(RangeNode *a_next = NULL, int a_start = 0, int a_end = 0, int a_step = 1)
	: next(a_next), start(a_start), step(a_step), end(a_end)
	    { fixup_end(); }
    unsigned int n_pts(void) { 
	unsigned int np = 0; 
	/*	if (step*(end-start) >= 0) {  */
	if (step != 0 || end != start) {
	    np = 1+((end-start)/step); 
	    assert(np >= 0);
	}; return np; }
    void fixup_end(void) {
	end = start + (n_pts()-1)*step; 
    }
    int SubtractVal(int val);
    int SubtractRange(RangeNode *sub);
};

typedef RangeNode* RangeList;

void PrintRangeList(RangeList rangeList, char *tag=NULL, FILE* stream=stderr);
int SubtractSeqs(RangeList *ppos, RangeList neg);
unsigned int RangeListSize(RangeList list);

// A number to indicate that this value hasn't been set.
#include <limits.h>
#define RNG_VAL_BAD INT_MIN

// Types of range object
#define RNG_TYPE_NONE	0
#define RNG_TYPE_LIST	1
#define RNG_TYPE_FILE	2

class Range {
    // Represent a range of numbers
public:
    Range(const char *defstr=NULL, int minval=0, int maxval=RNG_VAL_BAD);
    ~Range(void);
    int SetLimits(int minval=0, int maxval=RNG_VAL_BAD);
    int SetDefStr(const char *spec);
    const char *GetDefStr(void) const {return def_string;}
    int GetType(void) const { return type; }

    void PrintRanges(const char *tag=NULL, FILE *stream=stderr);

    unsigned int length(void);	// total number of points in range
    int index(int ix);	// return ix'th value in list (counting from 0)

    int first(void);	// first value in list, substitute for min()
    int last(void);	// last value in list, like max()

    int contains(int val);	// test for this value

    int full(void);	// is this the range of "all"?

    const RangeList getRangeList(void) const { return rangeList; }

protected:
    int _parseNumber(int *pval);
    int _parseMatlabRange(int *pstart, int *pstep, int *pend);
    int _parseRange(int *pstart, int *pstep, int *pend);
    int _parseSequence(RangeList *seq);
    int _parseTerm(RangeList *seq);
    int _parseSpec(RangeList *seq);
    void _reportParseError(char *def_str);
    int _compileDefStr(void);
    int _compileSpecFile(char *filename);

    int type;			// one of the RNG_TYPE_* vals

    char *def_string;		// our copy of the definition string
    char *pos;			// global for roach-parsing string
    int min_val;		// lower bound defined for string
    int max_val;		// upper bound defined for string
    const char *errmsg;		// message describing parse error

    RangeList rangeList;	// The root of the range list

    static const char *colons;	// valid separators for matlab range specs
    static const char *commas;  // valid separators for number lists
    static const char *slashes;	// valid separators of exclude lists
    static const char *semis;   // valid top-level term separators
    static const char *space;	// non-linebreak WS
    static const char *WS;      // any WS
    static const char *alpha;

public:
  class iterator {
  protected:
      int cur_value;
      int atEnd;
      const Range* myrange;

      // Stuff for regular range lists
      RangeNode* cur_node;

      // stuff for file management
      const char *filename;
      FILE *file_handle;

      // stuff for read_next()
      int buflen;
      char *buf;
      int bufpos;
      int bufline;

  protected:
      int read_next(int *val, FILE* file, const char *name);

  public:
      iterator () { };	// Ugly default constructor is DANGEROUS!
      iterator (const Range& rng);
      iterator (const iterator& it);
      ~iterator(void);

      int reset (const Range& rng);
      int reset (void); // reset to where we were before .. hope we were!

      int next_el(void);

      int step_by(int n);	// i.e. step on multiple steps

      iterator& operator ++(void) // prefix
	  { next_el(); return *this; }
      iterator& operator ++(int) // suffix (used to return a new it? (no &))
	  { next_el(); return *this; }

      int at_end(void) const { return atEnd; }
      int val(void) const    { return cur_value; }
      const int operator *(void) const { return cur_value; }
      operator int(void) const         { return cur_value; }

      // Does the range we're currently in end in a finite value, 
      // or does it go to a default-like INT_MAX or INT_MIN?
      int current_range_finite(void) const;

      int operator ==(const iterator& it){ return (cur_value == it.val());}
      int operator !=(const iterator& it){ return (cur_value != it.val());}
      int operator <(const iterator& it) { return (cur_value < it.val());}
      int operator <=(const iterator& it){ return (cur_value <= it.val());}
      int operator >(const iterator& it) { return (cur_value > it.val());}
      int operator >=(const iterator& it){ return (cur_value >= it.val());}
  };

    friend class iterator;

    iterator begin(void) { return iterator(*this); }

};

// Iterators on range objects.


#endif /* RANGE_H */


