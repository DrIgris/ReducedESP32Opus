/* Copyright (c) 2007-2012 IETF Trust, CSIRO, Xiph.Org Foundation,
                           Gregory Maxwell. All rights reserved.
   Written by Jean-Marc Valin and Gregory Maxwell */
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

#include "entcode.h"
#include "mfrngcod.h"


static int ec_read_byte(ec_dec *_this){
  return _this->offs<_this->storage?_this->buf[_this->offs++]:0;
}

static int ec_read_byte_from_end(ec_dec *_this){
  return _this->end_offs<_this->storage?
   _this->buf[_this->storage-++(_this->end_offs)]:0;
}

//(((int)sizeof(unsigned)*8)-(__builtin_clz(_this->rng)))

static void ec_dec_normalize(ec_dec *_this){
  /*If the range is too small, rescale it and input some bits.*/
  while(_this->rng<=EC_CODE_BOT){
    int sym;
    _this->nbits_total+=EC_SYM_BITS;
    _this->rng<<=EC_SYM_BITS;
    /*Use up the remaining bits from our last symbol.*/
    sym=_this->rem;
    /*Read the next value from the input.*/
    _this->rem=ec_read_byte(_this);
    /*Take the rest of the bits we need from this new symbol.*/
    sym=(sym<<EC_SYM_BITS|_this->rem)>>(EC_SYM_BITS-EC_CODE_EXTRA);
    /*And subtract them from val, capped to be less than EC_CODE_TOP.*/
    _this->val=((_this->val<<EC_SYM_BITS)+(EC_SYM_MAX&~sym))&(EC_CODE_TOP-1);
  }
}

void ec_dec_init(ec_dec *_this,unsigned char *_buf,uint32_t _storage){
  _this->buf=_buf;
  _this->storage=_storage;
  _this->end_offs=0;
  _this->end_window=0;
  _this->nend_bits=0;
  /*This is the offset from which ec_tell() will subtract partial bits.
    The final value after the ec_dec_normalize() call will be the same as in
     the encoder, but we have to compensate for the bits that are added there.*/
  _this->nbits_total=EC_CODE_BITS+1
   -((EC_CODE_BITS-EC_CODE_EXTRA)/EC_SYM_BITS)*EC_SYM_BITS;
  _this->offs=0;
  _this->rng=1U<<EC_CODE_EXTRA;
  _this->rem=ec_read_byte(_this);
  _this->val=_this->rng-1-(_this->rem>>(EC_SYM_BITS-EC_CODE_EXTRA));
  _this->error=0;
  /*Normalize the interval.*/
  ec_dec_normalize(_this);
}

unsigned ec_decode(ec_dec *_this,unsigned _ft){
  unsigned s;
  _this->ext=_this->rng/_ft; //splits rng into equal parts determined by the param. this creates equal probability splits
  s=(unsigned)(_this->val/_this->ext); //int division to find which split val is in; in simpler numbers lets say rng = 20 ft = 4 so ext = 5. if val is 0-4, 0->4 / 5 = 0, 5->9/5 = 1. 20/5 = 4etc.
  return _ft-EC_MINI(s+1,_ft); //s in an index so we add 1 to get to proper arithmetic. EC_MINI simply clamps the range to be between 0 and ft.
  /*
  the return flips the index. so if s is the highest pocket, the EC_MINI will return ft, ft-ft=0 so the highest slot is index 0.
  this is because range coding puts highest probability at the top of the range.
  [ 1 2 3 4 ] [ 5 6 7 8 ] [ 9 10 11 12 ] [ 13 14 15 16 ] [ 17 18 19 20 ]
      ^0           ^1           ^2              ^3              ^4
      ft = 4 so if s = 4 EC_MINI(5,4) = 4.
      4-4 = 0
      ft = 4 so if s = 1 EC_MINI(2,4) = 2
      4-2 = 2
  */
  /*
  EC_MINI is pretty beautiful here, the main component comes in this bitwise & section
  a+((b-a)&-(b < a))
  b < a gives 1 if true, 0 if false. Then we negate it. so either we get all 0s 00000000, or all 1s 11111111 (because 2 complement)
  if b<a is false (b is greater than a) we have whatever binary val b-a is & 00000000 which clears all bits so it returns a + 0 = a
  if b<a is true we have whatever binary val b-a is & 11111111 which leaves it unchanged so it returns a + b-a = b
  */
}

unsigned ec_decode_bin(ec_dec *_this,unsigned _bits){
   unsigned s;
   _this->ext=_this->rng>>_bits;
   s=(unsigned)(_this->val/_this->ext);
   return (1U<<_bits)-EC_MINI(s+1U,1U<<_bits);
}

void ec_dec_update(ec_dec *_this,unsigned _fl,unsigned _fh,unsigned _ft){
  //fl is the lower bound of the slice val fell in, fh is the non-inclusive upper bound (the start of the next slice to be exact in most cases)
  uint32_t s;
  s=(_this->ext * _ft-_fh); //calculates how many slices are in our range, then multiplies by ext which is the size of each slice to get the total size in the range coders abstract units
  _this->val-=s; //Changes val to be the offset from the start of the slice
  _this->rng=_fl>0?(_this->ext * _fh-_fl):_this->rng-s; //if we are not at the bottom slice, the range is just the number of units that our fl -> fh range exists in
  /*
  if we are at the bottom, we instead offset range from the first slice like we did to val. 
  This is because integer division truncates certain end values that accumalte in the first slice. 
  Creating a minute difference in size that can cause shifting over decoding if not accounted for. 
  */
  ec_dec_normalize(_this);
}


int ec_dec_bit_logp(ec_dec *_this,unsigned _logp){
  uint32_t r;
  uint32_t d;
  uint32_t s;
  int         ret;
  r=_this->rng;
  d=_this->val;
  s=r>>_logp;
  ret=d<s;
  if(!ret)_this->val=d-s;
  _this->rng=ret?s:r-s;
  ec_dec_normalize(_this);
  return ret;
}

uint32_t ec_dec_uint(ec_dec *_this,uint32_t _ft){
  unsigned ft;
  unsigned s;
  int      ftb;
  /*In order to optimize EC_ILOG(), it is undefined for the value 0.*/
  celt_assert(_ft>1);
  _ft--; //ft is the size of the range, the total number of possible values, so we have to decrement before calling ecILOG to get the relative pos ind
  ftb=EC_ILOG(_ft); //ec_ilog is the number of bits needed to represent ft, so if ft is 0b0000000000000010011 ftb=5
  /*
  to look at why the decrement is important, if we wanted to decode a total of 8 possible values ft=8 which would be 0b0000001000
  if we didn't decrement ft= 0b0000001000 EC_ILOG = 4, so we need 4 bits to represent 8 values
  but we only need 3 bits to represent 8 values (0-7) so we decrement ft to get ft=0b0000000111 EC_ILOG = 3 which is correct
  */
  if(ftb>EC_UINT_BITS){ //The # of bits needed to decode this many values is greater than can be done in one operation
    uint32_t t;
    ftb-=EC_UINT_BITS; //We know that we would need the entire range for decoding part of it
    ft=(unsigned)(_ft>>ftb)+1; // Shifts to grab the top bits of our ft range to decode first
    s=ec_decode(_this,ft);
    ec_dec_update(_this,s,s+1,ft);
    t=(uint32_t)s<<ftb|ec_dec_bits(_this,ftb); //one operation to decode the lower bits and combine with the decoded top bits
    if(t<=_ft)return t;
    _this->error=1;
    return _ft;
  }
  else{ //just decodes normally in one step
    _ft++;
    s=ec_decode(_this,(unsigned)_ft);
    ec_dec_update(_this,s,s+1,(unsigned)_ft);
    return s;
  }
}

uint32_t ec_dec_bits(ec_dec *_this,unsigned _bits){
  ec_window   window;
  int         available;
  uint32_t ret;
  window=_this->end_window;
  available=_this->nend_bits;
  if((unsigned)available<_bits){
    do{
      window|=(ec_window)ec_read_byte_from_end(_this)<<available;
      available+=EC_SYM_BITS;
    }
    while(available<=EC_WINDOW_SIZE-EC_SYM_BITS);
  }
  ret=(uint32_t)window&(((uint32_t)1<<_bits)-1U);
  window>>=_bits;
  available-=_bits;
  _this->end_window=window;
  _this->nend_bits=available;
  _this->nbits_total+=_bits;
  return ret;
}

int ec_dec_icdf(ec_dec *_this,const unsigned char *_icdf,unsigned _ftb){ 
  /*
  I believe the whole idea is that we use this icdf table to make the encoder efficient. 
  The encoder can encode the most likely symbols with smaller #s of bits, 
  since the table creates the largest range at the beginning as it is [2,1,0] and the cost of encoding a bit is log2(rng/width of region)
  */
  uint32_t r;
  uint32_t d;
  uint32_t s;
  uint32_t t;
  int         ret;
  s=_this->rng;
  d=_this->val;
  r=s>>_ftb; //creating 2^ftb equal segments
  ret=-1;
  do{ /*
    looping through the segments, saving t=s to keep as an upper bound
    then calculating our new lower bound s with r, our slices, and a value from a ICDF table
    which is a line of values essentially having a negative slope of probability mass. As in, we can see CDFs (especially discrete like this)
    with an increasing slope up. Where after every check, the probability mass in the next section is cumulative including both
    the current probability mass and the last we just checked. Our table is reversed wherein we start at the highest prob and slowly remove
    the probability mass of the last option as.
    after each caculation we check if our val is less than our new upper bound when it is we found our section with a range of [s, t)
    */
    t=s;
    s=(r*_icdf[++ret]); 
  }
  while(d<s);
  _this->val=d-s; //rebases val to be respective to the start of our range
  _this->rng=t-s; // rebases range to be [0, t-s) instead of [s, t)
  ec_dec_normalize(_this);
  return ret;
}

uint32_t ec_tell_frac(ec_ctx *_this){
  uint32_t nbits;
  uint32_t r;
  int         l;
  int         i;
  /*To handle the non-integral number of bits still left in the encoder/decoder
     state, we compute the worst-case number of bits of val that must be
     encoded to ensure that the value is inside the range for any possible
     subsequent bits.
    The computation here is independent of val itself (the decoder does not
     even track that value), even though the real number of bits used after
     ec_enc_done() may be 1 smaller if rng is a power of two and the
     corresponding trailing bits of val are all zeros.
    If we did try to track that special case, then coding a value with a
     probability of 1/(1<<n) might sometimes appear to use more than n bits.
    This may help explain the surprising result that a newly initialized
     encoder or decoder claims to have used 1 bit.*/
  nbits=_this->nbits_total<<BITRES;
  l=EC_ILOG(_this->rng);
  r=_this->rng>>(l-16);
  for(i=BITRES;i-->0;){
    int b;
    r=r*r>>15;
    b=(int)(r>>16);
    l=l<<1|b;
    r>>=b;
  }
  return nbits-l;
}


