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


#define DECODE_BUFFER_SIZE 2048
#define LPC_ORDER 24


struct OpusCustomDecoder {
   const OpusCustomMode *mode;
   int overlap;
   int channels;
   int stream_channels;

   int downsample;
   int start, end;
   int signalling; 

   /* Everything beyond this point gets cleared on a reset */
#define DECODER_RESET_START rng

   uint32_t rng;
   int error;
   int last_pitch_index;
   int loss_count;
   int postfilter_period;
   int postfilter_period_old;
   float postfilter_gain;
   float postfilter_gain_old;
   int postfilter_tapset;
   int postfilter_tapset_old;

   float preemph_memD[2];

   float _decode_mem[1]; /* Size = channels*(DECODE_BUFFER_SIZE+mode->overlap) */
   /* opus_val16 lpc[],  Size = channels*LPC_ORDER */
   /* opus_val16 oldEBands[], Size = 2*mode->nbEBands */
   /* opus_val16 oldLogE[], Size = 2*mode->nbEBands */
   /* opus_val16 oldLogE2[], Size = 2*mode->nbEBands */
   /* opus_val16 backgroundLogE[], Size = 2*mode->nbEBands */
};

static inline int opus_custom_decoder_get_size(const CELTMode *mode, int channels)
{
   int size = sizeof(struct CELTDecoder)
            + (channels*(DECODE_BUFFER_SIZE+mode->overlap)-1)*sizeof(float)
            + channels*LPC_ORDER*sizeof(float)
            + 4*2*mode->nbEBands*sizeof(float);
   return size;
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
}


static inline int opus_custom_decoder_init(CELTDecoder *st, const CELTMode *mode, int channels) {

   if (st==NULL)
      return OPUS_ALLOC_FAIL;

   OPUS_CLEAR((char*)st, opus_custom_decoder_get_size(mode, channels));

   st->mode = mode;
   st->overlap = mode->overlap;
   st->stream_channels = st->channels = channels;

   st->downsample = 1;
   st->start = START_BAND;
   st->end = END_BAND;
   st->signalling = 1;

   st->loss_count = 0;

   opus_celt_reset_state(st);

   return OPUS_OK;
}


