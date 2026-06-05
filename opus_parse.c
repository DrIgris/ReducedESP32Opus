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
#include "opus_parse.h"

//eventually I will add reading of opus tags to display to lcd

//variables
uint16_t preskip;
uint16_t pcm[MAX_FRAME_SIZE * CHANNELS];

//functions

int readHead(FILE* f){
    Oggs o = {0};
    readOggs(f, &o);

    fseek(f, 8, SEEK_CUR); //OpusHead 
    fseek(f, 1, SEEK_CUR); //version
    fseek(f, 1, SEEK_CUR); //channel count
    fread(&preskip, 1, 2, f); //preskip
    fseek(f, 4, SEEK_CUR); //input sample rate
    fseek(f, 2, SEEK_CUR); //output gain
    fseek(f, 1, SEEK_CUR); //channel mapping family
    return 0;
}

int readTags(FILE* f){ //Ill eventually parse and use these for LCD info
    Oggs o = {0};
    readOggs(f, &o);

    uint32_t vendor_string_len;
    uint32_t comment_amt;

    fseek(f, 8, SEEK_CUR); //OpusTags
    fread(&vendor_string_len, 4, 1, f); //vendor string length
    char vendor[vendor_string_len + 1];
    fread(vendor, 1, vendor_string_len, f);
    vendor[vendor_string_len] = '\0';  // null terminate

    fread(&comment_amt, 4, 1, f);
    for (uint32_t i = 0; i < comment_amt; i++) {
        uint32_t comment_len;
        fread(&comment_len, 4, 1, f);
        char comment[comment_len + 1];
        fread(comment, 1, comment_len, f);
        comment[comment_len] = '\0';  // null terminate
        // comment is now e.g. "TITLE=my song"
        // parse on the '='
    }
    return 0;
}

int readOggs(FILE* f, Oggs* o){
    fseek(f, 4, SEEK_CUR); //oggs header
    fseek(f, 1, SEEK_CUR); //version
    fseek(f, 1, SEEK_CUR); //header type
    fread(&o->granulepos, 1, 8, f); //granule position
    fseek(f, 4, SEEK_CUR); //bitstream serial number
    fseek(f, 4, SEEK_CUR); //page sequence number
    fseek(f, 4, SEEK_CUR); //checksum
    fread(&o->num_of_segments, 1, 1, f); //number of segments
    fread(&o->segment_table, 1, o->num_of_segments, f); //segment table
    return 0;
}

int readPackets(FILE* f) {
    Oggs o = {0};
    readOggs(f, &o);

    uint8_t packet_buf[MAX_PACKET_SIZE];

    int i = 0;
    while(i < o.num_of_segments) {
        int pack_len = 0;
        while (i < o.num_of_segments && o.segment_table[i] == 255) {
            pack_len += o.segment_table[i++];
        }
        pack_len += o.segment_table[i++];

        fread(packet_buf, 1, pack_len, f);
        int num_samples = opus_decode(/*decoder state,*/ packet_buf, pack_len, pcm, MAX_FRAME_SIZE);
        //discard preskip once
    }
    return 0;
}

int decodeFile(FILE* f) { //open file in SD card manager file and pass into function
    
    
    readHead(f);
    readTags(f);
    readPackets(f);
    while(!feof(f)) {
        readPackets(f);
    }
    return 0;
}
