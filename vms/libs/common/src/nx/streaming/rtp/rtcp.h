#pragma once

#include <cstdint>
#include <chrono>

namespace nx::streaming::rtp {

static const uint8_t kRtcpSenderReport = 200;
static const uint8_t kRtcpReceiverReport = 201;
static const uint8_t kRtcpSourceDesciption = 202;

struct RtcpSenderReport
{
    static const uint8_t kSize = 28;

    bool read(const uint8_t* data, int size);
    int write(uint8_t* data, int size) const;

    uint32_t ssrc = 0;
    uint64_t ntpTimestamp = 0;
    uint32_t rtpTimestamp = 0;
    uint32_t packetCount = 0;
    uint32_t octetCount = 0;
};

int buildClientRtcpReport(uint8_t* dstBuffer, int bufferLen);

class RtcpSenderReporter
{
public:
    static const uint8_t kRtcpSenderReportR = 200;
    static const uint8_t kRtcpSenderReport = 200;

    bool needReport(uint64_t ntpTimestamp, uint32_t rtpTimestamp);
    const RtcpSenderReport& getReport();
    void onPacket(uint32_t size);

private:
    RtcpSenderReport m_report;
    std::chrono::microseconds m_lastReportTimestamp = std::chrono::microseconds::zero();
    std::chrono::microseconds m_lastTimestamp = std::chrono::microseconds::zero();
};

} // namespace nx::streaming::rtp
