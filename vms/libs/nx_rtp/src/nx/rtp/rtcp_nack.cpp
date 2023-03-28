// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtcp_nack.h"

#include <nx/rtp/rtcp.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/bit_stream.h>

namespace nx::rtp {

constexpr int kRtcpNackPartSize = 4;

bool RtcpNackPart::parse(const uint8_t* data, size_t size)
{
    try
    {
        nx::utils::BitStreamReader reader(data, data + size);
        pid = reader.getBits(16);
        blp = reader.getBits(16);
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}

std::vector<uint16_t> RtcpNackPart::getSequenceNumbers() const
{
    std::vector<uint16_t> result{};
    result.reserve(17);
    result.push_back(pid);
    uint16_t bitmask = blp;
    uint16_t i = pid + 1;
    while (bitmask > 0)
    {
        if (bitmask & 0x1)
            result.push_back(i);
        i += 1;
        bitmask >>= 1;
    }
    return result;
}

uint8_t RtcpNackReportHeader::version() const
{
    return firstByte >> 6;
}

uint8_t RtcpNackReportHeader::padding() const
{
    return (firstByte >> 5) & 0x1;
}

uint8_t RtcpNackReportHeader::format() const
{
    return firstByte & 0x1f;
}

bool RtcpNackReport::parse(const uint8_t* data, size_t size)
{
    // https://www.rfc-editor.org/rfc/rfc4585.html#section-6.1
    try
    {
        nx::utils::BitStreamReader reader(data, data + size);
        header.firstByte = reader.getBits(8);
        header.payloadType = reader.getBits(8);
        header.length = reader.getBits(16);
        header.sourceSsrc = reader.getBits(32);
        header.senderSsrc = reader.getBits(32);
        if (header.payloadType != kRtcpGenericFeedback || header.format() != kRtcpFeedbackFormatNack)
            return 0;
        const uint8_t* p = data + kRtcpFeedbackHeaderSize;
        // length of the RTCP packet in 32-bit words minus one,
        // including the header and any padding.
        for (int i = 0; i < (int) (header.length + 1 - kRtcpFeedbackHeaderSize / 4); ++i)
        {
            RtcpNackPart part;
            if (!part.parse(p, size - (p - data)))
                break;
            nacks.push_back(part);
            p += kRtcpNackPartSize;
        }
        return true;
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
}

std::vector<uint16_t> RtcpNackReport::getAllSequenceNumbers() const
{
    std::vector<uint16_t> result;
    for (const auto& i: nacks)
    {
        const auto sequences = i.getSequenceNumbers();
        result.insert(result.end(), sequences.begin(), sequences.end());
    }
    return result;
}

} // namespace nx::rtp
