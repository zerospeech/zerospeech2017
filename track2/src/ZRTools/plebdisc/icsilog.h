/*
    ICSI header file of the logarithm function based on a lookup table
    icsi_log source code and fill_icsi_log_table header

    Version 0.6 beta
    Build date: November 13th, 2007

    Copyright (C) 2007 International Computer Science Institute
    1947 Center Street. Suite 600
    Berkeley, CA 94704
    
    Contact information: 
         Oriol Vinyals	vinyals@icsi.berkeley.edu
         Gerald Friedland 	fractor@icsi.berkeley.edu

    Acknowledgements:
    Thanks to Harrison Ainsworth (hxa7241@gmail.com) for his idea that
    doubled the accuracy.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __ICSI_LOG__
#define __ICSI_LOG__

#define UNION_CAST(x, destType)	\
   (((union {__typeof__(x) a; destType b;})x).b)

/*
This method fills a given array of floats with the information necessary to compute the icsi_log. This method has to be called before any call to icsi_log.
Parameters:
	n is the number of bits used from the mantissa (0<=n<=23). Higher n means higher accuracy but slower execution. We found that a good value for n is 14.
	lookup_table requires a float* pointing to a continuous (preallocated) memory array of 2^n*sizeof(float) bytes.
Return values: void
*/

void fill_icsi_log_table(const int n,float *lookup_table);
void fill_icsi_log_table2(const unsigned precision, float* const   pTable);

/*
This method computes the icsi_log. A fast approximation of the log() function with adjustable accuracy.
Parameters:
      val should be an IEEE 753 float with a value in the interval ]0,+inf[. A value smaller or equal zero results in undefined behaviour. 
	lookup_table requires a float* pointing to the table created by fill_icsi_log_table.
	n is the number of bits used from the mantissa (0<=n<=23). Higher n means higher accuracy but slower execution. We found that a good value for n is 14.
Return values: approximation of the natural logarithm of val
*/

inline float icsi_log(float val,register const float *lookup_table,register const int n)
{
//   register int *const     exp_ptr = ((int*)&val);
//   register int            x = *exp_ptr; /*x is the float treated as an integer*/

   union _ {int i; float f;};
   register int x = ((union _ *)&val)->i;
   register const int      log_2 = ((x >> 23) & 255) - 127; /*this is the exponent part*/
   x &= 0x7FFFFF;
   x = x >> (23-n); /*this is the index we should retrieve*/
   val = lookup_table[x];
   return ((val + log_2)* 0.69314718); /*natural logarithm*/
}

/* ICSIlog v2.0 */
inline float icsi_log_v2(const float val, register float* const pTable, register const unsigned precision)
{
   /* get access to float bits */
   //register const int* const pVal = (const int*)(&val);
   union _ {int i; float f;};
   register const int* const pVal = &((union _ *)&val)->i;
   
   /* extract exponent and mantissa (quantized) */
   register const int exp = ((*pVal >> 23) & 255) - 127;
   register const int man = (*pVal & 0x7FFFFF) >> (23 - precision);
   
   /* exponent plus lookup refinement */
   return ((float)(exp) + pTable[man]) * 0.69314718055995f;
}

#endif	

