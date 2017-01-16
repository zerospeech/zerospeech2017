//
// floatVec.H
//
// Cut down version of nArray.H to make feacalc independent
//
// 2001-04-16 dpwe@ee.columbia.edu
// $Header: /u/drspeech/repos/feacalc/floatVec.H,v 1.4 2006/03/11 00:20:55 davidj Exp $

#ifndef _FLOATVEC_H
#define _FLOATVEC_H

#include <iostream>
#include <assert.h>
#include <string.h>
#include <stdlib.h>    /* for abort() */

using namespace std;

inline int min(int a, int b) {if (a < b) return a; else return b;}
inline int max(int a, int b) {if (a > b) return a; else return b;}

class floatRef {
protected:
    int       len;
    float     *data;
    int       step;

    void      _Swap(floatRef& fr) 
                { float *td = data; data = fr.data; fr.data = td;
		  int ti = len; len  = fr.len;  fr.len  = ti;
		  ti = step; step = fr.step; fr.step = ti; }
    int	      _Check(int i) const
                { if(i<0 || i>=len)  { cerr << "floatRef: " \
		     << i << " bad for len " << len << endl; abort(); }
		  return 1;}
    void      _Init(int l, float *d, int s=1)
                { len = l; data = d;  step = s; }
public:
    floatRef(int l=0, float *d=0, int s=1) : len(l), data(d), step(s)
                { }
    floatRef(const floatRef& fr)
                { len = fr.len; data = fr.data; step = fr.step; }
    ~floatRef(void)
                { }
    floatRef& operator = (const floatRef& fr)   // copy referred data 
                { if(this != &fr)  Copy(fr);  return *this; }

    // Create a new ref, returned by val, for subrange
    floatRef  Ref(int start = 0, int olen = -1) {
	floatRef _fr;
	if (olen < 0)	olen = len - start;
	assert(olen >= 0 && olen+start <= len);
	_fr.SetLen(olen); _fr.SetStep(step); _fr.SetData(&_El(start));
	return _fr; }

    float     Val(int i) const	   { float v = 0; _Check(i);
                                     v = *(data+step*i); return v;}
    float&    _El(int i)	   { return *(data+step*i); }
    float&    El(int i) 	   { _Check(i); return _El(i); }
    float&    elem(int i) 	   { return El(i); }	// GNU compat
    float&    operator [] (int i)  { return El(i); }
    float     operator [] (int i) const   { return Val(i); }
    void      Set(int i, float f)  { El(i) = f; }
    void      Copy(const floatRef& fr)
		{ assert(len>0 || fr.len==0); // catches assignmt to `new' frs
		  int i, n = min(len, fr.len);
		  for(i=0; i<n; ++i)  {  _El(i) = fr.Val(i); } }
    int       CopyFrom(int start, const floatRef& fr) {  // read from middle
	          int i, n = min(fr.len-start, len);
		  for(i=0; i<n; ++i) {  _El(i) = fr.Val(i+start);} 
                  return n; }
    void      CloneRefFrom(int start, const floatRef& fr)  
                                            { start = min(fr.len, start);
					      len = fr.len-start; 
					      step = fr.step;
					      data = fr.data + step*start; }
    int       Size(void) const     { return len; }
    int       size(void) const     { return len; }	// GNU Vec compat
    int       Len(void)	const	   { return len; }
    int       Step(void) const     { return step; }
    float*    Data(void) const	   { return data; }

    void      SetLen(int l)	   { len = l; }
    void      SetData(float *d)	   { data = d; }
    void      SetStep(int s)	   { step = s; }

    void      Fill(double d)  { int i; for(i=0;i<len;++i) El(i)=d; }
    void      Clear(void)    { Fill(0); }

    void      Add(const floatRef& fr) 
                  { int i; for(i=0;i<len;++i) El(i)+=fr.Val(i); }
    void      Sub(const floatRef& fr) 
                  { int i; for(i=0;i<len;++i) El(i)-=fr.Val(i); }
    void      Mul(const floatRef& fr) 
                  { int i; for(i=0;i<len;++i) El(i)*=fr.Val(i); }
    void      Div(const floatRef& fr) 
                  { int i; for(i=0;i<len;++i) El(i)/=fr.Val(i); }
    void      Scale(double f)   { int i; for(i=0;i<len;++i) El(i)*=f; }
    void      Offset(double f)  { int i; for(i=0;i<len;++i) El(i)+=f; }

    floatRef& operator *= (const floatRef& fr)
              { Mul(fr); return *this; }
    floatRef& operator *= (const double d)
              { Scale(d); return *this; }

};

class floatVec : public floatRef {	// floatRef with allocation
public:
    floatVec(int l=0) : floatRef(l)
              { data = new float[len]; 
		/* Clear(); */ memset(data, 0, len*sizeof(float)); }
    floatVec(const floatVec& fv)
              { len = fv.Size(); data = new float[len]; 
		/* Copy(fv); */ memcpy(data, fv.data, len*sizeof(float)); }
    // a floatVec casts to a floatRef, but I think we need the above
    // to pre-empt the copy constructor.  Not sure if adding a floatRef
    // special case works, tho'
    floatVec(const floatRef& fr)
              { len = fr.Size(); data = new float[len]; floatRef::Copy(fr); }
    ~floatVec(void)
              { delete [] data; }
    floatVec& operator = (const floatRef& fr)    // assignment to fv clones
              { if((floatRef*)this != &fr) CloneVec(fr); return *this; }
    // g++ uses builtin copy assignment in preference to above - be explicit
    floatVec& operator = (const floatVec& fv)    // assignment to fv clones
              { if(this != &fv) CloneVec(fv); return *this; }
    void    Copy(const floatVec& fv)   // can copy faster if *know* a vec
              { int n = min(len, fv.len);
	        memcpy(data, fv.data, n*sizeof(float)); }
    void    CloneVec(const floatRef& fr)
              { delete [] data; len = fr.Size(); 
		data = new float[len]; Copy(fr); }
    void    Resize(int l)
              { floatVec tmp(l); tmp.Fill(0); tmp.Copy(*this); _Swap(tmp); }
    void    resize(int l)		// GNU vec compatibility
              { floatVec tmp(l); tmp.Fill(0); tmp.Copy(*this); _Swap(tmp); }
};

#endif /* ndef _FLOATVEC_H */
