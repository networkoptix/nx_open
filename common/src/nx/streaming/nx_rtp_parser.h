#ifndef NX_RTP_PARSER_H
#define NX_RTP_PARSER_H

#include <QByteArray>
#include <QMap>

#include <nx/streaming/rtp_stream_parser.h>

class QnRtspStatistic;

class QnNxRtpParser: public QnRtpVideoStreamParser
{
public:
    /** @param debugSourceId Human-readable stream source id for logging. */
    QnNxRtpParser(const QString& debugSourceId = QString());

    virtual ~QnNxRtpParser();

    virtual void setSDPInfo(QList<QByteArray> sdpInfo) override;
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const QnRtspStatistic& statistics, bool& gotData) override;

    qint64 position() const { return m_position; }
    QnConstMediaContextPtr mediaContext() const { return m_context; }

    /**
     * Don't parse incoming audio packets if value is false.
     */
    void setAudioEnabled(bool value);

private:
    void writeDetectionMetadataToLogFile(const QnAbstractMediaDataPtr& metadata);

private:
    const QString m_debugSourceId;
    QnConstMediaContextPtr m_context;
    QnAbstractMediaDataPtr m_nextDataPacket;
    QnByteArray* m_nextDataPacketBuffer;
    qint64 m_position;
    bool m_isAudioEnabled;
    QFile m_analyticsMetadataLogFile;
    bool m_isAnalyticsMetadataLogFileOpened = false;
    qint64 m_lastFramePtsUs; //< Intended for debug.
};

typedef QSharedPointer<QnNxRtpParser> QnNxRtpParserPtr;

#endif // NX_RTP_PARSER_H
