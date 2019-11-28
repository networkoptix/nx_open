#include "onvif_metadata_rtp_parser.h"

#include <nx/utils/log/log.h>

#include <nx/streaming/rtp/rtp.h>

using namespace nx::streaming::rtp;

const std::set<QString> OnvifMetadataRtpParser::kSupportedCodecs = {
    "vnd.onvif.metadata", //< Uncompressed.
    "vnd.onvif.metadata.gzip", //< GZIP compression.
    "vnd.onvif.metadata.exi.onvif", //< For EXI using ONVIF default compression parameters.
    "vnd.onvif.metadata.exi.ext", //< For EXI using compression parameters that are sent inband.
};

void OnvifMetadataRtpParser::setSdpInfo(const nx::streaming::Sdp::Media& sdp)
{
    NX_DEBUG(this, "Got SDP information about the media track: %1", sdp);
    m_sdp = sdp;
}

QnAbstractMediaDataPtr OnvifMetadataRtpParser::nextData()
{
    NX_VERBOSE(this, "Next data has been requested");
    return m_metadata;
}

bool OnvifMetadataRtpParser::processData(
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;
    NX_VERBOSE(this, "Processing next RTP packet");

    const quint8* const currentRtpPacketBuffer = rtpBufferBase + bufferOffset;
    if (bytesRead < RtpHeader::kSize)
    {
        NX_VERBOSE(this,
            "Provided buffer size (%1 bytes) is less than the mimimal RTP header size (%2 bytes)",
            bytesRead, RtpHeader::kSize);

        return cleanUpOnError();
    }

    const auto* const rtpHeader = (RtpHeader*)(currentRtpPacketBuffer);
    const bool receivingNewDataChunk = m_currentDataChunkTimestamp != kInvalidTimestamp
        && rtpHeader->timestamp != m_currentDataChunkTimestamp;

    if (receivingNewDataChunk)
    {
        m_metadata = makeCompressedMetadata();
        if (m_metadata)
            gotData = true;

        m_buffer.clear();
        m_currentDataChunkTimestamp = rtpHeader->timestamp;
    }

    const std::optional<int> fullRtpHeaderSize =
        calculateFullRtpHeaderSize(currentRtpPacketBuffer, bytesRead);

    if (!fullRtpHeaderSize)
    {
        NX_VERBOSE(this, "Unable to calculate header size");
        return cleanUpOnError();
    }

    m_buffer.append(
        (const char*) (currentRtpPacketBuffer + *fullRtpHeaderSize),
        bytesRead - *fullRtpHeaderSize);

    return true;
}

bool OnvifMetadataRtpParser::cleanUpOnError()
{
    m_buffer.clear();
    return false;
}

QnCompressedMetadataPtr OnvifMetadataRtpParser::makeCompressedMetadata()
{
    const auto metadataPacket = std::make_shared<QnCompressedMetadata>(
        MetadataType::Onvif,
        m_buffer.size());

    metadataPacket->setData(m_buffer);

    return metadataPacket;
}
