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

#include <stdint.h>
#include "stack.h"
#include "decoder.h"
#include "opus_vars.h"
#include "celt.h"

struct OpusDecoder {
   int          celt_dec_offset;
   int          channels;
   uint32_t   Fs;          /** Sampling rate (at the API level) */

   /* Everything beyond this point gets cleared on a reset */
#define OPUS_DECODER_RESET_START stream_channels
   int          stream_channels;

   int          bandwidth;
   int          mode;
   int          frame_size;
   int          prev_redundancy;

   uint32_t  rangeFinal;
};


int opus_decoder_get_size(int channels)
{
   int celtDecSizeBytes;
   int ret;
   celtDecSizeBytes = celt_decoder_get_size(channels); //Finds(creates if not found) a CELT mode based on sampling rate and frame size. Then calls a custom size function that is the struct size + some other funky stuff
   return align(sizeof(OpusDecoder))+celtDecSizeBytes;
}


int opus_decoder_init(OpusDecoder *st, int32_t Fs, int channels)
{
   void *silk_dec;
   CELTDecoder *celt_dec;
   int ret;

   OPUS_CLEAR((char*)st, opus_decoder_get_size(channels)); //This clears the memory of the decoder struct and makes sure all values are initialized.

   st->celt_dec_offset = align(sizeof(OpusDecoder)); // this sets the celt offset to be after the offset + the size of the silk decoder. (since we aligned silkdecsize we don't need an align here)
   celt_dec = (CELTDecoder*)((char*)st+st->celt_dec_offset); // assigns the celt decoder pointer
   st->stream_channels = st->channels = channels; //channels
   st->Fs = Fs; //sample rate

   /* Initialize CELT decoder */
   ret = celt_decoder_init(celt_dec, Fs, channels); // sets values of celt struct and finds downsampling factor
   if(ret!=OPUS_OK)return OPUS_INTERNAL_ERROR;

   // celt_dec->signalling = 0;

   st->frame_size = Fs/400;
   return OPUS_OK;
}

OpusDecoder *opus_decoder_create(int32_t Fs, int channels, int *error) //simply an automatic allocation of mem before initialization.
{
   int ret;
   OpusDecoder *st;
   st = (OpusDecoder *)opus_alloc(opus_decoder_get_size(channels));
   if (st == NULL)
   {
      if (error)
         *error = OPUS_ALLOC_FAIL;
      return NULL;
   }
   ret = opus_decoder_init(st, Fs, channels);
   if (error)
      *error = ret;
   if (ret != OPUS_OK)
   {
      opus_free(st);
      st = NULL;
   }
   return st;
}


static int opus_decode_frame(OpusDecoder *st, const unsigned char *data,
      int len, float *pcm)
{
   CELTDecoder *celt_dec;
   int i, celt_ret=0;
   ec_dec dec;

   int audiosize;
   int mode;
   int c;
   int F2_5, F5, F10, F20;
   ALLOC_STACK;

   celt_dec = (CELTDecoder*)((char*)st+st->celt_dec_offset);
   F20 = st->Fs/50;
   F10 = F20>>1;
   F5 = F10>>1;
   F2_5 = F5>>1;
   audiosize = MAX_FRAME_SIZE;
   if (data != NULL) //should probably only need/care about this function where it inits decoder val and normalizes
   {
      mode = st->mode;
      ec_dec_init(&dec,(unsigned char*)data,len); //just inits the decoder values and then normalizes val and rng
   }

   
   int celt_frame_size = MAX_FRAME_SIZE;
   /* Decode CELT */
   celt_ret = celt_decode_with_ec(celt_dec, data, //important call
                                    len, pcm, celt_frame_size, &dec);
   

   RESTORE_STACK;
   return celt_ret < 0 ? celt_ret : audiosize;

}


int opus_decode_native(OpusDecoder *st, const unsigned char *data,
      int len, float *pcm) {
   int i, nb_samples;
   int count, offset;
   unsigned char toc;
   int tot_offset;

   len--; //account for ToC byte
   tot_offset = 0;
   st->mode = MODE_CELT_ONLY; //since we are only doing CELT full band stereo, we can hardcode the mode. 
   st->bandwidth = OPUS_BANDWIDTH_FULLBAND;
   st->frame_size = MAX_FRAME_SIZE;
   st->stream_channels = CHANNELS;
   //set state variables from ToC byte in front of packet (FC in our CELT full band stereo case)

   data += 1; //Harcoded 1 since ToC byte is always one byte, FC
   tot_offset += 1;
   
   int ret;
   ret = opus_decode_frame(st, data, len, pcm); //we dont need to subtract nb_samples from frame_size since we are only doing one frame
   if (ret<0)
      return ret;
   tot_offset += len;
   pcm += ret*st->channels;

   return ret;
}









int opus_decode(OpusDecoder *st, const unsigned char *data,
      int len, int16_t *pcm) //doesnt do too much, just does some checks, then throws it to the actual decoder
{
   VARDECL(float, out);
   int ret, i;
   ALLOC_STACK;

   if(MAX_FRAME_SIZE<0)return OPUS_BAD_ARG;

   ALLOC(out, MAX_FRAME_SIZE*st->channels, float);

   ret = opus_decode_native(st, data, len, out);
   if (ret > 0)
   {
      for (i=0;i<ret*st->channels;i++)
         pcm[i] = FLOAT2INT16(out[i]);
   }
   RESTORE_STACK;
   return ret;
}