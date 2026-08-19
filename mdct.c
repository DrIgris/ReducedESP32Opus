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

/* This is a simple MDCT implementation that uses a N/4 complex FFT
   to do most of the work. It should be relatively straightforward to
   plug in pretty much and FFT here.

   This replaces the Vorbis FFT (and uses the exact same API), which
   was a bit too messy and that was ending up duplicating code
   (might as well use the same FFT everywhere).

   The algorithm is similar to (and inspired from) Fabrice Bellard's
   MDCT implementation in FFMPEG, but has differences in signs, ordering
   and scaling in many places.
*/

#include "mdct.h"

//functions
#define C_MULC(m,a,b) \
    do{ (m).r = (a).r*(b).r + (a).i*(b).i;\
        (m).i = (a).i*(b).r - (a).r*(b).i; }while(0)

#define  C_ADD( res, a,b)\
    do { \
            ((a).r + (b).r);\
            ((a).i + (b).i);\
            (res).r=(a).r+(b).r;  (res).i=(a).i+(b).i; \
    }while(0)

#define  C_SUB( res, a,b)\
    do { \
            ((a).r - (b).r);\
            ((a).i - (b).i);\
            (res).r=(a).r-(b).r;  (res).i=(a).i-(b).i; \
    }while(0)

#define C_ADDTO( res , a)\
    do { \
            ((res).r + (a).r);\
            ((res).i + (a).i);\
            (res).r += (a).r;  (res).i += (a).i;\
    }while(0)

#define C_MULBYSCALAR( c, s ) \
    do{ (c).r *= (s);\
        (c).i *= (s); }while(0)



static void ki_bfly2(
                     kiss_fft_cpx * Fout,
                     const size_t fstride,
                     const kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_fft_cpx * Fout2;
   const kiss_twiddle_cpx * tw1;
   kiss_fft_cpx t;
   int i,j;
   kiss_fft_cpx * Fout_beg = Fout;
   for (i=0;i<N;i++)
   {
      Fout = Fout_beg + i*mm;
      Fout2 = Fout + m;
      tw1 = st->twiddles;
      for(j=0;j<m;j++)
      {
         C_MULC (t,  *Fout2 , *tw1);
         tw1 += fstride;
         C_SUB( *Fout2 ,  *Fout , t );
         C_ADDTO( *Fout ,  t );
         ++Fout2;
         ++Fout;
      }
   }
}

static void ki_bfly4(
                     kiss_fft_cpx * Fout,
                     const size_t fstride,
                     const kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   const kiss_twiddle_cpx *tw1,*tw2,*tw3;
   kiss_fft_cpx scratch[6];
   const size_t m2=2*m;
   const size_t m3=3*m;
   int i, j;

   kiss_fft_cpx * Fout_beg = Fout;
   for (i=0;i<N;i++)
   {
      Fout = Fout_beg + i*mm;
      tw3 = tw2 = tw1 = st->twiddles;
      for (j=0;j<m;j++)
      {
         C_MULC(scratch[0],Fout[m] , *tw1 );
         C_MULC(scratch[1],Fout[m2] , *tw2 );
         C_MULC(scratch[2],Fout[m3] , *tw3 );

         C_SUB( scratch[5] , *Fout, scratch[1] );
         C_ADDTO(*Fout, scratch[1]);
         C_ADD( scratch[3] , scratch[0] , scratch[2] );
         C_SUB( scratch[4] , scratch[0] , scratch[2] );
         C_SUB( Fout[m2], *Fout, scratch[3] );
         tw1 += fstride;
         tw2 += fstride*2;
         tw3 += fstride*3;
         C_ADDTO( *Fout , scratch[3] );

         Fout[m].r = scratch[5].r - scratch[4].i;
         Fout[m].i = scratch[5].i + scratch[4].r;
         Fout[m3].r = scratch[5].r + scratch[4].i;
         Fout[m3].i = scratch[5].i - scratch[4].r;
         ++Fout;
      }
   }
}

static void ki_bfly3(
                     kiss_fft_cpx * Fout,
                     const size_t fstride,
                     const kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   int i, k;
   const size_t m2 = 2*m;
   const kiss_twiddle_cpx *tw1,*tw2;
   kiss_fft_cpx scratch[5];
   kiss_twiddle_cpx epi3;

   kiss_fft_cpx * Fout_beg = Fout;
   epi3 = st->twiddles[fstride*m];
   for (i=0;i<N;i++)
   {
      Fout = Fout_beg + i*mm;
      tw1=tw2=st->twiddles;
      k=m;
      do{

         C_MULC(scratch[1],Fout[m] , *tw1);
         C_MULC(scratch[2],Fout[m2] , *tw2);

         C_ADD(scratch[3],scratch[1],scratch[2]);
         C_SUB(scratch[0],scratch[1],scratch[2]);
         tw1 += fstride;
         tw2 += fstride*2;

         Fout[m].r = Fout->r - (scratch[3].r * 0.5f);
         Fout[m].i = Fout->i - (scratch[3].i * 0.5f);

         C_MULBYSCALAR( scratch[0] , -epi3.i );

         C_ADDTO(*Fout,scratch[3]);

         Fout[m2].r = Fout[m].r + scratch[0].i;
         Fout[m2].i = Fout[m].i - scratch[0].r;

         Fout[m].r -= scratch[0].i;
         Fout[m].i += scratch[0].r;

         ++Fout;
      }while(--k);
   }
}

static void ki_bfly5(
                     kiss_fft_cpx * Fout,
                     const size_t fstride,
                     const kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_fft_cpx *Fout0,*Fout1,*Fout2,*Fout3,*Fout4;
   int i, u;
   kiss_fft_cpx scratch[13];
   const kiss_twiddle_cpx * twiddles = st->twiddles;
   const kiss_twiddle_cpx *tw;
   kiss_twiddle_cpx ya,yb;
   kiss_fft_cpx * Fout_beg = Fout;

   ya = twiddles[fstride*m];
   yb = twiddles[fstride*2*m];
   tw=st->twiddles;

   for (i=0;i<N;i++)
   {
      Fout = Fout_beg + i*mm;
      Fout0=Fout;
      Fout1=Fout0+m;
      Fout2=Fout0+2*m;
      Fout3=Fout0+3*m;
      Fout4=Fout0+4*m;

      for ( u=0; u<m; ++u ) {
         scratch[0] = *Fout0;

         C_MULC(scratch[1] ,*Fout1, tw[u*fstride]);
         C_MULC(scratch[2] ,*Fout2, tw[2*u*fstride]);
         C_MULC(scratch[3] ,*Fout3, tw[3*u*fstride]);
         C_MULC(scratch[4] ,*Fout4, tw[4*u*fstride]);

         C_ADD( scratch[7],scratch[1],scratch[4]);
         C_SUB( scratch[10],scratch[1],scratch[4]);
         C_ADD( scratch[8],scratch[2],scratch[3]);
         C_SUB( scratch[9],scratch[2],scratch[3]);

         Fout0->r += scratch[7].r + scratch[8].r;
         Fout0->i += scratch[7].i + scratch[8].i;

         scratch[5].r = scratch[0].r + (scratch[7].r * ya.r) + (scratch[8].r * yb.r);
         scratch[5].i = scratch[0].i + (scratch[7].i * ya.r) + (scratch[8].i * yb.r);

         scratch[6].r = -(scratch[10].i * ya.i) - (scratch[9].i * yb.i);
         scratch[6].i =  (scratch[10].r * ya.i) + (scratch[9].r * yb.i);

         C_SUB(*Fout1,scratch[5],scratch[6]);
         C_ADD(*Fout4,scratch[5],scratch[6]);

         scratch[11].r = scratch[0].r + (scratch[7].r * yb.r) + (scratch[8].r * ya.r);
         scratch[11].i = scratch[0].i + (scratch[7].i * yb.r) + (scratch[8].i * ya.r);
         scratch[12].r =  (scratch[10].i * yb.i) - (scratch[9].i * ya.i);
         scratch[12].i = -(scratch[10].r * yb.i) + (scratch[9].r * ya.i);

         C_ADD(*Fout2,scratch[11],scratch[12]);
         C_SUB(*Fout3,scratch[11],scratch[12]);

         ++Fout0;++Fout1;++Fout2;++Fout3;++Fout4;
      }
   }
}


void opus_ifft(const kiss_fft_state *st,const kiss_fft_cpx *fin,kiss_fft_cpx *fout)
{
   int m2, m;
   int p;
   int L;
   int fstride[MAXFACTORS];
   int i;
   int shift;

   /* st->shift can be -1 */
   shift = st->shift>0 ? st->shift : 0;
   celt_assert2 (fin != fout, "In-place FFT not supported");
   /* Bit-reverse the input */
   for (i=0;i<st->nfft;i++)
      fout[st->bitrev[i]] = fin[i];

   fstride[0] = 1;
   L=0;
   do {
      p = st->factors[2*L];
      m = st->factors[2*L+1];
      fstride[L+1] = fstride[L]*p;
      L++;
   } while(m!=1);
   m = st->factors[2*L-1];
   for (i=L-1;i>=0;i--)
   {
      if (i!=0)
         m2 = st->factors[2*i-1];
      else
         m2 = 1;
      switch (st->factors[2*i])
      {
      case 2:
         ki_bfly2(fout,fstride[i]<<shift,st,m, fstride[i], m2);
         break;
      case 4:
         ki_bfly4(fout,fstride[i]<<shift,st,m, fstride[i], m2);
         break;
      case 3:
         ki_bfly3(fout,fstride[i]<<shift,st,m, fstride[i], m2);
         break;
      case 5:
         ki_bfly5(fout,fstride[i]<<shift,st,m, fstride[i], m2);
         break;
      }
      m = m2;
   }
}


void clt_mdct_backward(const mdct_lookup *l, float *in, float * restrict out,
      const float * restrict window, int overlap, int shift, int stride)
{
   int i;
   int N, N2, N4;
   float sine;
   VARDECL(float, f);
   VARDECL(float, f2);
   SAVE_STACK;
   N = l->n;
   N >>= shift;
   N2 = N>>1;
   N4 = N>>2;
   ALLOC(f, N2, float);
   ALLOC(f2, N2, float);
   /* sin(x) ~= x here */

   sine = (float)2*PI*(.125f)/N;
   /* Pre-rotate */
   {
      /* Temp pointers to make it really clear to the compiler what we're doing */
      const float * restrict xp1 = in;
      const float * restrict xp2 = in+stride*(N2-1);
      float * restrict yp = f2;
      const float *t = &l->trig[0];
      for(i=0;i<N4;i++)
      {
         float yr, yi;
         yr = -(*xp2 * t[i<<shift]) + (*xp1 * t[(N4-i)<<shift]);
         yi =  -(*xp2 * t[(N4-i)<<shift]) - (*xp1 * t[i<<shift]);
         /* works because the cos is nearly one */
         *yp++ = yr - (yi * sine);
         *yp++ = yi + (yr * sine);
         xp1+=2*stride;
         xp2-=2*stride;
      }
   }

   /* Inverse N/4 complex FFT. This one should *not* downscale even in fixed-point */
   opus_ifft(l->kfft[shift], (kiss_fft_cpx *)f2, (kiss_fft_cpx *)f);

   /* Post-rotate */
   {
      float * restrict fp = f;
      const float *t = &l->trig[0];

      for(i=0;i<N4;i++)
      {
         float re, im, yr, yi;
         re = fp[0];
         im = fp[1];
         /* We'd scale up by 2 here, but instead it's done when mixing the windows */
         yr = (re * t[i<<shift]) - (im * t[(N4-i)<<shift]);
         yi = (im * t[i<<shift]) + (re * t[(N4-i)<<shift]);
         /* works because the cos is nearly one */
         *fp++ = yr - (yi * sine);
         *fp++ = yi + (yr * sine);
      }
   }
   /* De-shuffle the components for the middle of the window only */
   {
      const float * restrict fp1 = f;
      const float * restrict fp2 = f+N2-1;
      float * restrict yp = f2;
      for(i = 0; i < N4; i++)
      {
         *yp++ =-*fp1;
         *yp++ = *fp2;
         fp1 += 2;
         fp2 -= 2;
      }
   }
   out -= (N2-overlap)>>1;
   /* Mirror on both sides for TDAC */
   {
      float * restrict fp1 = f2+N4-1;
      float * restrict xp1 = out+N2-1;
      float * restrict yp1 = out+N4-overlap/2;
      const float * restrict wp1 = window;
      const float * restrict wp2 = window+overlap-1;
      for(i = 0; i< N4-overlap/2; i++)
      {
         *xp1 = *fp1;
         xp1--;
         fp1--;
      }
      for(; i < N4; i++)
      {
         float x1;
         x1 = *fp1--;
         *yp1++ +=- (*wp1 * x1);
         *xp1-- +=  (*wp2 * x1);
         wp1++;
         wp2--;
      }
   }
   {
      float * restrict fp2 = f2+N4;
      float * restrict xp2 = out+N2;
      float * restrict yp2 = out+N-1-(N4-overlap/2);
      const float * restrict wp1 = window;
      const float * restrict wp2 = window+overlap-1;
      for(i = 0; i< N4-overlap/2; i++)
      {
         *xp2 = *fp2;
         xp2++;
         fp2++;
      }
      for(; i < N4; i++)
      {
         float x2;
         x2 = *fp2++;
         *yp2--  = (*wp1 * x2);
         *xp2++  = (*wp2 * x2);
         wp1++;
         wp2--;
      }
   }
   RESTORE_STACK;
}