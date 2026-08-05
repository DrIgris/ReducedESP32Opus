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


