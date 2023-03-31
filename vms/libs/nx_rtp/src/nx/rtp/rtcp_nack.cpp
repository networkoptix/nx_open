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

int RtcpNackPart::serialize(uint8_t* data, size_t size) const
{
    try
    {
        nx::utils::BitStreamWriter bitstream(data, data + size);
        bitstream.putBits(16, pid);
        bitstream.putBits(16, blp);
        bitstream.flushBits();
        return bitstream.getBytesCount();
    }
    catch (const nx::utils::BitStreamException&)
    {
        return 0;
    }
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

int RtcpNackReport::serialize(uint8_t* data, size_t size) const
{
    try
    {
        nx::utils::BitStreamWriter bitstream(data, data + size);
        bitstream.putBits(8, header.firstByte);
        bitstream.putBits(8, header.payloadType);
        bitstream.putBits(16, header.length);
        bitstream.putBits(32, header.sourceSsrc);
        bitstream.putBits(32, header.senderSsrc);
        bitstream.flushBits();
        for (const auto& i: nacks)
        {
            int serialized = bitstream.getBytesCount();
            int toSkip = i.serialize(data + serialized, size - serialized);
            bitstream.skipBits(toSkip * 8);
        }
        return bitstream.getBytesCount();
    }
    catch (const nx::utils::BitStreamException&)
    {
        return 0;
    }
}

size_t RtcpNackReport::serialized() const
{
    return kRtcpFeedbackHeaderSize + 4 * nacks.size();
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

RtcpNackReport buildNackReport(
    uint32_t sourceSsrc,
    uint32_t senderSsrc,
    const std::vector<uint16_t>& sequences)
{
    RtcpNackReport result;
    result.header.firstByte = 0x80 | kRtcpFeedbackFormatNack;
    result.header.payloadType = kRtcpGenericFeedback;
    // THe length of the RTCP packet in 32-bit words minus one, including the header and any
    // padding.
    result.header.length = (kRtcpFeedbackHeaderSize / 4) - 1;
    result.header.sourceSsrc = sourceSsrc;
    result.header.senderSsrc = senderSsrc;

    if (sequences.empty())
        return result;

    std::optional<uint16_t> startSequence;
    uint16_t sequenceMask = 0;
    constexpr int kMaxSequenceNumber = 4096;
    int sequenceLimit = std::min(kMaxSequenceNumber, (int) sequences.size());
    for (int i = 0; i < sequenceLimit; ++i)
    {
        if (!startSequence)
            startSequence = sequences[i];
        bool isStart = (*startSequence == sequences[i]);
        bool isLast = (i == (int) sequences.size() - 1);
        bool isOutOfMask = (sequences[i] - *startSequence > 16 || sequences[i] < *startSequence);
        if (!isOutOfMask && !isStart)
        {
            sequenceMask |= (1 << (sequences[i] - *startSequence - 1));
        }
        if (isLast || isOutOfMask)
        {
            result.nacks.emplace_back(RtcpNackPart{*startSequence, sequenceMask});
            sequenceMask = 0;
        }
        if (isOutOfMask)
        {
            *startSequence = sequences[i];
        }
        if (isLast && isOutOfMask)
        {
            result.nacks.emplace_back(RtcpNackPart{*startSequence, sequenceMask});
        }
    }
    result.header.length += result.nacks.size();
    return result;
}

} // namespace nx::rtp
