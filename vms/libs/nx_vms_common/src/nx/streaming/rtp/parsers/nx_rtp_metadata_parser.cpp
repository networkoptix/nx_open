// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_rtp_metadata_parser.h"

#include <nx/media/meta_data_packet.h>

namespace nx::rtp {

Result NxRtpMetadataParser::processData(
    const RtpHeader& /*rtpHeader*/,
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;

    const quint8* payload = rtpBufferBase + bufferOffset;
    QByteArray data((const char*) payload, bytesRead);
    if (data.startsWith("clock="))
    {
        gotData = true;
        m_rangeHeader = std::move(data);
    }

    return {true};
}

QnAbstractMediaDataPtr NxRtpMetadataParser::nextData()
{
    QnAbstractMediaDataPtr result;
    if (!m_rangeHeader.isEmpty())
    {
        result = QnCompressedMetadata::createMediaEventPacket(
            /*timestamp*/0,
            nx::media::StreamEvent::archiveRangeChanged,
            m_rangeHeader);
        m_rangeHeader.clear();
    }
    return result;
}

} // namespace nx::rtp
