// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtcp_nack_responder.h"

#include <nx/utils/log/log.h>

namespace nx::rtp {

RtcpNackResponder::RtcpNackResponder(size_t queueLimit)
    :
    m_queueLimit(std::min(queueLimit, (size_t) std::numeric_limits<uint16_t>::max()))
{
}

void RtcpNackResponder::pushPacket(const nx::utils::ByteArray& packet, uint16_t seq)
{
    if (m_queueLimit == 0)
        return;
    if (m_packets.size() == m_queueLimit)
    {
        m_packets.pop_front();
        ++m_startSeq;
    }
    if (m_packets.size() == 0)
        m_startSeq = seq;

    if (((uint16_t) (m_startSeq + (uint16_t) m_packets.size())) != seq)
    {
        NX_DEBUG(this, "Pushed unexpected sequence number: %1, expected %2. Cleanup queue.",
            (int) seq, (uint16_t) (m_startSeq + (uint16_t) m_packets.size()));
        m_packets.clear();
        m_startSeq = seq;
    }
    m_packets.emplace_back(packet);
}

void RtcpNackResponder::pushRtcpNack(const uint8_t* data, int size)
{
    RtcpNackReport report;
    if (!report.parse(data, size))
        return;
    const auto reports = report.getAllSequenceNumbers();
    {
        std::stringstream ss;
        for (const auto& i: reports)
        {
            ss << i << " ";
        }
        NX_VERBOSE(this, "Got NACK for seqs: %1", ss.str());
    }
    m_reports.insert(m_reports.end(), reports.begin(), reports.end());
}

bool RtcpNackResponder::getNextPacket(nx::utils::ByteArray& outputPacket)
{
    while (m_reports.size())
    {
        const auto report = m_reports.front();
        m_reports.pop_front();
        size_t offset = report < m_startSeq
            ? (std::numeric_limits<uint16_t>::max() - m_startSeq + report + 1)
            : report - m_startSeq;
        if (offset < m_packets.size())
        {
            NX_VERBOSE(this, "Re-send packet with seq: %1", (int) report);
            outputPacket = m_packets[offset];
            return true;
        }
        NX_VERBOSE(this, "Failed to re-send out of queue packet with seq: %1 (start seq: %2, size: %3, offset: %4)",
            (int) report, m_startSeq, m_packets.size(), offset);
    }
    return false;
}

} // namespace nx::rtp
