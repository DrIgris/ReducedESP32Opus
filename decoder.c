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


// int opus_decoder_init(OpusDecoder *st, int32_t Fs, int channels)
// {
//    void *silk_dec;
//    CELTDecoder *celt_dec;
//    int ret, silkDecSizeBytes;

//    OPUS_CLEAR((char*)st, opus_decoder_get_size(channels)); //This clears the memory of the decoder struct and makes sure all values are initialized.
//    /* Initialize SILK encoder */
//    ret = silk_Get_Decoder_Size(&silkDecSizeBytes);
//    if (ret)
//       return OPUS_INTERNAL_ERROR;

//    silkDecSizeBytes = align(silkDecSizeBytes);
//    st->silk_dec_offset = align(sizeof(OpusDecoder)); // this sets the offset of the silk decoder to be a proper address(the nearest sizeof(void*)) after the declared opus decoder
//    st->celt_dec_offset = st->silk_dec_offset+silkDecSizeBytes; // this sets the celt offset to be after the offset + the size of the silk decoder. (since we aligned silkdecsize we don't need an align here)
//    silk_dec = (char*)st+st->silk_dec_offset; // actually assigns the pointer to the silk decoder struct
//    celt_dec = (CELTDecoder*)((char*)st+st->celt_dec_offset); // assigns the celt decoder pointer
//    st->stream_channels = st->channels = channels; //channels

//    st->Fs = Fs; //sample rate
//    st->DecControl.API_sampleRate = st->Fs;
//    st->DecControl.nChannelsAPI      = st->channels;

//    /* Reset decoder */
//    ret = silk_InitDecoder( silk_dec ); // inits silk decoder for mono and/or stereo depending on channel count
//    if(ret)return OPUS_INTERNAL_ERROR;

//    /* Initialize CELT decoder */
//    ret = celt_decoder_init(celt_dec, Fs, channels); // sets values of celt struct and finds downsampling factor
//    if(ret!=OPUS_OK)return OPUS_INTERNAL_ERROR;

//    celt_decoder_ctl(celt_dec, CELT_SET_SIGNALLING(0));

//    st->prev_mode = 0;
//    st->frame_size = Fs/400;
//    return OPUS_OK;
// }

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


int opus_decode_native(OpusDecoder *st, const unsigned char *data,
      int len, float *pcm, int frame_size,
      int self_delimited, int *packet_offset)
{
   int i, nb_samples;
   int count, offset;
   unsigned char toc;
   int tot_offset;
   /* 48 x 2.5 ms = 120 ms */
   short size[48];

   tot_offset = 0;
   st->mode = opus_packet_get_mode(data);
   st->bandwidth = opus_packet_get_bandwidth(data);
   st->frame_size = opus_packet_get_samples_per_frame(data, st->Fs);
   st->stream_channels = opus_packet_get_nb_channels(data);
   //set state variables from ToC byte in front of packet (FC in our CELT full band stereo case)

   count = opus_packet_parse_impl(data, len, self_delimited, &toc, NULL, size, &offset); // unncessary for me, count is always 1 frame
   if (count < 0)
      return count;

   data += offset; // will need to just parse through ToC byte and then move data pointer to the actual start of the frame data
   tot_offset += offset;

   if (count*st->frame_size > frame_size) // dont need this check either
      return OPUS_BUFFER_TOO_SMALL;
   nb_samples=0;
   for (i=0;i<count;i++) //wont need a for loop since count is always 1 frame
   {
      int ret;
      ret = opus_decode_frame(st, data, size[i], pcm, frame_size-nb_samples); //we dont need to subtract nb_samples from frame_size since we are only doing one frame
      if (ret<0)
         return ret;
      data += size[i]; //the data assignment is just for the multiple call, which we wont be doing
      tot_offset += size[i];
      pcm += ret*st->channels;
      nb_samples += ret;
   }
   if (packet_offset != NULL)
      *packet_offset = tot_offset;
   return nb_samples;
}









int opus_decode(OpusDecoder *st, const unsigned char *data,
      int len, int16_t *pcm, int frame_size) //doesnt do too much, just does some checks, then throws it to the actual decoder
{
   VARDECL(float, out);
   int ret, i;
   ALLOC_STACK;

   if(frame_size<0)return OPUS_BAD_ARG;

   ALLOC(out, frame_size*st->channels, float);

   ret = opus_decode_native(st, data, len, out, frame_size, 0, NULL);
   if (ret > 0)
   {
      for (i=0;i<ret*st->channels;i++)
         pcm[i] = FLOAT2INT16(out[i]);
   }
   RESTORE_STACK;
   return ret;
}