// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp_parser.h"

#include <nx/rtp/onvif_header_extension.h>
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
        auto result = processRtpExtension(
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

QnAbstractMediaDataPtr RtpParser::nextData(const nx::rtp::RtcpSenderReport& senderReport)
{
    auto data = m_codecParser->nextData();
    if (m_codecParser->isUtcTime())
    {
        m_isUtcTime = true;
    }
    else if (data)
    {
        data->timestamp = getTimestamp(data->timestamp, senderReport);
        m_isUtcTime = senderReport.ntpTimestamp != 0 || m_onvifExtensionTimestamp.has_value();
        m_onvifExtensionTimestamp.reset();
    }
    return data;
}

bool RtpParser::isUtcTime()
{
    return m_isUtcTime;
}

QString RtpParser::idForToStringFromPtr() const
{
    return NX_FMT("Payload type %1", m_payloadType);
}

Result RtpParser::processRtpExtension(
    const RtpHeaderExtensionHeader& header, quint8* data, int size)
{
    // Try to parse onvif timestamp extension (increase the buffer to the extension header, as the
    // analyzer decodes it itself)
    if (header.id() == kOnvifHeaderExtensionId || header.id() == kOnvifHeaderExtensionAltId)
    {
        const uint8_t* headerData = data - RtpHeaderExtensionHeader::kSize;
        int headerSize = size + RtpHeaderExtensionHeader::kSize;
        nx::rtp::OnvifHeaderExtension onvifExtension;
        if (onvifExtension.read(headerData, headerSize))
            m_onvifExtensionTimestamp = onvifExtension.ntp;

        if (header.lengthBytes() == kOnvifHeaderExtensionLength * 4)
            return true;

        // See https://www.onvif.org/specs/stream/ONVIF-Streaming-Spec-v1606.pdf
        // 6.2.2 Compatibility with the JPEG header extension
        data += kOnvifHeaderExtensionLength * 4;
        size -= kOnvifHeaderExtensionLength * 4;
        if (size < RtpHeaderExtensionHeader::kSize)
        {
            NX_DEBUG(this, "Invalid size of RTP header with an double extension, size: %1", size);
            return false;
        }
        RtpHeaderExtensionHeader jpegHeader = *(RtpHeaderExtensionHeader*)(data);
        data += RtpHeaderExtensionHeader::kSize;
        size -= RtpHeaderExtensionHeader::kSize;
        return m_codecParser->processRtpExtension(jpegHeader, data, size);
    }

    return m_codecParser->processRtpExtension(header, data, size);
}

int64_t RtpParser::getTimestamp(uint32_t rtpTime, const nx::rtp::RtcpSenderReport& senderReport)
{
    // onvif
    if (m_onvifExtensionTimestamp)
        return m_onvifExtensionTimestamp->count();

    // rtcp
    if (senderReport.ntpTimestamp > 0)
    {
        const int64_t rtpTimeDiff = int32_t(rtpTime - senderReport.rtpTimestamp)
            * 1'000'000LL / m_codecParser->getFrequency();
        return senderReport.ntpTimestamp + rtpTimeDiff;
    }

    // rtp
    return m_linearizer.linearize(rtpTime) * 1'000'000LL / m_codecParser->getFrequency();
}

} // namespace nx::rtp
