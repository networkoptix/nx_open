// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <list>

#include <nx/utils/byte_array.h>

#include "rtcp_nack.h"
#include "time_linearizer.h"

namespace nx::rtp {

class NX_RTP_API ReorderingCache
{
public:
    enum Status
    {
        pass, /**< No reordering required, can pass the frame into the next filter. */
        skip, /**< No reordering required, the frame should be skipped because it was already passed. */
        wait, /**< Reordering is in progress, shouldn't pass the frame. */
        flush, /**< Reordering is done, can flush the frames from the reorderer. */
    };
public:
    ReorderingCache(int queueLimit = (int) kRtcpNackQueueSize);
    Status pushPacket(const nx::utils::ByteArray& packet, uint16_t seq);
    // Call on `flush` status.
    bool getNextPacket(nx::utils::ByteArray& outputPacket);
    // Call on `wait` status.
    std::optional<RtcpNackReport> getNextNackPacket(uint16_t sourceSsrc, uint16_t senderSsrc) const;
private:
    using Packet = std::pair<nx::utils::ByteArray, int64_t>;
private:
    void resetState(int64_t linearized);
    void popPackets();
    void insertPacket(const Packet& packet);
    bool outOfQueue(int64_t seq);
    int getTotalSize() const;
private:
    std::deque<Packet> m_packets;
    std::list<nx::utils::ByteArray> m_packetsToFlush;
    std::deque<int64_t> m_lostSequences;
    std::optional<int64_t> m_lastSeq;
    TimeLinearizer<uint16_t> m_linearizer;
    int m_queueLimit = 0;
};

} // namespace nx::rtp
