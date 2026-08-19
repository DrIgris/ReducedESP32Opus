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

#ifndef BANDS_H
#define BANDS_H

#include "modes.h"
#include "stack.h"
#include "entcode.h"
#include "rate.h"
#include "mathop.h"
#include "vq.h"


void haar1(celt_norm *X, int N0, int stride);


/** Quantisation/encoding of the residual spectrum
 * @param m Mode data
 * @param X Residual (normalised)
 * @param total_bits Total number of bits that can be used for the frame (including the ones already spent)
 * @param enc Entropy encoder
 */
void quant_all_bands(const CELTMode *m, int start, int end,
      celt_norm * X, celt_norm * Y, unsigned char *collapse_masks, const celt_ener *bandE, int *pulses,
      int time_domain, int fold, int dual_stereo, int intensity, int *tf_res,
      int32_t total_bits, int32_t balance, ec_ctx *ec, int M, int codedBands, uint32_t *seed);
   
void anti_collapse(const CELTMode *m, celt_norm *X_, unsigned char *collapse_masks, int LM, int C, int size,
      int start, int end, float *logE, float *prev1logE,
      float *prev2logE, int *pulses, uint32_t seed);

/** Denormalise each band of X to restore full amplitude
 * @param m Mode data
 * @param X Spectrum (returned de-normalised)
 * @param bands Square root of the energy for each band
 */
void denormalise_bands(const CELTMode *m, const celt_norm * restrict X, celt_sig * restrict freq, const celt_ener *bandE, int end, int C, int M);

uint32_t celt_lcg_rand(uint32_t seed);


#endif