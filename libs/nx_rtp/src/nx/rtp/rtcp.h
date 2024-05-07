// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <stdint.h>

namespace nx::rtp {

constexpr int kRtcpHeaderSize = 8;
constexpr int kRtcpFeedbackHeaderSize = 12;

constexpr uint8_t kRtcpSenderReport = 200;
constexpr uint8_t kRtcpReceiverReport = 201;
constexpr uint8_t kRtcpSourceDesciption = 202;

constexpr uint8_t kRtcpGenericFeedback = 205;
constexpr uint8_t kRtcpPayloadSpecificFeedback = 206;

// https://www.rfc-editor.org/rfc/rfc4585.html
constexpr uint8_t kRtcpFeedbackFormatNack = 1;
constexpr uint8_t kRtcpFeedbackFormatNackPli = 1;
constexpr uint8_t kRtcpFeedbackFormatNackSli = 2;
constexpr uint8_t kRtcpFeedbackFormatNackRpsi = 3;
constexpr uint8_t kRtcpFeedbackFormatNackAfb = 15;

// https://www.rfc-editor.org/rfc/rfc5104.html
constexpr uint8_t kRtcpFeedbackFormatFir = 4;
constexpr uint8_t kRtcpFeedbackFormatTstr = 5;
constexpr uint8_t kRtcpFeedbackFormatTstn = 6;
constexpr uint8_t kRtcpFeedbackFormatVbcm = 7;

struct NX_RTP_API RtcpSenderReport
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

NX_RTP_API int buildClientRtcpReport(
    uint8_t* dstBuffer,
    int bufferLen,
    const std::optional<std::string>& cname = {});

class NX_RTP_API RtcpSenderReporter
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

struct NX_RTP_API RtcpFirFeedback
{
    static constexpr int kSize = 20;
    static int size() { return kSize; }
    bool getNextFeedback(uint32_t senderSsrc, uint8_t* data, int size);
    uint32_t sourceSsrc = 0;
    uint8_t sequence = 0;
};

NX_RTP_API uint32_t getRtcpSsrc(const uint8_t* data, int size);

} // namespace nx::rtp
