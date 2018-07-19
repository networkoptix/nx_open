#pragma once

#include <chrono>

#include <utils/media/bitStream.h>
#include <nx/streaming/rtp/rtp.h>

namespace nx {
namespace streaming {
namespace rtp {

struct OnvifHeaderExtension
{
    static const int kSize = RtpHeaderExtension::kSize + 12;

    bool read(const uint8_t* data, int size);
    int write(uint8_t* data, int size) const;

    std::chrono::microseconds ntp;
    bool C = false;
    bool E = false;
    bool D = false;
    bool T = false;
    uint8_t Cseq = 0;
};


} // namespace rtp
} // namespace streaming
} // namespace nx
