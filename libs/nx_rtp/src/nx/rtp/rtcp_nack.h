// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <vector>

#include <stddef.h>
#include <stdint.h>

namespace nx::rtp {

constexpr size_t kRtcpNackQueueSize = 1024;

struct NX_RTP_API RtcpNackPart
{
    uint16_t pid;
    uint16_t blp;
    bool parse(const uint8_t* data, size_t size);
    int serialize(uint8_t* data, size_t size) const;
    std::vector<uint16_t> getSequenceNumbers() const;
    bool operator==(const RtcpNackPart& lhs) const = default;
};

struct NX_RTP_API RtcpNackReportHeader
{
    uint8_t firstByte;
    uint8_t payloadType;
    uint16_t length;
    uint32_t sourceSsrc;
    uint32_t senderSsrc;
    uint8_t version() const;
    uint8_t padding() const;
    uint8_t format() const;
    bool operator==(const RtcpNackReportHeader& lhs) const = default;
};

struct NX_RTP_API RtcpNackReport
{
    RtcpNackReportHeader header;
    std::vector<RtcpNackPart> nacks;
    bool parse(const uint8_t* data, size_t size);
    int serialize(uint8_t* data, size_t size) const;
    size_t serialized() const;
    std::vector<uint16_t> getAllSequenceNumbers() const;
    bool operator==(const RtcpNackReport& lhs) const = default;
};

NX_RTP_API RtcpNackReport buildNackReport(
    uint32_t sourceSsrc,
    uint32_t senderSsrc,
    const std::vector<uint16_t>& sequences);

} // namespace nx::rtp
