// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace nx::rtp {

constexpr size_t kRtcpNackQueueSize = 1024;

struct NX_RTP_API RtcpNackPart
{
    uint16_t pid;
    uint16_t blp;
    bool parse(const uint8_t* data, size_t size);
    std::vector<uint16_t> getSequenceNumbers() const;
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
};

struct NX_RTP_API RtcpNackReport
{
    RtcpNackReportHeader header;
    std::vector<RtcpNackPart> nacks;
    bool parse(const uint8_t* data, size_t size);
    std::vector<uint16_t> getAllSequenceNumbers() const;
};

} // namespace nx::rtp
