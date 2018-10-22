#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <decoders/audio/aac.h>

namespace nx::streaming::rtp {

class AacParser: public AudioStreamParser
{
public:
    AacParser();
    virtual ~AacParser();
    virtual void setSdpInfo(QList<QByteArray> sdpInfo) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, bool& gotData) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
private:
    int m_sizeLength; // 0 if constant size. see RFC3640
    int m_constantSize;
    int m_indexLength;
    int m_indexDeltaLength;
    int m_CTSDeltaLength;
    int m_DTSDeltaLength;
    int m_randomAccessIndication;
    int m_streamStateIndication;
    int m_profile;
    int m_bitrate;
    int m_channels;
    int m_streamtype;
    QByteArray m_config;
    QByteArray m_mode;
    bool m_auHeaderExists;

    AACCodec m_aacHelper;
    QnConstMediaContextPtr m_context;
    QSharedPointer<QnRtspAudioLayout> m_audioLayout;
};

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS
