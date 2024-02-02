// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reordering_cache.h"

#include <nx/utils/log/log.h>

namespace nx::rtp {

// Should be less then half of sequence number range for the linearization to work correctly.
constexpr int kMaxCacheSize = std::numeric_limits<uint16_t>::max() / 2;

// Can't make any reorder with limit < 2.
constexpr int kMinCacheSize = 2;

ReorderingCache::ReorderingCache(int queueLimit)
    :
    m_queueLimit(std::min(queueLimit, kMaxCacheSize))
{
}

ReorderingCache::Status ReorderingCache::pushPacket(
    const nx::utils::ByteArray& packet, uint16_t seq)
{
    if (m_queueLimit < kMinCacheSize) //< Always pass.
        return ReorderingCache::Status::pass;

    const auto linearized = m_linearizer.linearize(seq);
    if (!m_lastSeq) //< First packet.
    {
        m_lastSeq = linearized;
        return ReorderingCache::Status::pass;
    }

    if (linearized - *m_lastSeq > m_queueLimit) //< Unexpected case.
    {
        NX_DEBUG(this, "Got sequence with too big distance: %1 vs %2", (uint16_t) *m_lastSeq, (uint16_t) linearized);
        resetState(linearized);
        return ReorderingCache::Status::pass;
    }
    if (linearized - *m_lastSeq == 1
        && m_lostSequences.empty()) //< Normal case without any drops.
    {
        NX_ASSERT(m_packets.empty());
        *m_lastSeq = linearized;
        return ReorderingCache::Status::pass;
    }

    if (outOfQueue(linearized))
        return ReorderingCache::Status::skip;

    insertPacket({packet, linearized});

    popPackets();

    if (!m_packetsToFlush.empty())
    {
        return Status::flush;
    }
    else if (!m_lostSequences.empty())
    {
        return Status::wait;
    }
    else
    {
        NX_ASSERT(false, "Unexpected status");
        return Status::pass;
    }
}

bool ReorderingCache::outOfQueue(int64_t linearized)
{
    bool outOfPackets = (m_packets.empty() || linearized < m_packets.front().second);
    bool outOfLost = (m_lostSequences.empty() || linearized < m_lostSequences.front());
    bool outOfSeq = (!m_lastSeq || *m_lastSeq >= linearized);
    return outOfPackets && outOfLost && outOfSeq;
}

void ReorderingCache::popPackets()
{
    NX_ASSERT(!m_packets.empty());
    while (!m_packets.empty() || !m_lostSequences.empty())
    {
        bool popPackets = false; //< Pop packets queue or lost sequences queue.
        if (!m_packets.empty() && m_lostSequences.empty())
        {
            popPackets = true;
        }
        else if (m_packets.empty() && !m_lostSequences.empty())
        {
            popPackets = false;
        }
        else
        {
            auto firstSequence = m_packets.front().second;
            auto firstLostSequence = m_lostSequences.front();
            NX_ASSERT(firstSequence != firstLostSequence);
            popPackets = (firstSequence < firstLostSequence);
        }
        if (popPackets)
        {
            // It's a packet.
            m_packetsToFlush.emplace_back(std::move(m_packets.front().first));
            m_packets.pop_front();
        }
        else
        {
            // It's a lost packet.
            if (getTotalSize() <= m_queueLimit)
                break;
            m_lostSequences.pop_front();
        }
    }
}

void ReorderingCache::insertPacket(const Packet& packet)
{
    auto pos = std::lower_bound(
        m_packets.begin(),
        m_packets.end(),
        packet.second,
        [](const Packet& packet, int64_t linearized)
        {
            return packet.second < linearized;
        });
    if (pos == m_packets.end())
    {
        NX_ASSERT(packet.second >= *m_lastSeq && packet.second - *m_lastSeq <= m_queueLimit);
        while ((++*m_lastSeq) != packet.second)
            m_lostSequences.push_back(*m_lastSeq);
    }
    else if (pos->second == packet.second)
    {
        pos = m_packets.erase(pos);
    }
    std::erase(m_lostSequences, packet.second);
    m_packets.insert(pos, packet);
}

int ReorderingCache::getTotalSize() const
{
    return m_packets.size() + m_lostSequences.size();
}

void ReorderingCache::resetState(int64_t linearized)
{
    m_packets.clear();
    m_lostSequences.clear();
    m_lastSeq = linearized;
}

bool ReorderingCache::getNextPacket(nx::utils::ByteArray& outputPacket)
{
    if (m_packetsToFlush.empty())
        return false;
    outputPacket = std::move(m_packetsToFlush.front());
    m_packetsToFlush.pop_front();
    return true;
}

std::optional<RtcpNackReport> ReorderingCache::getNextNackPacket(
    uint16_t sourceSsrc, uint16_t senderSsrc) const
{
    if (m_lostSequences.empty())
        return std::nullopt;
    std::vector<uint16_t> sequences(m_lostSequences.size());
    std::transform(
        m_lostSequences.begin(),
        m_lostSequences.end(),
        sequences.begin(),
        [](int64_t linearized)
        {
            return (uint16_t) linearized;
        });
    // A simple way - for any moment report about all lost sequences.
    // Maybe this logic should be corrected in future.
    return buildNackReport(sourceSsrc, senderSsrc, sequences);
}

} // namespace nx::rtp
