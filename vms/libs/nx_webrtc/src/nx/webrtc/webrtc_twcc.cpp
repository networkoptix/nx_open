// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_twcc.h"

#include <algorithm>
#include <cstring>

#include <nx/utils/log/log.h>

namespace nx::webrtc {

namespace {

using namespace std::chrono;

constexpr microseconds kTwccInterval = milliseconds(100);

// RTCP common header constants -- see <nx/rtp/rtcp.h>.
constexpr uint8_t kRtcpRtpFb = 205; //< Generic RTP feedback.
constexpr uint8_t kFmtTwcc = 15; //< Transport-wide-cc / FMT=15.

// TWCC reference time has 64 ms resolution.
constexpr int64_t kTwccTimeBaseUs = 64'000;
// TWCC receive-delta resolution is 250 us.
constexpr int64_t kTwccDeltaUs = 250;

// Upper bound on the packet-status range covered by a single feedback packet.
// A 100 ms window can't legitimately span this many sequence numbers; a larger
// span means a spurious sequence gap, so drop it rather than allocate for it.
constexpr int kMaxStatusCount = 8192;

// Big-endian writers that advance the cursor past the bytes they emit, so the
// caller lays out a packet as a sequence of writes with no offset arithmetic.
void writeUint8(uint8_t*& p, uint8_t v)
{
    *p++ = v;
}

void writeUint(uint8_t*& p, uint32_t v, int bytes)
{
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8)
        *p++ = uint8_t(v >> shift);
}

void writeUint16(uint8_t*& p, uint16_t v) { writeUint(p, v, 2); }
void writeUint24(uint8_t*& p, uint32_t v) { writeUint(p, v, 3); }
void writeUint32(uint8_t*& p, uint32_t v) { writeUint(p, v, 4); }

// Treat 16-bit transport seq as monotonically increasing with wraparound when
// the forward jump is small.
int seqDiff(uint16_t a, uint16_t b)
{
    return int16_t(a - b);
}

} // namespace

std::optional<uint16_t> parseTransportCcSeq(
    uint16_t definedByProfile,
    const uint8_t* data,
    int size,
    uint8_t transportCcId)
{
    if (!data || size <= 0)
        return std::nullopt;

    const bool oneByte = (definedByProfile == 0xbede);
    const bool twoByte = ((definedByProfile & 0xfff0) == 0x1000);
    if (!oneByte && !twoByte)
        return std::nullopt;

    int i = 0;
    while (i < size)
    {
        if (oneByte)
        {
            uint8_t byte = data[i++];
            uint8_t id = byte >> 4;
            if (id == 0) //< Padding.
                continue;
            if (id == 15) //< Reserved terminator.
                break;
            int len = (byte & 0x0f) + 1;
            if (i + len > size)
                break;
            if (id == transportCcId && len == 2)
                return uint16_t((uint16_t(data[i]) << 8) | uint16_t(data[i + 1]));
            i += len;
        }
        else //< Two-byte.
        {
            if (i + 2 > size)
                break;
            uint8_t id = data[i++];
            uint8_t len = data[i++];
            if (id == 0)
                continue;
            if (i + len > size)
                break;
            if (id == transportCcId && len == 2)
                return uint16_t((uint16_t(data[i]) << 8) | uint16_t(data[i + 1]));
            i += len;
        }
    }
    return std::nullopt;
}

// -------- TwccFeedbackBuilder --------

void TwccFeedbackBuilder::onPacket(uint16_t transportSeq, microseconds arrival)
{
    m_packets.push_back({transportSeq, arrival});
}

int TwccFeedbackBuilder::build(
    uint8_t* dst, int dstSize, uint32_t senderSsrc, uint32_t mediaSourceSsrc)
{
    if (m_packets.empty() || dstSize < 20)
        return 0;

    // Sort by sequence, accounting for 16-bit wraparound by comparing signed
    // differences (valid because a 100 ms window spans far fewer than 2^15 seqs).
    std::sort(m_packets.begin(), m_packets.end(),
        [](const Entry& a, const Entry& b)
        {
            return seqDiff(a.seq, b.seq) < 0;
        });

    const uint16_t baseSeq = m_packets.front().seq;
    const microseconds baseArrival = m_packets.front().arrival;

    // Packet status count = (last seq - base seq) + 1, includes gaps.
    const int statusCount = seqDiff(m_packets.back().seq, baseSeq) + 1;
    if (statusCount <= 0 || statusCount > kMaxStatusCount)
    {
        if (statusCount > kMaxStatusCount)
            NX_VERBOSE(this, "Dropping TWCC feedback: status count %1 too large", statusCount);
        m_packets.clear();
        return 0;
    }

    // Map: index-within-range -> arrival microseconds (if received).
    std::vector<std::optional<microseconds>> arrivals(statusCount);
    for (const auto& p: m_packets)
    {
        int idx = seqDiff(p.seq, baseSeq);
        if (idx >= 0 && idx < statusCount)
            arrivals[idx] = p.arrival;
    }

    // Reference time: 24-bit, multiples of 64 ms.
    int64_t refUs = (baseArrival.count() / kTwccTimeBaseUs) * kTwccTimeBaseUs;
    int32_t refTime24 = int32_t(refUs / kTwccTimeBaseUs) & 0x00ffffff;

    // Build per-packet symbols (00=missing, 01=8-bit delta, 10=16-bit delta).
    enum Symbol : uint8_t { kMissing = 0, kSmall = 1, kLarge = 2 };
    std::vector<uint8_t> symbols(statusCount, kMissing);
    std::vector<int16_t> deltas; //< In units of 250 us, one per received packet.

    int64_t prevUs = refUs;
    for (int i = 0; i < statusCount; ++i)
    {
        if (!arrivals[i])
            continue;
        int64_t arrUs = arrivals[i]->count();
        int64_t deltaTicks = (arrUs - prevUs) / kTwccDeltaUs;
        if (deltaTicks >= 0 && deltaTicks <= 0xff)
        {
            symbols[i] = kSmall;
            deltas.push_back(int16_t(deltaTicks));
        }
        else if (deltaTicks >= INT16_MIN && deltaTicks <= INT16_MAX)
        {
            symbols[i] = kLarge;
            deltas.push_back(int16_t(deltaTicks));
        }
        else
        {
            // Out-of-range delta: mark as missing rather than lie. Leave prevUs
            // untouched -- the receiver does not advance its clock for a missing
            // packet, so neither must we.
            symbols[i] = kMissing;
            continue;
        }
        // Advance by the *quantized* delta, not the true arrival: the receiver
        // reconstructs each arrival as refTime + sum(deltaTicks * 250 us), so
        // basing the next delta on the true time would let truncation error
        // accumulate on the receiver's reconstructed timeline.
        prevUs += deltaTicks * kTwccDeltaUs;
    }

    // Build status chunks. Run-length only -- each chunk encodes up to 8191
    // packets sharing the same symbol.
    std::vector<uint16_t> chunks;
    int i = 0;
    while (i < statusCount)
    {
        uint8_t sym = symbols[i];
        int run = 1;
        while (i + run < statusCount && symbols[i + run] == sym && run < 0x1fff)
            ++run;
        // Run-length chunk layout: bit 15 = 0, bits 14-13 = symbol, bits 12-0 = run.
        uint16_t chunk = uint16_t((sym & 0x3) << 13) | uint16_t(run & 0x1fff);
        chunks.push_back(chunk);
        i += run;
    }

    // Compute total size: 20 (header) + 2*chunks + sum(delta bytes), padded to 4.
    int deltaBytes = 0;
    for (uint8_t s: symbols)
    {
        if (s == kSmall) deltaBytes += 1;
        else if (s == kLarge) deltaBytes += 2;
    }
    int rawSize = 20 + int(chunks.size()) * 2 + deltaBytes;
    int padding = (4 - (rawSize % 4)) % 4;
    int totalSize = rawSize + padding;
    if (totalSize > dstSize)
    {
        NX_VERBOSE(this, "TWCC packet too large for dst buffer (%1 > %2)",
            totalSize, dstSize);
        m_packets.clear();
        return 0;
    }

    // Header.
    uint8_t* p = dst;
    writeUint8(p, 0x80 | kFmtTwcc);
    writeUint8(p, kRtcpRtpFb);
    writeUint16(p, uint16_t((totalSize / 4) - 1));
    writeUint32(p, senderSsrc);
    writeUint32(p, mediaSourceSsrc);
    writeUint16(p, baseSeq);
    writeUint16(p, uint16_t(statusCount));
    writeUint24(p, uint32_t(refTime24));
    writeUint8(p, m_feedbackCount++);

    for (uint16_t c: chunks)
        writeUint16(p, c);

    // Deltas in order.
    int di = 0;
    for (uint8_t s: symbols)
    {
        if (s == kSmall)
            writeUint8(p, uint8_t(deltas[di++]));
        else if (s == kLarge)
            writeUint16(p, uint16_t(deltas[di++]));
    }

    // Zero padding to 4-byte boundary; mark padding bit and length if any.
    if (padding > 0)
    {
        std::memset(p, 0, padding);
        dst[0] |= 0x20;
        dst[totalSize - 1] = uint8_t(padding);
    }

    m_packets.clear();
    return totalSize;
}

// -------- TwccController --------

void TwccController::setSsrcs(uint32_t mediaSourceSsrc, uint32_t senderSsrc)
{
    m_mediaSourceSsrc = mediaSourceSsrc;
    m_senderSsrc = senderSsrc;
}

void TwccController::onRtpPacket(uint16_t transportSeq, microseconds arrival)
{
    // Only buffer if we can eventually flush: maybeBuildFeedback() is a no-op
    // until both SSRCs are known, so buffering earlier would grow unbounded.
    if (m_mediaSourceSsrc == 0 || m_senderSsrc == 0)
        return;
    m_twcc.onPacket(transportSeq, arrival);
}

void TwccController::maybeBuildFeedback(
    microseconds now, std::vector<nx::Buffer>& out)
{
    if (m_mediaSourceSsrc == 0 || m_senderSsrc == 0)
        return;

    if (m_twcc.hasPending() && (now - m_lastTwccTime) >= kTwccInterval)
    {
        nx::Buffer buf(1500, 0);
        int written = m_twcc.build(
            (uint8_t*) buf.data(), (int) buf.size(),
            m_senderSsrc, m_mediaSourceSsrc);
        if (written > 0)
        {
            buf.resize(written);
            out.emplace_back(std::move(buf));
            m_lastTwccTime = now;
        }
    }
}

} // namespace nx::webrtc
