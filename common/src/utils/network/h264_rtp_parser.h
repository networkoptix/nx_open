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
    bool m_keyDataExists;
    quint64 m_timeCycles;
    quint64 m_timeCycleValue;
    quint64 m_lastTimeStamp;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;
    int m_prevSequenceNum;

    QnCompressedVideoDataPtr m_videoData;
    CLByteArray m_videoBuffer;
    bool m_frameExists;
private:
    void serializeSpsPps(CLByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
    QnCompressedVideoDataPtr createVideoData(quint32 rtpTime, const RtspStatistic& statistics);
    bool clearInternalBuffer(); // function always returns false to convenient exit from main routine
    void updateNalFlags(int nalUnitType);
    int getSpsPpsSize() const;
};

#endif
