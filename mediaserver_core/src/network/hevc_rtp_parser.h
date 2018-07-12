#pragma once

#include <nx/streaming/rtp_stream_parser.h>
#include <nx/streaming/video_data_packet.h>

#include <utils/media/hevc_common.h>

namespace nx {
namespace network {
namespace rtp {

struct HevcContext
{
    int frequency = 90000;
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
class HevcParser: public QnRtpVideoStreamParser
{
    // Decoding order number field type.
    enum class DonType
    {
        donl,
        dond
    };

public:

    // Implementation of QnRtpStreamParser::processData
    virtual bool processData(
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        const QnRtspStatistic& statistics,
        bool& gotData) override;

    // Implementation of QnRtpStreamParser::setSDPInfo
    virtual void setSdpInfo(QByteArrayList lines) override;

private:

    // Returns true if RTP header is correct. In this case value of outIsFatalError
    // is undefined and we can ignore it. If this method has returned false and
    // value of outIsFatalError is false we can just skip the data,
    // otherwise we must reset the parser.
    bool processRtpHeader(
        uint8_t** outPayload,
        int* outPayloadLength,
        bool* outIsFatalError,
        uint32_t* outRtpTimestamp);

    int calculateFullRtpHeaderSize(
        const uint8_t* rtpHeaderStart,
        int bufferSize) const;

    bool detectPacketLoss(
        const RtpHeader* rtpHeader);

    bool handlePacketLoss(
        int previousSequenceNumber,
        int currentSequenceNumber);

    bool isApropriatePayloadType(const RtpHeader* rtpHeader) const;

    bool handlePayload(const uint8_t* payload, int payloadLength);

    bool handleSingleNalUnitPacket(
        const nx::media_utils::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);
    bool handleAggregationPacket(
        const nx::media_utils::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);
    bool handleFragmentationPacket(
        const nx::media_utils::hevc::NalUnitHeader* header,
        const uint8_t* payload, //< payload is already shifted for header size
        int payloadLength);
    bool handlePaciPacket(
        const nx::media_utils::hevc::NalUnitHeader* header,
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
        nx::media_utils::hevc::NalUnitType header,
        const uint8_t* payload,
        int payloadLength);

    void insertPayloadHeader(
        uint8_t** payloadStart,
        int* payloadLength,
        nx::media_utils::hevc::NalUnitType unitType,
        uint8_t tid);

    int additionalBufferSize() const;
    void addSdpParameterSetsIfNeeded(QnByteArray& buffer);

    void createVideoDataIfNeeded(
        bool* outGotData,
        const QnRtspStatistic& statistic,
        uint32_t rtpTimestamp);

    QnCompressedVideoDataPtr createVideoData(
        const uint8_t* rtpBuffer,
        uint32_t rtpTime,
        const QnRtspStatistic& statistics);

    void parseRtpMap(const nx::Buffer& rtpMapLine);
    void parseFmtp(const nx::Buffer& fmtpLine);
    bool extractPictureDimensionsFromSps(const nx::Buffer& rawSps);
    bool extractPictureDimensionsFromSps(const uint8_t* buffer, int bufferLength);

    bool reset(bool softReset = false); //< Always returns false

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

} // namespace rtp
} // namespace network
} // namespace nx
