// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/utils/byte_array.h>

#include "rtcp_nack.h"

namespace nx::rtp {

class NX_RTP_API RtcpNackResponder
{
public:
    RtcpNackResponder(size_t queueLimit = kRtcpNackQueueSize);
    void pushPacket(const nx::utils::ByteArray& packet, uint16_t seq);
    void pushRtcpNack(const uint8_t* data, int size);
    bool getNextPacket(nx::utils::ByteArray& outputPacket);
private:
    std::deque<nx::utils::ByteArray> m_packets;
    std::deque<uint16_t> m_reports;
    uint16_t m_startSeq = 0;
    size_t m_queueLimit = 0;
};

} // namespace nx::rtp
