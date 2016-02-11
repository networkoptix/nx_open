#ifndef NX_RTP_PARSER_H
#define NX_RTP_PARSER_H

#include <QByteArray>
#include <QMap>

#include <nx/streaming/rtp_stream_parser.h>

class QnRtspStatistic;

class QnNxRtpParser: public QnRtpVideoStreamParser
{
public:
    QnNxRtpParser();
    virtual ~QnNxRtpParser();

    virtual void setSDPInfo(QList<QByteArray> sdpInfo) override;
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const QnRtspStatistic& statistics, bool& gotData) override;

    qint64 position() const { return m_position; }
    QnConstMediaContextPtr mediaContext() const { return m_context; }
private:
    QnConstMediaContextPtr m_context;
    QnAbstractMediaDataPtr m_nextDataPacket;
    QnByteArray* m_nextDataPacketBuffer;
    qint64 m_position;
};
typedef QSharedPointer<QnNxRtpParser> QnNxRtpParserPtr;

#endif // NX_RTP_PARSER_H
