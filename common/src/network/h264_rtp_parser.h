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

    virtual bool processData(
        quint8* rtpBufferBase, 
        int bufferOffset, 
        int bytesRead, 
        const RtspStatistic& statistics, 
        bool& gotData) override;

private:
    QMap <int, QByteArray> m_allNonSliceNal;
    QList<QByteArray> m_sdpSpsPps;
    SPSUnit m_sps;
    bool m_spsInitialized;
    bool m_canUseMarkerBit;
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

    int m_videoFrameSize;
    std::vector<quint8> m_nextFrameChunksBuffer;
    mutable bool m_previousPacketHasMarkerBit;

private:
    void serializeSpsPps(QnByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);

    QnCompressedVideoDataPtr createVideoData(
        const quint8            *rtpBuffer,
        quint32                 rtpTime,
        const RtspStatistic     &statistics
    );

    bool clearInternalBuffer(); // function always returns false to convenient exit from main routine
    void updateNalFlags(int nalUnitType, const quint8* data, int dataLen);
    int getSpsPpsSize() const;

    bool isPacketStartsNewFrame(int packetType, const quint8*, const quint8* rtpBufferStart, const quint8* bufferEnd) const;
    bool isSliceNal(quint8 nalUnitType) const;
    bool isFirstSliceNal(const quint8 nalType, const quint8* data, int dataLen ) const;
    bool isIFrame(const quint8* data, int dataLen) const;

    void backupNextFrameChunks(const char* bufferStart);
};

#endif // ENABLE_DATA_PROVIDERS

#endif
