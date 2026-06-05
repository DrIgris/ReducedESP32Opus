#ifndef OPUS_PARSE_H
#define OPUS_PARSE_H

#include <stdint.h>
#include <stdio.h>
#include "opus_vars.h"

typedef struct {
    uint64_t granulepos;
    uint8_t num_of_segments;
    uint8_t segment_table[255];
} Oggs;

int readHead(FILE* f);

int readTags(FILE* f);

int readOggs(FILE* f, Oggs* o);

int readPackets(FILE* f);

int decodeFile(FILE* f);

#endif