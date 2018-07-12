#ifndef __G711_RTP_PARSER_H
#define __G711_RTP_PARSER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/rtp_stream_parser.h>

// used for G711, G726 e.t.c simple audio codecs with one frame per packet

class QnSimpleAudioRtpParser: public QnRtpAudioStreamParser
{
public:
    QnSimpleAudioRtpParser();
    virtual ~QnSimpleAudioRtpParser();
    virtual void setSdpInfo(QList<QByteArray> sdpInfo) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const QnRtspStatistic& statistics, bool& gotData) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
    void setCodecId(AVCodecID codecId);
    void setBitsPerSample(int value);
    void setSampleFormat(AVSampleFormat sampleFormat);
private:
    QnConstMediaContextPtr m_context;
    QSharedPointer<QnRtspAudioLayout> m_audioLayout;
    int m_frequency;
    int m_channels;
    int m_bits_per_coded_sample;
    AVCodecID m_codecId;
    AVSampleFormat m_sampleFormat;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __AAC_RTP_PARSER_H
