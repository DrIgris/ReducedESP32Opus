/* Copyright (c) 2007-2012 IETF Trust, CSIRO, Xiph.Org Foundation,
                           Timothy B. Terriberry. All rights reserved.
   Written by Timothy B. Terriberry and Jean-Marc Valin */
/*

   This file is extracted from RFC6716. Please see that RFC for additional
   information.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of Internet Society, IETF or IETF Trust, nor the
   names of specific contributors, may be used to endorse or promote
   products derived from this software without specific prior written
   permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cwrs.h"

/*Computes the next row/column of any recurrence that obeys the relation
   u[i][j]=u[i-1][j]+u[i][j-1]+u[i-1][j-1].
  _ui0 is the base case for the new row/column.*/
static inline void unext(uint32_t *_ui,unsigned _len,uint32_t _ui0){
  uint32_t ui1;
  unsigned      j;
  /*This do-while will overrun the array if we don't have storage for at least
     2 values.*/
  j=1; do {
    ui1=((_ui[j] + _ui[j-1]) + _ui0);
    _ui[j-1]=_ui0;
    _ui0=ui1;
  } while (++j<_len);
  _ui[j-1]=_ui0;
}


/*Computes the previous row/column of any recurrence that obeys the relation
   u[i-1][j]=u[i][j]-u[i][j-1]-u[i-1][j-1].
  _ui0 is the base case for the new row/column.*/
static inline void uprev(uint32_t *_ui,unsigned _n,uint32_t _ui0){
  uint32_t ui1;
  unsigned      j;
  /*This do-while will overrun the array if we don't have storage for at least
     2 values.*/
  j=1; do {
    ui1=((_ui[j] - _ui[j-1]) - _ui0);
    _ui[j-1]=_ui0;
    _ui0=ui1;
  } while (++j<_n);
  _ui[j-1]=_ui0;
}

/*Compute V(_n,_k), as well as U(_n,0..._k+1).
  _u: On exit, _u[i] contains U(_n,i) for i in [0..._k+1].*/
static uint32_t ncwrs_urow(unsigned _n,unsigned _k,uint32_t *_u){
  uint32_t um2;
  unsigned      len;
  unsigned      k;
  len=_k+2;
  /*We require storage at least 3 values (e.g., _k>0).*/
  celt_assert(len>=3);
  _u[0]=0;
  _u[1]=um2=1;
 {
    /*If _n==0, _u[0] should be 1 and the rest should be 0.*/
    /*If _n==1, _u[i] should be 1 for i>1.*/
    celt_assert(_n>=2);
    /*If _k==0, the following do-while loop will overflow the buffer.*/
    celt_assert(_k>0);
    k=2;
    do _u[k]=(k<<1)-1;
    while(++k<len);
    for(k=2;k<_n;k++)unext(_u+1,_k+1,1);
  }
  return _u[_k]+_u[_k+1];
}


/*Returns the _i'th combination of _k elements chosen from a set of size _n
   with associated sign bits.
  _y: Returns the vector of pulses.
  _u: Must contain entries [0..._k+1] of row _n of U() on input.
      Its contents will be destructively modified.*/
static void cwrsi(int _n,int _k,uint32_t _i,int *_y,uint32_t *_u){
    int j;
    celt_assert(_n>0);
    j=0;
    do{
        uint32_t p;
        int           s;
        int           yj;
        p=_u[_k+1];
        s=-(_i>=p);
        _i-=p&s;
        yj=_k;
        p=_u[_k];
        while(p>_i)p=_u[--_k];
        _i-=p;
        yj-=_k;
        _y[j]=(yj+s)^s;
        uprev(_u,_k+2,0);
    }
    while(++j<_n);
}



void decode_pulses(int *_y,int _n,int _k,ec_dec *_dec)
{
    celt_assert(_k>0);
    VARDECL(uint32_t,u);
    SAVE_STACK;
    ALLOC(u,_k+2U,uint32_t);
    cwrsi(_n,_k,ec_dec_uint(_dec,ncwrs_urow(_n,_k,u)),_y,u);
    RESTORE_STACK;
}