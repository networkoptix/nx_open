// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <ranges>
#include <string_view>

#include <gtest/gtest.h>

#include <nx/network/socket_global.h>
#include <nx/network/ssl/context.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/webrtc/webrtc_session.h>
#include <nx/webrtc/webrtc_session_pool.h>

namespace nx::webrtc::test {
namespace {

using namespace std::literals;

CodecParametersConstPtr makeVideoParameters(AVCodecID codec)
{
    AVCodecParameters* parameters = avcodec_parameters_alloc();
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = codec;
    parameters->width = 640;
    parameters->height = 480;
    const auto result = std::make_shared<const CodecParameters>(parameters);
    avcodec_parameters_free(&parameters);
    return result;
}

CodecParametersConstPtr makeAudioParameters(AVCodecID codec)
{
    AVCodecParameters* parameters = avcodec_parameters_alloc();
    parameters->codec_type = AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = codec;
    parameters->sample_rate = 8000;
    av_channel_layout_default(&parameters->ch_layout, 1);
    const auto result = std::make_shared<const CodecParameters>(parameters);
    avcodec_parameters_free(&parameters);
    return result;
}

// A Device that has a video codec, and an audio one when `audioCodec` is given, but no frames
// and no transcoder.
class Device: public AbstractCameraDataProvider
{
public:
    Device(QnVirtualCameraResourcePtr camera,
        AVCodecID codec,
        AVCodecID audioCodec = AV_CODEC_ID_NONE):
        m_camera(std::move(camera)),
        m_video(makeVideoParameters(codec)),
        m_audio(audioCodec == AV_CODEC_ID_NONE ? nullptr : makeAudioParameters(audioCodec))
    {
    }

    bool canAcceptData() const override { return true; }
    void putData(const QnAbstractDataPacketPtr&) override {}
    void clearUnprocessedData() override {}

    bool isStarted() const override { return false; }
    bool addVideoTranscoder(
        QnFfmpegVideoTranscoder::Config, const nx::core::transcoding::Settings&) override
    {
        return false;
    }
    bool addAudioTranscoder(AVCodecID) override { return false; }
    bool isTranscodingEnabled(QnAbstractMediaData::DataType) const override { return false; }
    CodecParametersConstPtr getVideoCodecParameters() const override { return m_video; }
    CodecParametersConstPtr getAudioCodecParameters() const override { return m_audio; }
    void setStreamDataFilter(nx::vms::api::StreamDataFilters) override {}
    QnVirtualCameraResourcePtr resource() override { return m_camera; }

    bool isLiveMode() const override { return true; }
    bool paused() const override { return false; }
    bool play(int64_t, bool) override { return true; }
    bool changeQuality(nx::vms::api::StreamIndex) override { return true; }
    bool pause() override { return true; }
    bool resume() override { return true; }
    bool nextFrame() override { return true; }
    void stop() override {}
    void suspend() override {}

private:
    const QnVirtualCameraResourcePtr m_camera;
    const CodecParametersConstPtr m_video;
    const CodecParametersConstPtr m_audio;
};

// The space-separated values of every SDP line starting with `prefix`, in order.
std::vector<std::string> valuesOf(std::string_view sdp, std::string_view prefix)
{
    std::vector<std::string> result;
    for (const auto line: sdp | std::views::split("\r\n"sv))
    {
        const std::string_view text(line.begin(), line.end());
        if (!text.starts_with(prefix))
            continue;
        for (const auto value: text.substr(prefix.size()) | std::views::split(' '))
            result.emplace_back(value.begin(), value.end());
    }
    return result;
}

struct BundleTagInSdp
{
    std::string mid;
    int mLineIndex = -1;
};

// The tag of the BUNDLE group as the offer itself states it: the first mid of the group, and
// where that mid's m-line sits. Every m-line has exactly one a=mid, so the a=mid lines are the
// m-lines, in order.
BundleTagInSdp bundleTagOf(std::string_view offer)
{
    const auto bundled = valuesOf(offer, "a=group:BUNDLE ");
    if (bundled.empty())
        return {};

    const auto mids = valuesOf(offer, "a=mid:");
    const auto it = std::ranges::find(mids, bundled.front());
    if (it == mids.end())
        return {bundled.front(), -1};

    return {bundled.front(), (int) (it - mids.begin())};
}

// The media description of `mid`: its m= line and everything up to the next one.
std::string mediaSectionOf(std::string_view sdp, const std::string& mid)
{
    std::string section;
    bool found = false;
    for (const auto line: sdp | std::views::split("\r\n"sv))
    {
        const std::string_view text(line.begin(), line.end());
        if (text.starts_with("m="))
        {
            if (found)
                break;
            section.clear();
        }
        section += std::string(text) + "\r\n";
        found = found || text == "a=mid:" + mid;
    }
    return found ? section : std::string();
}

// Of the right shape, and that is all processSdpAnswer() asks of it: it is not checked against
// a certificate.
constexpr auto kFingerprint =
    "a=fingerprint:sha-256 "
    "00:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:"
    "10:11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F";

// An answer that carries every m-line of `mids` but takes only those of `bundledMids`. Nothing
// but the BUNDLE group and the a=mid lines is read out of an answer, see SdpParseResult::parse(),
// so the m-lines here say nothing else about themselves.
std::string makeAnswer(
    const std::vector<std::string>& mids, const std::vector<std::string>& bundledMids)
{
    std::string answer = "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\n";
    answer += std::string(kFingerprint) + "\r\n";

    answer += "a=group:BUNDLE";
    for (const auto& mid: bundledMids)
        answer += " " + mid;
    answer += "\r\n";

    for (const auto& mid: mids)
        answer += "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\na=mid:" + mid + "\r\n";

    return answer;
}

// A Session that streams Device `a`, and can be handed `b`, `c` or `d`. Only `b` is MJPEG and
// needs a transcoder its Device lacks, so adding `b` always fails, while `a`, `c` and `d` stream
// VP8 as is -- `d` with a G.711 audio track beside it, the only Device here that has two.
class WebRtcSession: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        nx::network::ssl::Context::instance()->setDefaultCertificate(
            nx::network::ssl::makeCertificateAndKey({"Common Name", "US", "Organization"}));

        m_session = std::make_unique<Session>(
            m_context.systemContext(),
            &m_pool,
            [this](nx::Uuid id, auto&&...) -> AbstractCameraDataProviderPtr
            {
                if (m_b->getId() == id)
                    return std::make_shared<Device>(m_b, AV_CODEC_ID_MJPEG);

                if (m_d->getId() == id)
                    return std::make_shared<Device>(m_d, AV_CODEC_ID_VP8, AV_CODEC_ID_PCM_MULAW);

                return std::make_shared<Device>(m_c->getId() == id ? m_c : m_a, AV_CODEC_ID_VP8);
            },
            Session::generateLocalUfrag(),
            nx::network::SocketAddress("127.0.0.1", 7001),
            SessionConfig{},
            /*stunServers*/ std::vector<nx::network::SocketAddress>{},
            Purpose::send);
        m_session->setTranscodingParams(nx::vms::api::WebRtcMethod::srtp,
            /*transcodingParams*/ {}, //< Nothing forces transcoding; the codec decides.
            /*resolution*/ std::nullopt,
            /*resolutionWhenTranscoding*/ std::nullopt,
            /*mseFormat*/ {});
    }

    nx::network::SocketGlobals::InitGuard m_socketGlobals;
    nx::vms::common::test::Context m_context;
    nx::utils::TimerManager m_timerManager;
    SessionPool m_pool{m_context.systemContext(),
        &m_timerManager,
        /*transcodePolicy*/ {},
        /*defaultStunServers*/ {}};
    const QnVirtualCameraResourcePtr m_a = QnResourcePoolTestHelper::createCamera();
    const QnVirtualCameraResourcePtr m_b = QnResourcePoolTestHelper::createCamera();
    const QnVirtualCameraResourcePtr m_c = QnResourcePoolTestHelper::createCamera();
    const QnVirtualCameraResourcePtr m_d = QnResourcePoolTestHelper::createCamera();
    std::unique_ptr<Session> m_session;
};

// Given: the Device that was streaming is removed.
// When: the next offer is built.
// Expect: its m-line stays in the offer, rejected, since an offer may gain m-lines but cannot
//     lose or reorder the ones the client has already accepted.
TEST_F(WebRtcSession, DeviceRemoved)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));

    const std::string offer = m_session->constructSdp();
    EXPECT_EQ(valuesOf(offer, "a=mid:"), (std::vector<std::string>{"0", "128"})) << offer;
    EXPECT_NE(offer.find("\r\nm=video 0 "), std::string::npos) << offer;
    EXPECT_EQ(valuesOf(offer, "a=group:BUNDLE "), (std::vector<std::string>{"128"})) << offer;
}

// Given: Device A was removed, freeing its m-line (mid 0), and Device B takes that m-line but
//     fails to initialize.
// When: the next offer is built.
// Expect: the m-line is offered as rejected again, and the BUNDLE group names only m-lines the
//     offer carries -- a mid in the group with no m-line of its own makes
//     setRemoteDescription() reject the offer, and the whole session is over.
TEST_F(WebRtcSession, DeviceFailingOnRecycledMid)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));
    ASSERT_FALSE(m_session->createProvider(m_b->getId(), std::nullopt, {}, std::nullopt));

    const std::string offer = m_session->constructSdp();
    const auto mids = valuesOf(offer, "a=mid:");
    ASSERT_EQ(mids.size(), 2u) << offer; //< the m-line was taken over, not added to
    EXPECT_EQ(mids.back(), "128") << offer;
    EXPECT_NE(offer.find("\r\nm=video 0 "), std::string::npos) << offer;

    for (const auto& bundled: valuesOf(offer, "a=group:BUNDLE "))
        EXPECT_NE(std::ranges::find(mids, bundled), mids.end()) << bundled << "\n" << offer;
}

// Given: the client takes the only track offered.
// When: the answer is applied.
// Expect: the session goes on.
TEST_F(WebRtcSession, ClientTakesTheTrack)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));

    const auto mids = valuesOf(m_session->constructSdp(), "a=mid:");
    ASSERT_EQ(mids, (std::vector<std::string>{"0", "128"}));
    EXPECT_TRUE(m_session->processSdpAnswer(makeAnswer(mids, mids)));
}

// Given: the client takes the data channel but refuses the only media track, as it does when it
//     cannot decode the codec offered.
// When: the answer is applied.
// Expect: the session ends. There is no second offer to try, and nothing is ever going to be
//     played, so the client should not be left waiting out the keep-alive.
TEST_F(WebRtcSession, ClientTakesNoTrack)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));

    const auto mids = valuesOf(m_session->constructSdp(), "a=mid:");
    EXPECT_FALSE(m_session->processSdpAnswer(makeAnswer(mids, {"128"})));
}

// Given: the only Device was removed, so the offer that follows holds no active track at all.
// When: the client answers it.
// Expect: the session goes on. Nothing was offered, so nothing was refused, and the client is
//     free to add another Device next.
TEST_F(WebRtcSession, AnswerToAnOfferWithoutTracks)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));

    const auto mids = valuesOf(m_session->constructSdp(), "a=mid:");
    EXPECT_TRUE(m_session->processSdpAnswer(makeAnswer(mids, {"128"})));
}

// Given: the Device that was streaming is removed.
// When: whatever it left behind is looked for.
// Expect: its encoder is gone with it. Left in place, it would be picked by
//     getLastTimestamps() for the rest of the session, and would packetize whatever of the
//     Device is still queued, under the ssrc of an m-line the Device no longer holds.
TEST_F(WebRtcSession, RemovedDeviceLeavesNoEncoder)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_NE(m_session->muxer()->videoEncoder(m_a->getId()), nullptr);

    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));
    EXPECT_EQ(m_session->muxer()->videoEncoder(m_a->getId()), nullptr);
}

// Given: a Device is added while the client is still answering the offer sent before it, so the
//     offer that announces the new Device goes out before that answer arrives.
// When: the answer to the earlier offer is applied, knowing nothing of the new m-line.
// Expect: the Device added in between keeps streaming. An answer says nothing of m-lines that
//     were not in the offer it answers, and reading that silence as a refusal would drop a
//     Device the client has not been asked about yet.
TEST_F(WebRtcSession, AnswerToAnEarlierOffer)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    const auto offered = valuesOf(m_session->constructSdp(), "a=mid:");
    ASSERT_EQ(offered, (std::vector<std::string>{"0", "128"}));

    ASSERT_TRUE(m_session->createProvider(m_c->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_EQ(valuesOf(m_session->constructSdp(), "a=mid:"),
        (std::vector<std::string>{"0", "1", "128"}));

    EXPECT_TRUE(m_session->processSdpAnswer(makeAnswer(offered, offered)));
    EXPECT_NE(m_session->muxer()->videoEncoder(m_c->getId()), nullptr);
}

// Given: a Device that is already streaming is added once more, as a repeated `add` over the
//     data channel would.
// When: the second add is made.
// Expect: it is refused and changes nothing. The Device is keyed by its id, so a second add used
//     to leave the provider that streams in place while giving it another set of m-lines -- and
//     if that second add failed, it was the streaming one that dropProvider() took down.
TEST_F(WebRtcSession, DeviceAddedTwice)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    const auto mids = valuesOf(m_session->constructSdp(), "a=mid:");

    EXPECT_FALSE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    EXPECT_NE(m_session->muxer()->videoEncoder(m_a->getId()), nullptr);
    EXPECT_EQ(valuesOf(m_session->constructSdp(), "a=mid:"), mids);
}

// Given: the Device of the first m-line is removed, which offers that m-line as rejected, and
//     another Device is added and takes it over.
// When: the next offer is built.
// Expect: the m-line keeps its place in the offer, and carries a mid the client has not seen
//     before. A rejected m-line stops the transceiver behind it for good, so offering it again
//     under the mid it used to have would tie the new Device to a dead transceiver (RFC 8829
//     section 5.2.2).
TEST_F(WebRtcSession, RecycledMLineGetsANewMid)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_EQ(
        valuesOf(m_session->constructSdp(), "a=mid:"), (std::vector<std::string>{"0", "128"}));

    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));
    ASSERT_TRUE(m_session->createProvider(m_c->getId(), std::nullopt, {}, std::nullopt));

    const std::string offer = m_session->constructSdp();
    const auto mids = valuesOf(offer, "a=mid:");
    ASSERT_EQ(mids.size(), 2u) << offer; //< the m-line was taken over, not added to
    EXPECT_NE(mids.front(), "0") << offer;
    EXPECT_EQ(valuesOf(offer, "a=group:BUNDLE "), (std::vector<std::string>{mids.front(), "128"}))
        << offer;
}

// Given: the client takes the video track of a Device but refuses its audio one, as it does when
//     it cannot decode the audio codec offered.
// When: the next offer is built.
// Expect: the m-line the client refused is offered rejected, port 0 and out of the BUNDLE group,
//     while the Device goes on streaming its video. The client answered that m-line with port 0
//     and left its mid out of its own BUNDLE group, which stops the transceiver behind it for
//     good, so offering it as a live, bundled m-line again is what RFC 8829 section 5.2.2
//     forbids -- and the client is free to reject the whole offer over it, taking the video down
//     with it.
TEST_F(WebRtcSession, RefusedTrackIsRetired)
{
    ASSERT_TRUE(m_session->createProvider(m_d->getId(), std::nullopt, {}, std::nullopt));
    const auto mids = valuesOf(m_session->constructSdp(), "a=mid:");
    ASSERT_EQ(mids, (std::vector<std::string>{"0", "1", "128"})); //< video, audio, data channel
    ASSERT_NE(m_session->muxer()->audioEncoder(m_d->getId()), nullptr);

    ASSERT_TRUE(m_session->processSdpAnswer(makeAnswer(mids, {"0", "128"})));

    const std::string offer = m_session->constructSdp();
    EXPECT_EQ(valuesOf(offer, "a=mid:"), mids) << offer; //< the client has seen it, it stays
    EXPECT_NE(offer.find("\r\nm=audio 0 "), std::string::npos) << offer;
    EXPECT_EQ(valuesOf(offer, "a=group:BUNDLE "), (std::vector<std::string>{"0", "128"})) << offer;
    EXPECT_EQ(m_session->muxer()->audioEncoder(m_d->getId()), nullptr);
    EXPECT_NE(m_session->muxer()->videoEncoder(m_d->getId()), nullptr);
}

// Given: the client refused the audio m-line of a Device, and another Device is added, which is
//     what sends the next offer.
// When: that offer is built.
// Expect: the refused m-line is still offered rejected. It is every later offer that has to keep
//     saying so, not just the one right after the answer, and an offer that revives it is one
//     the client may reject whole -- taking down the Device it is adding and the one streaming.
TEST_F(WebRtcSession, RefusedTrackStaysRetiredInLaterOffers)
{
    ASSERT_TRUE(m_session->createProvider(m_d->getId(), std::nullopt, {}, std::nullopt));
    const auto mids = valuesOf(m_session->constructSdp(), "a=mid:");
    ASSERT_EQ(mids, (std::vector<std::string>{"0", "1", "128"}));
    ASSERT_TRUE(m_session->processSdpAnswer(makeAnswer(mids, {"0", "128"})));

    ASSERT_TRUE(m_session->createProvider(m_c->getId(), std::nullopt, {}, std::nullopt));

    const std::string offer = m_session->constructSdp();
    EXPECT_EQ(valuesOf(offer, "a=mid:"), (std::vector<std::string>{"0", "1", "2", "128"}))
        << offer;
    EXPECT_NE(offer.find("\r\nm=audio 0 "), std::string::npos) << offer;
    EXPECT_EQ(valuesOf(offer, "a=group:BUNDLE "), (std::vector<std::string>{"0", "2", "128"}))
        << offer;
}

// Given: a Device is streaming.
// When: the offer is built.
// Expect: the tag of its BUNDLE group is the Device's m-line, not the data channel one. A
//     candidate naming a bundled m-line that is not the tag is lost with that m-line's
//     transport, and the client makes no connectivity check at all (Tracker::sendIce()).
TEST_F(WebRtcSession, BundleTagIsTheFirstBundledMLine)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));

    Session::BundleTag tag;
    const std::string offer = m_session->constructSdp(&tag);
    const auto expected = bundleTagOf(offer);
    ASSERT_EQ(expected.mid, "0") << offer;
    ASSERT_EQ(expected.mLineIndex, 0) << offer;

    EXPECT_EQ(std::to_string(tag.mid), expected.mid) << offer;
    EXPECT_EQ(tag.mLineIndex, expected.mLineIndex) << offer;
}

// Given: the Device of the first m-line is removed and no other takes that m-line over, while a
//     second one goes on streaming from the m-line after it.
// When: the offer is built.
// Expect: the tag passes over the rejected m-line, which is out of the group.
TEST_F(WebRtcSession, BundleTagSkipsARejectedMLine)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(m_session->createProvider(m_c->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));

    Session::BundleTag tag;
    const std::string offer = m_session->constructSdp(&tag);
    ASSERT_NE(offer.find("\r\nm=video 0 "), std::string::npos) << offer;
    const auto expected = bundleTagOf(offer);
    ASSERT_EQ(expected.mLineIndex, 1) << offer; //< the rejected m-line is the one before it

    EXPECT_EQ(std::to_string(tag.mid), expected.mid) << offer;
    EXPECT_EQ(tag.mLineIndex, expected.mLineIndex) << offer;
}

// Given: the only Device is removed, so no m-line of a track is left.
// When: the offer is built.
// Expect: the tag is the data channel m-line, all the group has left.
TEST_F(WebRtcSession, BundleTagWithoutTracksIsTheDataChannel)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));

    Session::BundleTag tag;
    const std::string offer = m_session->constructSdp(&tag);
    const auto expected = bundleTagOf(offer);
    ASSERT_EQ(expected.mid, std::to_string(kDataChannelMid)) << offer;
    ASSERT_EQ(expected.mLineIndex, 1) << offer; //< the rejected m-line of the Device comes first

    EXPECT_EQ(tag.mid, kDataChannelMid) << offer;
    EXPECT_EQ(tag.mLineIndex, expected.mLineIndex) << offer;

    // As the tag it holds the address:port of the group, instead of being `bundle-only` on 0.
    const auto tagSection = mediaSectionOf(offer, std::to_string(kDataChannelMid));
    EXPECT_FALSE(tagSection.starts_with("m=application 0 ")) << tagSection;
    EXPECT_EQ(tagSection.find("a=bundle-only"), std::string::npos) << tagSection;
}

// Given: a Device with a video and an audio track, both taken into the BUNDLE group.
// When: the offer is built.
// Expect: the tag alone carries the address:port of the group, every other bundled m-line is
//     `bundle-only` on port 0 (RFC 8843 section 7.5). The candidates name the tag, so it has to
//     be the m-line that actually holds the transport.
TEST_F(WebRtcSession, OnlyTheBundleTagCarriesThePort)
{
    ASSERT_TRUE(m_session->createProvider(m_d->getId(), std::nullopt, {}, std::nullopt));

    Session::BundleTag tag;
    const std::string offer = m_session->constructSdp(&tag);
    ASSERT_EQ(valuesOf(offer, "a=group:BUNDLE "), (std::vector<std::string>{"0", "1", "128"}))
        << offer;
    ASSERT_EQ(tag.mid, 0) << offer;

    const auto tagSection = mediaSectionOf(offer, "0");
    EXPECT_FALSE(tagSection.starts_with("m=video 0 ")) << tagSection;
    EXPECT_EQ(tagSection.find("a=bundle-only"), std::string::npos) << tagSection;

    for (const auto& mid: {"1"s, std::to_string(kDataChannelMid)})
        EXPECT_NE(mediaSectionOf(offer, mid).find("a=bundle-only"), std::string::npos) << offer;
}

// Given: the Device of the tag m-line is removed after the client answered the offer that
//     carried it, and another Device takes that m-line over under a mid of its own.
// When: the next offer is built.
// Expect: the tag follows to the new mid. RFC 8843 section 7.5 lets a later offer pick any
//     bundled m-line as its tag, and this one is the first of the group.
TEST_F(WebRtcSession, BundleTagFollowsARecycledMLine)
{
    ASSERT_TRUE(m_session->createProvider(m_a->getId(), std::nullopt, {}, std::nullopt));
    Session::BundleTag tag;
    const auto mids = valuesOf(m_session->constructSdp(&tag), "a=mid:");
    ASSERT_EQ(tag.mid, 0);
    ASSERT_TRUE(m_session->processSdpAnswer(makeAnswer(mids, mids)));

    ASSERT_TRUE(m_session->removeProvider(m_a->getId()));
    ASSERT_TRUE(m_session->createProvider(m_c->getId(), std::nullopt, {}, std::nullopt));

    const std::string offer = m_session->constructSdp(&tag);
    const auto expected = bundleTagOf(offer);
    EXPECT_NE(expected.mid, "0") << offer; //< the recycled m-line carries a new mid
    EXPECT_EQ(std::to_string(tag.mid), expected.mid) << offer;
    EXPECT_EQ(tag.mLineIndex, 0) << offer;
    EXPECT_EQ(mediaSectionOf(offer, expected.mid).find("a=bundle-only"), std::string::npos)
        << offer;
}

// Given: the client takes the audio track of a Device but refuses its video one, which was the
//     tag of the offer it answered.
// When: the next offer is built.
// Expect: the tag moves to the audio m-line. The refused one is offered rejected and out of the
//     group, so it holds no transport for a candidate to name.
TEST_F(WebRtcSession, BundleTagLeavesARefusedTrack)
{
    ASSERT_TRUE(m_session->createProvider(m_d->getId(), std::nullopt, {}, std::nullopt));
    Session::BundleTag tag;
    const auto mids = valuesOf(m_session->constructSdp(&tag), "a=mid:");
    ASSERT_EQ(mids, (std::vector<std::string>{"0", "1", "128"}));
    ASSERT_EQ(tag.mid, 0);

    ASSERT_TRUE(m_session->processSdpAnswer(makeAnswer(mids, {"1", "128"})));

    const std::string offer = m_session->constructSdp(&tag);
    ASSERT_NE(offer.find("\r\nm=video 0 "), std::string::npos) << offer;
    EXPECT_EQ(valuesOf(offer, "a=group:BUNDLE "), (std::vector<std::string>{"1", "128"})) << offer;
    EXPECT_EQ(tag.mid, 1) << offer;
    EXPECT_EQ(tag.mLineIndex, 1) << offer;
}

} // namespace
} // namespace nx::webrtc::test
