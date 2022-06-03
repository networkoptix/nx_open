// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>
#include <chrono>
#include <string>
#include <optional>

namespace nx::streaming::rtp {

constexpr uint8_t kRtcpSenderReport = 200;
constexpr uint8_t kRtcpReceiverReport = 201;
constexpr uint8_t kRtcpSourceDesciption = 202;

struct NX_VMS_COMMON_API RtcpSenderReport
{
    static constexpr uint8_t kSize = 28;

    bool read(const uint8_t* data, int size);
    int write(uint8_t* data, int size) const;

    uint32_t ssrc = 0;
    uint64_t ntpTimestamp = 0;
    uint32_t rtpTimestamp = 0;
    uint32_t packetCount = 0;
    uint32_t octetCount = 0;
    std::optional<std::string> cname;
};

NX_VMS_COMMON_API int buildClientRtcpReport(
    uint8_t* dstBuffer,  
    int bufferLen,
    const std::optional<std::string>& cname = {});

class NX_VMS_COMMON_API RtcpSenderReporter
{
public:
    bool needReport(uint64_t ntpTimestamp, uint32_t rtpTimestamp);
    const RtcpSenderReport& getReport();
    void onPacket(uint32_t size);
    void setSsrc(uint32_t ssrc);
    void setCName(const std::string& cname);
private:
    RtcpSenderReport m_report;
    std::chrono::microseconds m_lastReportTimestamp = std::chrono::microseconds::zero();
    std::chrono::microseconds m_lastTimestamp = std::chrono::microseconds::zero();
};

} // namespace nx::streaming::rtp
