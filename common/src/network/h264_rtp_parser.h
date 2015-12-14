#ifndef __H264_RTP_PARSER_H
#define __H264_RTP_PARSER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/rtp_stream_parser.h>
#include <utils/media/nalUnits.h>
#include <nx/streaming/rtsp_client.h>

class CLH264RtpParser: public QnRtpVideoStreamParser
{
public:
    CLH264RtpParser();
    virtual ~CLH264RtpParser();
    virtual void setSDPInfo(QList<QByteArray> lines) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const QnRtspStatistic& statistics, bool& gotData) override;

private:
    QMap <int, QByteArray> m_allNonSliceNal;
    QList<QByteArray> m_sdpSpsPps;
    SPSUnit m_sps;
    bool m_spsInitialized;
    int m_frequency;
    int m_rtpChannel;
    int m_prevSequenceNum;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    bool m_keyDataExists;
    bool m_frameExists;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;

    //QnByteArray m_videoBuffer;
    int m_videoFrameSize;

private:
    void serializeSpsPps(QnByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
    QnCompressedVideoDataPtr createVideoData(const quint8* rtpBuffer, quint32 rtpTime, const QnRtspStatistic& statistics);
    bool clearInternalBuffer(); // function always returns false to convenient exit from main routine
    void updateNalFlags(int nalUnitType);
    int getSpsPpsSize() const;
};

#endif // ENABLE_DATA_PROVIDERS

#endif
