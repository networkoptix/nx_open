#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

namespace nx::streaming::rtp {

class Mpeg12AudioParser: public AudioStreamParser
{
    struct DataChunk
    {
        const uint8_t* start = nullptr;
        int length = 0;
    };

public:
    Mpeg12AudioParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual Result processData(quint8* rtpBufferBase, int bufferOffset, int readed, bool& gotData) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

private:
    void createAudioFrame();
    Result updateFromMpegAudioHeader(const uint8_t* mpegAudioHeaderStart, int payloadSize);
    void cleanUp();

private:
    QnConstMediaContextPtr m_mediaContext;
    QSharedPointer<QnRtspAudioLayout> m_audioLayout;
    quint32 m_rtpTimestamp = 0;
    int m_audioFrameSize = 0;
    bool m_skipFragmentsUntilNextAudioFrame = false;

    std::vector<DataChunk> m_audioDataChunks;
};

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS