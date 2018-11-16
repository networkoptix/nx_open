#include "rtcp.h"

#include <utils/media/bitStream.h>
#include <utils/common/synctime.h>
#include <nx/streaming/rtp/rtp.h>

namespace nx::streaming::rtp {

static const uint32_t kSsrcConst = 0x2a55a9e8;
static const uint32_t kCsrcConst = 0xe8a9552a;

static const uint8_t kRtpVersion = 2;
static const std::chrono::microseconds kRtcpInterval(5000000);
static const std::chrono::microseconds kRtcpDiscontinuityThreshold(1000000);

int buildClientRtcpReport(uint8_t* dstBuffer, int bufferLen)
{
    QByteArray esDescr("netoptix");

    NX_ASSERT(bufferLen >= 20 + esDescr.size());

    uint8_t* curBuffer = dstBuffer;
    *curBuffer++ = (RtpHeader::kVersion << 6);
    *curBuffer++ = kRtcpReceiverReport;  // packet type
    curBuffer += 2; // skip len field;

    quint32* curBuf32 = (quint32*) curBuffer;
    *curBuf32++ = htonl(kSsrcConst);
    /*
    *curBuf32++ = htonl((quint32) stats->nptTime);
    quint32 fracTime = (quint32) ((stats->nptTime - (quint32) stats->nptTime) * UINT_MAX); // UINT32_MAX
    *curBuf32++ = htonl(fracTime);
    *curBuf32++ = htonl(stats->timestamp);
    *curBuf32++ = htonl(stats->receivedPackets);
    *curBuf32++ = htonl(stats->receivedOctets);
    */

    // correct len field (count of 32 bit words -1)
    quint16 len = (quint16) (curBuf32 - (quint32*) dstBuffer);
    * ((quint16*) (dstBuffer + 2)) = htons(len-1);

    // build source description
    curBuffer = (uint8_t*) curBuf32;
    *curBuffer++ = (RtpHeader::kVersion << 6) + 1;  // source count = 1
    *curBuffer++ = kRtcpSourceDesciption;  // packet type
    *curBuffer++ = 0; // len field = 6 (hi)
    *curBuffer++ = 4; // len field = 6 (low), (4+1)*4=20 bytes
    curBuf32 = (quint32*) curBuffer;
    *curBuf32 = htonl(kCsrcConst);
    curBuffer+=4;
    *curBuffer++ = 1; // ES_TYPE CNAME
    *curBuffer++ = esDescr.size();
    memcpy(curBuffer, esDescr.data(), esDescr.size());
    curBuffer += esDescr.size();
    while ((curBuffer - dstBuffer)%4 != 0)
        *curBuffer++ = 0;
    //return len * sizeof(quint32);

    return curBuffer - dstBuffer;
}

int RtcpSenderReport::write(uint8_t* data, int size) const
{
    try
    {
        auto fraction = makeFraction(ntpTimestamp + kNtpEpochTimeDiff.count() * 1000000, 1000000);

        BitStreamWriter bitstream(data, data + size);
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
        bitstream.flushBits();
        return bitstream.getBytesCount();
    }
    catch(const BitStreamException&)
    {
        return 0;
    }
}

bool RtcpSenderReport::read(const uint8_t* data, int size)
{
    try {
        uint8_t packetType;
        uint32_t seconds;
        uint32_t fractions;
        BitStreamReader reader(data, data + size);
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

        uint64_t usec = fractions * 1000000ULL / std::numeric_limits<uint32_t>::max();
        ntpTimestamp = (seconds - kNtpEpochTimeDiff.count()) * 1000000ULL + usec;
    }
    catch(const BitStreamException&)
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

bool RtcpSenderReporter::needReport(uint64_t ntpTimestamp, uint32_t rtpTimestamp)
{
    m_report.ntpTimestamp = ntpTimestamp;
    m_report.rtpTimestamp = rtpTimestamp;
    std::chrono::microseconds nextNtpTimestamp(ntpTimestamp);
    bool needReport =
        nextNtpTimestamp - m_lastTimestamp > kRtcpDiscontinuityThreshold ||
        nextNtpTimestamp - m_lastReportTimestamp > kRtcpInterval;
    m_lastTimestamp = nextNtpTimestamp;
    return needReport;
}

const RtcpSenderReport& RtcpSenderReporter::getReport()
{
    m_lastReportTimestamp = std::chrono::microseconds(m_report.ntpTimestamp);
    return m_report;
}

} // namespace nx::streaming::rtp
