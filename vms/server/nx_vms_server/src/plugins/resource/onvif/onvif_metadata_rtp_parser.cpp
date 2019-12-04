#include "onvif_metadata_rtp_parser.h"

#include <nx/utils/log/log.h>

#include <nx/streaming/in_stream_compressed_metadata.h>
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
    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);

    m_sdp = sdp;
}

QnAbstractMediaDataPtr OnvifMetadataRtpParser::nextData()
{
    NX_VERBOSE(this, "Next data has been requested");
    QnAbstractMediaDataPtr result;
    if (m_metadata)
    {
        NX_VERBOSE(this, "Got metadata");
        result = m_metadata;
        m_metadata.reset();
    }

    return result;
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
    const std::optional<int> fullRtpHeaderSize =
        calculateFullRtpHeaderSize(currentRtpPacketBuffer, bytesRead);

    if (!fullRtpHeaderSize)
    {
        NX_VERBOSE(this, "Unable to calculate header size");
        return cleanUpOnError();
    }

    if (bytesRead < fullRtpHeaderSize)
    {
        NX_VERBOSE(this,
            "Provided buffer size (%1 bytes) is less than calculated RTP header size (%2 bytes)",
            bytesRead, fullRtpHeaderSize);

        return cleanUpOnError();
    }

    const auto* const rtpHeader = (RtpHeader*)(currentRtpPacketBuffer);
    m_currentDataChunkTimestamp = rtpHeader->timestamp;

    m_buffer.append(
        (const char*) (currentRtpPacketBuffer + *fullRtpHeaderSize),
        bytesRead - *fullRtpHeaderSize);

    if (rtpHeader->marker)
    {
        NX_VERBOSE(this, "Got an RTP packet with the marker bit set, creating metadata");
        m_metadata = makeCompressedMetadata();
        if (m_metadata)
            gotData = true;
    }

    return true;
}

bool OnvifMetadataRtpParser::cleanUpOnError()
{
    m_buffer.clear();
    return false;
}

QnCompressedMetadataPtr OnvifMetadataRtpParser::makeCompressedMetadata()
{
    const auto metadataPacket = std::make_shared<nx::streaming::InStreamCompressedMetadata>(
        m_sdp.rtpmap.codecName,
        std::move(m_buffer));

    metadataPacket->timestamp = m_currentDataChunkTimestamp;

    m_buffer.clear();

    return metadataPacket;
}
