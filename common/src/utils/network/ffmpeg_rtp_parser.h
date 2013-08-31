#ifndef __FFMPEG_RTP_PARSER_H
#define __FFMPEG_RTP_PARSER_H

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

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, QnAbstractMediaDataPtr& result) override;
private:
    QnMediaContextPtr m_context;
    QnAbstractMediaDataPtr m_nextDataPacket;
    qint64 m_position;
};

#endif // __FFMPEG_RTP_PARSER_H
