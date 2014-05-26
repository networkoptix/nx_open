#ifndef __H264_RTP_PARSER_H
#define __H264_RTP_PARSER_H

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include "rtp_stream_parser.h"
#include "../media/nalUnits.h"
#include "rtpsession.h"


class CLH264RtpParser: public QnRtpVideoStreamParser
{
public:
    CLH264RtpParser();
    virtual ~CLH264RtpParser();
    virtual void setSDPInfo(QList<QByteArray> lines) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, QnAbstractMediaDataPtr& result) override;
private:
    QMap <int, QByteArray> m_allNonSliceNal;
    QList<QByteArray> m_sdpSpsPps;
    boost::optional<SPSUnit> m_sps;
    int m_frequency;
    int m_rtpChannel;
    int m_prevSequenceNum;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    bool m_keyDataExists;
    bool m_frameExists;
    quint64 m_timeCycles;
    quint64 m_timeCycleValue;
    quint64 m_lastTimeStamp;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;

    QnCompressedVideoDataPtr m_videoData;
    //QnByteArray m_videoBuffer;
    int m_videoFrameSize;
private:
    void serializeSpsPps(QnByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
    QnCompressedVideoDataPtr createVideoData(const quint8* rtpBuffer, quint32 rtpTime, const RtspStatistic& statistics);
    bool clearInternalBuffer(); // function always returns false to convenient exit from main routine
    void updateNalFlags(int nalUnitType);
    int getSpsPpsSize() const;
};

#endif
