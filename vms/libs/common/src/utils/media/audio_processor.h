#pragma once

#include "utils/common/byte_array.h"
#include "nx/streaming/audio_data_packet.h"

class QnAudioProcessor
{
public:
    static QnCodecAudioFormat downmix(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat float2int16(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat float2int32(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat int32Toint16(QnByteArray& audio, QnCodecAudioFormat format);
};
