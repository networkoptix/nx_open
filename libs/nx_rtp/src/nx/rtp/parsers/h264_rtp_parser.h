// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include <nx/codec/nal_units.h>
#include <nx/media/video_data_packet.h>
#include <nx/rtp/parsers/rtp_chunk_buffer.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_RTP_API H264Parser: public VideoStreamParser
{
public:
    H264Parser();
    virtual ~H264Parser() = default;
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;
    virtual void clear() override;

private:
    RtpChunkBuffer m_chunks;
    QMap <int, QByteArray> m_allNonSliceNal;
    std::vector<uint8_t> m_sdpSpsPps;
    SPSUnit m_sps;
    bool m_spsInitialized;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    bool m_keyDataExists;
    int m_idrCounter;
    bool m_frameExists;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;
    quint32 m_lastRtpTime;
    CodecParametersConstPtr m_codecParameters;

private:
    void decodeSpsInfo(const uint8_t* data, int size);
    bool isFirstSliceNal(const quint8 nalType, const quint8* data, int dataLen) const;
    bool isPacketStartsNewFrame(const quint8* curPtr, const quint8* bufferEnd) const;

    QnCompressedVideoDataPtr createVideoData(const quint8* rtpBuffer, quint32 rtpTime);
   // handleAggregationPacket();
    bool isBufferOverflow() const;

    void updateNalFlags(const quint8* data, int dataLen);
    void handleSingleNalunitPacket(uint8_t* buffer, int offset, int size);
};

} // namespace nx::rtp
