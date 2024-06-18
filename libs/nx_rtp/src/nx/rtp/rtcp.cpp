// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtcp.h"

#include <nx/rtp/rtp.h>
#include <nx/utils/bit_stream.h>

namespace nx::rtp {

constexpr uint8_t kRtpVersion = 2;
constexpr std::chrono::seconds kRtcpInterval(5);
constexpr std::chrono::seconds kRtcpDiscontinuityThreshold(1);

uint32_t getRtcpSsrc(const uint8_t* data, int size)
{
    if (data == nullptr || size < kRtcpHeaderSize)
        return 0;
    uint8_t messageType = data[1];
    uint32_t ssrc = 0;
    switch (messageType)
    {
        case kRtcpGenericFeedback:
        case kRtcpPayloadSpecificFeedback:
        case kRtcpReceiverReport:
            if (size < kRtcpFeedbackHeaderSize)
                break;
            memcpy(&ssrc, &data[8], sizeof(ssrc));
            break;
        case kRtcpSenderReport:
        case kRtcpSourceDesciption:
        default:
            memcpy(&ssrc, &data[4], sizeof(ssrc));
            break;
    }
    return ntohl(ssrc);
}

int buildReceiverReport(
    uint8_t* dstBuffer,
    int bufferLen,
    uint32_t ssrc)
{
    if (bufferLen < kRtcpReceiverReportLength)
        return 0;
    uint8_t* curBuffer = dstBuffer;
    *curBuffer++ = (RtpHeader::kVersion << 6);
    *curBuffer++ = kRtcpReceiverReport;  // packet type
    curBuffer += 2; // skip len field;

    quint32* curBuf32 = (quint32*)curBuffer;
    *curBuf32++ = htonl(ssrc);

    // correct len field (count of 32 bit words -1)
    quint16 len = (quint16)(curBuf32 - (quint32*)dstBuffer);
    *((quint16*)(dstBuffer + 2)) = htons(len - 1);

    return (uint8_t*)curBuf32 - dstBuffer;
}

void buildSourceDescriptionReportInternal(
    nx::utils::BitStreamWriter& bitstream,
    uint32_t ssrc,
    const std::optional<std::string>& cname)
{
    const std::string esDescr = cname.value_or("nx");
    const int sizeBefore = bitstream.getBytesCount();
    bitstream.putBits(2, kRtpVersion);
    bitstream.putBit(0); //< P (padding)
    bitstream.putBits(5, 1); //< source count (SC)
    bitstream.putBits(8, kRtcpSourceDesciption);

    uint16_t* lenPtr = (uint16_t*) (bitstream.getBuffer() + bitstream.getBytesCount());
    bitstream.putBits(16, 0); //< skip length in words - 1

    bitstream.putBits(32, ssrc);
    // SDES items
    bitstream.putBits(8, 1); //< ES_TYPE CNAME
    bitstream.putBits(8, esDescr.size());
    bitstream.putBytes((const uint8_t*)esDescr.data(), esDescr.size());
    while (bitstream.getBitsCount() % 32 != 0)
        bitstream.putBits(8, 0);
    bitstream.flushBits();
    const int size = bitstream.getBytesCount() - sizeBefore;
    // correct len field (count of 32 bit words -1)
    *lenPtr = htons(size / 4 - 1);
}

int buildSourceDescriptionReport(
    uint8_t* dstBuffer,
    int bufferLen,
    uint32_t ssrc,
    const std::optional<std::string>& cname = {})
{
    try
    {
        nx::utils::BitStreamWriter bitstream(dstBuffer, dstBuffer + bufferLen);
        buildSourceDescriptionReportInternal(bitstream, ssrc, cname);
        return bitstream.getBytesCount();
    }
    catch (const nx::utils::BitStreamException&)
    {
        NX_ASSERT(0, "Not enough buffer to write RTCP report");
        return 0;
    }
}

int buildClientRtcpReport(
    uint8_t* dstBuffer,
    int bufferLen,
    const std::optional<std::string>& cname)
{
    const std::string esDescr = cname.value_or("nx");

    NX_ASSERT((size_t)bufferLen >= 20 + esDescr.size());

    constexpr uint32_t kSsrcConst = 0x2a55a9e8;
    constexpr uint32_t kCsrcConst = 0xe8a9552a;

    int size = buildReceiverReport(dstBuffer, bufferLen, kSsrcConst);
    int size2 = buildSourceDescriptionReport(dstBuffer + size, bufferLen - size, kCsrcConst);
    return size + size2;
}

int RtcpSenderReport::write(uint8_t* data, int size) const
{
    try
    {
        auto fraction = makeFraction(ntpTimestamp + kNtpEpochTimeDiff.count() * 1'000'000, 1'000'000);

        nx::utils::BitStreamWriter bitstream(data, data + size);
        bitstream.putBits(2, kRtpVersion);
        bitstream.putBits(6, 0); //< P + RC
        bitstream.putBits(8, kRtcpSenderReport);
        bitstream.putBits(16, 6); //< length in words - 1
        bitstream.putBits(32, ssrc);
        bitstream.putBits(32, fraction.first);
        bitstream.putBits(32, fraction.second);
        bitstream.putBits(32, rtpTimestamp);
        bitstream.putBits(32, packetCount);
        bitstream.putBits(32, octetCount);

        if (cname)
            buildSourceDescriptionReportInternal(bitstream, ssrc, cname);

        bitstream.flushBits();
        return bitstream.getBytesCount();
    }
    catch(const nx::utils::BitStreamException&)
    {
        NX_ASSERT(0, "Not enough buffer to write RTCP report");
        return 0;
    }
}

bool RtcpSenderReport::read(const uint8_t* data, int size)
{
    try {
        uint8_t packetType;
        uint32_t seconds;
        uint32_t fractions;
        nx::utils::BitStreamReader reader(data, data + size);
        reader.skipBits(8); //< version
        packetType = reader.getBits(8);
        if (packetType != kRtcpSenderReport)
            return false;
        reader.skipBits(16); //< length
        ssrc = reader.getBits(32);
        seconds = reader.getBits(32);
        fractions = reader.getBits(32);
        rtpTimestamp = reader.getBits(32);
        packetCount = reader.getBits(32);
        octetCount = reader.getBits(32);

        uint64_t usec = fractions * 1'000'000ULL / std::numeric_limits<uint32_t>::max();
        ntpTimestamp = (seconds - kNtpEpochTimeDiff.count()) * 1'000'000ULL + usec;
    }
    catch(const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}

void RtcpSenderReporter::onPacket(uint32_t size)
{
    m_report.octetCount += size;
    ++m_report.packetCount;
}

bool RtcpSenderReporter::needReport(uint64_t ntpTimestampUSec, uint32_t rtpTimestamp)
{
    m_report.ntpTimestamp = ntpTimestampUSec;
    m_report.rtpTimestamp = rtpTimestamp;
    std::chrono::microseconds ntpTimestamp(ntpTimestampUSec);
    bool needReport =
        ntpTimestamp - m_lastTimestamp > kRtcpDiscontinuityThreshold ||
        ntpTimestamp - m_lastReportTimestamp > kRtcpInterval;
    m_lastTimestamp = ntpTimestamp;
    return needReport;
}

const RtcpSenderReport& RtcpSenderReporter::getReport()
{
    m_lastReportTimestamp = std::chrono::microseconds(m_report.ntpTimestamp);
    return m_report;
}

void RtcpSenderReporter::setSsrc(uint32_t ssrc)
{
    m_report.ssrc = ssrc;
}

void RtcpSenderReporter::setCName(const std::string& cname)
{
    m_report.cname = cname;
}

bool RtcpFirFeedback::getNextFeedback(uint32_t senderSsrc, uint8_t* data, int size)
{
    if (size != kSize)
        return false;
    static constexpr uint16_t kRtcpFirLength = kSize / 4 - 1; //< For header.
    try
    {
        nx::utils::BitStreamWriter bitstream(data, data + size);
        bitstream.putBits(8, 0x80 | kRtcpFeedbackFormatFir);
        bitstream.putBits(8, kRtcpPayloadSpecificFeedback);
        bitstream.putBits(16, kRtcpFirLength);
        bitstream.putBits(32, senderSsrc); //< Sender SSRC.
        bitstream.putBits(32, sourceSsrc); //< Source SSRC. Probably unused.
        bitstream.putBits(32, sourceSsrc);
        bitstream.putBits(8, sequence);
        bitstream.putBits(24, 0);
        bitstream.flushBits();
        NX_ASSERT(kSize == bitstream.getBytesCount());
    }
    catch (const nx::utils::BitStreamException&)
    {
        NX_ASSERT(false, "Got exception while serializing RTCP FIR");
        return false;
    }
    ++sequence;
    return true;
}

} // namespace nx::rtp
