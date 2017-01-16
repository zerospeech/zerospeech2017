// -*- C++ -*- std header.	

#ifndef __ICSIARRAY__
#define __ICSIARRAY__

#include<error.h>
#include<string.h>
#include<stdio.h>
#include<iostream>

#define SIZE_CHECKED(_i1,_i2) (((_i1).size()==(_i2).size()) ? (_i1).size() : (error("icsiarray: sizes do not match!"), 0))



// for use with standard types only
template<class T> class icsiarray {
public:
//    typedef T value_type;
 
    icsiarray()
	{
	    //cerr << "icsiarray()\n";
	    //thesize = 0;
	    set_size(0);
	    thearray = NULL;
	}
    icsiarray(size_t sz, const T& v= T())
	{
	    //cerr << "icsiarray(size_t sz, const T& v= T())\n";
	    //thesize = 0;
	    set_size(0);
	    thearray = NULL;
	    resize_nocopy(sz);
	    for (int i = 0; i < thesize; i++)
		thearray[i] = v;
	}
    icsiarray(const T* vals, size_t sz)
	{
	    //cerr << "icsiarray(const T* vals, size_t sz)\n";
	    //thesize = 0;
	    set_size(0);
	    thearray = NULL;
	    resize_nocopy(sz);
	    //bjoern (Tue Dec  3 09:36:30 2002)
	    //assert(NULL != thearray);
	    memcpy(thearray, vals, thesize*sizeof(T));
	}
    icsiarray(const icsiarray& s)
	{
	    //cerr << "icsiarray(const icsiarray& s)\n";
	    //thesize = 0;
	    set_size(0);
	    thearray = NULL;
	    resize_nocopy(s.size());
	    //bjoern (Tue Dec  3 09:36:25 2002)
	    //assert(NULL != thearray);
	    memcpy(thearray, s.get_thearray(), thesize*sizeof(T));
	}

    ~icsiarray()
	{
	    //cerr << "~icsiarray()\n";
	    clear();
	}

    const T get (size_t ind) const
	{ 
	    if ((0 > ind) || (ind >= thesize))
		error("access out of bounds (%d not in [0;%d])", ind, thesize-1);
	    return thearray[ind];
	}
    const T& get (size_t ind)
	{ 
	    if ((0 > ind) || (ind >= thesize))
		error("access out of bounds (%d not in [0;%d])", ind, thesize-1);
	    return thearray[ind];
	}

    T operator[] (size_t ind) const
	{ 
	    if ((0 > ind) || (ind >= thesize))
		error("access out of bounds (%d not in [0;%d])", ind, thesize-1);
	    return thearray[ind];
	}    
    T& operator[] (size_t ind)
	{
	    if ((0 > ind) || (ind >= thesize))
		error("access out of bounds (%d not in [0;%d])", ind, thesize-1);
	    return thearray[ind];
	}
    
    void set (size_t ind, const T& v)
	{
	    if ((0 > ind) || (ind >= thesize))
		error("access out of bounds (%d not in [0;%d])", ind, thesize-1);
	    thearray[ind] = v;
	}
    void set (const T& v)
	{
	    for (int i = 0; i < thesize; i++)
		thearray[i] = v;
	}
    void set (const icsiarray& src)
	{
	    if (thesize != src.size())
	    {
		fprintf(stderr, "icsiarray.set(): sizes do not match\n");
		resize_nocopy(src.size());
	    }
	    for (int i = 0; i < thesize; i++)
		thearray[i] = src.get(i);
	}

    void add (const icsiarray& src)
	{
	    int sz = ((thesize == src.size()) ? 
		      thesize : 
		      (error("icsiarray: sizes do not match\n"),0));
	    for (int i = 0; i < sz; i++)
	    {
		thearray[i] += src.get(i);
	    }
	}
    void add (const T& v)
	{
	    for (int i = 0; i < thesize; i++)
	    {
		thearray[i] += v;
	    }
	}

    void sub (const icsiarray& src)
	{
	    int sz = ((thesize == src.size()) ? thesize : (error("icsiarray: sizes do not match\n"),0));
	    for (int i = 0; i < sz; i++)
	    {
		thearray[i] -= src.get(i);
	    }
	}

    void sub (const T& v)
	{
	    for (int i = 0; i < thesize; i++)
	    {
		thearray[i] -= v;
	    }
	}

    void mul (const icsiarray& src)
	{
	    int sz = ((thesize == src.size()) ? thesize : (error("icsiarray: sizes do not match\n"),0));
	    for (int i = 0; i < sz; i++)
	    {
		thearray[i] *= src.get(i);
	    }
	}

    void mul (const T& v)
	{
	    for (int i = 0; i < thesize; i++)
	    {
		thearray[i] *= v;
	    }
	}

    void div (const icsiarray& src)
	{
	    int sz = ((thesize == src.size()) ? thesize : (error("icsiarray: sizes do not match\n"),0));
	    for (int i = 0; i < sz; i++)
	    {
		thearray[i] /= src.get(i);
	    }
	}

    void div (const T& v)
	{
	    for (int i = 0; i < thesize; i++)
	    {
		thearray[i] /= v;
	    }
	}

    void add_prod(const icsiarray& src1, const icsiarray& src2)
	{
	    int sz;
	    sz = ((src1.size() == src2.size() == thesize) ? 
		  thesize : 
		  error("icsiarray: sizes do not match!\n"),0);
	    for (int i = 0; i < sz; i++)
	    {
		thearray[i] += src1.get(i) * src2.get(i);
	    }
	}
    void add_prod(const icsiarray& src1, const T& src2)
	{
	    int sz;
	    sz = ((src1.size() == thesize) ? 
		  thesize : 
		  (error("icsiarray: sizes do not match!\n"),0));
	    for (int i = 0; i < sz; i++)
	    {
		thearray[i] += src1.get(i) * src2;
	    }
	}

    void set_sum(const icsiarray& src1, const icsiarray& src2);
    void set_sum(const icsiarray& src1, const T& src2);
    void set_sum(const T& src1, const icsiarray& src2)
	{
	    set_sum(src2, src1);
	}
    void set_diff(const icsiarray& src1, const icsiarray& src2);
    void set_diff(const icsiarray& src1, const T& src2);
    void set_diff(const T& src1, const icsiarray& src2);
    void set_prod(const icsiarray& src1, const icsiarray& src2);
    void set_prod(const icsiarray& src1, const T& src2);
    void set_prod(const T& src1, const icsiarray& src2)
	{
	    set_prod(src2, src1);
	}
    void set_quot(const icsiarray& src1, const icsiarray& src2);
    void set_quot(const icsiarray& src1, const T& src2);
    void set_quot(const T& src1, const icsiarray& src2);

    void set_size(size_t sz)
	{
	    thesize = sz;
	    //cerr << "size set to " << sz << '\n';
	}

    const size_t size(void)
	{ 
	    return thesize; 
	}
    const size_t size(void) const
	{ 
	    return thesize; 
	}
    int resize(size_t sz, const T c= T())
	{
	    //print_hdr("resize");
	    consistency("resize");
	    
	    // action required?
	    if (sz == thesize)
	    {
		return 0;
	    }

	    // empty array
	    if (0 == sz)
	    {
		clear();
		return 0;
	    }

	    // allocation
	    T* oldarray = thearray;
	    thearray = (T*)malloc(sz * sizeof(T));
	    if (NULL == thearray)
	    {
		fprintf(stderr, "icsiarray.resize(): memory allocation error\n");
		thesize = 0;
		return 1;
	    }

	    // copy and fill remainder
	    size_t elems = (sz > thesize) ? thesize : sz;
	    if (NULL != oldarray)
	    { 
		memcpy(thearray, oldarray, elems * sizeof(T));
		free(oldarray);
	    }
	    for (int i = elems; i < sz; i++)
		thearray[i] = c;
	    
	    //thesize = sz;
	    set_size(sz);
	    return 0;
	}

    T sum()
	{
	    T su = T();
	    for (int i = 0; i < thesize; i++)
		su += thearray[i];
	    return su;
	}

    T max()
	{
	    T ma = T();
	    for (int i = 0; i < thesize; i++)
		ma = (ma >= (thearray[i])) ? ma : thearray[i];
	    return ma;
	}
    T min()
	{
	    T mi = T();
	    for (int i = 0; i < thesize; i++)
		mi = (mi <= (thearray[i])) ? mi : thearray[i];
	    return mi;
	}
    
    T* get_thearray()
	{
	    //cerr << "unsafe: returning pointer to internal\n";
	    return thearray;
	}
    T* get_thearray() const
	{
	    return thearray;
	}
	       
    void print_hdr(const char* pref="lda")
	{
	    //consistency(pref);
	    //cerr << pref << ": " << "size: " << thesize
	    // << " | " << "pointer: " << thearray << '\n';
	}

private:
    T* thearray;
    size_t thesize;

    void resize_nocopy(size_t sz)
	{
	    //print_hdr("resize_nocopy");
	    consistency("resize_nocopy");
	    
	    T* newarray;

	    if (thesize == sz)
		return;

	    if (0 == sz)
	    {
		clear();
		return;
	    }
	    newarray = (T*)malloc(sz * sizeof(T));
	    if (NULL != thearray) free(thearray);

	    if (NULL == newarray)
	    {
		error("icsiarray internal: memory allocation error\n");
		return;
	    }
	    thearray = newarray;
	    //thesize = sz;
	    set_size(sz);
	}

    void clear()
	{
	    consistency("clear");
	    if (NULL != thearray)
	    {
		free(thearray);
	    }
	    thearray = NULL;
	    //thesize = 0;
	    set_size(0);
	}

    void repair()
	{
	    thearray = NULL;
	    set_size(0);
	}

    void consistency(const char* pref="lda")
	{
	    if ((0==thesize) && (NULL!=thearray))
		error("%s: size is 0, but array not NULL\n", pref);
	    if ((0!=thesize) && (NULL==thearray))
		error("%s: array is NULL, but size not 0\n", pref);
	}

//     void print(const char* pref="lda")
// 	{
// 	    consistency("print");
// 	    cerr << pref << ": " << "size: " << thesize << '\n';
// 	    cerr << pref << ": " << "pointer: " << thearray << '\n';
// 	    for (int i=0; i<thesize; i++)
// 		cerr << pref << ": " << i << ": " << thearray[i] << '\n';
// 	}

};

template<class T> void icsiarray<T>::set_sum(const icsiarray& src1, const icsiarray& src2)
{
    int sz = SIZE_CHECKED(src1, src2);

    if (thesize != sz)
	resize_nocopy(sz);

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) + src2.get(i);
    }
}

template<class T> void icsiarray<T>::set_sum(const icsiarray& src1, const T& src2)
{
    if (thesize != src1.size())
	resize_nocopy(src1.size());

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) + src2;
    }
}

template<class T> void icsiarray<T>::set_diff(const icsiarray& src1, const icsiarray& src2)
{
    int sz = SIZE_CHECKED(src1, src2);

    if (thesize != sz)
	resize_nocopy(sz);

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) - src2.get(i);
    }
}

template<class T> void icsiarray<T>::set_diff(const icsiarray& src1, const T& src2)
{
    if (thesize != src1.size())
	resize_nocopy(src1.size());

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) - src2;
    }
}

template<class T> void icsiarray<T>::set_diff(const T& src1, const icsiarray& src2)
{
    if (thesize != src1.size())
	resize_nocopy(src1.size());

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1 - src2.get(i);
    }
}

template<class T> void icsiarray<T>::set_prod(const icsiarray& src1, const icsiarray& src2)
{
    int sz = SIZE_CHECKED(src1, src2);

    if (thesize != sz)
	resize_nocopy(sz);

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) * src2.get(i);
    }
}

template<class T> void icsiarray<T>::set_prod(const icsiarray& src1, const T& src2)
{
    if (thesize != src1.size())
	resize_nocopy(src1.size());

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) * src2;
    }
}

template<class T> void icsiarray<T>::set_quot(const icsiarray& src1, const icsiarray& src2)
{
    int sz = SIZE_CHECKED(src1, src2);

    if (thesize != sz)
	resize_nocopy(sz);

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) / src2.get(i);
    }
}

template<class T> void icsiarray<T>::set_quot(const icsiarray& src1, const T& src2)
{
    if (thesize != src1.size())
	resize_nocopy(src1.size());

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1.get(i) / src2;
    }
}

template<class T> void icsiarray<T>::set_quot(const T& src1, const icsiarray& src2)
{
    if (thesize != src1.size())
	resize_nocopy(src1.size());

    for (int i = 0; i < thesize; i++)
    {
	thearray[i] = src1 / src2.get(i);
    }
}


#endif // #ifndef ICSIARRAY



