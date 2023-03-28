// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_metadata_rtp_parser.h"

#include <nx/media/in_stream_compressed_metadata.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

static constexpr int kMaxMetadataPacketSizeBytes = 10'000'000;

void BaseMetadataRtpParser::setSdpInfo(const Sdp::Media& sdp)
{
    NX_DEBUG(this, "Got SDP information about the media track: %1", sdp);
    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);

    m_sdp = sdp;
}

QnAbstractMediaDataPtr BaseMetadataRtpParser::nextData()
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

Result BaseMetadataRtpParser::processData(
    const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;
    NX_VERBOSE(this, "Processing next RTP packet");

    const quint8* const data = rtpBufferBase + bufferOffset;

    if (rtpHeader.marker)
        m_trustMarkerBit = true;

    const uint32_t currentDataChunkTimestamp = rtpHeader.getTimestamp();

    if ((m_previousDataChunkTimestamp
        && *m_previousDataChunkTimestamp != currentDataChunkTimestamp
        && !m_trustMarkerBit)
        || (m_buffer.size() > kMaxMetadataPacketSizeBytes))
    {
        NX_VERBOSE(this,
            "Got an RTP packet with timestamp that differs from the previous packet, "
            "creating metadata");

        m_metadata = makeCompressedMetadata(*m_previousDataChunkTimestamp);
        if (m_metadata)
            gotData = true;
    }

    m_previousDataChunkTimestamp = currentDataChunkTimestamp;
    m_buffer.append((const char*)(data), bytesRead);

    if (gotData)
        return { true };

    if (rtpHeader.marker)
    {
        NX_VERBOSE(this, "Got an RTP packet with the marker bit set, creating metadata");
        m_metadata = makeCompressedMetadata(currentDataChunkTimestamp);
        if (m_metadata)
            gotData = true;
    }

    return { true };
}

void BaseMetadataRtpParser::clear()
{
    m_buffer.clear();
}

QnCompressedMetadataPtr BaseMetadataRtpParser::makeCompressedMetadata(int32_t timestamp)
{
    const auto metadataPacket = std::make_shared<nx::media::InStreamCompressedMetadata>(
        m_sdp.rtpmap.codecName, m_buffer);

    metadataPacket->timestamp = timestamp;

    m_buffer.clear();

    return metadataPacket;
}

} // namespace nx::rtp
