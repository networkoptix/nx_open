// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "rtcp_nack.h"

#include <deque>

#include <utils/common/byte_array.h>

namespace nx::streaming::rtp {

class NX_VMS_COMMON_API RtcpNackResponder
{
public:
    RtcpNackResponder(size_t queueLimit = kRtcpNackQueueSize);
    void pushPacket(const QnByteArray& packet, uint16_t seq);
    void pushRtcpNack(const uint8_t* data, int size);
    bool getNextPacket(QnByteArray& outputPacket);
private:
    std::deque<QnByteArray> m_packets;
    std::deque<uint16_t> m_reports;
    uint16_t m_startSeq = 0;
    size_t m_queueLimit = 0;
};

} // namespace nx::streaming::rtp
