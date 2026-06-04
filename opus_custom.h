#ifndef OPUS_CUSTOM_H
#define OPUS_CUSTOM_H

/** State of the decoder. One decoder state is needed for each stream.
    It is initialised once at the beginning of the stream. Do *not*
    re-initialise the state for every frame */
typedef struct OpusCustomDecoder OpusCustomDecoder;

/** The mode contains all the information necessary to create an
    encoder. Both the encoder and decoder need to be initialised
    with exactly the same mode, otherwise the quality will be very
    bad */
typedef struct OpusCustomMode OpusCustomMode;

#endif