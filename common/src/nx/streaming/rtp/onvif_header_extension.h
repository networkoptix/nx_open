#pragma once

#include <chrono>

#include <utils/media/bitStream.h>
#include <nx/streaming/rtp/rtp.h>

namespace nx {
namespace streaming {
namespace rtp {

// See: ONVIF Streaming Specification Ver. 17.06 (6.3 RTP header extension)
struct OnvifHeaderExtension
{
    static const int kSize = RtpHeaderExtension::kSize + 12;

    bool read(const uint8_t* data, int size);
    int write(uint8_t* data, int size) const;

    std::chrono::microseconds ntp;
    bool cBit = false;
    bool eBit = false;
    bool dBit = false;
    bool tBit = false;
    uint8_t cSeq = 0;
};


} // namespace rtp
} // namespace streaming
} // namespace nx
