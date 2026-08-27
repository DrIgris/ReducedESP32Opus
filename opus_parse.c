
#include <stdlib.h>
#include "opus_parse.h"
#include "decoder.h"



//eventually I will add reading of opus tags to display to lcd

//variables
uint16_t preskip;
float pcm[MAX_FRAME_SIZE * CHANNELS];
OpusDecoder* OpDec;

int deb = 0;

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

int readPackets(FILE* f, FILE* out) {
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
        if (deb < 200)
            printf("pack_len=%d first_bytes=%02x %02x %02x %02x\n",
       pack_len, packet_buf[0], packet_buf[1], packet_buf[2], packet_buf[3]);

        int num_samples = opus_decode(OpDec, packet_buf, pack_len, pcm);
        if (deb < 200)
            printf("num_samples=%d pcm[0]=%f pcm[1]=%f\n", num_samples, pcm[0], pcm[1]);
        deb++;
        float *out_ptr = pcm;
        int out_len = num_samples;
        if(preskip > 0) {
            int to_drop = preskip < num_samples ? preskip : num_samples;
            out_ptr = pcm + to_drop * CHANNELS; 
            out_len = num_samples - to_drop;
            preskip -= to_drop;
        }

        if (out_len > 0) {
            fwrite(out_ptr, sizeof(float), out_len * CHANNELS, out);
        }
    }
    return 0;
}

int decodeFile(FILE* f, FILE* out) { //open file in SD card manager file and pass into function
    int* error = 0;
    OpDec = opus_decoder_create(SAMPLE_RATE, CHANNELS, error);
    if(error == OPUS_ALLOC_FAIL)
        return 1;
    
    readHead(f);
    readTags(f);
    readPackets(f, out);
    while(!feof(f)) {
        readPackets(f, out);
    }
    return 0;
}

