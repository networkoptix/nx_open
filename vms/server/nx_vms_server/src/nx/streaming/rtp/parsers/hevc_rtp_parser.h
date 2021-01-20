#pragma once

#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <nx/streaming/video_data_packet.h>

#include <utils/media/hevc_common.h>
#include <nx/network/buffer.h>

namespace nx::streaming::rtp {

struct HevcContext
{
    int rtpChannel = 0;
    int spropMaxDonDiff = 0;
    boost::optional<nx::Buffer> spropVps = boost::none;
    boost::optional<nx::Buffer> spropSps = boost::none;
    boost::optional<nx::Buffer> spropPps = boost::none;
    boost::optional<nx::Buffer> spropSei = boost::none;
    bool inStreamVpsFound = false;
    bool inStreamSpsFound = false;
    bool inStreamPpsFound = false;
    bool keyDataFound = false;
    int width = -1;
    int height = -1;
};

/**
 * Implements RTP payload parsing for HEVC according to RFC 7798
 */
class HevcParser: public VideoStreamParser
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
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

    // Implementation of StreamParser::setSDPInfo
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

private:

    // Returns true if RTP header is correct. In this case value of outIsFatalError
    // is undefined and we can ignore it. If this method has returned false and
    // value of outIsFatalError is false we can just skip the data,
    // otherwise we must reset the parser.
    Result processRtpHeader(
        uint8_t** outPayload,
        int* outPayloadLength,
        bool* outIsFatalError,
        uint32_t* outRtpTimestamp,
        uint16_t* sequenceNumber);

    int calculateFullRtpHeaderSize(
        const uint8_t* rtpHeaderStart,
        int bufferSize) const;

    bool detectPacketLoss(
        const RtpHeader* rtpHeader);

    Result handlePacketLoss(
        int previousSequenceNumber,
        int currentSequenceNumber);

    bool isApropriatePayloadType(const RtpHeader* rtpHeader) const;

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
    void addSdpParameterSetsIfNeeded(QnByteArray& buffer);

    void createVideoDataIfNeeded(bool* outGotData, uint32_t rtpTimestamp);
    QnCompressedVideoDataPtr createVideoData(const uint8_t* rtpBuffer, uint32_t rtpTime);

    void parseFmtp(const QStringList& fmtp);
    bool extractPictureDimensionsFromSps(const nx::Buffer& rawSps);
    bool extractPictureDimensionsFromSps(const uint8_t* buffer, int bufferLength);

    void reset(bool softReset = false); //< Always returns false
    void addChunk(int bufferOffset, int payloadLength, bool hasStartCode);
private:
    HevcContext m_context;

    int m_numberOfNalUnits = 0;
    int m_previousPacketSequenceNumber = -1; //< TODO: #dmishin name this constant
    int m_videoFrameSize = 0;

    uint32_t m_lastRtpTimestamp = 0;
    uint32_t m_lastCreatedPacketTimestamp = 0;
    bool m_trustMarkerBit = true;
    bool m_gotMarkerBit = false;

    const uint8_t* m_rtpBufferBase;
};

} // namespace nx::streaming::rtp
