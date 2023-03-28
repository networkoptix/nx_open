// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp_parser.h"

#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

RtpParser::RtpParser(int payloadType, StreamParserPtr codecParser):
    m_codecParser(std::move(codecParser)),
    m_payloadType(payloadType)
{
}

Result RtpParser::processData(
    uint8_t* buffer, int offset, int size, bool& packetLoss, bool& gotData)
{
    gotData = false;
    packetLoss = false;
    auto packetData = buffer + offset;

    RtpHeaderData rtpHeader;
    if (!rtpHeader.read(packetData, size))
        return {false, "Failed to parse RTP header"};

    if (rtpHeader.header.payloadType != m_payloadType)
    {
        // Reset sequence check, since some poor cameras use same sequence for different payload
        // types and other cameras use the separate sequences. The both cases lead to false packet
        // loss.
        m_sequenceNumber.reset();
        return true; // Skip other payloads.
    }

    uint16_t sequenceNumber = rtpHeader.header.getSequence();
    if (m_sequenceNumber.has_value() &&
        m_sequenceNumber.value() != uint16_t(sequenceNumber - 1) &&
        m_sequenceNumber != sequenceNumber) // Some cameras send duplicate packets, ignore it
    {
        packetLoss = true;
        NX_DEBUG(this, "RTP packet loss detected: previous sequence number: %1, current: %2",
            m_sequenceNumber.value(), sequenceNumber);
    }
    m_sequenceNumber = sequenceNumber;

    if (rtpHeader.extension.has_value())
    {
        auto result = m_codecParser->processRtpExtension(
            rtpHeader.extension.value().header,
            packetData + rtpHeader.extension->extensionOffset,
            rtpHeader.extension->extensionSize);
        if (!result.success)
            return result;
    }

    if (rtpHeader.payloadSize == 0 && !m_codecParser->forceProcessEmptyData())
    {
        NX_DEBUG(this, "Empty RTP packet");
        return true;
    }

    return m_codecParser->processData(
        rtpHeader.header, buffer, offset + rtpHeader.payloadOffset, rtpHeader.payloadSize, gotData);
}

void RtpParser::clear()
{
    m_codecParser->clear();
}

int RtpParser::getFrequency() const
{
    return m_codecParser->getFrequency();
}

QnAbstractMediaDataPtr RtpParser::nextData()
{
    return m_codecParser->nextData();
}

QString RtpParser::idForToStringFromPtr() const
{
    return NX_FMT("Payload type %1", m_payloadType);
}

} // namespace nx::rtp
