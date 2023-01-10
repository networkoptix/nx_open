// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/audio_data_packet.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

namespace nx::media::ffmpeg {

class AudioEncoder
{
public:
    AudioEncoder();
    ~AudioEncoder();

    bool initialize(
        AVCodecID codec,
        int sampleRate,
        int channels,
        AVSampleFormat format,
        uint64_t layout,
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