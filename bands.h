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
#include "stack.h"
#include "entcode.h"
#include "rate.h"


/** Quantisation/encoding of the residual spectrum
 * @param m Mode data
 * @param X Residual (normalised)
 * @param total_bits Total number of bits that can be used for the frame (including the ones already spent)
 * @param enc Entropy encoder
 */
void quant_all_bands(int encode, const CELTMode *m, int start, int end,
      celt_norm * X, celt_norm * Y, unsigned char *collapse_masks, const celt_ener *bandE, int *pulses,
      int time_domain, int fold, int dual_stereo, int intensity, int *tf_res,
      int32_t total_bits, int32_t balance, ec_ctx *ec, int M, int codedBands, uint32_t *seed);