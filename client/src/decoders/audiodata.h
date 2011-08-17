#ifndef AUDIODATA_H
#define AUDIODATA_H

#include <QtMultimedia/QAudioFormat>

#include "utils/common/aligned_data.h"

// input to the decoder
struct CLAudioData
{
    int codec;

    const uchar *inbuf; // pointer to compressed data
    int inbuf_len; // compressed data len

    CLAlignedData *outbuf; // pointer where decoder puts decompressed data;
    unsigned long outbuf_len;

    QAudioFormat format;
};

#endif // AUDIODATA_H
