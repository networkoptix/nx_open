#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

namespace nx::streaming::rtp {

class AacParser: public AudioStreamParser
{
public:
    AacParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, bool& gotData) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
private:
    int m_sizeLength = 0; // 0 if constant size. see RFC3640
    int m_constantSize = 0;
    int m_indexLength = 0;
    int m_indexDeltaLength = 0;
    int m_CTSDeltaLength = 0;
    int m_DTSDeltaLength = 0;
    int m_randomAccessIndication = 0;
    int m_streamStateIndication = 0;
    int m_profile = 0;
    int m_bitrate = 0;
    int m_channels = 0;
    int m_streamtype = 0;
    bool m_auHeaderExists = false;

    QnConstMediaContextPtr m_context;
    QSharedPointer<QnRtspAudioLayout> m_audioLayout;
};

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS
