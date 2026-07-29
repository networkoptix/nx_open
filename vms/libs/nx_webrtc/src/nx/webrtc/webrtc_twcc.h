// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include <nx/utils/buffer.h>

namespace nx::webrtc {

// RFC 8285 one-byte extension id we advertise for transport-wide-cc. Id 1 is
// left free because it is conventionally used for sdes:mid.
constexpr uint8_t kTransportCcExtId = 3;

constexpr char kTransportCcUri[] =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";

/**
 * Extract the transport-wide-cc sequence number from an RFC 8285 one- or
 * two-byte RTP header extension block.
 * @param definedByProfile The "defined by profile" field of the extension
 *     header: 0xbede for one-byte, 0x100X for two-byte.
 * @param data Extension bytes after the 4-byte extension header, i.e.
 *     `RtpExtensionData::extensionOffset`.
 * @param size Size of those bytes, i.e. `RtpExtensionData::extensionSize`.
 * @param transportCcId The extension id assigned to transport-wide-cc.
 * @return The sequence number, or std::nullopt if the extension is absent.
 */
NX_WEBRTC_API std::optional<uint16_t> parseTransportCcSeq(
    uint16_t definedByProfile,
    const uint8_t* data,
    int size,
    uint8_t transportCcId);

/**
 * Transport-wide congestion control feedback serializer
 * (draft-holmer-rmcat-transport-wide-cc-extensions-01).
 *
 * The server is a pure receiver: it does not estimate bandwidth. It only
 * records (transport_seq, arrival) tuples and echoes them back so the browser's
 * send-side estimator (libwebrtc GCC) can run. Uses run-length status chunks
 * only, which is sufficient for the typical 100 ms feedback interval.
 */
class NX_WEBRTC_API TwccFeedbackBuilder
{
public:
    void onPacket(uint16_t transportSeq, std::chrono::microseconds arrival);

    bool hasPending() const { return !m_packets.empty(); }

    /**
     * Serialize a TWCC feedback packet covering all currently-buffered packets.
     * On success the internal buffer is cleared.
     * @param dst Destination buffer for the un-encrypted RTCP packet.
     * @param dstSize Capacity of `dst` in bytes.
     * @param senderSsrc Our own SSRC, written as the packet sender.
     * @param mediaSourceSsrc The publisher's video SSRC (RTPFB target).
     * @return Number of bytes written, or 0 on error / nothing-to-send.
     */
    int build(
        uint8_t* dst,
        int dstSize,
        uint32_t senderSsrc,
        uint32_t mediaSourceSsrc);

private:
    struct Entry { uint16_t seq; std::chrono::microseconds arrival; };
    std::vector<Entry> m_packets;
    uint8_t m_feedbackCount = 0;
};

/**
 * Glues packet ingest and the periodic feedback flush. One instance lives in
 * the Demuxer and is driven from its RTP processing path. Output buffers are
 * un-encrypted RTCP packets ready to be SRTCP-encrypted by the caller.
 */
class TwccController
{
public:
    /**
     * @param mediaSourceSsrc The publisher's video SSRC (RTPFB target).
     * @param senderSsrc Our own SSRC (the one we advertise on the recv track).
     */
    void setSsrcs(uint32_t mediaSourceSsrc, uint32_t senderSsrc);

    uint8_t transportCcExtId() const { return kTransportCcExtId; }

    // Feed the transport-cc sequence number of one incoming video RTP packet.
    void onRtpPacket(uint16_t transportSeq, std::chrono::microseconds arrival);

    /**
     * If enough time has elapsed since the last emission, build a feedback
     * packet and append it to `out` as one un-encrypted RTCP packet.
     * @param now Current steady-clock time.
     * @param out Vector the feedback packet is appended to, when one is emitted.
     */
    void maybeBuildFeedback(
        std::chrono::microseconds now,
        std::vector<nx::Buffer>& out);

private:
    uint32_t m_mediaSourceSsrc = 0;
    uint32_t m_senderSsrc = 0;
    TwccFeedbackBuilder m_twcc;
    std::chrono::microseconds m_lastTwccTime{0};
};

} // namespace nx::webrtc
