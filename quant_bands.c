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

/* Mean energy in each band quantized in Q6 and converted back to float */

#include "quant_bands.h"
#include "rate.h"

static const float eMeans[25] = {
      6.437500f, 6.250000f, 5.750000f, 5.312500f, 5.062500f,
      4.812500f, 4.500000f, 4.375000f, 4.875000f, 4.687500f,
      4.562500f, 4.437500f, 4.875000f, 4.625000f, 4.312500f,
      4.500000f, 4.375000f, 4.625000f, 4.750000f, 4.437500f,
      3.750000f, 3.750000f, 3.750000f, 3.750000f, 3.750000f
};

static const float pred_coef[4] = {29440/32768., 26112/32768., 21248/32768., 16384/32768.};
static const float beta_coef[4] = {30147/32768., 22282/32768., 12124/32768., 6554/32768.};
static const float beta_intra = 4915/32768.;

/*Parameters of the Laplace-like probability models used for the coarse energy.
  There is one pair of parameters for each frame size, prediction type
   (inter/intra), and band number.
  The first number of each pair is the probability of 0, and the second is the
   decay rate, both in Q8 precision.*/
static const unsigned char e_prob_model[4][2][42] = {
   /*120 sample frames.*/
   {
      /*Inter*/
      {
          72, 127,  65, 129,  66, 128,  65, 128,  64, 128,  62, 128,  64, 128,
          64, 128,  92,  78,  92,  79,  92,  78,  90,  79, 116,  41, 115,  40,
         114,  40, 132,  26, 132,  26, 145,  17, 161,  12, 176,  10, 177,  11
      },
      /*Intra*/
      {
          24, 179,  48, 138,  54, 135,  54, 132,  53, 134,  56, 133,  55, 132,
          55, 132,  61, 114,  70,  96,  74,  88,  75,  88,  87,  74,  89,  66,
          91,  67, 100,  59, 108,  50, 120,  40, 122,  37,  97,  43,  78,  50
      }
   },
   /*240 sample frames.*/
   {
      /*Inter*/
      {
          83,  78,  84,  81,  88,  75,  86,  74,  87,  71,  90,  73,  93,  74,
          93,  74, 109,  40, 114,  36, 117,  34, 117,  34, 143,  17, 145,  18,
         146,  19, 162,  12, 165,  10, 178,   7, 189,   6, 190,   8, 177,   9
      },
      /*Intra*/
      {
          23, 178,  54, 115,  63, 102,  66,  98,  69,  99,  74,  89,  71,  91,
          73,  91,  78,  89,  86,  80,  92,  66,  93,  64, 102,  59, 103,  60,
         104,  60, 117,  52, 123,  44, 138,  35, 133,  31,  97,  38,  77,  45
      }
   },
   /*480 sample frames.*/
   {
      /*Inter*/
      {
          61,  90,  93,  60, 105,  42, 107,  41, 110,  45, 116,  38, 113,  38,
         112,  38, 124,  26, 132,  27, 136,  19, 140,  20, 155,  14, 159,  16,
         158,  18, 170,  13, 177,  10, 187,   8, 192,   6, 175,   9, 159,  10
      },
      /*Intra*/
      {
          21, 178,  59, 110,  71,  86,  75,  85,  84,  83,  91,  66,  88,  73,
          87,  72,  92,  75,  98,  72, 105,  58, 107,  54, 115,  52, 114,  55,
         112,  56, 129,  51, 132,  40, 150,  33, 140,  29,  98,  35,  77,  42
      }
   },
   /*960 sample frames.*/
   {
      /*Inter*/
      {
          42, 121,  96,  66, 108,  43, 111,  40, 117,  44, 123,  32, 120,  36,
         119,  33, 127,  33, 134,  34, 139,  21, 147,  23, 152,  20, 158,  25,
         154,  26, 166,  21, 173,  16, 184,  13, 184,  10, 150,  13, 139,  15
      },
      /*Intra*/
      {
          22, 178,  63, 114,  74,  82,  84,  83,  92,  82, 103,  62,  96,  72,
          96,  67, 101,  73, 107,  72, 113,  55, 118,  52, 125,  52, 118,  52,
         117,  55, 135,  49, 137,  39, 157,  32, 145,  29,  97,  33,  77,  40
      }
   }
};

static const unsigned char small_energy_icdf[3]={2,1,0};

void unquant_coarse_energy(const CELTMode *m, int start, int end, float *oldEBands, int intra, ec_dec *dec, int C, int LM)
{
   const unsigned char *prob_model = e_prob_model[LM][intra]; //table of values found by LM value and whether it's intra or inter frame
   int i, c;
   float prev[2] = {0, 0}; //tracking error for both channel
   float coef;
   float beta;
   int32_t budget;
   int32_t tell;

   if (intra) //no prediction for intra frames
   {
      coef = 0;
      beta = beta_intra;
   } else {
      beta = beta_coef[LM]; //setting variables using tables based on LM value for prediction
      coef = pred_coef[LM];
   }

   budget = dec->storage*8; //bits in packet, we track to see how many bits we use and where

   /* Decode at a fixed coarse resolution */
   for (i=start;i<end;i++) //Since we are CELT fullband this is looping through every band
   {
      c=0;
      do { //looping through both channels
         int qi;
         float q;
         float tmp;
         /* It would be better to express this invariant as a
            test on C at function entry, but that isn't enough
            to make the static analyzer happy. */
         celt_assert(c<2);
         tell = ec_tell(dec);
         if(budget-tell>=15) //if we have enough bits to decode using the full probability model, do so
         {
            int pi;
            pi = 2*IMIN(i,20); //caps band at 20 as higher bands have negligible difference in probability model
            qi = ec_laplace_decode(dec,
                  prob_model[pi]<<7, prob_model[pi+1]<<6); //decoding laplace using proabability parameters from band and band+1 using tables and shifting values to get them in the right format for decoding
         }
         else if(budget-tell>=2) //shorter decoding 
         {
            qi = ec_dec_icdf(dec, small_energy_icdf, 2); //passing a 3 value arr [2,1,0] and number of bits of precision to use
            qi = (qi>>1)^-(qi&1); //converts uint into signed 
         }
         else if(budget-tell>=1) //even shorter just decoding a bit and then negating
         {
            qi = -ec_dec_bit_logp(dec, 1);
         }
         else  //with no bits assume energy decreased
            qi = -1;
         q = (float)(qi);// just casts to float. q = dB steps

         oldEBands[i+c*m->nbEBands] = IMAX(-9.f, oldEBands[i+c*m->nbEBands]); //clamps previous energy band to -9dB  on the low end
         oldEBands[i+c*m->nbEBands] = (coef*oldEBands[i+c*m->nbEBands]) + prev[c] + q; 
         //its just the coef * old bands + prev + q residual we just calculated
         //in intra frame coef is set to 0 so the expression is just = prev+q
         prev[c] = prev[c] + q - (beta*q); //overwrites prev to prev + q - beta_coef * q 
      } while (++c < C);
   }
}

void unquant_fine_energy(const CELTMode *m, int start, int end, float *oldEBands, int *fine_quant, ec_dec *dec, int C)
{
   int i, c;
   /* Decode finer resolution */
   for (i=start;i<end;i++)
   {
      if (fine_quant[i] <= 0)
         continue;
      c=0;
      do {
         int q2;
         float offset;
         q2 = ec_dec_bits(dec, fine_quant[i]);
         offset = (q2+.5f)*(1<<(14-fine_quant[i]))*(1.f/16384) - .5f;
         oldEBands[i+c*m->nbEBands] += offset;
      } while (++c < C);
   }
}

void unquant_energy_finalise(const CELTMode *m, int start, int end, float *oldEBands, int *fine_quant,  int *fine_priority, int bits_left, ec_dec *dec, int C)
{ //This and the fine energy quant are just about improving the sound and steps between the bands. Coarse energy quant gives big integer steps, fine makes them more defined, and this takes any remaining bits in the packet to add more refinement
   int i, prio, c;

   /* Use up the remaining bits */
   for (prio=0;prio<2;prio++)
   {
      for (i=start;i<end && bits_left>=C ;i++)
      {
         if (fine_quant[i] >= MAX_FINE_BITS || fine_priority[i]!=prio)
            continue;
         c=0;
         do {
            int q2;
            float offset;
            q2 = ec_dec_bits(dec, 1);

            offset = (q2-.5f)*(1<<(14-fine_quant[i]-1))*(1.f/16384);
            oldEBands[i+c*m->nbEBands] += offset;
            bits_left--;
         } while (++c < C);
      }
   }
}

void log2Amp(const CELTMode *m, int start, int end,
      celt_ener *eBands, const float *oldEBands, int C)
{
   int c, i;
   c=0;
   do {
      for (i=0;i<start;i++)
         eBands[i+c*m->nbEBands] = 0;
      for (;i<end;i++)
      {
         float lg = (oldEBands[i+c*m->nbEBands] + (float)eMeans[i]);
         eBands[i+c*m->nbEBands] = celt_exp2(lg);
      }
      for (;i<m->nbEBands;i++)
         eBands[i+c*m->nbEBands] = 0;
   } while (++c < C);
}