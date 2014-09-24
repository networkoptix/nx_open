#ifndef __AAC_RTP_PARSER_H
#define __AAC_RTP_PARSER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include "rtp_stream_parser.h"
#include "../media/nalUnits.h"
#include "decoders/audio/aac.h"


class QnAacRtpParser: public QnRtpAudioStreamParser
{
public:
    QnAacRtpParser();
    virtual ~QnAacRtpParser();
    virtual void setSDPInfo(QList<QByteArray> sdpInfo) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, QList<QnAbstractMediaDataPtr>& result) override;
    virtual QnResourceAudioLayoutPtr getAudioLayout() override;
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
    int m_frequency;
    int m_channels;
    int m_streamtype;
    QByteArray m_config;
    QByteArray m_mode;
    bool m_auHeaderExists;

    AACCodec m_aacHelper;
    QnMediaContextPtr m_context;
    QSharedPointer<QnRtspAudioLayout> m_audioLayout;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __AAC_RTP_PARSER_H
