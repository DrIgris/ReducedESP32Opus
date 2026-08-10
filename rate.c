/* Copyright (c) 2007-2012 IETF Trust, CSIRO, Xiph.Org Foundation. All rights reserved.
   Written by Jean-Marc Valin */
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

#include "modes.h"
#include "rate.h"
#include "entcode.h"
#include "stack.h"

static const unsigned char LOG2_FRAC_TABLE[24]={
   0,
   8,13,
  16,19,21,23,
  24,26,27,28,29,30,31,32,
  32,33,34,34,35,36,36,37,37
};

int compute_allocation(const CELTMode *m, int start, int end, const int *offsets, const int *cap, int alloc_trim, int *intensity, int *dual_stereo,
      int32_t total, int32_t *balance, int *pulses, int *ebits, int *fine_priority, int C, int LM, ec_ctx *ec, int encode, int prev)
{
   int lo, hi, len, j;
   int codedBands;
   int skip_start;
   int skip_rsv;
   int intensity_rsv;
   int dual_stereo_rsv;
   VARDECL(int, bits1);
   VARDECL(int, bits2);
   VARDECL(int, thresh);
   VARDECL(int, trim_offset);
   SAVE_STACK;

   total = IMAX(total, 0);//clamp total to be at least 0 | In celt decoder this is the remaining bits - 1 in fractional format
   len = m->nbEBands;
   skip_start = start;
   /* Reserve a bit to signal the end of manually skipped bands. */
   skip_rsv = total >= 1<<BITRES ? 1<<BITRES : 0; //We are checking 1<<BITRES because we are working in fractional bits. this is actually checking only one bit
   total -= skip_rsv;
   /* Reserve bits for the intensity and dual stereo parameters. */
   intensity_rsv = dual_stereo_rsv = 0;
   if (C==2) //This will always be true for us
   {
      intensity_rsv = LOG2_FRAC_TABLE[end-start]; //get the # of reserved bits from a table that depends on the range of bands we decode
      if (intensity_rsv>total) //if the # of bits required is more than available we skip intensity
         intensity_rsv = 0;
      else
      {
         total -= intensity_rsv;
         dual_stereo_rsv = total>=1<<BITRES ? 1<<BITRES : 0;
         total -= dual_stereo_rsv;
      }
   }
   ALLOC(bits1, len, int);
   ALLOC(bits2, len, int);
   ALLOC(thresh, len, int);
   ALLOC(trim_offset, len, int);

   for (j=start;j<end;j++) //loop through each band
   {
      /* Below this threshold, we're sure not to allocate any PVQ bits */
      thresh[j] = IMAX((C)<<BITRES, (3*(m->eBands[j+1]-m->eBands[j])<<LM<<BITRES)>>4);
      /* Tilt of the allocation curve */
      trim_offset[j] = C*(m->eBands[j+1]-m->eBands[j])*(alloc_trim-5-LM)*(end-j-1)
            *(1<<(LM+BITRES))>>6;
      /* Giving less resolution to single-coefficient bands because they get
         more benefit from having one coarse value per coefficient*/
      if ((m->eBands[j+1]-m->eBands[j])<<LM==1)
         trim_offset[j] -= C<<BITRES;
   }
   lo = 1;
   hi = m->nbAllocVectors - 1;
   do
   {
      int done = 0;
      int psum = 0;
      int mid = (lo+hi) >> 1;
      for (j=end;j-->start;)
      {
         int bitsj;
         int N = m->eBands[j+1]-m->eBands[j];
         bitsj = C*N*m->allocVectors[mid*len+j]<<LM>>2;
         if (bitsj > 0)
            bitsj = IMAX(0, bitsj + trim_offset[j]);
         bitsj += offsets[j];
         if (bitsj >= thresh[j] || done)
         {
            done = 1;
            /* Don't allocate more than we can actually use */
            psum += IMIN(bitsj, cap[j]);
         } else {
            if (bitsj >= C<<BITRES)
               psum += C<<BITRES;
         }
      }
      if (psum > total)
         hi = mid - 1;
      else
         lo = mid + 1;
      /*printf ("lo = %d, hi = %d\n", lo, hi);*/
   }
   while (lo <= hi);
   hi = lo--;
   /*printf ("interp between %d and %d\n", lo, hi);*/
   for (j=start;j<end;j++)
   {
      int bits1j, bits2j;
      int N = m->eBands[j+1]-m->eBands[j];
      bits1j = C*N*m->allocVectors[lo*len+j]<<LM>>2;
      bits2j = hi>=m->nbAllocVectors ?
            cap[j] : C*N*m->allocVectors[hi*len+j]<<LM>>2;
      if (bits1j > 0)
         bits1j = IMAX(0, bits1j + trim_offset[j]);
      if (bits2j > 0)
         bits2j = IMAX(0, bits2j + trim_offset[j]);
      if (lo > 0)
         bits1j += offsets[j];
      bits2j += offsets[j];
      if (offsets[j]>0)
         skip_start = j;
      bits2j = IMAX(0,bits2j-bits1j);
      bits1[j] = bits1j;
      bits2[j] = bits2j;
   }
   codedBands = interp_bits2pulses(m, start, end, skip_start, bits1, bits2, thresh, cap,
         total, balance, skip_rsv, intensity, intensity_rsv, dual_stereo, dual_stereo_rsv,
         pulses, ebits, fine_priority, C, LM, ec, encode, prev);
   RESTORE_STACK;
   return codedBands;
}