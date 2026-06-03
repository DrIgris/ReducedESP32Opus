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
   if (len==0 || data==NULL) // lost data check
      return opus_decode_frame(st, NULL, 0, pcm, frame_size, 0);
   else if (len<0)
      return OPUS_BAD_ARG;

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
      ret = opus_decode_frame(st, data, size[i], pcm, frame_size-nb_samples, decode_fec); //we dont need to subtract nb_samples from frame_size since we are only doing one frame
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

   ret = opus_decode_native(st, data, len, out, frame_size, decode_fec, 0, NULL);
   if (ret > 0)
   {
      for (i=0;i<ret*st->channels;i++)
         pcm[i] = FLOAT2INT16(out[i]);
   }
   RESTORE_STACK;
   return ret;
}