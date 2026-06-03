#ifndef OPUS_VARS_H
#define OPUS_VARS_H

#include <stdlib.h>
#include <math.h>

//return vals
#define OPUS_BAD_ARG -1

//constants
#define CELT_SIG_SCALE 32768.f

//functions
#define MAX32(a,b) ((a) > (b) ? (a) : (b))
#define MIN32(a,b) ((a) < (b) ? (a) : (b))

//static inline functions
static inline int16_t FLOAT2INT16(float x)
{
   x = x*CELT_SIG_SCALE;
   x = MAX32(x, -32768);
   x = MIN32(x, 32767);
   return (int16_t)((int)(floor(.5+x)));
}



#endif