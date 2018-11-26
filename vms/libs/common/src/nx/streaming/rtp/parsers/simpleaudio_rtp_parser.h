#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

namespace nx::streaming::rtp {

// used for G711, G726 e.t.c simple audio codecs with one frame per packet
class SimpleAudioParser: public AudioStreamParser
{
public:
    SimpleAudioParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, bool& gotData) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
    void setCodecId(AVCodecID codecId);
    void setBitsPerSample(int value);
    void setSampleFormat(AVSampleFormat sampleFormat);
private:
    QnConstMediaContextPtr m_context;
    QSharedPointer<QnRtspAudioLayout> m_audioLayout;
    int m_channels;
    int m_bits_per_coded_sample;
    AVCodecID m_codecId;
    AVSampleFormat m_sampleFormat;
};

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS

