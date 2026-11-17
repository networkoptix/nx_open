// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/rtp.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/webrtc/webrtc_demuxer.h>
#include <utils/common/synctime.h>

namespace nx::webrtc::test {
namespace {

constexpr uint32_t kSsrc = 1234;

// An SDP offer of a browser publishing H.264: no sprop-parameter-sets, so the codec parameters
// come only from the SPS and PPS in the stream.
constexpr auto kSdp =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "a=rtpmap:96 H264/90000\r\n"
    "a=ssrc:1234 cname:test\r\n";

// A 16x16 frame encoded by x264.
const QByteArray kSps = QByteArray::fromHex(
    "6742c00ad91ec0440000030004000003"
    "00c83c489920");
const QByteArray kPps = QByteArray::fromHex("68cb83cb20");
const QByteArray kIdr = QByteArray::fromHex("6588840af2628000a7be");
const QByteArray kDelta = QByteArray::fromHex("419a020408");

class NoTracks: public Tracks
{
public:
    NoTracks(): Tracks(nullptr) {}
    std::string mimeType() const override { return {}; }
    bool examineSdp(const std::string&) override { return true; }
};

class WebRtcDemuxer: public ::testing::Test
{
protected:
    WebRtcDemuxer()
    {
        m_demuxer.setResource(QnResourcePoolTestHelper::createCamera());
        m_demuxer.setSdp(kSdp);
    }

    // Sends `nal` as a single NAL unit packet; `marker` ends the frame.
    void send(const QByteArray& nal, uint32_t timestamp, bool marker)
    {
        std::vector<uint8_t> packet(sizeof(nx::rtp::RtpHeader));
        auto header = (nx::rtp::RtpHeader*) packet.data();
        header->version = nx::rtp::RtpHeader::kVersion;
        header->marker = marker;
        header->payloadType = 96;
        header->sequence = htons(m_sequence++);
        header->timestamp = htonl(timestamp);
        header->ssrc = htonl(kSsrc);
        packet.insert(packet.end(), nal.begin(), nal.end());
        ASSERT_TRUE(m_demuxer.processData((const char*) packet.data(), packet.size()));
    }

    nx::vms::common::test::Context m_context;
    QnSyncTime m_syncTime; //< The Demuxer timestamps frames with it.
    NoTracks m_tracks;
    Demuxer m_demuxer{&m_tracks};
    uint16_t m_sequence = 1;
};

} // namespace

// A publisher that starts with a delta frame, or whose first SPS/PPS were lost, gives frames with
// no codec parameters. The parser drops them; the demuxer must survive that and pick the stream up
// at the next keyframe.
TEST_F(WebRtcDemuxer, frameWithoutCodecParametersIsDropped)
{
    send(kDelta, 0, /*marker*/ true);
    ASSERT_FALSE(m_demuxer.getNextFrame());

    send(kSps, 3000, /*marker*/ false);
    send(kPps, 3000, /*marker*/ false);
    send(kIdr, 3000, /*marker*/ true);
    const auto frame = m_demuxer.getNextFrame();
    ASSERT_TRUE(frame);
    ASSERT_TRUE(frame->context);
    ASSERT_TRUE(frame->flags & QnAbstractMediaData::MediaFlags_AVKey);
}

} // namespace nx::webrtc::test
