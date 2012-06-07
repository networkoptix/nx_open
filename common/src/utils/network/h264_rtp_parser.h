#ifndef __H264_RTP_PARSER_H
#define __H264_RTP_PARSER_H

#include <QByteArray>
#include <QMap>

#include "rtp_stream_parser.h"
#include "../media/nalUnits.h"
#include "rtpsession.h"


class CLH264RtpParser: public QnRtpStreamParser
{
public:
    CLH264RtpParser();
    virtual ~CLH264RtpParser();
    virtual void setSDPInfo(QList<QByteArray> lines) override;

    virtual bool processData(quint8* rtpBuffer, int readed, const RtspStatistic& statistics, QList<QnAbstractMediaDataPtr>& result) override;
private:
    QMap <int, QByteArray> m_allNonSliceNal;
    QList<QByteArray> m_sdpSpsPps;
    SPSUnit m_sps;
    int m_frequency;
    int m_rtpChannel;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    quint64 m_timeCycles;
    quint64 m_timeCycleValue;
    quint64 m_lastTimeStamp;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;

    QnCompressedVideoDataPtr m_videoData;
private:
    void serializeSpsPps(CLByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
};

#endif
