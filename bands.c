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

#include "bands.h"



/* This function is responsible for encoding and decoding a band for both
   the mono and stereo case. Even in the mono case, it can split the band
   in two and transmit the energy difference with the two half-bands. It
   can be called recursively so bands can end up being split in 8 parts. */
static unsigned quant_band(int encode, const CELTMode *m, int i, celt_norm *X, celt_norm *Y,
      int N, int b, int spread, int B, int intensity, int tf_change, celt_norm *lowband, ec_ctx *ec,
      int32_t *remaining_bits, int LM, celt_norm *lowband_out, const celt_ener *bandE, int level,
      uint32_t *seed, float gain, celt_norm *lowband_scratch, int fill)
{
   const unsigned char *cache;
   int q;
   int curr_bits;
   int stereo, split;
   int imid=0, iside=0;
   int N0=N;
   int N_B=N;
   int N_B0;
   int B0=B;
   int time_divide=0;
   int recombine=0;
   int inv = 0;
   float mid=0, side=0;
   int longBlocks;
   unsigned cm=0;
#ifdef RESYNTH
   int resynth = 1;
#else
   int resynth = !encode;
#endif

   longBlocks = B0==1;

   N_B /= B;
   N_B0 = N_B;

   split = stereo = Y != NULL;

   /* Special case for one sample */
   if (N==1)
   {
      int c;
      celt_norm *x = X;
      c=0; do {
         int sign=0;
         if (*remaining_bits>=1<<BITRES)
         {
            if (encode)
            {
               sign = x[0]<0;
               ec_enc_bits(ec, sign, 1);
            } else {
               sign = ec_dec_bits(ec, 1);
            }
            *remaining_bits -= 1<<BITRES;
            b-=1<<BITRES;
         }
         if (resynth)
            x[0] = sign ? -1.f : 1.f;
         x = Y;
      } while (++c<1+stereo);
      if (lowband_out)
         lowband_out[0] = SHR16(X[0],4);
      return 1;
   }

   if (!stereo && level == 0)
   {
      int k;
      if (tf_change>0)
         recombine = tf_change;
      /* Band recombining to increase frequency resolution */

      if (lowband && (recombine || ((N_B&1) == 0 && tf_change<0) || B0>1))
      {
         int j;
         for (j=0;j<N;j++)
            lowband_scratch[j] = lowband[j];
         lowband = lowband_scratch;
      }

      for (k=0;k<recombine;k++)
      {
         static const unsigned char bit_interleave_table[16]={
           0,1,1,1,2,3,3,3,2,3,3,3,2,3,3,3
         };
         if (encode)
            haar1(X, N>>k, 1<<k);
         if (lowband)
            haar1(lowband, N>>k, 1<<k);
         fill = bit_interleave_table[fill&0xF]|bit_interleave_table[fill>>4]<<2;
      }
      B>>=recombine;
      N_B<<=recombine;

      /* Increasing the time resolution */
      while ((N_B&1) == 0 && tf_change<0)
      {
         if (encode)
            haar1(X, N_B, B);
         if (lowband)
            haar1(lowband, N_B, B);
         fill |= fill<<B;
         B <<= 1;
         N_B >>= 1;
         time_divide++;
         tf_change++;
      }
      B0=B;
      N_B0 = N_B;

      /* Reorganize the samples in time order instead of frequency order */
      if (B0>1)
      {
         if (encode)
            deinterleave_hadamard(X, N_B>>recombine, B0<<recombine, longBlocks);
         if (lowband)
            deinterleave_hadamard(lowband, N_B>>recombine, B0<<recombine, longBlocks);
      }
   }

   /* If we need 1.5 more bit than we can produce, split the band in two. */
   cache = m->cache.bits + m->cache.index[(LM+1)*m->nbEBands+i];
   if (!stereo && LM != -1 && b > cache[cache[0]]+12 && N>2)
   {
      N >>= 1;
      Y = X+N;
      split = 1;
      LM -= 1;
      if (B==1)
         fill = (fill&1)|(fill<<1);
      B = (B+1)>>1;
   }

   if (split)
   {
      int qn;
      int itheta=0;
      int mbits, sbits, delta;
      int qalloc;
      int pulse_cap;
      int offset;
      int orig_fill;
      int32_t tell;

      /* Decide on the resolution to give to the split parameter theta */
      pulse_cap = m->logN[i]+LM*(1<<BITRES);
      offset = (pulse_cap>>1) - (stereo&&N==2 ? QTHETA_OFFSET_TWOPHASE : QTHETA_OFFSET);
      qn = compute_qn(N, b, offset, pulse_cap, stereo);
      if (stereo && i>=intensity)
         qn = 1;
      if (encode)
      {
         /* theta is the atan() of the ratio between the (normalized)
            side and mid. With just that parameter, we can re-scale both
            mid and side because we know that 1) they have unit norm and
            2) they are orthogonal. */
         itheta = stereo_itheta(X, Y, stereo, N);
      }
      tell = ec_tell_frac(ec);
      if (qn!=1)
      {
         if (encode)
            itheta = (itheta*qn+8192)>>14;

         /* Entropy coding of the angle. We use a uniform pdf for the
            time split, a step for stereo, and a triangular one for the rest. */
         if (stereo && N>2)
         {
            int p0 = 3;
            int x = itheta;
            int x0 = qn/2;
            int ft = p0*(x0+1) + x0;
            /* Use a probability of p0 up to itheta=8192 and then use 1 after */
            if (encode)
            {
               ec_encode(ec,x<=x0?p0*x:(x-1-x0)+(x0+1)*p0,x<=x0?p0*(x+1):(x-x0)+(x0+1)*p0,ft);
            } else {
               int fs;
               fs=ec_decode(ec,ft);
               if (fs<(x0+1)*p0)
                  x=fs/p0;
               else
                  x=x0+1+(fs-(x0+1)*p0);
               ec_dec_update(ec,x<=x0?p0*x:(x-1-x0)+(x0+1)*p0,x<=x0?p0*(x+1):(x-x0)+(x0+1)*p0,ft);
               itheta = x;
            }
         } else if (B0>1 || stereo) {
            /* Uniform pdf */
            if (encode)
               ec_enc_uint(ec, itheta, qn+1);
            else
               itheta = ec_dec_uint(ec, qn+1);
         } else {
            int fs=1, ft;
            ft = ((qn>>1)+1)*((qn>>1)+1);
            if (encode)
            {
               int fl;

               fs = itheta <= (qn>>1) ? itheta + 1 : qn + 1 - itheta;
               fl = itheta <= (qn>>1) ? itheta*(itheta + 1)>>1 :
                ft - ((qn + 1 - itheta)*(qn + 2 - itheta)>>1);

               ec_encode(ec, fl, fl+fs, ft);
            } else {
               /* Triangular pdf */
               int fl=0;
               int fm;
               fm = ec_decode(ec, ft);

               if (fm < ((qn>>1)*((qn>>1) + 1)>>1))
               {
                  itheta = (isqrt32(8*(uint32_t)fm + 1) - 1)>>1;
                  fs = itheta + 1;
                  fl = itheta*(itheta + 1)>>1;
               }
               else
               {
                  itheta = (2*(qn + 1)
                   - isqrt32(8*(uint32_t)(ft - fm - 1) + 1))>>1;
                  fs = qn + 1 - itheta;
                  fl = ft - ((qn + 1 - itheta)*(qn + 2 - itheta)>>1);
               }

               ec_dec_update(ec, fl, fl+fs, ft);
            }
         }
         itheta = (int32_t)itheta*16384/qn;
         if (encode && stereo)
         {
            if (itheta==0)
               intensity_stereo(m, X, Y, bandE, i, N);
            else
               stereo_split(X, Y, N);
         }
         /* NOTE: Renormalising X and Y *may* help fixed-point a bit at very high rate.
                  Let's do that at higher complexity */
      } else if (stereo) {
         if (encode)
         {
            inv = itheta > 8192;
            if (inv)
            {
               int j;
               for (j=0;j<N;j++)
                  Y[j] = -Y[j];
            }
            intensity_stereo(m, X, Y, bandE, i, N);
         }
         if (b>2<<BITRES && *remaining_bits > 2<<BITRES)
         {
            if (encode)
               ec_enc_bit_logp(ec, inv, 2);
            else
               inv = ec_dec_bit_logp(ec, 2);
         } else
            inv = 0;
         itheta = 0;
      }
      qalloc = ec_tell_frac(ec) - tell;
      b -= qalloc;

      orig_fill = fill;
      if (itheta == 0)
      {
         imid = 32767;
         iside = 0;
         fill &= (1<<B)-1;
         delta = -16384;
      } else if (itheta == 16384)
      {
         imid = 0;
         iside = 32767;
         fill &= ((1<<B)-1)<<B;
         delta = 16384;
      } else {
         imid = bitexact_cos(itheta);
         iside = bitexact_cos(16384-itheta);
         /* This is the mid vs side allocation that minimizes squared error
            in that band. */
         delta = FRAC_MUL16((N-1)<<7,bitexact_log2tan(iside,imid));
      }

#ifdef FIXED_POINT
      mid = imid;
      side = iside;
#else
      mid = (1.f/32768)*imid;
      side = (1.f/32768)*iside;
#endif

      /* This is a special case for N=2 that only works for stereo and takes
         advantage of the fact that mid and side are orthogonal to encode
         the side with just one bit. */
      if (N==2 && stereo)
      {
         int c;
         int sign=0;
         celt_norm *x2, *y2;
         mbits = b;
         sbits = 0;
         /* Only need one bit for the side */
         if (itheta != 0 && itheta != 16384)
            sbits = 1<<BITRES;
         mbits -= sbits;
         c = itheta > 8192;
         *remaining_bits -= qalloc+sbits;

         x2 = c ? Y : X;
         y2 = c ? X : Y;
         if (sbits)
         {
            if (encode)
            {
               /* Here we only need to encode a sign for the side */
               sign = x2[0]*y2[1] - x2[1]*y2[0] < 0;
               ec_enc_bits(ec, sign, 1);
            } else {
               sign = ec_dec_bits(ec, 1);
            }
         }
         sign = 1-2*sign;
         /* We use orig_fill here because we want to fold the side, but if
             itheta==16384, we'll have cleared the low bits of fill. */
         cm = quant_band(encode, m, i, x2, NULL, N, mbits, spread, B, intensity, tf_change, lowband, ec, remaining_bits, LM, lowband_out, NULL, level, seed, gain, lowband_scratch, orig_fill);
         /* We don't split N=2 bands, so cm is either 1 or 0 (for a fold-collapse),
             and there's no need to worry about mixing with the other channel. */
         y2[0] = -sign*x2[1];
         y2[1] = sign*x2[0];
         if (resynth)
         {
            celt_norm tmp;
            X[0] = MULT16_16_Q15(mid, X[0]);
            X[1] = MULT16_16_Q15(mid, X[1]);
            Y[0] = MULT16_16_Q15(side, Y[0]);
            Y[1] = MULT16_16_Q15(side, Y[1]);
            tmp = X[0];
            X[0] = SUB16(tmp,Y[0]);
            Y[0] = ADD16(tmp,Y[0]);
            tmp = X[1];
            X[1] = SUB16(tmp,Y[1]);
            Y[1] = ADD16(tmp,Y[1]);
         }
      } else {
         /* "Normal" split code */
         celt_norm *next_lowband2=NULL;
         celt_norm *next_lowband_out1=NULL;
         int next_level=0;
         int32_t rebalance;

         /* Give more bits to low-energy MDCTs than they would otherwise deserve */
         if (B0>1 && !stereo && (itheta&0x3fff))
         {
            if (itheta > 8192)
               /* Rough approximation for pre-echo masking */
               delta -= delta>>(4-LM);
            else
               /* Corresponds to a forward-masking slope of 1.5 dB per 10 ms */
               delta = IMIN(0, delta + (N<<BITRES>>(5-LM)));
         }
         mbits = IMAX(0, IMIN(b, (b-delta)/2));
         sbits = b-mbits;
         *remaining_bits -= qalloc;

         if (lowband && !stereo)
            next_lowband2 = lowband+N; /* >32-bit split case */

         /* Only stereo needs to pass on lowband_out. Otherwise, it's
            handled at the end */
         if (stereo)
            next_lowband_out1 = lowband_out;
         else
            next_level = level+1;

         rebalance = *remaining_bits;
         if (mbits >= sbits)
         {
            /* In stereo mode, we do not apply a scaling to the mid because we need the normalized
               mid for folding later */
            cm = quant_band(encode, m, i, X, NULL, N, mbits, spread, B, intensity, tf_change,
                  lowband, ec, remaining_bits, LM, next_lowband_out1,
                  NULL, next_level, seed, stereo ? 1.0f : MULT16_16_P15(gain,mid), lowband_scratch, fill);
            rebalance = mbits - (rebalance-*remaining_bits);
            if (rebalance > 3<<BITRES && itheta!=0)
               sbits += rebalance - (3<<BITRES);

            /* For a stereo split, the high bits of fill are always zero, so no
               folding will be done to the side. */
            cm |= quant_band(encode, m, i, Y, NULL, N, sbits, spread, B, intensity, tf_change,
                  next_lowband2, ec, remaining_bits, LM, NULL,
                  NULL, next_level, seed, MULT16_16_P15(gain,side), NULL, fill>>B)<<((B0>>1)&(stereo-1));
         } else {
            /* For a stereo split, the high bits of fill are always zero, so no
               folding will be done to the side. */
            cm = quant_band(encode, m, i, Y, NULL, N, sbits, spread, B, intensity, tf_change,
                  next_lowband2, ec, remaining_bits, LM, NULL,
                  NULL, next_level, seed, MULT16_16_P15(gain,side), NULL, fill>>B)<<((B0>>1)&(stereo-1));
            rebalance = sbits - (rebalance-*remaining_bits);
            if (rebalance > 3<<BITRES && itheta!=16384)
               mbits += rebalance - (3<<BITRES);
            /* In stereo mode, we do not apply a scaling to the mid because we need the normalized
               mid for folding later */
            cm |= quant_band(encode, m, i, X, NULL, N, mbits, spread, B, intensity, tf_change,
                  lowband, ec, remaining_bits, LM, next_lowband_out1,
                  NULL, next_level, seed, stereo ? 1.0f : MULT16_16_P15(gain,mid), lowband_scratch, fill);
         }
      }

   } else {
      /* This is the basic no-split case */
      q = bits2pulses(m, i, LM, b);
      curr_bits = pulses2bits(m, i, LM, q);
      *remaining_bits -= curr_bits;

      /* Ensures we can never bust the budget */
      while (*remaining_bits < 0 && q > 0)
      {
         *remaining_bits += curr_bits;
         q--;
         curr_bits = pulses2bits(m, i, LM, q);
         *remaining_bits -= curr_bits;
      }

      if (q!=0)
      {
         int K = get_pulses(q);

         /* Finally do the actual quantization */
         if (encode)
         {
            cm = alg_quant(X, N, K, spread, B, ec
#ifdef RESYNTH
                 , gain
#endif
                 );
         } else {
            cm = alg_unquant(X, N, K, spread, B, ec, gain);
         }
      } else {
         /* If there's no pulse, fill the band anyway */
         int j;
         if (resynth)
         {
            unsigned cm_mask;
            /*B can be as large as 16, so this shift might overflow an int on a
               16-bit platform; use a long to get defined behavior.*/
            cm_mask = (unsigned)(1UL<<B)-1;
            fill &= cm_mask;
            if (!fill)
            {
               for (j=0;j<N;j++)
                  X[j] = 0;
            } else {
               if (lowband == NULL)
               {
                  /* Noise */
                  for (j=0;j<N;j++)
                  {
                     *seed = celt_lcg_rand(*seed);
                     X[j] = (celt_norm)((int32_t)*seed>>20);
                  }
                  cm = cm_mask;
               } else {
                  /* Folded spectrum */
                  for (j=0;j<N;j++)
                  {
                     float tmp;
                     *seed = celt_lcg_rand(*seed);
                     /* About 48 dB below the "normal" folding level */
                     tmp = QCONST16(1.0f/256, 10);
                     tmp = (*seed)&0x8000 ? tmp : -tmp;
                     X[j] = lowband[j]+tmp;
                  }
                  cm = fill;
               }
               renormalise_vector(X, N, gain);
            }
         }
      }
   }

   /* This code is used by the decoder and by the resynthesis-enabled encoder */
   if (resynth)
   {
      if (stereo)
      {
         if (N!=2)
            stereo_merge(X, Y, mid, N);
         if (inv)
         {
            int j;
            for (j=0;j<N;j++)
               Y[j] = -Y[j];
         }
      } else if (level == 0)
      {
         int k;

         /* Undo the sample reorganization going from time order to frequency order */
         if (B0>1)
            interleave_hadamard(X, N_B>>recombine, B0<<recombine, longBlocks);

         /* Undo time-freq changes that we did earlier */
         N_B = N_B0;
         B = B0;
         for (k=0;k<time_divide;k++)
         {
            B >>= 1;
            N_B <<= 1;
            cm |= cm>>B;
            haar1(X, N_B, B);
         }

         for (k=0;k<recombine;k++)
         {
            static const unsigned char bit_deinterleave_table[16]={
              0x00,0x03,0x0C,0x0F,0x30,0x33,0x3C,0x3F,
              0xC0,0xC3,0xCC,0xCF,0xF0,0xF3,0xFC,0xFF
            };
            cm = bit_deinterleave_table[cm];
            haar1(X, N0>>k, 1<<k);
         }
         B<<=recombine;

         /* Scale output for later folding */
         if (lowband_out)
         {
            int j;
            float n;
            n = celt_sqrt(SHL32(EXTEND32(N0),22));
            for (j=0;j<N0;j++)
               lowband_out[j] = MULT16_16_Q15(n,X[j]);
         }
         cm &= (1<<B)-1;
      }
   }
   return cm;
}







void quant_all_bands(int encode, const CELTMode *m, int start, int end,
      celt_norm *X_, celt_norm *Y_, unsigned char *collapse_masks, const celt_ener *bandE, int *pulses,
      int shortBlocks, int spread, int dual_stereo, int intensity, int *tf_res,
      int32_t total_bits, int32_t balance, ec_ctx *ec, int LM, int codedBands, uint32_t *seed)
{
   int i;
   int32_t remaining_bits;
   const int16_t * restrict eBands = m->eBands;
   celt_norm * restrict norm, * restrict norm2;
   VARDECL(celt_norm, _norm);
   VARDECL(celt_norm, lowband_scratch);
   int B;
   int M;
   int lowband_offset;
   int update_lowband = 1;
   int C = Y_ != NULL ? 2 : 1;

   int resynth = !encode;

   SAVE_STACK;

   M = 1<<LM;
   B = shortBlocks ? M : 1;
   ALLOC(_norm, C*M*eBands[m->nbEBands], celt_norm);
   ALLOC(lowband_scratch, M*(eBands[m->nbEBands]-eBands[m->nbEBands-1]), celt_norm);
   norm = _norm;
   norm2 = norm + M*eBands[m->nbEBands];

   lowband_offset = 0;
   for (i=start;i<end;i++)
   {
      int32_t tell;
      int b;
      int N;
      int32_t curr_balance;
      int effective_lowband=-1;
      celt_norm * restrict X, * restrict Y;
      int tf_change=0;
      unsigned x_cm;
      unsigned y_cm;

      X = X_+M*eBands[i];
      if (Y_!=NULL)
         Y = Y_+M*eBands[i];
      else
         Y = NULL;
      N = M*eBands[i+1]-M*eBands[i];
      tell = ec_tell_frac(ec);

      /* Compute how many bits we want to allocate to this band */
      if (i != start)
         balance -= tell;
      remaining_bits = total_bits-tell-1;
      if (i <= codedBands-1)
      {
         curr_balance = balance / IMIN(3, codedBands-i);
         b = IMAX(0, IMIN(16383, IMIN(remaining_bits+1,pulses[i]+curr_balance)));
      } else {
         b = 0;
      }

      if (resynth && M*eBands[i]-N >= M*eBands[start] && (update_lowband || lowband_offset==0))
            lowband_offset = i;

      tf_change = tf_res[i];
      if (i>=m->effEBands)
      {
         X=norm;
         if (Y_!=NULL)
            Y = norm;
      }

      /* Get a conservative estimate of the collapse_mask's for the bands we're
          going to be folding from. */
      if (lowband_offset != 0 && (spread!=SPREAD_AGGRESSIVE || B>1 || tf_change<0))
      {
         int fold_start;
         int fold_end;
         int fold_i;
         /* This ensures we never repeat spectral content within one band */
         effective_lowband = IMAX(M*eBands[start], M*eBands[lowband_offset]-N);
         fold_start = lowband_offset;
         while(M*eBands[--fold_start] > effective_lowband);
         fold_end = lowband_offset-1;
         while(M*eBands[++fold_end] < effective_lowband+N);
         x_cm = y_cm = 0;
         fold_i = fold_start; do {
           x_cm |= collapse_masks[fold_i*C+0];
           y_cm |= collapse_masks[fold_i*C+C-1];
         } while (++fold_i<fold_end);
      }
      /* Otherwise, we'll be using the LCG to fold, so all blocks will (almost
          always) be non-zero.*/
      else
         x_cm = y_cm = (1<<B)-1;

      if (dual_stereo && i==intensity)
      {
         int j;

         /* Switch off dual stereo to do intensity */
         dual_stereo = 0;
         for (j=M*eBands[start];j<M*eBands[i];j++)
            norm[j] = 0.5f*(norm[j]+norm2[j]);
      }
      if (dual_stereo)
      {
         x_cm = quant_band(encode, m, i, X, NULL, N, b/2, spread, B, intensity, tf_change,
               effective_lowband != -1 ? norm+effective_lowband : NULL, ec, &remaining_bits, LM,
               norm+M*eBands[i], bandE, 0, seed, 1.0f, lowband_scratch, x_cm);
         y_cm = quant_band(encode, m, i, Y, NULL, N, b/2, spread, B, intensity, tf_change,
               effective_lowband != -1 ? norm2+effective_lowband : NULL, ec, &remaining_bits, LM,
               norm2+M*eBands[i], bandE, 0, seed, 1.0f, lowband_scratch, y_cm);
      } else {
         x_cm = quant_band(encode, m, i, X, Y, N, b, spread, B, intensity, tf_change,
               effective_lowband != -1 ? norm+effective_lowband : NULL, ec, &remaining_bits, LM,
               norm+M*eBands[i], bandE, 0, seed, 1.0f, lowband_scratch, x_cm|y_cm);
         y_cm = x_cm;
      }
      collapse_masks[i*C+0] = (unsigned char)x_cm;
      collapse_masks[i*C+C-1] = (unsigned char)y_cm;
      balance += pulses[i] + tell;

      /* Update the folding position only as long as we have 1 bit/sample depth */
      update_lowband = b>(N<<BITRES);
   }
   RESTORE_STACK;
}

/* This prevents energy collapse for transients with multiple short MDCTs */
void anti_collapse(const CELTMode *m, celt_norm *X_, unsigned char *collapse_masks, int LM, int C, int size,
      int start, int end, float *logE, float *prev1logE,
      float *prev2logE, int *pulses, uint32_t seed)
{
   int c, i, j, k;
   for (i=start;i<end;i++)
   {
      int N0;
      float thresh, sqrt_1;
      int depth;


      N0 = m->eBands[i+1]-m->eBands[i];
      /* depth in 1/8 bits */
      depth = (1+pulses[i])/((m->eBands[i+1]-m->eBands[i])<<LM);

      thresh = .5f*celt_exp2(-.125f*depth);
      sqrt_1 = celt_rsqrt(N0<<LM);
      c=0; do
      {
         celt_norm *X;
         float prev1;
         float prev2;
         float Ediff;
         float r;
         int renormalize=0;
         prev1 = prev1logE[c*m->nbEBands+i];
         prev2 = prev2logE[c*m->nbEBands+i];
         if (C==1)
         {
            prev1 = IMAX(prev1,prev1logE[m->nbEBands+i]);
            prev2 = IMAX(prev2,prev2logE[m->nbEBands+i]);
         }
         Ediff = (logE[c*m->nbEBands+i])-(IMIN(prev1,prev2));
         Ediff = IMAX(0, Ediff);
         /* r needs to be multiplied by 2 or 2*sqrt(2) depending on LM because
            short blocks don't have the same energy as long */
         r = 2.f*celt_exp2(-Ediff);
         if (LM==3)
            r *= 1.41421356f;
         r = IMIN(thresh, r);
         r = r*sqrt_1;
         X = X_+c*size+(m->eBands[i]<<LM);
         for (k=0;k<1<<LM;k++)
         {
            /* Detect collapse */
            if (!(collapse_masks[i*C+c]&1<<k))
            {
               /* Fill with noise */
               for (j=0;j<N0;j++)
               {
                  seed = celt_lcg_rand(seed);
                  X[(j<<LM)+k] = (seed&0x8000 ? r : -r);
               }
               renormalize = 1;
            }
         }
         /* We just added some energy, so we need to renormalise */
         if (renormalize)
            renormalise_vector(X, N0<<LM, 1.0f);
      } while (++c<C);
   }
}

/* De-normalise the energy to produce the synthesis from the unit-energy bands */
void denormalise_bands(const CELTMode *m, const celt_norm * restrict X, celt_sig * restrict freq, const celt_ener *bandE, int end, int C, int M)
{
   int i, c, N;
   const int16_t *eBands = m->eBands;
   N = M*m->shortMdctSize;
   celt_assert2(C<=2, "denormalise_bands() not implemented for >2 channels");
   c=0; do {
      celt_sig * restrict f;
      const celt_norm * restrict x;
      f = freq+c*N;
      x = X+c*N;
      for (i=0;i<end;i++)
      {
         int j, band_end;
         float g = bandE[i+c*m->nbEBands];
         j=M*eBands[i];
         band_end = M*eBands[i+1];
         do {
            *f++ = *x * g; //literally just the unit vectors x being multiplied by their respective amplitude g
            x++;
         } while (++j<band_end);
      }
      for (i=M*eBands[end];i<N;i++)
         *f++ = 0;
   } while (++c<C);
}