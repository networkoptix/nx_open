// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/rtp/rtp.h>

namespace nx::rtp {

// See: ONVIF Streaming Specification Ver. 17.06 (6.3 RTP header extension)
struct NX_RTP_API OnvifHeaderExtension
{
    static constexpr int kSize = RtpHeaderExtensionHeader::kSize + 12;

    bool read(const uint8_t* data, int size);
    int write(uint8_t* data, int size) const;

    std::chrono::microseconds ntp;
    bool cBit = false;
    bool eBit = false;
    bool dBit = false;
    bool tBit = false;
    uint8_t cSeq = 0;
};

}  // namespace rtp::streaming::nx
