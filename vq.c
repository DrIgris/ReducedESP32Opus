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

#include "vq.h"

static void exp_rotation1(celt_norm *X, int len, int stride, float c, float s)
{
   int i;
   celt_norm *Xptr;
   Xptr = X;
   for (i=0;i<len-stride;i++)
   {
      celt_norm x1, x2;
      x1 = Xptr[0];
      x2 = Xptr[stride];
      Xptr[stride] = (c * x2) + (s * x1);
      *Xptr++      = (c * x1) - (s * x2);
   }
   Xptr = &X[len-2*stride-1];
   for (i=len-2*stride-1;i>=0;i--)
   {
      celt_norm x1, x2;
      x1 = Xptr[0];
      x2 = Xptr[stride];
      Xptr[stride] = (c * x2) + (s * x1);
      *Xptr--      = (c * x1) - (s * x2);
   }
}

static void exp_rotation(celt_norm *X, int len, int dir, int stride, int K, int spread)
{
   static const int SPREAD_FACTOR[3]={15,10,5};
   int i;
   float c, s;
   float gain, theta;
   int stride2=0;
   int factor;

   if (2*K>=len || spread==SPREAD_NONE)
      return;
   factor = SPREAD_FACTOR[spread-1];

   gain = (float)(len) / (float)(len+factor*K);
   theta = 0.5f*((gain * gain));

   c = celt_cos_norm(theta);
   s = celt_cos_norm(1.0f - theta); /*  sin(theta) */

   if (len>=8*stride)
   {
      stride2 = 1;
      /* This is just a simple (equivalent) way of computing sqrt(len/stride) with rounding.
         It's basically incrementing long as (stride2+0.5)^2 < len/stride. */
      while ((stride2*stride2+stride2)*stride + (stride>>2) < len)
         stride2++;
   }
   /*NOTE: As a minor optimization, we could be passing around log2(B), not B, for both this and for
      extract_collapse_mask().*/
   len /= stride;
   for (i=0;i<stride;i++)
   {
      if (dir < 0)
      {
         if (stride2)
            exp_rotation1(X+i*len, len, stride2, s, c);
         exp_rotation1(X+i*len, len, 1, c, s);
      } else {
         exp_rotation1(X+i*len, len, 1, c, -s);
         if (stride2)
            exp_rotation1(X+i*len, len, stride2, s, -c);
      }
   }
}

/** Takes the pitch vector and the decoded residual vector, computes the gain
    that will give ||p+g*y||=1 and mixes the residual with the pitch. */
static void normalise_residual(int * restrict iy, celt_norm * restrict X,
      int N, float Ryy, float gain)
{
   int i;
   float t;
   float g;

   t = Ryy;
   g = (celt_rsqrt_norm(t) * gain);

   i=0;
   do
      X[i] = g * iy[i];
   while (++i < N);
}

static unsigned extract_collapse_mask(int *iy, int N, int B)
{
   unsigned collapse_mask;
   int N0;
   int i;
   if (B<=1)
      return 1;
   /*NOTE: As a minor optimization, we could be passing around log2(B), not B, for both this and for
      exp_rotation().*/
   N0 = N/B;
   collapse_mask = 0;
   i=0; do {
      int j;
      j=0; do {
         collapse_mask |= (iy[i*N0+j]!=0)<<i;
      } while (++j<N0);
   } while (++i<B);
   return collapse_mask;
}


/** Decode pulse vector and combine the result with the pitch vector to produce
    the final normalised signal in the current band. */
unsigned alg_unquant(celt_norm *X, int N, int K, int spread, int B,
      ec_dec *dec, float gain)
{
   int i;
   float Ryy;
   unsigned collapse_mask;
   VARDECL(int, iy);
   SAVE_STACK;

   celt_assert2(K>0, "alg_unquant() needs at least one pulse");
   celt_assert2(N>1, "alg_unquant() needs at least two dimensions");
   ALLOC(iy, N, int);
   decode_pulses(iy, N, K, dec);
   Ryy = 0;
   i=0;
   do {
      Ryy = MAC16_16(Ryy, iy[i], iy[i]);
   } while (++i < N);
   normalise_residual(iy, X, N, Ryy, gain);
   exp_rotation(X, N, -1, B, K, spread);
   collapse_mask = extract_collapse_mask(iy, N, B);
   RESTORE_STACK;
   return collapse_mask;
}

void renormalise_vector(celt_norm *X, int N, float gain)
{
   int i;
   float E = EPSILON;
   float g;
   celt_norm *xptr = X;
   for (i=0;i<N;i++)
   {
      E = MAC16_16(E, *xptr, *xptr);
      xptr++;
   }
   g = (celt_rsqrt_norm(E) * gain);

   xptr = X;
   for (i=0;i<N;i++)
   {
      *xptr *= g;
      xptr++;
   }
   /*return celt_sqrt(E);*/
}

int stereo_itheta(celt_norm *X, celt_norm *Y, int stereo, int N)
{
   int i;
   int itheta;
   float mid, side;
   float Emid, Eside;

   Emid = Eside = EPSILON;
   if (stereo)
   {
      for (i=0;i<N;i++)
      {
         celt_norm m, s;
         m = ((X[i]) + (Y[i]));
         s = ((X[i]) - (Y[i]));
         Emid = MAC16_16(Emid, m, m);
         Eside = MAC16_16(Eside, s, s);
      }
   } else {
      for (i=0;i<N;i++)
      {
         celt_norm m, s;
         m = X[i];
         s = Y[i];
         Emid = MAC16_16(Emid, m, m);
         Eside = MAC16_16(Eside, s, s);
      }
   }
   mid = celt_sqrt(Emid);
   side = celt_sqrt(Eside);

   itheta = (int)floor(.5f+16384*0.63662f*atan2(side,mid));

   return itheta;
}