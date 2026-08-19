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

#include "laplace.h"

/* The minimum probability of an energy delta (out of 32768). */
#define LAPLACE_LOG_MINP (0)
#define LAPLACE_MINP (1<<LAPLACE_LOG_MINP)
/* The minimum number of guaranteed representable energy deltas (in one
    direction). */
#define LAPLACE_NMIN (16)

static unsigned ec_laplace_get_freq1(unsigned fs0, int decay)
{
   unsigned ft;
   ft = 32768 - LAPLACE_MINP*(2*LAPLACE_NMIN) - fs0;
   return ft*(16384-decay)>>15;
}

int ec_laplace_decode(ec_dec *dec, unsigned fs, int decay) //fs is probability of 0, decay is how fast probability drops off
{
   int val=0;
   unsigned fl;
   unsigned fm;
   fm = ec_decode_bin(dec, 15); //gets a section from 2^15 equally probable sections
   fl = 0;
   if (fm >= fs) //if this section is greater than our assigned prob of 0 | essentially a search function through the less probable tails
   {
      val++; // start searching at val=1
      fl = fs;
      fs = ec_laplace_get_freq1(fs, decay)+LAPLACE_MINP;
      /* Search the decaying part of the PDF.*/
      while(fs > LAPLACE_MINP && fm >= fl+2*fs) //laplace is symmetrical so we check two times for +-val
      //loops until we reached the tail where all values remaining share the same low prob or the decoded value fm falls within the fl+2*fs magnitude region
      {
         fs *= 2; //combine the +- prob
         fl += fs; //move past region | also used for the condition, making sure we accumulate the magnitudes we've already searched
         fs = ((fs-2*LAPLACE_MINP)*(int32_t)decay)>>15; //subtracting the two floor prob that were there to safeguard to ensure decay affects the actual fs. then just multiplying the decay in
         fs += LAPLACE_MINP; // readding in the prob floor for safeguarding
         val++; //incrementing for next search
      }
      /* Everything beyond that has probability LAPLACE_MINP. */
      if (fs <= LAPLACE_MINP) //infite tail end
      {
         int di;
         di = (fm-fl)>>(LAPLACE_LOG_MINP+1); //finds the offset of our decoded value from the end of all other probs we searched, then divides by 2 to get the actual index since we were *2 because of the +- sections combined
         val += di; //updates val to be di out after the last val it was at
         fl += 2*di*LAPLACE_MINP; //updating fl to reflect our distance out on the tail
      }
      if (fm < fl+fs) // our mag region is [fl, fl+2*fs) half is neg half is pos, simply checking the value at the split of fl+fs
         val = -val;
      else
         fl += fs; //since the mag is split by the two regions [fl       fl+fs][fl+fs       fl+2*fs] if we are in the second pos half we add fs to fl to get the correct position
   } //no if then val = 0 and we can just assert and update the decoder
   celt_assert(fl<32768);
   celt_assert(fs>0);
   celt_assert(fl<=fm);
   celt_assert(fm<IMIN(fl+fs,32768));
   ec_dec_update(dec, fl, IMIN(fl+fs,32768), 32768); //updates val and range and normalizes
   return val;
}