#ifndef __AUDIO_PROCESSOR_H_
#define __AUDIO_PROCESSOR_H_

#include "utils/common/byte_array.h"
#include "core/datapacket/audio_data_packet.h"

class QnAudioProcessor
{
public:
    static int downmix(quint8* data, int size, AVCodecContext* ctx);

    static QnCodecAudioFormat downmix(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat float2int16(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat float2int32(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat int32Toint16(QnByteArray& audio, QnCodecAudioFormat format);
};

#endif // __AUDIO_PROCESSOR_H_
