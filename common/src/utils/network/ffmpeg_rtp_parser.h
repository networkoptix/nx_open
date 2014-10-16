#ifndef __FFMPEG_RTP_PARSER_H
#define __FFMPEG_RTP_PARSER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QByteArray>
#include <QMap>

#include "rtp_stream_parser.h"
#include "../media/nalUnits.h"
#include "rtpsession.h"


class QnFfmpegRtpParser: public QnRtpVideoStreamParser
{
public:
    QnFfmpegRtpParser();
    virtual ~QnFfmpegRtpParser();

    virtual void setSDPInfo(QList<QByteArray> sdpInfo) override;
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, bool& gotData) override;

    qint64 position() const { return m_position; }
    QnMediaContextPtr mediaContext() const { return m_context; }
private:
    QnMediaContextPtr m_context;
    QnAbstractMediaDataPtr m_nextDataPacket;
    QnByteArray* m_nextDataPacketBuffer;
    qint64 m_position;
};
typedef QSharedPointer<QnFfmpegRtpParser> QnFfmpegRtpParserPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif // __FFMPEG_RTP_PARSER_H
