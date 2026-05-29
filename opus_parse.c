//includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//constants
//Since we know we are downloading the opus file at fullband 48kHz we can set constants for sampling rate etc.

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define MAX_FRAME_SIZE 2880
#define MAX_PACKET_SIZE 1500

//eventually I will add reading of opus tags to display to lcd

//typedef / struct
typedef struct {
    uint64_t granulepos;
    uint8_t num_of_segments;
    uint8_t segment_table[255];
} Oggs;

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
        int num_samples = opus_decode(/*decoder state*/, packet_buf, pack_len, pcm, MAX_FRAME_SIZE);
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
