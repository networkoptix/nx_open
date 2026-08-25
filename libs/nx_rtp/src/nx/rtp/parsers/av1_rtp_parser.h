// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <nx/codec/av1/av1_common.h>
#include <nx/codec/av1/sequence_header.h>
#include <nx/media/video_data_packet.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/rtp/rtp.h>

namespace nx::rtp {

struct NX_RTP_API Av1Context
{
    int rtpChannel = 0;

    // The SDP fmtp line also carries profile, level-idx and tier, but they are not parsed: the
    // sequence header in the bitstream is authoritative and is what the codec parameters are
    // built from.
    std::optional<nx::media::av1::SequenceHeader> sequenceHeader;
    int width = -1;
    int height = -1;
};

/**
 * Implements RTP payload parsing for AV1 according to the AOMedia "RTP Payload Format For AV1"
 * v1.0. Produces one QnCompressedVideoData per temporal unit in the AV1 low overhead bitstream
 * format, i.e. a sequence of OBUs each having obu_has_size_field set.
 */
class NX_RTP_API Av1Parser: public VideoStreamParser
{
public:
    Av1Parser();

    // Implementation of StreamParser::processData
    virtual Result processData(const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

    // Implementation of StreamParser::setSdpInfo
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual void clear() override;

private:
    struct AggregationHeader
    {
        bool continuesPreviousObu = false; //< Z
        bool continuesInNextPacket = false; //< Y
        int obuElementCount = 0; //< W, 0 means every element has a length field.
        // N, the first packet of a coded video sequence, is not decoded: it is advisory, and a
        // sequence header can only be trusted once it has actually been seen in the bitstream.

        void decode(uint8_t value);
    };

    Result handlePayload(const uint8_t* payload, int payloadLength);
    Result handleObuElement(const uint8_t* data, int size, bool isContinuation, bool willContinue);
    /** Rewrites the OBU header, adds obu_size and appends the OBU to the temporal unit. */
    Result appendObu(const uint8_t* data, int size);

    void updateContext(
        const nx::media::av1::ObuHeader& obuHeader, const uint8_t* payload, int size);

    void finishTemporalUnit(uint32_t rtpTimestamp, bool* gotData);
    /** The current temporal unit is damaged and will not be emitted. */
    void dropTemporalUnit();
    bool isKeyTemporalUnit() const;

    QnCompressedVideoDataPtr createVideoData(uint32_t rtpTime);
    void updateCodecParameters();

private:
    Av1Context m_context;

    std::vector<uint8_t> m_temporalUnit; //< Assembled OBU stream of the current temporal unit.
    std::vector<uint8_t> m_pendingObu; //< OBU fragmented over several packets.
    bool m_hasPendingObuFragment = false;

    bool m_temporalUnitDropped = false;
    bool m_temporalUnitComplete = false; //< The marker bit has been seen.
    bool m_waitingForSequenceHeader = true;
    bool m_sequenceHeaderInTemporalUnit = false;
    bool m_keyFrameInTemporalUnit = false;
    bool m_frameInTemporalUnit = false; //< An OBU_FRAME or OBU_FRAME_HEADER has been appended.

    std::optional<uint32_t> m_lastRtpTimestamp;
    std::optional<uint16_t> m_lastSequenceNumber;
    CodecParametersConstPtr m_codecParameters;
};

} // namespace nx::rtp
