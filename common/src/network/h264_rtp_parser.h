#ifndef __H264_RTP_PARSER_H
#define __H264_RTP_PARSER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include "core/datapacket/video_data_packet.h"
#include "rtp_stream_parser.h"
#include "utils/media/nalUnits.h"
#include "rtpsession.h"

const unsigned int MAX_ALLOWED_FRAME_SIZE = 1024*1024*10;

class CLH264RtpParser: public QnRtpVideoStreamParser
{
public:
    CLH264RtpParser();
    virtual ~CLH264RtpParser();
    virtual void setSDPInfo(QList<QByteArray> lines) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, bool& gotData) override;

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
    bool m_idrFound;
    bool m_frameExists;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;

    //QnByteArray m_videoBuffer;
    int m_videoFrameSize;
    bool m_previousPacketHasMarkerBit;
    std::vector<quint8> m_nextFrameChunksBuffer;
    quint32 m_lastRtpTime;
private:
    void serializeSpsPps(QnByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
    bool isSliceNal(quint8 nalUnitType) const;
    bool isFirstSliceNal(const quint8 nalType, const quint8* data, int dataLen) const;
    bool isPacketStartsNewFrame(
        const quint8* curPtr,
        const quint8* bufferEnd) const;
    void backupCurrentData(quint8* rtpBufferBase);
    bool isIFrame(const quint8* data, int dataLen) const;

    QnCompressedVideoDataPtr createVideoData(
        const quint8            *rtpBuffer,
        quint32                 rtpTime,
        const RtspStatistic     &statistics
        );
    bool isBufferOverflow() const;

    bool clearInternalBuffer(); // function always returns false to convenient exit from main routine
    void updateNalFlags(const quint8* data, int dataLen);
    int getSpsPpsSize() const;
};

#endif // ENABLE_DATA_PROVIDERS

#endif
