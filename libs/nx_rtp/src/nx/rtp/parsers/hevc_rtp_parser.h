// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/codec/hevc/hevc_common.h>
#include <nx/media/video_data_packet.h>
#include <nx/rtp/parsers/rtp_chunk_buffer.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/buffer.h>

namespace nx::rtp {

struct NX_RTP_API HevcContext
{
    int rtpChannel = 0;
    int spropMaxDonDiff = 0;
    std::optional<QByteArray> vps = std::nullopt;
    std::optional<QByteArray> sps = std::nullopt;
    std::optional<QByteArray> pps = std::nullopt;
    std::optional<QByteArray> sei = std::nullopt;
    bool inStreamVpsFound = false;
    bool inStreamSpsFound = false;
    bool inStreamPpsFound = false;
    bool keyDataFound = false;
    bool sliceFound = false;
    int width = -1;
    int height = -1;
};

/**
 * Implements RTP payload parsing for HEVC according to RFC 7798
 */
class NX_RTP_API HevcParser: public VideoStreamParser
{
    // Decoding order number field type.
    enum class DonType
    {
        donl,
        dond
    };

public:
    HevcParser();

    // Implementation of StreamParser::processData
    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

    virtual void clear() override;

    // Implementation of StreamParser::setSDPInfo
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

private:
    Result handlePayload(const uint8_t* payload, int payloadLength);

    Result handleSingleNalUnitPacket(
        const nx::media::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);
    Result handleAggregationPacket(
        const nx::media::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);
    Result handleFragmentationPacket(
        const nx::media::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);
    Result handlePaciPacket(
        const nx::media::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);

    inline bool skipDonIfNeeded(
        const uint8_t** outPayload,
        int* outPayloadLength,
        DonType donType = DonType::donl);

    inline void skipPayloadHeader(const uint8_t** outPayload, int* outPayloadLength);
    inline void skipFuHeader(const uint8_t** outPayload, int* outPayloadLength);
    inline void goBackForPayloadHeader(const uint8_t** outPayload, int* outPayloadLength);

    void updateNalFlags(
        nx::media::hevc::NalUnitType header,
        const uint8_t* payload,
        int payloadLength);

    void insertPayloadHeader(
        uint8_t** payloadStart,
        int* payloadLength,
        nx::media::hevc::NalUnitType unitType,
        uint8_t tid);

    int additionalBufferSize() const;
    nx::utils::ByteArray buildParameterSetsIfNeeded();
    QnCompressedVideoDataPtr createVideoData(const uint8_t* rtpBuffer, uint32_t rtpTime);
    void parseFmtp(const QStringList& fmtp);
    bool extractPictureDimensionsFromSps(const uint8_t* buffer, int bufferLength);
    void reset(bool softReset = false); //< Always returns false
    void addChunk(nx::media::hevc::NalUnitType, int bufferOffset, int payloadLength, bool hasStartCode);

    bool isNewFrame(
        nx::media::hevc::NalUnitType unitType,
        const uint8_t* payload,
        int payloadLength);

private:
    RtpChunkBuffer m_chunks;
    HevcContext m_context;

    uint32_t m_lastRtpTimestamp = 0;
    const uint8_t* m_rtpBufferBase;
    CodecParametersConstPtr m_codecParameters;
    bool m_frameStartFound = false;
    bool m_gotData = false; //< It is only needed to forward got data from addChunk(need to be refactored)
    bool m_trustMarkerBit = true;
};

} // namespace nx::rtp
