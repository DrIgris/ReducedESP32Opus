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


#ifndef ENT_CODE_H
#define ENT_CODE_H

#include <stdint.h>
#include "opus_vars.h"

typedef uint32_t ec_window;

# define EC_WINDOW_SIZE ((int)sizeof(ec_window)*CHAR_BIT)

typedef struct ec_ctx ec_ctx;
typedef struct ec_ctx ec_dec;

/*The number of bits to use for the range-coded part of unsigned integers.*/
# define EC_UINT_BITS (8)

/*The resolution of fractional-precision bit usage measurements, i.e.,
   3 => 1/8th bits.*/
# define BITRES 3

/*The entropy encoder/decoder context.
  We use the same structure for both, so that common functions like ec_tell()
   can be used on either one.*/
struct ec_ctx{
   /*Buffered input/output.*/
   unsigned char *buf;
   /*The size of the buffer.*/
   uint32_t storage;
   /*The offset at which the last byte containing raw bits was read/written.*/
   uint32_t end_offs;
   /*Bits that will be read from/written at the end.*/
   ec_window end_window;
   /*Number of valid bits in end_window.*/
   int nend_bits;
   /*The total number of whole bits read/written.
     This does not include partial bits currently in the range coder.*/
   int nbits_total;
   /*The offset at which the next range coder byte will be read/written.*/
   uint32_t offs;
   /*The number of values in the current range.*/
   uint32_t rng;
   /*In the decoder: the difference between the top of the current range and
      the input value, minus one.
     In the encoder: the low end of the current range.*/
   uint32_t val;
   /*In the decoder: the saved normalization factor from ec_decode().
     In the encoder: the number of oustanding carry propagating symbols.*/
   uint32_t ext;
   /*A buffered input/output symbol, awaiting carry propagation.*/
   int rem;
   /*Nonzero if an error occurred.*/
   int error;
};

/*Initializes the decoder.
  _buf: The input buffer to use.
  Return: 0 on success, or a negative value on error.*/
void ec_dec_init(ec_dec *_this,unsigned char *_buf,uint32_t _storage);



static inline uint32_t ec_range_bytes(ec_ctx *_this){
  return _this->offs;
}

static inline unsigned char *ec_get_buffer(ec_ctx *_this){
  return _this->buf;
}

static inline int ec_get_error(ec_ctx *_this){
  return _this->error;
}

/*Returns the number of bits "used" by the encoded or decoded symbols so far.
  This same number can be computed in either the encoder or the decoder, and is
   suitable for making coding decisions.
  Return: The number of bits.
          This will always be slightly larger than the exact value (e.g., all
           rounding error is in the positive direction).*/
static inline int ec_tell(ec_ctx *_this){
  return _this->nbits_total-EC_ILOG(_this->rng);
}


/*Calculates the cumulative frequency for the next symbol.
  This can then be fed into the probability model to determine what that
   symbol is, and the additional frequency information required to advance to
   the next symbol.
  This function cannot be called more than once without a corresponding call to
   ec_dec_update(), or decoding will not proceed correctly.
  _ft: The total frequency of the symbols in the alphabet the next symbol was
        encoded with.
  Return: A cumulative frequency representing the encoded symbol.
          If the cumulative frequency of all the symbols before the one that
           was encoded was fl, and the cumulative frequency of all the symbols
           up to and including the one encoded is fh, then the returned value
           will fall in the range [fl,fh).*/
unsigned ec_decode(ec_dec *_this,unsigned _ft);

/*Equivalent to ec_decode() with _ft==1<<_bits.*/
unsigned ec_decode_bin(ec_dec *_this,unsigned _bits);

/*Advance the decoder past the next symbol using the frequency information the
   symbol was encoded with.
  Exactly one call to ec_decode() must have been made so that all necessary
   intermediate calculations are performed.
  _fl:  The cumulative frequency of all symbols that come before the symbol
         decoded.
  _fh:  The cumulative frequency of all symbols up to and including the symbol
         decoded.
        Together with _fl, this defines the range [_fl,_fh) in which the value
         returned above must fall.
  _ft:  The total frequency of the symbols in the alphabet the symbol decoded
         was encoded in.
        This must be the same as passed to the preceding call to ec_decode().*/
void ec_dec_update(ec_dec *_this,unsigned _fl,unsigned _fh,unsigned _ft);



/*Returns the number of bits "used" by the encoded or decoded symbols so far.
  This same number can be computed in either the encoder or the decoder, and is
   suitable for making coding decisions.
  Return: The number of bits scaled by 2**BITRES.
          This will always be slightly larger than the exact value (e.g., all
           rounding error is in the positive direction).*/
uint32_t ec_tell_frac(ec_ctx *_this);

/* Decode a bit that has a 1/(1<<_logp) probability of being a one */
int ec_dec_bit_logp(ec_dec *_this,unsigned _logp);


/*Extracts a raw unsigned integer with a non-power-of-2 range from the stream.
  The bits must have been encoded with ec_enc_uint().
  No call to ec_dec_update() is necessary after this call.
  _ft: The number of integers that can be decoded (one more than the max).
       This must be at least one, and no more than 2**32-1.
  Return: The decoded bits.*/
uint32_t ec_dec_uint(ec_dec *_this,uint32_t _ft);

/*Extracts a sequence of raw bits from the stream.
  The bits must have been encoded with ec_enc_bits().
  No call to ec_dec_update() is necessary after this call.
  _ftb: The number of bits to extract.
        This must be between 0 and 25, inclusive.
  Return: The decoded bits.*/
uint32_t ec_dec_bits(ec_dec *_this,unsigned _ftb);

/*Decodes a symbol given an "inverse" CDF table.
  No call to ec_dec_update() is necessary after this call.
  _icdf: The "inverse" CDF, such that symbol s falls in the range
          [s>0?ft-_icdf[s-1]:0,ft-_icdf[s]), where ft=1<<_ftb.
         The values must be monotonically non-increasing, and the last value
          must be 0.
  _ftb: The number of bits of precision in the cumulative distribution.
  Return: The decoded symbol s.*/
int ec_dec_icdf(ec_dec *_this,const unsigned char *_icdf,unsigned _ftb);

#endif