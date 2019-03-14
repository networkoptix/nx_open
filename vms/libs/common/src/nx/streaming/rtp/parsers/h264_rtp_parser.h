#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <utils/media/nalUnits.h>

namespace nx::streaming::rtp {

class H264Parser: public VideoStreamParser
{
public:
    H264Parser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, bool& gotData) override;

private:
    QMap <int, QByteArray> m_allNonSliceNal;
    QList<QByteArray> m_sdpSpsPps;
    SPSUnit m_sps;
    bool m_spsInitialized;
    int m_rtpChannel;
    int m_prevSequenceNum;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    bool m_keyDataExists;
    int m_idrCounter;
    bool m_frameExists;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;

    //QnByteArray m_videoBuffer;
    int m_videoFrameSize;
    quint32 m_lastRtpTime;
    QString m_resourceId;
private:
    void serializeSpsPps(QnByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
    bool isFirstSliceNal(const quint8 nalType, const quint8* data, int dataLen) const;
    bool isPacketStartsNewFrame(const quint8* curPtr, const quint8* bufferEnd) const;

    QnCompressedVideoDataPtr createVideoData(const quint8* rtpBuffer, quint32 rtpTime);
    bool isBufferOverflow() const;

    bool clearInternalBuffer(); // function always returns false to convenient exit from main routine
    void updateNalFlags(const quint8* data, int dataLen);
    int getSpsPpsSize() const;
};

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS

