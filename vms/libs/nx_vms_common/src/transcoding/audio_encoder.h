// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <transcoding/ffmpeg_audio_resampler.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx::media::ffmpeg {

class NX_VMS_COMMON_API AudioEncoder
{
public:
    AudioEncoder();
    ~AudioEncoder();

    bool open(
        AVCodecID codec,
        int sampleRate,
        AVSampleFormat format,
        AVChannelLayout layout,
        int bitrate,
        int dstFrameSize = 0);
    bool sendFrame(uint8_t* data, int size);
    bool sendFrame(AVFrame* inputFrame);
    bool receivePacket(QnWritableCompressedAudioDataPtr& result);

    CodecParametersPtr codecParameters() const;

    /**
     * \brief Sets destination sample rate.
     * \param value Desired destination sample rate.
     */
    void setSampleRate(int value);

private:
    QnWritableCompressedAudioDataPtr createMediaDataFromAVPacket(const AVPacket& packet);
    void close();

private:
    bool m_flushMode = false;
    int m_dstSampleRate = 0;
    int m_bitrate = -1;
    int64_t m_ptsUs = 0;
    AVFrame* m_inputFrame = nullptr;
    AVCodecContext* m_encoderContext = nullptr;
    CodecParametersPtr m_codecParams;
    std::unique_ptr<FfmpegAudioResampler> m_resampler;
    AVSampleFormat m_sourceformat{};
};

} // namespace nx::media::ffmpeg
