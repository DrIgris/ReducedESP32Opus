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
#include <stdlib.h>
#include <stdint.h>
#include "celt.h"

static const signed char tf_select_table[4][8] = {
      {0, -1, 0, -1,    0,-1, 0,-1},
      {0, -1, 0, -2,    1, 0, 1,-1},
      {0, -2, 0, -3,    2, 0, 1,-1},
      {0, -2, 0, -3,    3, 0, 1,-1},
};


static void tf_decode(int start, int end, int isTransient, int *tf_res, int LM, ec_dec *dec)
{
   int i, curr, tf_select;
   int tf_select_rsv;
   int tf_changed;
   int logp;
   uint32_t budget;
   uint32_t tell;

   budget = dec->storage*8; //similar to laplace decode, we have a budget and must work around it when decoding
   tell = ec_tell(dec);
   logp = isTransient ? 2 : 4;
   tf_select_rsv = LM>0 && tell+logp+1<=budget;
   budget -= tf_select_rsv;
   tf_changed = curr = 0;
   for (i=start;i<end;i++) //since fullband celt this is every band
   {
      if (tell+logp<=budget)
      {
         curr ^= ec_dec_bit_logp(dec, logp); //Here curr can switch between 0 or 1 depending on an even amount of 1s decoded since its an XOR
         tell = ec_tell(dec);
         tf_changed |= curr; //if curr EVER changes throughout the bands
      } //if we dont have budget to decode we just store the last curr we calculated
      tf_res[i] = curr;
      logp = isTransient ? 4 : 5; //for the first logp of the loop we are either at 2 or 4, from every loop on we are 4 or 5
   }
   tf_select = 0;
   if (tf_select_rsv &&
     tf_select_table[LM][4*isTransient+0+tf_changed] !=
     tf_select_table[LM][4*isTransient+2+tf_changed])
   {
      tf_select = ec_dec_bit_logp(dec, 1);
   }
   for (i=start;i<end;i++)
   {
      tf_res[i] = tf_select_table[LM][4*isTransient+2*tf_select+tf_res[i]];
   }
}

static void init_caps(const CELTMode *m,int *cap,int LM,int C)
{
   int i;
   for (i=0;i<m->nbEBands;i++)
   {
      int N;
      N=(m->eBands[i+1]-m->eBands[i])<<LM;
      cap[i] = (m->cache.caps[m->nbEBands*(2*LM+C-1)+i]+64)*C*N>>2;
   }
}

int celt_decoder_get_size(int channels)
{
   const CELTMode *mode = opus_custom_mode_create(SAMPLE_RATE, MAX_FRAME_SIZE, NULL);
   return opus_custom_decoder_get_size(mode, channels);
}

int celt_decoder_init(CELTDecoder *st, int32_t sampling_rate, int channels)
{
   int ret;
   ret = opus_custom_decoder_init(st, opus_custom_mode_create(SAMPLE_RATE, MAX_FRAME_SIZE, NULL), channels); // just sets elements of struct to proper values
   if (ret != OPUS_OK)
      return ret;
   st->downsample = 1; // simple switch statement (ratio of decoder sample rate to chosen output rate (for my use case always 1))
   if (st->downsample==0)
      return OPUS_BAD_ARG;
   else
      return OPUS_OK;
}

int opus_celt_reset_state(CELTDecoder *st) {
   int i;
   float *lpc, *oldBandE, *oldLogE, *oldLogE2;
   lpc = (float*)(st->_decode_mem+(DECODE_BUFFER_SIZE+st->overlap)*st->channels);
   oldBandE = lpc+st->channels*LPC_ORDER;
   oldLogE = oldBandE + 2*st->mode->nbEBands;
   oldLogE2 = oldLogE + 2*st->mode->nbEBands;
   OPUS_CLEAR((char*)&st->DECODER_RESET_START,
         opus_custom_decoder_get_size(st->mode, st->channels)-
         ((char*)&st->DECODER_RESET_START - (char*)st));
   for (i=0;i<2*st->mode->nbEBands;i++)
      oldLogE[i]=oldLogE2[i]=-28.f;
   return OPUS_OK;
}















int celt_decode_with_ec(CELTDecoder * restrict st, const unsigned char *data, int len, float * restrict pcm, int frame_size, ec_dec *dec)
{
   int c, i, N;
   int spread_decision;
   uint32_t bits;
   ec_dec _dec;
   VARDECL(celt_sig, freq);
   VARDECL(celt_norm, X);
   VARDECL(celt_ener, bandE);
   VARDECL(int, fine_quant);
   VARDECL(int, pulses);
   VARDECL(int, cap);
   VARDECL(int, offsets);
   VARDECL(int, fine_priority);
   VARDECL(int, tf_res);
   VARDECL(unsigned char, collapse_masks);
   celt_sig *out_mem[2];
   celt_sig *decode_mem[2];
   celt_sig *overlap_mem[2];
   celt_sig *out_syn[2];
   float *lpc;
   float *oldBandE, *oldLogE, *oldLogE2, *backgroundLogE;

   int shortBlocks;
   int isTransient;
   int intra_ener;
   const int CC = st->channels;
   int LM, M;
   int effEnd;
   int codedBands;
   int alloc_trim;
   int postfilter_pitch;
   float postfilter_gain;
   int intensity=0;
   int dual_stereo=0;
   int32_t total_bits;
   int32_t balance;
   int32_t tell;
   int dynalloc_logp;
   int postfilter_tapset;
   int anti_collapse_rsv;
   int anti_collapse_on=0;
   int silence;
   int C = st->stream_channels;
   ALLOC_STACK;

   frame_size *= st->downsample; //if Fs is less than 48kHz downsample adjusts to fit CELT desired Fs

   c=0; do { //Creates memory pointers for each channel to the appropriate locations in the decode buffer; I can just do this without a loop, but i think for simplicity and non-redundancy I'll leave it.
      decode_mem[c] = st->_decode_mem + c*(DECODE_BUFFER_SIZE+st->overlap);
      out_mem[c] = decode_mem[c]+DECODE_BUFFER_SIZE-MAX_PERIOD;
      overlap_mem[c] = decode_mem[c]+DECODE_BUFFER_SIZE;
   } while (++c<CC);
   lpc = (float*)(st->_decode_mem+(DECODE_BUFFER_SIZE+st->overlap)*CC); //Creating pointer to LPC coefficients in the decode buffer
   oldBandE = lpc+CC*LPC_ORDER;
   oldLogE = oldBandE + 2*st->mode->nbEBands;
   oldLogE2 = oldLogE + 2*st->mode->nbEBands;
   backgroundLogE = oldLogE2  + 2*st->mode->nbEBands;

   for (LM=0;LM<=st->mode->maxLM;LM++) //checking LM values to find desired frame size
      if (st->mode->shortMdctSize<<LM==frame_size)
         break;
   if (LM>st->mode->maxLM)
      return OPUS_BAD_ARG;

   M=1<<LM; //setting Log mode val

   if (len<0 || len>1275 || pcm==NULL)
      return OPUS_BAD_ARG;

   N = M*st->mode->shortMdctSize; // calcutes total samples in frame; should be 960 for me (20ms at 48kHz)

   total_bits = len*8; //total bits in the packet 
   tell = ec_tell(dec); //how many bits we've read, starts at 1 by default. makes sure we don't read more than total bits in the packet

   //QUICK RANGE DECODING EXPLANATION
   /*
   It solely relies on probability. The way it does this is by creating the large range of possible values by normalizing. range becomes >= 2^32.
   Through the normalization process val picks up a certain number of bits (not as in less than 8, it can have many bits spanning multiple bytes)
   using probability and log in silence case we use 15. we create a number that essentially splits the chance. Silence becomes range / 2^15
   since 2^15 is so large the split is very uneven somewhere around 0.003% chance of silence. 
   So we now have two ranges, if we say range = 2^32 our total range is 0-4,294,967,296. our split point is now at 131,072.
   Our accumulation of val (remember multiple bits creating a large 0b number such as 0b11010111010100011100100 = 7055588)
   has to be less than our split of 131,072 to be decoded as silence. Since val has so many bits and the split is so small, and even more importantly range is so big
   it becomes incredibly unlikely that val will be less than the split. This is just a way to use less bits for more likely events.
   There is no actual "probability" when decoding it. There's not a chance that sometimes its silence and sometimes its not.
   The probability is essentially baked into the bitstream on encoding. And you must just decode with an equal probability coefficient.
   */

   if (tell >= total_bits) //if we have read all bits in the packet and nothing meaningful is decoded we say its silence
      silence = 1;
   else if (tell==1) // We are at the beginning of the packet so we read the first bit to determine if its silence or not.
   //this is established to be log 15, not an optional choice
      silence = ec_dec_bit_logp(dec, 15);
   else //if we are not at the beginning of the packet and we have not read all bits in the packet, we are not silence
      silence = 0;
   if (silence) //The packet is silent so we "skip" to the end, setting the decoder's bit read to be at the end of the packet (this is the total length of the packet - how much of it we have already read)
   {
      /* Pretend we've read all the remaining bits */
      tell = len*8;
      dec->nbits_total+=tell-ec_tell(dec);
   }

   postfilter_gain = 0;
   postfilter_pitch = 0;
   postfilter_tapset = 0;
   //The start and end parts of the decoder st is where celt starts and ends its decoding in bandwidth. If this was hybrid start and end would be important. Since we are doing fullband CELT we dont have to check this
   if (st->start==0 && tell+16 <= total_bits) //the second part of this && is skipped if we are near the end of the packet, but importantly, if silence was detected because we set tell = total_bits
   {
      if(ec_dec_bit_logp(dec, 1)) //logp with 1 means 50% chance so essentially reading a bit like "normal" the range is still very large and val is many bits mashed together, but the split point is in the exact middle so its equally likely for val to be on either side
      //what logp reads here is the postfilter flag, so we are just checking if its on
      {
         int qg, octave;
         octave = ec_dec_uint(dec, 6); //uses to get an order of mag 0-5
         postfilter_pitch = (16<<octave)+ec_dec_bits(dec, 4+octave)-1; //ec_dec_bits just reads 4 + octave bits from the stream 
         //16*2^octave + a bin number with a size of 4-9 bits - 1
         qg = ec_dec_bits(dec, 3); //reads 3 bits from the stream to get gain
         if (ec_tell(dec)+2<=total_bits) // if we have 2 bits left in the packet
            postfilter_tapset = ec_dec_icdf(dec, tapset_icdf, 2); //ilter taps to use for the comb filter uses values with table for result
         postfilter_gain = (.09375f)*(qg+1); //quantizes gain to 8 values between 0.09375 and 0.75 QCONST here is just in case fixed point mode is being used
      }
      tell = ec_tell(dec);
   }

   if (LM > 0 && tell+3 <= total_bits) //LM > 0, frames are long enough to have transients, check as well there are 3 bits left in the packet
   {
      isTransient = ec_dec_bit_logp(dec, 3); //decodes transiet flag with 1/2^3 prob, as they are relatively uncommon
      tell = ec_tell(dec);
   }
   else
      isTransient = 0;

   if (isTransient) //sets up decoding for transient frames using smaller short MDCT instead of long
      shortBlocks = M;
   else
      shortBlocks = 0;

   /* Decode the global flags (first symbols in the stream) */
   intra_ener = tell+3<=total_bits ? ec_dec_bit_logp(dec, 3) : 0; // sets intra ener to either the decoded value with 1/8 prob or 0 if there aren't enough bits in the packet
   /* Get band energies */
   unquant_coarse_energy(st->mode, st->start, st->end, oldBandE,
         intra_ener, dec, C, LM);

   ALLOC(tf_res, st->mode->nbEBands, int);
   tf_decode(st->start, st->end, isTransient, tf_res, LM, dec); 
   // bands choose between better time or frequency resolution, the decision for each is decoded and stored in the tf_res var

   tell = ec_tell(dec);
   spread_decision = SPREAD_NORMAL;
   if (tell+4 <= total_bits)
      spread_decision = ec_dec_icdf(dec, spread_icdf, 5); //same dec function in laplace transform using an icdf. Different table and ftb

   ALLOC(pulses, st->mode->nbEBands, int);
   ALLOC(cap, st->mode->nbEBands, int);
   ALLOC(offsets, st->mode->nbEBands, int);
   ALLOC(fine_priority, st->mode->nbEBands, int);

   init_caps(st->mode,cap,LM,C); 
   // uses a preset table and arithmetic to calculate the maximum number of bits to use on each band depending on its index
   //This is because the quality after a certain number of bits isn't noticable and so there is no reason to waste space encoding more

   dynalloc_logp = 6;
   total_bits<<=BITRES;
   tell = ec_tell_frac(dec); // This upcoming loop we are working in fractional bits
   for (i=st->start;i<st->end;i++) //all bands
   {
      int width, quanta;
      int dynalloc_loop_logp;
      int boost;
      width = C*(st->mode->eBands[i+1]-st->mode->eBands[i])<<LM;
      /* quanta is 6 bits, but no more than 1 bit/sample
         and no less than 1/8 bit/sample */
      quanta = IMIN(width<<BITRES, IMAX(6<<BITRES, width));
      dynalloc_loop_logp = dynalloc_logp;
      boost = 0;
      while (tell+(dynalloc_loop_logp<<BITRES) < total_bits && boost < cap[i])
      {
         int flag;
         flag = ec_dec_bit_logp(dec, dynalloc_loop_logp); //decoding flag
         tell = ec_tell_frac(dec);
         if (!flag) //if this band has no flag we break
            break;
         boost += quanta;
         total_bits -= quanta;
         dynalloc_loop_logp = 1; //Within each band we start our first flag check with loop logp, then after we switch it to even prob
      }
      offsets[i] = boost;
      /* Making dynalloc more likely */
      if (boost>0)
         dynalloc_logp = IMAX(2, dynalloc_logp-1);
   }

   ALLOC(fine_quant, st->mode->nbEBands, int);
   alloc_trim = tell+(6<<BITRES) <= total_bits ? //a param determining if bits are biased to high or low freq
         ec_dec_icdf(dec, trim_icdf, 7) : 5;

   bits = (((int32_t)len*8)<<BITRES) - ec_tell_frac(dec) - 1; //total bits minus 1 in fractional bits
   anti_collapse_rsv = isTransient&&LM>=2&&bits>=((LM+2)<<BITRES) ? (1<<BITRES) : 0;
   bits -= anti_collapse_rsv;
   codedBands = compute_allocation(st->mode, st->start, st->end, offsets, cap,
         alloc_trim, &intensity, &dual_stereo, bits, &balance, pulses,
         fine_quant, fine_priority, C, LM, dec, 0, 0);

   unquant_fine_energy(st->mode, st->start, st->end, oldBandE, fine_quant, dec, C);

   /* Decode fixed codebook */
   ALLOC(collapse_masks, C*st->mode->nbEBands, unsigned char);
   quant_all_bands(0, st->mode, st->start, st->end, X, C==2 ? X+N : NULL, collapse_masks,
         NULL, pulses, shortBlocks, spread_decision, dual_stereo, intensity, tf_res,
         len*(8<<BITRES)-anti_collapse_rsv, balance, dec, LM, codedBands, &st->rng);

   if (anti_collapse_rsv > 0)
   {
      anti_collapse_on = ec_dec_bits(dec, 1);
   }

   unquant_energy_finalise(st->mode, st->start, st->end, oldBandE,
         fine_quant, fine_priority, len*8-ec_tell(dec), dec, C);

   if (anti_collapse_on)
      anti_collapse(st->mode, X, collapse_masks, LM, C, N,
            st->start, st->end, oldBandE, oldLogE, oldLogE2, pulses, st->rng);

   log2Amp(st->mode, st->start, st->end, bandE, oldBandE, C);

   if (silence)
   {
      for (i=0;i<C*st->mode->nbEBands;i++)
      {
         bandE[i] = 0;
         oldBandE[i] = -(28.f);
      }
   }
   /* Synthesis */
   denormalise_bands(st->mode, X, freq, bandE, effEnd, C, M);

   OPUS_MOVE(decode_mem[0], decode_mem[0]+N, DECODE_BUFFER_SIZE-N);
   if (CC==2)
      OPUS_MOVE(decode_mem[1], decode_mem[1]+N, DECODE_BUFFER_SIZE-N);

   c=0; do
      for (i=0;i<M*st->mode->eBands[st->start];i++)
         freq[c*N+i] = 0;
   while (++c<C);
   c=0; do {
      int bound = M*st->mode->eBands[effEnd];
      if (st->downsample!=1)
         bound = IMIN(bound, N/st->downsample);
      for (i=bound;i<N;i++)
         freq[c*N+i] = 0;
   } while (++c<C);

   out_syn[0] = out_mem[0]+MAX_PERIOD-N;
   if (CC==2)
      out_syn[1] = out_mem[1]+MAX_PERIOD-N;

   if (CC==2&&C==1)
   {
      for (i=0;i<N;i++)
         freq[N+i] = freq[i];
   }
   if (CC==1&&C==2)
   {
      for (i=0;i<N;i++)
         freq[i] = 0.5f*(freq[i]+freq[N+i]);
   }

   /* Compute inverse MDCTs */
   compute_inv_mdcts(st->mode, shortBlocks, freq, out_syn, overlap_mem, CC, LM);

   c=0; do {
      st->postfilter_period=IMAX(st->postfilter_period, COMBFILTER_MINPERIOD);
      st->postfilter_period_old=IMAX(st->postfilter_period_old, COMBFILTER_MINPERIOD);
      comb_filter(out_syn[c], out_syn[c], st->postfilter_period_old, st->postfilter_period, st->mode->shortMdctSize,
            st->postfilter_gain_old, st->postfilter_gain, st->postfilter_tapset_old, st->postfilter_tapset,
            st->mode->window, st->overlap);
      if (LM!=0)
         comb_filter(out_syn[c]+st->mode->shortMdctSize, out_syn[c]+st->mode->shortMdctSize, st->postfilter_period, postfilter_pitch, N-st->mode->shortMdctSize,
               st->postfilter_gain, postfilter_gain, st->postfilter_tapset, postfilter_tapset,
               st->mode->window, st->mode->overlap);

   } while (++c<CC);
   st->postfilter_period_old = st->postfilter_period;
   st->postfilter_gain_old = st->postfilter_gain;
   st->postfilter_tapset_old = st->postfilter_tapset;
   st->postfilter_period = postfilter_pitch;
   st->postfilter_gain = postfilter_gain;
   st->postfilter_tapset = postfilter_tapset;
   if (LM!=0)
   {
      st->postfilter_period_old = st->postfilter_period;
      st->postfilter_gain_old = st->postfilter_gain;
      st->postfilter_tapset_old = st->postfilter_tapset;
   }

   if (C==1) {
      for (i=0;i<st->mode->nbEBands;i++)
         oldBandE[st->mode->nbEBands+i]=oldBandE[i];
   }

   /* In case start or end were to change */
   if (!isTransient)
   {
      for (i=0;i<2*st->mode->nbEBands;i++)
         oldLogE2[i] = oldLogE[i];
      for (i=0;i<2*st->mode->nbEBands;i++)
         oldLogE[i] = oldBandE[i];
      for (i=0;i<2*st->mode->nbEBands;i++)
         backgroundLogE[i] = IMIN(backgroundLogE[i] + M*(0.001f), oldBandE[i]);
   } else {
      for (i=0;i<2*st->mode->nbEBands;i++)
         oldLogE[i] = IMIN(oldLogE[i], oldBandE[i]);
   }
   c=0; do
   {
      for (i=0;i<st->start;i++)
      {
         oldBandE[c*st->mode->nbEBands+i]=0;
         oldLogE[c*st->mode->nbEBands+i]=oldLogE2[c*st->mode->nbEBands+i]=-(28.f);
      }
      for (i=st->end;i<st->mode->nbEBands;i++)
      {
         oldBandE[c*st->mode->nbEBands+i]=0;
         oldLogE[c*st->mode->nbEBands+i]=oldLogE2[c*st->mode->nbEBands+i]=-(28.f);
      }
   } while (++c<2);
   st->rng = dec->rng;

   deemphasis(out_syn, pcm, N, CC, st->downsample, st->mode->preemph, st->preemph_memD);
   st->loss_count = 0;
   RESTORE_STACK;
   if (ec_tell(dec) > 8*len)
      return OPUS_INTERNAL_ERROR;
   if(ec_get_error(dec))
      st->error = 1;
   return frame_size/st->downsample;
}





