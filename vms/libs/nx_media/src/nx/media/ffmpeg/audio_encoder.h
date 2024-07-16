// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx::media::ffmpeg {

class NX_MEDIA_API AudioEncoder
{
public:
    AudioEncoder();
    ~AudioEncoder();

    bool initialize(
        AVCodecID codec,
        int sampleRate,
        AVSampleFormat format,
        AVChannelLayout layout,
        int bitrate);
    bool sendFrame(uint8_t* data, int size);
    bool receivePacket(QnWritableCompressedAudioDataPtr& result);
    CodecParametersPtr codecParams();

private:
    void close();

private:
    AVFrame* m_inputFrame = nullptr;
    AVPacket* m_outputPacket = nullptr;
    AVCodecContext* m_encoderContext = nullptr;
    CodecParametersPtr m_codecParams;
};

} // namespace nx::media::ffmpeg
